package com.a51.android

import android.app.Application
import com.a51.android.core.NativeBridge
import com.a51.android.data.GameLibrary
import com.a51.android.perf.AppSettings
import com.a51.android.perf.DeviceProfile

/**
 * Wires up the process wide singletons.  Nothing heavy happens here: the native
 * library is loaded lazily by [NativeBridge] and the game data is only touched
 * when a screen asks for it.
 */
class A51Application : Application() {

    override fun onCreate() {
        super.onCreate()
        instance = this

        DeviceProfile.detect(this)
        AppSettings.init(this)
        GameLibrary.init(this)

        android.util.Log.i(
            "A51",
            "Area 51 Android runtime, native available=${NativeBridge.available}, " +
                "tier=${DeviceProfile.tier}"
        )
    }

    companion object {
        @JvmStatic
        lateinit var instance: A51Application
            private set
    }
}
