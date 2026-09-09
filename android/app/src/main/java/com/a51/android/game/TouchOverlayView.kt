package com.a51.android.game

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.DashPathEffect
import android.graphics.Paint
import android.graphics.RectF
import android.util.AttributeSet
import android.view.MotionEvent
import android.view.View
import com.a51.android.core.NativeBridge
import org.json.JSONArray

/**
 * The on screen controls.
 *
 * All the *logic* (hit testing, joystick maths, dead zones, the save format)
 * lives in C++ - see a51/touch_layout.cpp - and is covered by the host unit
 * tests.  This view only does two jobs: draw what the model describes and turn
 * raw [MotionEvent] coordinates into the model's units.
 *
 * Units: the model works in "screen heights", so a point is (px / height,
 * py / height).  That keeps circles round on any aspect ratio.
 *
 * Edit mode: drag a button to move it, drag the little knob on its rim to
 * resize it, use the bar at the bottom for visibility / opacity / style.
 */
class TouchOverlayView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    companion object {
        const val KIND_STICK = 0
        const val KIND_BUTTON = 1
        const val KIND_DPAD = 2

        const val STYLE_CIRCLE = 0
        const val STYLE_ROUNDED = 1
        const val STYLE_SQUARE = 2

        private const val EDIT_HANDLE_UNITS = 0.035f
    }

    /** One control, as the drawing code sees it. */
    class Ctrl(
        val id: Int,
        val kind: Int,
        var style: Int,
        val name: String,
        val label: String,
        val gadget: String,
        var xFrac: Float,
        var yFrac: Float,
        var size: Float,
        var opacity: Float,
        var visible: Boolean
    ) {
        var cx = 0f
        var cy = 0f
        var radius = 0f
        var active = false
    }

    private val controls = ArrayList<Ctrl>()

    // ------------------------------------------------------------------ state
    var editMode: Boolean = false
        set(value) {
            field = value
            if (!value) {
                dragIndex = -1
                resizeIndex = -1
                selectedIndex = -1
            }
            invalidate()
        }

    var selectedIndex: Int = -1
        private set

    /** Called after the user moved/resized something so the bar can refresh. */
    var onSelectionChanged: (() -> Unit)? = null

    /** Fired when the pause button is tapped. */
    var onPausePressed: (() -> Unit)? = null

    /** Fired when the user commits a change in edit mode. */
    var onLayoutEdited: (() -> Unit)? = null

    private var dragIndex = -1
    private var resizeIndex = -1
    private var dragPointerId = -1

    // -------------------------------------------------------------- painting
    private val fillPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.FILL }
    private val strokePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 3f
    }
    private val textPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
        textAlign = Paint.Align.CENTER
        isFakeBoldText = true
    }
    private val dashPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 2.5f
        color = Color.parseColor("#FFFFA726")
        pathEffect = DashPathEffect(floatArrayOf(10f, 8f), 0f)
    }
    private val knobPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.FILL }
    private val rect = RectF()

    init {
        // The overlay is transparent; we still want it to receive touches over
        // its whole area so that camera drags work anywhere.
        setBackgroundColor(Color.TRANSPARENT)
    }

    // ------------------------------------------------------------------ model

    /** Re-reads the layout from the native model.  Called on change, not per frame. */
    fun refresh() {
        controls.clear()
        if (NativeBridge.available) {
            try {
                val json = NativeBridge.nativeLayoutSnapshot()
                val arr = org.json.JSONObject(json).optJSONArray("controls") ?: JSONArray()
                for (i in 0 until arr.length()) {
                    val o = arr.optJSONObject(i) ?: continue
                    controls.add(
                        Ctrl(
                            id = o.optInt("id", i),
                            kind = o.optInt("kind", KIND_BUTTON),
                            style = o.optInt("style", STYLE_CIRCLE),
                            name = o.optString("name", "?"),
                            label = o.optString("label", ""),
                            gadget = o.optString("gadget", ""),
                            xFrac = o.optDouble("x", 0.5).toFloat(),
                            yFrac = o.optDouble("y", 0.5).toFloat(),
                            size = o.optDouble("size", 0.12).toFloat(),
                            opacity = o.optDouble("opacity", 0.5).toFloat(),
                            visible = o.optBoolean("visible", true)
                        )
                    )
                }
            } catch (e: Exception) {
                android.util.Log.e("A51", "could not read the touch layout", e)
            }
        }
        if (selectedIndex >= controls.size) selectedIndex = -1
        recomputePixels()
        invalidate()
    }

    fun controlAt(index: Int): Ctrl? = controls.getOrNull(index)

    fun controlCount(): Int = controls.size

    fun indexOfId(id: Int): Int = controls.indexOfFirst { it.id == id }

    fun select(index: Int) {
        selectedIndex = if (index in controls.indices) index else -1
        onSelectionChanged?.invoke()
        invalidate()
    }

    private fun recomputePixels() {
        val w = width.toFloat()
        val h = height.toFloat()
        if (w <= 0f || h <= 0f) return
        for (c in controls) {
            c.cx = c.xFrac * w
            c.cy = c.yFrac * h
            c.radius = c.size * h * 0.5f
        }
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        super.onSizeChanged(w, h, oldw, oldh)
        recomputePixels()
        if (NativeBridge.available) NativeBridge.nativeLayoutClamp()
        refresh()
    }

    // ------------------------------------------------------------------ input

    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (!NativeBridge.available) return false
        if (!NativeBridge.nativeIsTouchEnabled()) return false

        val height = height.toFloat()
        if (height <= 0f) return false

        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN,
            MotionEvent.ACTION_POINTER_DOWN -> {
                val index = event.actionIndex
                val id = event.getPointerId(index)
                val ux = event.getX(index) / height
                val uy = event.getY(index) / height

                if (editMode) {
                    beginEdit(id, ux, uy)
                } else {
                    val action =
                        if (event.actionMasked == MotionEvent.ACTION_DOWN) 0 else 5
                    NativeBridge.nativeTouchEvent(action, id, ux, uy)
                    updateActiveFlags()
                    maybePause(ux, uy)
                }
                invalidate()
                return true
            }

            MotionEvent.ACTION_MOVE -> {
                for (i in 0 until event.pointerCount) {
                    val id = event.getPointerId(i)
                    val ux = event.getX(i) / height
                    val uy = event.getY(i) / height
                    if (editMode) {
                        if (id == dragPointerId) continueEdit(ux, uy)
                    } else {
                        NativeBridge.nativeTouchEvent(2, id, ux, uy)
                    }
                }
                if (!editMode) updateActiveFlags()
                invalidate()
                return true
            }

            MotionEvent.ACTION_UP,
            MotionEvent.ACTION_POINTER_UP,
            MotionEvent.ACTION_CANCEL -> {
                val index = event.actionIndex
                val id = event.getPointerId(index)
                val ux = event.getX(index) / height
                val uy = event.getY(index) / height

                if (editMode) {
                    if (id == dragPointerId) endEdit()
                } else {
                    val action = if (event.actionMasked == MotionEvent.ACTION_UP) 1
                    else if (event.actionMasked == MotionEvent.ACTION_CANCEL) 3 else 6
                    NativeBridge.nativeTouchEvent(action, id, ux, uy)
                    updateActiveFlags()
                }
                invalidate()
                return true
            }
        }
        return false
    }

    private fun maybePause(ux: Float, uy: Float) {
        val height = height.toFloat()
        if (height <= 0f) return
        val hit = NativeBridge.nativeLayoutHitTest(ux, uy)
        val ctrl = controls.getOrNull(hit) ?: return
        if (ctrl.gadget == "INPUT_XBOX_BTN_START") {
            onPausePressed?.invoke()
        }
    }

    private fun updateActiveFlags() {
        for (i in controls.indices) {
            controls[i].active = NativeBridge.nativeLayoutControlActive(i)
        }
    }

    // ------------------------------------------------------------------ editing

    private fun beginEdit(pointerId: Int, ux: Float, uy: Float) {
        dragPointerId = pointerId

        val handle = NativeBridge.nativeLayoutHitHandle(ux, uy)
        if (handle >= 0) {
            resizeIndex = handle
            selectedIndex = handle
            onSelectionChanged?.invoke()
            return
        }

        val hit = NativeBridge.nativeLayoutHitTest(ux, uy)
        if (hit >= 0) {
            dragIndex = hit
            selectedIndex = hit
            onSelectionChanged?.invoke()
        } else {
            dragIndex = -1
            selectedIndex = -1
            onSelectionChanged?.invoke()
        }
    }

    private fun continueEdit(ux: Float, uy: Float) {
        val h = height.toFloat()
        val w = width.toFloat()
        if (h <= 0f || w <= 0f) return

        if (resizeIndex in controls.indices) {
            val c = controls[resizeIndex]
            val dx = ux * h - c.cx
            val dy = uy * h - c.cy
            val dist = Math.sqrt((dx * dx + dy * dy).toDouble()).toFloat()
            val diameter = (dist * 2f / h).coerceIn(0.05f, 0.60f)
            NativeBridge.nativeLayoutSetSize(c.id, diameter)
            c.size = diameter
            c.radius = diameter * h * 0.5f
            onSelectionChanged?.invoke()
            return
        }

        if (dragIndex in controls.indices) {
            val c = controls[dragIndex]
            NativeBridge.nativeLayoutMove(c.id, ux * h / w, uy)
            // Native clamps, so read the result back instead of guessing.
            val refreshed = readControl(c.id)
            if (refreshed != null) {
                c.xFrac = refreshed.xFrac
                c.yFrac = refreshed.yFrac
                c.cx = c.xFrac * w
                c.cy = c.yFrac * h
            }
        }
    }

    private fun endEdit() {
        dragIndex = -1
        resizeIndex = -1
        dragPointerId = -1
        onLayoutEdited?.invoke()
        invalidate()
    }

    private fun readControl(id: Int): Ctrl? {
        if (!NativeBridge.available) return null
        return try {
            val json = org.json.JSONObject(NativeBridge.nativeLayoutSnapshot())
            val arr = json.optJSONArray("controls") ?: return null
            for (i in 0 until arr.length()) {
                val o = arr.optJSONObject(i) ?: continue
                if (o.optInt("id", -1) == id) {
                    return Ctrl(
                        id = id,
                        kind = o.optInt("kind", KIND_BUTTON),
                        style = o.optInt("style", STYLE_CIRCLE),
                        name = o.optString("name", "?"),
                        label = o.optString("label", ""),
                        gadget = o.optString("gadget", ""),
                        xFrac = o.optDouble("x", 0.5).toFloat(),
                        yFrac = o.optDouble("y", 0.5).toFloat(),
                        size = o.optDouble("size", 0.12).toFloat(),
                        opacity = o.optDouble("opacity", 0.5).toFloat(),
                        visible = o.optBoolean("visible", true)
                    )
                }
            }
            null
        } catch (e: Exception) {
            null
        }
    }

    // ------------------------------------------------------------------ drawing

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)

        val h = height.toFloat()
        if (h <= 0f) return

        textPaint.textSize = (h * 0.028f).coerceIn(18f, 42f)
        strokePaint.strokeWidth = (h * 0.004f).coerceIn(2f, 6f)

        for (i in controls.indices) {
            val c = controls[i]
            if (!c.visible && !editMode) continue
            if (c.radius <= 0f) continue
            drawControl(canvas, c, i)
        }

        if (editMode) {
            for (i in controls.indices) {
                drawHandle(canvas, i, i == selectedIndex)
            }
        }
    }

    private fun drawControl(canvas: Canvas, c: Ctrl, index: Int) {
        val base = if (c.visible) c.opacity else 0.18f
        val alpha = ((if (c.active) base + 0.30f else base).coerceIn(0f, 1f) * 255f).toInt()

        val fill = when {
            c.active -> Color.argb(alpha, 255, 167, 38)
            c.kind == KIND_STICK -> Color.argb((alpha * 0.35f).toInt(), 51, 198, 244)
            else -> Color.argb((alpha * 0.55f).toInt(), 24, 33, 46)
        }

        fillPaint.color = fill
        strokePaint.color = Color.argb(alpha, 255, 255, 255)

        rect.set(c.cx - c.radius, c.cy - c.radius, c.cx + c.radius, c.cy + c.radius)

        when (c.style) {
            STYLE_SQUARE -> {
                canvas.drawRect(rect, fillPaint)
                canvas.drawRect(rect, strokePaint)
            }
            STYLE_ROUNDED -> {
                val r = c.radius * 0.35f
                canvas.drawRoundRect(rect, r, r, fillPaint)
                canvas.drawRoundRect(rect, r, r, strokePaint)
            }
            else -> {
                canvas.drawCircle(c.cx, c.cy, c.radius, fillPaint)
                canvas.drawCircle(c.cx, c.cy, c.radius, strokePaint)
            }
        }

        if (c.kind == KIND_STICK) {
            // Knob: follows the finger while the stick is held.
            var kx = c.cx
            var ky = c.cy
            val knob = NativeBridge.nativeLayoutStickKnob(index)
            if (knob != null && knob.size >= 2) {
                kx = knob[0] * height
                ky = knob[1] * height
            }
            knobPaint.color = Color.argb((alpha * 0.9f).toInt(), 237, 239, 243)
            canvas.drawCircle(kx, ky, c.radius * 0.42f, knobPaint)
        }

        if (c.kind == KIND_DPAD) {
            strokePaint.color = Color.argb(alpha, 255, 255, 255)
            val r = c.radius * 0.75f
            canvas.drawLine(c.cx - r, c.cy, c.cx + r, c.cy, strokePaint)
            canvas.drawLine(c.cx, c.cy - r, c.cx, c.cy + r, strokePaint)
        }

        if (c.label.isNotEmpty()) {
            textPaint.color = Color.argb(alpha.coerceAtMost(255), 255, 255, 255)
            val ty = c.cy - (textPaint.descent() + textPaint.ascent()) * 0.5f
            canvas.drawText(c.label, c.cx, ty, textPaint)
        }

        if (editMode && index == selectedIndex) {
            canvas.drawCircle(c.cx, c.cy, c.radius + 6f, dashPaint)
        }
    }

    private fun drawHandle(canvas: Canvas, index: Int, selected: Boolean) {
        val c = controls.getOrNull(index) ?: return
        val handle = NativeBridge.nativeLayoutHandle(index) ?: return
        if (handle.size < 2) return

        val hx = handle[0] * height
        val hy = handle[1] * height
        val r = (EDIT_HANDLE_UNITS * height).coerceIn(8f, 26f)

        knobPaint.color = if (selected) Color.parseColor("#FFFFA726") else Color.parseColor("#9933C6F4")
        canvas.drawCircle(hx, hy, r, knobPaint)
        strokePaint.color = Color.WHITE
        canvas.drawCircle(hx, hy, r, strokePaint)
    }
}
