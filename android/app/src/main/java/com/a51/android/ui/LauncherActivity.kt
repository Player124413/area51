package com.a51.android.ui

import android.content.Intent
import android.os.Bundle
import android.view.View
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.recyclerview.widget.LinearLayoutManager
import com.a51.android.R
import com.a51.android.core.NativeBridge
import com.a51.android.data.GameLibrary
import com.a51.android.data.GameProfile
import com.a51.android.databinding.ActivityLauncherBinding
import com.a51.android.perf.AppSettings
import com.a51.android.perf.DeviceProfile
import com.google.android.material.dialog.MaterialAlertDialogBuilder

/**
 * The launcher: what game data is installed, and the way in to the import
 * screen, the settings and the layout editor.
 */
class LauncherActivity : AppCompatActivity() {

    private lateinit var binding: ActivityLauncherBinding
    private lateinit var adapter: GameAdapter

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityLauncherBinding.inflate(layoutInflater)
        setContentView(binding.root)

        adapter = GameAdapter(
            onPlay = { play(it) },
            onFiles = { openFiles(it) },
            onDelete = { confirmDelete(it) }
        )
        binding.rvGames.layoutManager = LinearLayoutManager(this)
        binding.rvGames.adapter = adapter

        binding.btnImport.setOnClickListener {
            startActivity(Intent(this, ImportActivity::class.java))
        }
        binding.btnSettings.setOnClickListener {
            startActivity(Intent(this, SettingsActivity::class.java))
        }
        binding.btnEditLayout.setOnClickListener {
            // The editor lives inside the game activity so you edit the layout
            // exactly where you will be using it.
            val intent = Intent(this, GameActivity::class.java)
            intent.putExtra(GameActivity.EXTRA_EDIT_MODE, true)
            intent.putExtra(GameActivity.EXTRA_PROFILE_ID, AppSettings.lastProfileId)
            startActivity(intent)
        }

        binding.tvDevice.text = DeviceProfile.describe(this)

        handleSharedIntent(intent)
    }

    override fun onNewIntent(intent: Intent?) {
        super.onNewIntent(intent)
        intent?.let { handleSharedIntent(it) }
    }

    override fun onResume() {
        super.onResume()
        reload()
    }

    private fun handleSharedIntent(intent: Intent) {
        val action = intent.action ?: return
        if (action != Intent.ACTION_SEND && action != Intent.ACTION_SEND_MULTIPLE) return

        val target = Intent(this, ImportActivity::class.java).apply {
            this.action = action
            if (action == Intent.ACTION_SEND_MULTIPLE) {
                putExtra(
                    ImportActivity.EXTRA_URIS,
                    intent.getParcelableArrayListExtra<android.net.Uri>(Intent.EXTRA_STREAM)
                )
            } else {
                val uri = intent.getParcelableExtra<android.net.Uri>(Intent.EXTRA_STREAM)
                if (uri != null) {
                    putExtra(ImportActivity.EXTRA_URIS, arrayListOf(uri))
                }
            }
        }
        startActivity(target)
        intent.action = null
    }

    private fun reload() {
        val profiles = GameLibrary.all().sortedByDescending { it.lastPlayedAt }
        adapter.submit(profiles)
        binding.emptyView.visibility = if (profiles.isEmpty()) View.VISIBLE else View.GONE

        val nativeNote = if (!NativeBridge.available) "\nnative library unavailable!" else ""
        binding.tvDevice.text = DeviceProfile.describe(this) + nativeNote
    }

    private fun play(profile: GameProfile) {
        AppSettings.lastProfileId = profile.id
        GameLibrary.touch(profile.id)
        val intent = Intent(this, GameActivity::class.java)
        intent.putExtra(GameActivity.EXTRA_PROFILE_ID, profile.id)
        startActivity(intent)
    }

    private fun openFiles(profile: GameProfile) {
        val intent = Intent(this, ArchiveActivity::class.java)
        intent.putExtra(ArchiveActivity.EXTRA_DIR, profile.dir)
        startActivity(intent)
    }

    private fun confirmDelete(profile: GameProfile) {
        MaterialAlertDialogBuilder(this)
            .setTitle(R.string.delete_confirm_title)
            .setMessage(
                getString(
                    R.string.delete_confirm_body,
                    profile.name,
                    android.text.format.Formatter.formatFileSize(this, profile.sizeBytes)
                )
            )
            .setNegativeButton(R.string.action_cancel, null)
            .setPositiveButton(R.string.action_delete) { _, _ ->
                GameLibrary.remove(profile.id)
                reload()
                Toast.makeText(this, R.string.deleted, Toast.LENGTH_SHORT).show()
            }
            .show()
    }
}
