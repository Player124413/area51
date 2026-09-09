package com.a51.android.game

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF
import android.util.AttributeSet
import android.view.View
import com.a51.android.core.NativeBridge

/**
 * The performance HUD.
 *
 * It is a plain [View] on top of the GL surface rather than something drawn in
 * GL: text is far cheaper and sharper this way, and it keeps the renderer free
 * of anything that could cost frame time.  It repaints about ten times a second
 * instead of every frame - a stats overlay does not need 120 Hz.
 */
class HudView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    companion object {
        private const val HISTORY = 120
        private const val REFRESH_MS = 100L
    }

    /** PowerManager thermal status, pushed in by the activity. */
    var thermalStatus: Int = 0

    var enabled: Boolean = true
        set(value) {
            field = value
            visibility = if (value) VISIBLE else GONE
            invalidate()
        }

    private val frameHistory = FloatArray(HISTORY)
    private var historyCount = 0
    private var historyIndex = 0
    private var lastRefresh = 0L

    // Cached strings, rebuilt at most ten times a second.
    private val lines = ArrayList<String>(16)
    private var thermalLabel = ""
    private var fpsValue = 0f

    private val textPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.parseColor("#FFEDEFF3")
        textSize = 24f
    }
    private val bigPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.parseColor("#FFFFA726")
        textSize = 44f
        isFakeBoldText = true
    }
    private val panelPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.parseColor("#7A000000")
        style = Paint.Style.FILL
    }
    private val graphPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.parseColor("#9933C6F4")
        style = Paint.Style.FILL
    }
    private val targetPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.parseColor("#807BE07B")
        style = Paint.Style.STROKE
        strokeWidth = 2f
    }
    private val panelRect = RectF()
    private val graphPath = Path()

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        super.onSizeChanged(w, h, oldw, oldh)
        val scale = (h / 720f).coerceIn(0.6f, 2.2f)
        textPaint.textSize = 20f * scale
        bigPaint.textSize = 40f * scale
    }

    /** Called from the GL thread once per frame. */
    fun onFrame(frameMs: Float) {
        frameHistory[historyIndex] = frameMs
        historyIndex = (historyIndex + 1) % HISTORY
        if (historyCount < HISTORY) historyCount++

        val now = System.currentTimeMillis()
        if (now - lastRefresh < REFRESH_MS) return
        lastRefresh = now

        rebuildLines()
        postInvalidate()
    }

    private fun rebuildLines() {
        if (!NativeBridge.available) return

        lines.clear()
        fpsValue = NativeBridge.nativeGetFps()
        val target = NativeBridge.nativeGetSuggestedFpsCap()

        lines.add(String.format("frame  %.2f ms   avg %.2f   p95 %.2f   worst %.2f",
            NativeBridge.nativeGetFrameTimeMs(),
            NativeBridge.nativeGetAvgFrameMs(),
            NativeBridge.nativeGetP95FrameMs(),
            NativeBridge.nativeGetWorstFrameMs()))
        lines.add(String.format("scale  %.0f%%   back buffer %dx%d   quality %d/%d",
            NativeBridge.nativeGetRenderScale() * 100f,
            NativeBridge.nativeGlGetBackWidth(),
            NativeBridge.nativeGlGetBackHeight(),
            NativeBridge.nativeGetQualityLevel() + 1,
            4))
        lines.add(String.format("stability %.0f%%   dropped %d   down %d / up %d   calls %d",
            NativeBridge.nativeGetStability() * 100f,
            NativeBridge.nativeGetDroppedFrames(),
            NativeBridge.nativeGetQualityDrops(),
            NativeBridge.nativeGetQualityRaises(),
            NativeBridge.nativeGlGetDrawCalls()))
        lines.add(String.format("target %d fps   cache %d MB   files %d   frames %d",
            target,
            NativeBridge.nativeGetTextureBudgetMb(),
            NativeBridge.nativeMountedFileTotal(),
            NativeBridge.nativeGetFrameCount()))

        thermalLabel = thermalName(thermalStatus)
    }

    private fun thermalName(status: Int): String = when {
        status >= 4 -> "CRITICAL"
        status >= 3 -> "SEVERE"
        status >= 2 -> "MODERATE"
        status >= 1 -> "LIGHT"
        else -> "NORMAL"
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        if (!enabled || lines.isEmpty()) return

        val pad = 12f
        val lineHeight = textPaint.textSize * 1.35f
        val panelW = (width * 0.46f).coerceAtMost(760f)
        val panelH = bigPaint.textSize + lineHeight * lines.size + 90f

        panelRect.set(pad, pad, pad + panelW, pad + panelH)
        canvas.drawRoundRect(panelRect, 12f, 12f, panelPaint)

        var y = pad + bigPaint.textSize
        val fpsColor = when {
            fpsValue >= 55f -> Color.parseColor("#FF7BE07B")
            fpsValue >= 28f -> Color.parseColor("#FFFFA726")
            else -> Color.parseColor("#FFFF5A5A")
        }
        bigPaint.color = fpsColor
        canvas.drawText(String.format("%.0f FPS", fpsValue), pad + 14f, y, bigPaint)
        canvas.drawText(thermalLabel, pad + panelW - 150f, y, textPaint)

        y += lineHeight * 0.9f
        for (line in lines) {
            canvas.drawText(line, pad + 14f, y, textPaint)
            y += lineHeight
        }

        drawGraph(canvas, pad + 14f, y - lineHeight * 0.35f, panelW - 28f, 60f)
    }

    private fun drawGraph(canvas: Canvas, x: Float, y: Float, w: Float, h: Float) {
        if (historyCount < 2) return

        // 33 ms covers everything from 30 fps to a bad hitch.
        val maxMs = 33f
        graphPath.reset()
        graphPath.moveTo(x, y + h)

        for (i in 0 until historyCount) {
            val idx = (historyIndex - historyCount + i + HISTORY * 2) % HISTORY
            val ms = frameHistory[idx].coerceAtMost(maxMs)
            val px = x + w * i.toFloat() / (HISTORY - 1).toFloat()
            val py = y + h - h * (ms / maxMs)
            graphPath.lineTo(px, py)
        }
        graphPath.lineTo(x + w, y + h)
        graphPath.close()
        canvas.drawPath(graphPath, graphPaint)

        // Where the target frame rate sits.
        if (NativeBridge.available) {
            val target = NativeBridge.nativeGetSuggestedFpsCap().toFloat()
            if (target > 0f) {
                val targetMs = 1000f / target
                val ty = y + h - h * (targetMs.coerceAtMost(maxMs) / maxMs)
                canvas.drawLine(x, ty, x + w, ty, targetPaint)
            }
        }
    }
}
