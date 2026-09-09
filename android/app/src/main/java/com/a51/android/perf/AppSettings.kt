package com.a51.android.perf

import android.content.Context
import android.content.SharedPreferences

/**
 * Every user facing setting, persisted in one SharedPreferences file.
 *
 * The defaults come from [DeviceProfile], so the very first launch is already
 * tuned for the phone it runs on.
 */
object AppSettings {

    private const val FILE = "a51_settings"

    private const val KEY_TOUCH_ENABLED = "touch_enabled"
    private const val KEY_LOOK_ENABLED = "look_enabled"
    private const val KEY_INVERT_Y = "invert_look_y"
    private const val KEY_DEADZONE = "deadzone"
    private const val KEY_SENSITIVITY = "look_sensitivity"
    private const val KEY_TARGET_FPS = "target_fps"
    private const val KEY_AUTO_QUALITY = "auto_quality"
    private const val KEY_QUALITY_CEILING = "quality_ceiling"
    private const val KEY_FIXED_SCALE = "fixed_scale"
    private const val KEY_SHOW_HUD = "show_hud"
    private const val KEY_THERMAL = "thermal_reaction"
    private const val KEY_LAST_PROFILE = "last_profile"

    private lateinit var prefs: SharedPreferences

    @JvmStatic
    fun init(context: Context) {
        if (!::prefs.isInitialized) {
            prefs = context.applicationContext.getSharedPreferences(FILE, Context.MODE_PRIVATE)
        }
    }

    private fun p(): SharedPreferences {
        check(::prefs.isInitialized) { "AppSettings.init() must be called first" }
        return prefs
    }

    // ---------------------------------------------------------- touch
    var touchEnabled: Boolean
        get() = p().getBoolean(KEY_TOUCH_ENABLED, true)
        set(value) = p().edit().putBoolean(KEY_TOUCH_ENABLED, value).apply()

    var lookEnabled: Boolean
        get() = p().getBoolean(KEY_LOOK_ENABLED, true)
        set(value) = p().edit().putBoolean(KEY_LOOK_ENABLED, value).apply()

    var invertLookY: Boolean
        get() = p().getBoolean(KEY_INVERT_Y, false)
        set(value) = p().edit().putBoolean(KEY_INVERT_Y, value).apply()

    /** 0.0 .. 0.40 */
    var deadZone: Float
        get() = p().getFloat(KEY_DEADZONE, 0.15f)
        set(value) = p().edit().putFloat(KEY_DEADZONE, value.coerceIn(0f, 0.4f)).apply()

    /** 0.25 .. 3.0 */
    var lookSensitivity: Float
        get() = p().getFloat(KEY_SENSITIVITY, 1.0f)
        set(value) = p().edit().putFloat(KEY_SENSITIVITY, value.coerceIn(0.25f, 3.0f)).apply()

    // ---------------------------------------------------------- performance
    var targetFps: Int
        get() = p().getInt(KEY_TARGET_FPS, DeviceProfile.defaultTargetFps())
        set(value) = p().edit().putInt(KEY_TARGET_FPS, value).apply()

    var autoQuality: Boolean
        get() = p().getBoolean(KEY_AUTO_QUALITY, true)
        set(value) = p().edit().putBoolean(KEY_AUTO_QUALITY, value).apply()

    /** 0..3 */
    var qualityCeiling: Int
        get() = p().getInt(KEY_QUALITY_CEILING, DeviceProfile.defaultQualityLevels() - 1)
        set(value) = p().edit().putInt(KEY_QUALITY_CEILING, value.coerceIn(0, 3)).apply()

    /** 0 = automatic, otherwise 0.2 .. 1.0 */
    var fixedRenderScale: Float
        get() = p().getFloat(KEY_FIXED_SCALE, 0f)
        set(value) = p().edit().putFloat(KEY_FIXED_SCALE, value.coerceIn(0f, 1f)).apply()

    var showHud: Boolean
        get() = p().getBoolean(KEY_SHOW_HUD, true)
        set(value) = p().edit().putBoolean(KEY_SHOW_HUD, value).apply()

    var reactToThermal: Boolean
        get() = p().getBoolean(KEY_THERMAL, true)
        set(value) = p().edit().putBoolean(KEY_THERMAL, value).apply()

    // ---------------------------------------------------------- misc
    var lastProfileId: String
        get() = p().getString(KEY_LAST_PROFILE, "") ?: ""
        set(value) = p().edit().putString(KEY_LAST_PROFILE, value).apply()
}
