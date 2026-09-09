package com.a51.android.core

/**
 * Every native entry point, in one place.
 *
 * The signatures here must match jni_bridge.cpp exactly; the JVM binds by
 * symbol name, so a mismatch shows up as an UnsatisfiedLinkError the first time
 * the method is called - which is why [available] is checked before use.
 */
object NativeBridge {

    /** False when the native library is missing (wrong ABI, bad install). */
    @JvmStatic
    val available: Boolean = try {
        System.loadLibrary("a51")
        true
    } catch (e: Throwable) {
        android.util.Log.e("A51", "liba51.so could not be loaded", e)
        false
    }

    // ------------------------------------------------------------------ info
    @JvmStatic external fun nativeGetVersion(): String

    // --------------------------------------------------------------- runtime
    @JvmStatic external fun nativeInit(): Boolean
    @JvmStatic external fun nativeShutdown()
    @JvmStatic external fun nativeSetScreen(width: Int, height: Int)
    @JvmStatic external fun nativeGetAspect(): Float
    @JvmStatic external fun nativeBeginFrame()
    @JvmStatic external fun nativeEndFrame(frameMs: Float, cpuMs: Float)
    @JvmStatic external fun nativeSleepMs(frameStartMs: Float, nowMs: Float): Float
    @JvmStatic external fun nativeGetLog(): String

    // ----------------------------------------------------------------- perf
    @JvmStatic external fun nativeSetTargetFps(fps: Float)
    @JvmStatic external fun nativeSetAutoQuality(enabled: Boolean)
    @JvmStatic external fun nativeSetQualityCeiling(level: Int)
    @JvmStatic external fun nativeSetFixedScale(scale: Float)
    @JvmStatic external fun nativeSetThermalStatus(status: Int)
    @JvmStatic external fun nativeSetMemoryPressure(pressure: Float)
    @JvmStatic external fun nativeConfigurePerf(
        targetFps: Float,
        minScale: Float,
        qualityLevels: Int,
        textureBudgetMb: Int
    )

    @JvmStatic external fun nativeGetRenderScale(): Float
    @JvmStatic external fun nativeGetQualityLevel(): Int
    @JvmStatic external fun nativeGetFps(): Float
    @JvmStatic external fun nativeGetFrameTimeMs(): Float
    @JvmStatic external fun nativeGetAvgFrameMs(): Float
    @JvmStatic external fun nativeGetWorstFrameMs(): Float
    @JvmStatic external fun nativeGetP95FrameMs(): Float
    @JvmStatic external fun nativeGetStability(): Float
    @JvmStatic external fun nativeGetSuggestedFpsCap(): Int
    @JvmStatic external fun nativeGetTextureBudgetMb(): Int
    @JvmStatic external fun nativeGetFrameCount(): Long
    @JvmStatic external fun nativeGetDroppedFrames(): Int
    @JvmStatic external fun nativeGetQualityDrops(): Int
    @JvmStatic external fun nativeGetQualityRaises(): Int
    @JvmStatic external fun nativeResetPerf()

    // --------------------------------------------------------------- open gl
    @JvmStatic external fun nativeGlInit(width: Int, height: Int): Boolean
    @JvmStatic external fun nativeGlResize(width: Int, height: Int)
    @JvmStatic external fun nativeGlDrawFrame(timeSec: Float, dtSec: Float)
    @JvmStatic external fun nativeGlShutdown()
    @JvmStatic external fun nativeGlGetError(): String
    @JvmStatic external fun nativeGlGetBackWidth(): Int
    @JvmStatic external fun nativeGlGetBackHeight(): Int
    @JvmStatic external fun nativeGlGetDrawCalls(): Int

    // ---------------------------------------------------------------- input
    @JvmStatic external fun nativeSetTouchEnabled(enabled: Boolean)
    @JvmStatic external fun nativeIsTouchEnabled(): Boolean
    @JvmStatic external fun nativeSetDeadZone(deadZone: Float)
    @JvmStatic external fun nativeSetLookSensitivity(sensitivity: Float)
    @JvmStatic external fun nativeSetLookEnabled(enabled: Boolean)
    @JvmStatic external fun nativeSetInvertLookY(invert: Boolean)
    @JvmStatic external fun nativeTouchEvent(action: Int, pointerId: Int, x: Float, y: Float)
    @JvmStatic external fun nativeReleaseAllTouches()

    @JvmStatic external fun nativeGadgetId(name: String): Int
    @JvmStatic external fun nativeGadgetName(id: Int): String
    @JvmStatic external fun nativeSetGadget(gadget: Int, value: Float)
    @JvmStatic external fun nativeSetGadgetDigital(gadget: Int, pressed: Boolean)
    @JvmStatic external fun nativeGetGadgetValue(gadget: Int): Float
    @JvmStatic external fun nativeIsGadgetPressed(gadget: Int): Boolean
    @JvmStatic external fun nativeWasGadgetPressed(gadget: Int): Boolean

    // ---------------------------------------------------------- touch layout
    @JvmStatic external fun nativeLayoutSnapshot(): String
    @JvmStatic external fun nativeLayoutLoad(json: String): Boolean
    @JvmStatic external fun nativeLayoutReset()
    @JvmStatic external fun nativeLayoutSetVisible(id: Int, visible: Boolean)
    @JvmStatic external fun nativeLayoutSetSize(id: Int, size: Float)
    @JvmStatic external fun nativeLayoutSetOpacity(id: Int, opacity: Float)
    @JvmStatic external fun nativeLayoutSetStyle(id: Int, style: Int)
    @JvmStatic external fun nativeLayoutMove(id: Int, x: Float, y: Float)
    @JvmStatic external fun nativeLayoutResetControl(id: Int)
    @JvmStatic external fun nativeLayoutClamp()
    @JvmStatic external fun nativeLayoutHitTest(x: Float, y: Float): Int
    @JvmStatic external fun nativeLayoutHitHandle(x: Float, y: Float): Int
    @JvmStatic external fun nativeLayoutHandle(index: Int): FloatArray?
    @JvmStatic external fun nativeLayoutStickKnob(index: Int): FloatArray?
    @JvmStatic external fun nativeLayoutControlActive(index: Int): Boolean

    // ----------------------------------------------------------- game data
    @JvmStatic external fun nativeScanFolder(path: String, maxDepth: Int): String
    @JvmStatic external fun nativeFolderSize(path: String, maxDepth: Int): Long
    @JvmStatic external fun nativeBrowseOpen(path: String): Boolean
    @JvmStatic external fun nativeBrowseClose()
    @JvmStatic external fun nativeBrowseInfo(): String
    @JvmStatic external fun nativeBrowseList(first: Int, maxEntries: Int): String
    @JvmStatic external fun nativeBrowseRead(index: Int, offset: Int, length: Int): ByteArray?
    @JvmStatic external fun nativeBrowseVerify(maxChunks: Int): Int

    @JvmStatic external fun nativeMountArchive(path: String): Boolean
    @JvmStatic external fun nativeUnmountAll()
    @JvmStatic external fun nativeMountedCount(): Int
    @JvmStatic external fun nativeMountedFileTotal(): Int
    @JvmStatic external fun nativeFileExists(path: String): Boolean
    @JvmStatic external fun nativeReadFile(path: String, offset: Int, length: Int): ByteArray?
}
