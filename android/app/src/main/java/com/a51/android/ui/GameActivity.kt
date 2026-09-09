package com.a51.android.ui

import android.app.ActivityManager
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.BatteryManager
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.PowerManager
import android.view.Gravity
import android.view.View
import android.view.WindowManager
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import com.a51.android.R
import com.a51.android.core.NativeBridge
import com.a51.android.data.ArchiveScanner
import com.a51.android.data.GameLibrary
import com.a51.android.databinding.ActivityGameBinding
import com.a51.android.perf.AppSettings
import com.a51.android.perf.DeviceProfile
import java.io.File
import java.util.concurrent.Executor
import java.util.concurrent.Executors

/**
 * The game screen.
 *
 * Responsibilities:
 *   * push the settings into the native runtime;
 *   * mount the imported archives;
 *   * keep the platform informed (thermal status, memory pressure, display
 *     refresh rate) so the governor can react before the phone does;
 *   * host the touch overlay and its editor.
 */
class GameActivity : AppCompatActivity() {

    companion object {
        const val EXTRA_PROFILE_ID = "extra_profile_id"
        const val EXTRA_EDIT_MODE = "extra_edit_mode"
        private const val MONITOR_INTERVAL_MS = 2000L
    }

    private lateinit var binding: ActivityGameBinding
    private val handler = Handler(Looper.getMainLooper())
    private val executor: Executor = Executors.newSingleThreadExecutor()

    private var profileId = ""
    private var editMode = false
    private var thermalListener: Any? = null
    private var thermalStatus = 0

    // ------------------------------------------------------------------
    //  Lifecycle
    // ------------------------------------------------------------------

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityGameBinding.inflate(layoutInflater)
        setContentView(binding.root)

        profileId = intent?.getStringExtra(EXTRA_PROFILE_ID).orEmpty()

        if (!NativeBridge.available) {
            Toast.makeText(this, R.string.game_gl_failed, Toast.LENGTH_LONG).show()
        } else {
            NativeBridge.nativeInit()
            applySettings()
        }

        binding.glView.hud = binding.hudView
        binding.hudView.enabled = AppSettings.showHud

        binding.touchView.onPausePressed = { showPause(true) }
        binding.touchView.onSelectionChanged = { updateEditBar() }
        loadLayout()

        binding.btnResume.setOnClickListener { showPause(false) }
        binding.btnQuit.setOnClickListener { finish() }

        setupEditBar()

        if (intent?.getBooleanExtra(EXTRA_EDIT_MODE, false) == true) {
            binding.glView.post { enterEditMode() }
        }

        mountArchives()
        keepAwake()
    }

    override fun onResume() {
        super.onResume()
        binding.glView.onResume()
        enterImmersive()
        applyDisplayRate()
        startMonitors()
    }

    override fun onPause() {
        stopMonitors()
        binding.glView.onPause()
        super.onPause()
    }

    override fun onDestroy() {
        stopMonitors()
        if (NativeBridge.available) {
            try {
                NativeBridge.nativeUnmountAll()
            } catch (e: Exception) {
                // shutting down
            }
        }
        super.onDestroy()
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) enterImmersive()
    }

    // ------------------------------------------------------------------
    //  Settings -> native
    // ------------------------------------------------------------------

    private fun applySettings() {
        if (!NativeBridge.available) return

        NativeBridge.nativeConfigurePerf(
            AppSettings.targetFps.toFloat(),
            DeviceProfile.defaultMinScale(),
            DeviceProfile.defaultQualityLevels(),
            DeviceProfile.defaultTextureBudgetMb()
        )
        NativeBridge.nativeSetTargetFps(AppSettings.targetFps.toFloat())
        NativeBridge.nativeSetAutoQuality(AppSettings.autoQuality)
        NativeBridge.nativeSetQualityCeiling(AppSettings.qualityCeiling)
        NativeBridge.nativeSetFixedScale(AppSettings.fixedRenderScale)

        NativeBridge.nativeSetTouchEnabled(AppSettings.touchEnabled)
        NativeBridge.nativeSetLookEnabled(AppSettings.lookEnabled)
        NativeBridge.nativeSetInvertLookY(AppSettings.invertLookY)
        NativeBridge.nativeSetDeadZone(AppSettings.deadZone)
        NativeBridge.nativeSetLookSensitivity(AppSettings.lookSensitivity)
    }

    private fun applyDisplayRate() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) return
        if (!AppSettings.autoQuality) return
        try {
            val suggested = if (NativeBridge.available) {
                NativeBridge.nativeGetSuggestedFpsCap()
            } else {
                AppSettings.targetFps
            }
            display?.setFrameRate(
                suggested.toFloat(),
                android.view.Display.FRAME_RATE_COMPATIBILITY_DEFAULT
            )
        } catch (e: Exception) {
            // Not every device supports it; the frame pacer covers the rest.
        }
    }

    // ------------------------------------------------------------------
    //  Game data
    // ------------------------------------------------------------------

    private fun mountArchives() {
        if (!NativeBridge.available) return

        val profile = GameLibrary.find(profileId)
        if (profile == null) {
            Toast.makeText(this, R.string.game_no_data, Toast.LENGTH_LONG).show()
            return
        }

        Thread {
            val archives = ArchiveScanner.scan(profile.dir)
            var mounted = 0
            for (a in archives) {
                if (NativeBridge.nativeMountArchive(a.path)) mounted++
            }
            runOnUiThread {
                if (mounted == 0) {
                    android.util.Log.w(
                        "A51",
                        "no archive could be mounted from ${profile.dir} (${archives.size} found)"
                    )
                }
                // The mount log is drawn on the HUD, so nothing else to do here.
                binding.hudView.invalidate()
            }
        }.start()
    }

    // ------------------------------------------------------------------
    //  Platform monitors
    // ------------------------------------------------------------------

    private val monitorRunnable = object : Runnable {
        override fun run() {
            updateMemoryPressure()
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) updateThermalFromBattery()
            handler.postDelayed(this, MONITOR_INTERVAL_MS)
        }
    }

    private fun startMonitors() {
        registerThermalListener()
        handler.removeCallbacks(monitorRunnable)
        handler.post(monitorRunnable)
    }

    private fun stopMonitors() {
        handler.removeCallbacks(monitorRunnable)
        unregisterThermalListener()
    }

    private fun updateMemoryPressure() {
        if (!NativeBridge.available) return
        val am = getSystemService(Context.ACTIVITY_SERVICE) as? ActivityManager ?: return
        val info = ActivityManager.MemoryInfo()
        am.getMemoryInfo(info)

        val pressure = when {
            info.lowMemory -> 1f
            info.totalMem <= 0L -> 0f
            else -> {
                // Pressure starts once less than a quarter of the RAM is free.
                val threshold = info.totalMem * 0.25
                (1.0 - info.availMem.toDouble() / threshold).toFloat().coerceIn(0f, 1f)
            }
        }
        NativeBridge.nativeSetMemoryPressure(pressure)
    }

    private fun registerThermalListener() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) return
        if (thermalListener != null) return

        val pm = getSystemService(Context.POWER_SERVICE) as? PowerManager ?: return
        val listener = PowerManager.OnThermalStatusChangedListener { status ->
            thermalStatus = status
            binding.hudView.thermalStatus = status
            if (NativeBridge.available) {
                NativeBridge.nativeSetThermalStatus(if (AppSettings.reactToThermal) status else 0)
            }
        }
        try {
            pm.addThermalStatusListener(executor, listener)
            thermalListener = listener
            thermalStatus = pm.currentThermalStatus
            binding.hudView.thermalStatus = thermalStatus
        } catch (e: Exception) {
            thermalListener = null
        }
    }

    private fun unregisterThermalListener() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) return
        val listener = thermalListener as? PowerManager.OnThermalStatusChangedListener ?: return
        try {
            (getSystemService(Context.POWER_SERVICE) as? PowerManager)
                ?.removeThermalStatusListener(listener)
        } catch (e: Exception) {
            // ignore
        }
        thermalListener = null
    }

    /** Pre API 29 there is no thermal API, so the battery temperature is used. */
    private fun updateThermalFromBattery() {
        if (!NativeBridge.available) return
        val intent: Intent? = try {
            registerReceiver(null as BroadcastReceiver?, IntentFilter(Intent.ACTION_BATTERY_CHANGED))
        } catch (e: Exception) {
            null
        }
        if (intent == null) return

        val tenths = intent.getIntExtra(BatteryManager.EXTRA_TEMPERATURE, 0)
        val celsius = tenths / 10f
        val status = when {
            celsius >= 46f -> 4
            celsius >= 43f -> 3
            celsius >= 40f -> 2
            celsius >= 36f -> 1
            else -> 0
        }
        thermalStatus = status
        binding.hudView.thermalStatus = status
        NativeBridge.nativeSetThermalStatus(if (AppSettings.reactToThermal) status else 0)
    }

    // ------------------------------------------------------------------
    //  Immersive / wake lock
    // ------------------------------------------------------------------

    private fun keepAwake() {
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
    }

    private fun enterImmersive() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            window.setDecorFitsSystemWindows(false)
            window.insetsController?.let {
                it.hide(
                    android.view.WindowInsets.Type.statusBars() or
                        android.view.WindowInsets.Type.navigationBars()
                )
                it.systemBarsBehavior =
                    android.view.WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            }
        } else {
            @Suppress("DEPRECATION")
            window.decorView.systemUiVisibility = (
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                    or View.SYSTEM_UI_FLAG_FULLSCREEN
                    or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    or View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                )
        }
    }

    // ------------------------------------------------------------------
    //  Pause
    // ------------------------------------------------------------------

    private fun showPause(show: Boolean) {
        binding.pauseOverlay.visibility = if (show) View.VISIBLE else View.GONE
        if (show && NativeBridge.available) NativeBridge.nativeReleaseAllTouches()
        if (!show) enterImmersive()
    }

    // ------------------------------------------------------------------
    //  Layout editor
    // ------------------------------------------------------------------

    private fun layoutFile(): File {
        val id = profileId.ifEmpty { "default" }
        return File(filesDir, "layout_$id.json")
    }

    private fun loadLayout() {
        if (!NativeBridge.available) return
        val file = layoutFile()
        if (file.exists()) {
            try {
                NativeBridge.nativeLayoutLoad(file.readText())
            } catch (e: Exception) {
                android.util.Log.e("A51", "could not load the layout", e)
            }
        }
        NativeBridge.nativeLayoutClamp()
        binding.touchView.refresh()
    }

    private fun enterEditMode() {
        editMode = true
        binding.editBar.visibility = View.VISIBLE
        binding.touchView.editMode = true
        binding.hudView.enabled = false
        buildChips()
        if (binding.touchView.controlCount() > 0 && binding.touchView.selectedIndex < 0) {
            binding.touchView.select(0)
        }
        updateEditBar()
    }

    private fun exitEditMode(save: Boolean) {
        if (save) saveLayout() else loadLayout()

        editMode = false
        binding.editBar.visibility = View.GONE
        binding.touchView.editMode = false
        binding.hudView.enabled = AppSettings.showHud
        if (save) Toast.makeText(this, R.string.edit_saved, Toast.LENGTH_SHORT).show()
    }

    private fun saveLayout() {
        if (!NativeBridge.available) return
        try {
            layoutFile().writeText(NativeBridge.nativeLayoutSnapshot())
        } catch (e: Exception) {
            android.util.Log.e("A51", "could not save the layout", e)
        }
    }

    private fun setupEditBar() {
        binding.btnSaveLayout.setOnClickListener { exitEditMode(true) }
        binding.btnCancelEdit.setOnClickListener { exitEditMode(false) }

        binding.sbSize.setOnSeekBarChangeListener(editSeek { value ->
            val ctrl = binding.touchView.controlAt(binding.touchView.selectedIndex)
                ?: return@editSeek
            // 0.05 .. 0.60 of the screen height
            NativeBridge.nativeLayoutSetSize(ctrl.id, 0.05f + (value / 100f) * 0.55f)
            binding.touchView.refresh()
        })

        binding.sbOpacity.setOnSeekBarChangeListener(editSeek { value ->
            val ctrl = binding.touchView.controlAt(binding.touchView.selectedIndex) ?: return@editSeek
            NativeBridge.nativeLayoutSetOpacity(ctrl.id, 0.10f + (value / 100f) * 0.90f)
            binding.touchView.refresh()
        })

        binding.btnVisible.setOnClickListener {
            val ctrl = binding.touchView.controlAt(binding.touchView.selectedIndex) ?: return@setOnClickListener
            NativeBridge.nativeLayoutSetVisible(ctrl.id, !ctrl.visible)
            binding.touchView.refresh()
            updateEditBar()
        }

        binding.btnStyle.setOnClickListener {
            val ctrl = binding.touchView.controlAt(binding.touchView.selectedIndex) ?: return@setOnClickListener
            NativeBridge.nativeLayoutSetStyle(ctrl.id, (ctrl.style + 1) % 3)
            binding.touchView.refresh()
        }

        binding.btnResetControl.setOnClickListener {
            val ctrl = binding.touchView.controlAt(binding.touchView.selectedIndex) ?: return@setOnClickListener
            NativeBridge.nativeLayoutResetControl(ctrl.id)
            binding.touchView.refresh()
            updateEditBar()
        }

        binding.btnShowAll.setOnClickListener { setAllVisible(true) }
        binding.btnHideAll.setOnClickListener { setAllVisible(false) }

        binding.btnResetAll.setOnClickListener {
            NativeBridge.nativeLayoutReset()
            binding.touchView.refresh()
            buildChips()
            updateEditBar()
        }
    }

    private fun setAllVisible(visible: Boolean) {
        for (i in 0 until binding.touchView.controlCount()) {
            binding.touchView.controlAt(i)?.let { NativeBridge.nativeLayoutSetVisible(it.id, visible) }
        }
        binding.touchView.refresh()
        updateEditBar()
    }

    private fun buildChips() {
        binding.chipsRow.removeAllViews()
        for (i in 0 until binding.touchView.controlCount()) {
            val ctrl = binding.touchView.controlAt(i) ?: continue
            val chip = TextView(this).apply {
                text = ctrl.name
                textSize = 11f
                setPadding(20, 10, 20, 10)
                setBackgroundResource(R.drawable.bg_panel_alt)
                val params = android.widget.LinearLayout.LayoutParams(
                    android.widget.LinearLayout.LayoutParams.WRAP_CONTENT,
                    android.widget.LinearLayout.LayoutParams.WRAP_CONTENT
                )
                params.marginEnd = 8
                layoutParams = params
                setOnClickListener {
                    binding.touchView.select(i)
                    updateEditBar()
                }
            }
            binding.chipsRow.addView(chip)
        }
    }

    private fun updateEditBar() {
        val index = binding.touchView.selectedIndex
        val ctrl = binding.touchView.controlAt(index)

        binding.tvSelected.text = ctrl?.name ?: "-"
        binding.sbSize.isEnabled = ctrl != null
        binding.sbOpacity.isEnabled = ctrl != null
        binding.btnVisible.isEnabled = ctrl != null
        binding.btnStyle.isEnabled = ctrl != null
        binding.btnResetControl.isEnabled = ctrl != null

        if (ctrl == null) return

        binding.sbSize.setOnSeekBarChangeListener(null)
        binding.sbOpacity.setOnSeekBarChangeListener(null)
        binding.sbSize.progress = (((ctrl.size - 0.05f) / 0.55f) * 100f).toInt().coerceIn(0, 100)
        binding.sbOpacity.progress = (((ctrl.opacity - 0.10f) / 0.90f) * 100f).toInt().coerceIn(0, 100)
        binding.sbSize.setOnSeekBarChangeListener(editSeek { value ->
            val c = binding.touchView.controlAt(binding.touchView.selectedIndex) ?: return@editSeek
            NativeBridge.nativeLayoutSetSize(c.id, 0.05f + (value / 100f) * 0.55f)
            binding.touchView.refresh()
            binding.touchView.select(index)
        })
        binding.sbOpacity.setOnSeekBarChangeListener(editSeek { value ->
            val c = binding.touchView.controlAt(binding.touchView.selectedIndex) ?: return@editSeek
            NativeBridge.nativeLayoutSetOpacity(c.id, 0.10f + (value / 100f) * 0.90f)
            binding.touchView.refresh()
        })

        binding.btnVisible.text = getString(
            if (ctrl.visible) R.string.edit_visible else R.string.edit_hidden
        )
        binding.tvEditHint.text = getString(R.string.edit_hint) +
            "   ·   " + getString(if (ctrl.visible) R.string.edit_visible else R.string.edit_hidden)
        binding.tvEditHint.gravity = Gravity.START
    }

    private fun editSeek(onChange: (Int) -> Unit): android.widget.SeekBar.OnSeekBarChangeListener {
        return object : android.widget.SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(
                seekBar: android.widget.SeekBar?,
                progress: Int,
                fromUser: Boolean
            ) {
                if (fromUser) onChange(progress)
            }

            override fun onStartTrackingTouch(seekBar: android.widget.SeekBar?) {}
            override fun onStopTrackingTouch(seekBar: android.widget.SeekBar?) {}
        }
    }

    override fun onBackPressed() {
        if (editMode) {
            exitEditMode(false)
            return
        }
        if (binding.pauseOverlay.visibility == View.VISIBLE) {
            showPause(false)
            return
        }
        super.onBackPressed()
    }
}
