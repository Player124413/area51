package com.a51.android.game

import android.content.Context
import android.opengl.GLSurfaceView
import android.util.AttributeSet
import com.a51.android.core.NativeBridge
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10
import kotlin.math.max

/**
 * The GL surface.
 *
 * The interesting part is the frame loop in [Renderer.onDrawFrame]: it measures
 * the real frame time (start to start, so vsync waits count), feeds it to the
 * governor, and then asks the native frame pacer how long to sleep when the
 * target rate is below the display rate.
 */
class GameGLView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null
) : GLSurfaceView(context, attrs) {

    var hud: HudView? = null

    /** Set when OpenGL could not be initialised, so the activity can tell the user. */
    var glError: String? = null
        private set

    private val renderer = Renderer()

    init {
        setEGLContextClientVersion(3)
        // 888 RGB, no alpha (we never blend with the window), 16 bit depth is
        // plenty for the current scene and costs half the bandwidth of 24.
        setEGLConfigChooser(8, 8, 8, 0, 16, 0)
        setPreserveEGLContextOnPause(true)
        setRenderer(renderer)
        renderMode = RENDERMODE_CONTINUOUSLY
    }

    fun setTargetFps(fps: Int) {
        renderer.targetFps = fps
    }

    private inner class Renderer : GLSurfaceView.Renderer {

        var targetFps: Int = 60

        private var startTimeNs = System.nanoTime()
        private var lastFrameStartNs = startTimeNs
        private var initialised = false

        override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
            // Nothing to do: the native side is initialised in onSurfaceChanged
            // where the real size is known.
            startTimeNs = System.nanoTime()
            lastFrameStartNs = startTimeNs
        }

        override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
            if (!NativeBridge.available) {
                glError = "native library unavailable"
                return
            }
            if (!initialised) {
                if (!NativeBridge.nativeGlInit(width, height)) {
                    glError = NativeBridge.nativeGlGetError()
                    return
                }
                initialised = true
            } else {
                NativeBridge.nativeGlResize(width, height)
            }
            NativeBridge.nativeSetScreen(width, height)
        }

        override fun onDrawFrame(gl: GL10?) {
            if (!NativeBridge.available || !initialised) return

            val frameStartNs = System.nanoTime()
            val frameStartMs = (frameStartNs - startTimeNs) / 1_000_000f
            val dtMs = max(0.0001f, (frameStartNs - lastFrameStartNs) / 1_000_000f)
            lastFrameStartNs = frameStartNs

            NativeBridge.nativeBeginFrame()

            val cpuStartNs = System.nanoTime()
            NativeBridge.nativeGlDrawFrame((frameStartNs - startTimeNs) / 1_000_000_000f, dtMs)
            val cpuMs = (System.nanoTime() - cpuStartNs) / 1_000_000f

            // The governor sees the *whole* frame, including the time the
            // previous swap made us wait - that is what "stable" means.
            NativeBridge.nativeEndFrame(dtMs, cpuMs)

            hud?.onFrame(dtMs)

            // Pace: only meaningful when the target is below the refresh rate.
            val nowMs = (System.nanoTime() - startTimeNs) / 1_000_000f
            val sleepMs = NativeBridge.nativeSleepMs(frameStartMs, nowMs)
            if (sleepMs > 1.0f) {
                try {
                    Thread.sleep(sleepMs.toLong())
                } catch (e: InterruptedException) {
                    Thread.currentThread().interrupt()
                }
            }
        }
    }

    override fun onPause() {
        super.onPause()
        if (NativeBridge.available) NativeBridge.nativeReleaseAllTouches()
    }
}
