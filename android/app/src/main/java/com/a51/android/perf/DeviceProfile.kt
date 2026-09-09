package com.a51.android.perf

import android.app.ActivityManager
import android.content.Context
import android.os.Build
import android.text.format.Formatter
import com.a51.android.R
import java.io.File

/**
 * What kind of phone are we running on?
 *
 * The tier drives the *defaults* for the performance settings: a low end device
 * starts at 30 fps with a 0.35 render scale floor and a small asset cache, a
 * flagship starts at the display refresh rate with everything on.  The governor
 * then adapts from there, so a wrong guess only costs the first few seconds.
 */
object DeviceProfile {

    const val TIER_LOW = 0
    const val TIER_MEDIUM = 1
    const val TIER_HIGH = 2
    const val TIER_FLAGSHIP = 3

    @JvmStatic var tier: Int = TIER_MEDIUM
        private set

    @JvmStatic var cpuCores: Int = 1
        private set

    @JvmStatic var totalRamBytes: Long = 0
        private set

    @JvmStatic var memoryClassMb: Int = 64
        private set

    @JvmStatic var refreshRate: Float = 60f
        private set

    @JvmStatic var primaryAbi: String = "?"
        private set

    @JvmStatic var socName: String = "?"
        private set

    @JvmStatic var freeSpaceBytes: Long = 0
        private set

    /**
     * Detect once at startup.  Deliberately cheap: no GL context, no threads,
     * nothing that could slow the splash screen down.
     */
    @JvmStatic
    fun detect(context: Context) {
        cpuCores = Runtime.getRuntime().availableProcessors().coerceAtLeast(1)

        val am = context.getSystemService(Context.ACTIVITY_SERVICE) as? ActivityManager
        if (am != null) {
            val info = ActivityManager.MemoryInfo()
            am.getMemoryInfo(info)
            totalRamBytes = info.totalMem
            memoryClassMb = am.memoryClass
        }

        refreshRate = readRefreshRate(context)
        primaryAbi = Build.SUPPORTED_ABIS.firstOrNull() ?: "?"
        socName = readSoc()
        freeSpaceBytes = try {
            context.filesDir.let { File(it.absolutePath).usableSpace }
        } catch (e: Exception) {
            0L
        }

        tier = classify(totalRamBytes, cpuCores)
    }

    private fun readRefreshRate(context: Context): Float {
        return try {
            val display = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                context.display
            } else {
                @Suppress("DEPRECATION")
                (context.getSystemService(Context.WINDOW_SERVICE) as android.view.WindowManager).defaultDisplay
            }
            val rate = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M && display != null) {
                display.mode?.refreshRate ?: display.refreshRate
            } else {
                display?.refreshRate ?: 60f
            }
            if (rate in 20f..240f) rate else 60f
        } catch (e: Exception) {
            60f
        }
    }

    private fun readSoc(): String {
        return try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                val soc = Build.SOC_MANUFACTURER + " " + Build.SOC_MODEL
                if (soc.trim().length > 1) soc.trim() else "${Build.MANUFACTURER} ${Build.MODEL}"
            } else {
                "${Build.MANUFACTURER} ${Build.MODEL}"
            }
        } catch (e: Throwable) {
            "unknown"
        }
    }

    private fun classify(ramBytes: Long, cores: Int): Int {
        val gb = ramBytes / (1024L * 1024L * 1024L)
        return when {
            ramBytes <= 0L -> TIER_MEDIUM               // could not read it: assume mid
            gb < 3 || cores <= 4 -> TIER_LOW
            gb < 6 -> TIER_MEDIUM
            gb < 8 -> TIER_HIGH
            else -> TIER_FLAGSHIP
        }
    }

    // ------------------------------------------------------------------
    //  Defaults per tier
    // ------------------------------------------------------------------

    /** Frame rate we aim for.  Never above what the panel can show. */
    @JvmStatic
    fun defaultTargetFps(): Int {
        val base = when (tier) {
            TIER_LOW -> 30
            else -> 60
        }
        // Only offer a high frame rate when the screen can actually show it.
        return if (refreshRate >= 89f && tier >= TIER_HIGH) 90 else base
    }

    /** Lowest render scale the governor is allowed to fall back to. */
    @JvmStatic
    fun defaultMinScale(): Float = when (tier) {
        TIER_LOW -> 0.35f
        TIER_MEDIUM -> 0.45f
        TIER_HIGH -> 0.50f
        else -> 0.60f
    }

    @JvmStatic
    fun defaultQualityLevels(): Int = 4

    /** Asset cache budget in megabytes. */
    @JvmStatic
    fun defaultTextureBudgetMb(): Int = when (tier) {
        TIER_LOW -> 64
        TIER_MEDIUM -> 128
        TIER_HIGH -> 192
        else -> 256
    }

    @JvmStatic
    fun tierNameRes(): Int = when (tier) {
        TIER_LOW -> R.string.tier_low
        TIER_MEDIUM -> R.string.tier_medium
        TIER_HIGH -> R.string.tier_high
        else -> R.string.tier_flagship
    }

    @JvmStatic
    fun describe(context: Context): String {
        val ram = if (totalRamBytes > 0) Formatter.formatFileSize(context, totalRamBytes) else "?"
        return buildString {
            append(context.getString(tierNameRes())).append("\n")
            append(socName).append("\n")
            append("RAM: ").append(ram)
            append("  ·  CPU: ").append(cpuCores).append(" cores").append("\n")
            append("ABI: ").append(primaryAbi)
            append("  ·  Display: ").append(String.format("%.0f Hz", refreshRate)).append("\n")
            append("Android ").append(Build.VERSION.RELEASE)
            append(" (API ").append(Build.VERSION.SDK_INT).append(")")
            if (freeSpaceBytes > 0) {
                append("\nFree space: ").append(Formatter.formatFileSize(context, freeSpaceBytes))
            }
        }
    }
}
