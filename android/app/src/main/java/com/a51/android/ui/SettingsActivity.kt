package com.a51.android.ui

import android.content.Intent
import android.os.Bundle
import android.widget.SeekBar
import androidx.appcompat.app.AppCompatActivity
import com.a51.android.R
import com.a51.android.databinding.ActivitySettingsBinding
import com.a51.android.perf.AppSettings
import com.a51.android.perf.DeviceProfile

/**
 * All user settings.  Nothing here talks to the native layer directly - the
 * game activity pushes everything into the runtime when it starts, which keeps
 * this screen trivial and safe.
 */
class SettingsActivity : AppCompatActivity() {

    private lateinit var binding: ActivitySettingsBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivitySettingsBinding.inflate(layoutInflater)
        setContentView(binding.root)

        // ------------------------------------------------------------ touch
        binding.swTouch.isChecked = AppSettings.touchEnabled
        binding.swTouch.setOnCheckedChangeListener { _, on -> AppSettings.touchEnabled = on }

        binding.swLook.isChecked = AppSettings.lookEnabled
        binding.swLook.setOnCheckedChangeListener { _, on -> AppSettings.lookEnabled = on }

        binding.swInvertY.isChecked = AppSettings.invertLookY
        binding.swInvertY.setOnCheckedChangeListener { _, on -> AppSettings.invertLookY = on }

        binding.sbDeadzone.progress = (AppSettings.deadZone * 100).toInt()
        binding.sbDeadzone.setOnSeekBarChangeListener(seekListener { value ->
            AppSettings.deadZone = value / 100f
        })

        binding.sbSensitivity.progress = (AppSettings.lookSensitivity * 100).toInt()
        binding.sbSensitivity.setOnSeekBarChangeListener(seekListener { value ->
            AppSettings.lookSensitivity = (value / 100f).coerceIn(0.25f, 3f)
        })

        binding.btnEditLayout.setOnClickListener {
            val intent = Intent(this, GameActivity::class.java)
            intent.putExtra(GameActivity.EXTRA_EDIT_MODE, true)
            intent.putExtra(GameActivity.EXTRA_PROFILE_ID, AppSettings.lastProfileId)
            startActivity(intent)
        }

        // ------------------------------------------------------------- perf
        when (AppSettings.targetFps) {
            30 -> binding.rbFps30.isChecked = true
            90 -> binding.rbFps90.isChecked = true
            120 -> binding.rbFps120.isChecked = true
            else -> binding.rbFps60.isChecked = true
        }
        binding.rgFps.setOnCheckedChangeListener { _, checkedId ->
            AppSettings.targetFps = when (checkedId) {
                R.id.rbFps30 -> 30
                R.id.rbFps90 -> 90
                R.id.rbFps120 -> 120
                else -> 60
            }
        }

        binding.swAutoQuality.isChecked = AppSettings.autoQuality
        binding.swAutoQuality.setOnCheckedChangeListener { _, on ->
            AppSettings.autoQuality = on
        }

        binding.swThermal.isChecked = AppSettings.reactToThermal
        binding.swThermal.setOnCheckedChangeListener { _, on ->
            AppSettings.reactToThermal = on
        }

        binding.sbQuality.progress = AppSettings.qualityCeiling
        binding.sbQuality.setOnSeekBarChangeListener(seekListener { value ->
            AppSettings.qualityCeiling = value
        })

        // 0 = automatic, then 0.20 .. 1.00
        binding.sbScale.progress = (AppSettings.fixedRenderScale * 100).toInt()
        binding.sbScale.setOnSeekBarChangeListener(seekListener { value ->
            AppSettings.fixedRenderScale = if (value == 0) 0f else (value / 100f).coerceIn(0.2f, 1f)
        })

        binding.swHud.isChecked = AppSettings.showHud
        binding.swHud.setOnCheckedChangeListener { _, on -> AppSettings.showHud = on }

        // ----------------------------------------------------------- device
        binding.tvDevice.text = DeviceProfile.describe(this)
    }

    private fun seekListener(onChange: (Int) -> Unit): SeekBar.OnSeekBarChangeListener {
        return object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                if (fromUser) onChange(progress)
            }

            override fun onStartTrackingTouch(seekBar: SeekBar?) {}
            override fun onStopTrackingTouch(seekBar: SeekBar?) {}
        }
    }
}
