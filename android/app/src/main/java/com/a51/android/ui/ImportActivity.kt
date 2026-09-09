package com.a51.android.ui

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.text.format.Formatter
import android.view.View
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.LinearLayoutManager
import com.a51.android.R
import com.a51.android.data.ArchiveScanner
import com.a51.android.data.DataImporter
import com.a51.android.data.GameLibrary
import com.a51.android.data.GameProfile
import com.a51.android.databinding.ActivityImportBinding
import java.io.File

/**
 * Getting the game files onto the phone.
 *
 * Three ways in: pick a folder, let another app share files here, or type a
 * path (useful when the files are already on storage the app can read).
 */
class ImportActivity : AppCompatActivity() {

    companion object {
        const val EXTRA_URIS = "extra_uris"
        private const val REQ_STORAGE = 41
    }

    private lateinit var binding: ActivityImportBinding
    private val adapter = ArchiveAdapter()

    @Volatile
    private var cancelled = false

    @Volatile
    private var busy = false

    private var treeUri: Uri? = null
    private var sourceDir: File? = null

    private val pickFolder = registerForActivityResult(
        ActivityResultContracts.OpenDocumentTree()
    ) { uri ->
        if (uri == null) return@registerForActivityResult
        treeUri = uri
        sourceDir = null
        try {
            contentResolver.takePersistableUriPermission(
                uri,
                Intent.FLAG_GRANT_READ_URI_PERMISSION
            )
        } catch (e: SecurityException) {
            // Not fatal: the permission lasts for this session anyway.
        }
        scanSaf(uri)
    }

    private val pickFiles = registerForActivityResult(
        ActivityResultContracts.OpenMultipleDocuments()
    ) { uris ->
        if (uris.isNullOrEmpty()) return@registerForActivityResult
        receiveUris(uris)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityImportBinding.inflate(layoutInflater)
        setContentView(binding.root)

        binding.rvArchives.layoutManager = LinearLayoutManager(this)
        binding.rvArchives.adapter = adapter

        binding.btnPickFolder.setOnClickListener { pickFolder.launch(null) }
        binding.btnPickFiles.setOnClickListener { pickFiles.launch(arrayOf("*/*")) }
        binding.btnOpenPath.setOnClickListener { openManualPath() }
        binding.btnImport.setOnClickListener { startImport() }

        binding.tvStatus.text = getString(R.string.launcher_empty_body)

        // Files shared into the app.
        val shared = intent?.getParcelableArrayListExtra<Uri>(EXTRA_URIS)
        if (!shared.isNullOrEmpty()) {
            receiveUris(shared)
            Toast.makeText(
                this,
                getString(R.string.import_shared_received, shared.size),
                Toast.LENGTH_SHORT
            ).show()
        }
    }

    // ------------------------------------------------------------------
    //  Scanning
    // ------------------------------------------------------------------

    private fun scanSaf(uri: Uri) {
        setBusy(true, getString(R.string.import_scanning))
        Thread {
            val entries = DataImporter.listFolder(this, uri)
                .filter { it.isArchive || !DataImporter.isSplitName(it.name) }
            runOnUiThread {
                setBusy(false, null)
                if (entries.isEmpty()) {
                    binding.tvStatus.text = getString(R.string.import_no_archives)
                } else {
                    val bytes = entries.sumOf { it.size.coerceAtLeast(0L) }
                    binding.tvStatus.text = getString(
                        R.string.import_found,
                        entries.count { it.isArchive },
                        Formatter.formatFileSize(this, bytes)
                    )
                }
                adapter.submit(entries)
                updateImportButton()
            }
        }.start()
    }

    private fun openManualPath() {
        val path = binding.etPath.text?.toString()?.trim().orEmpty()
        if (path.isEmpty()) return

        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.S_V2 &&
            ContextCompat.checkSelfPermission(this, Manifest.permission.READ_EXTERNAL_STORAGE)
            != PackageManager.PERMISSION_GRANTED
        ) {
            requestPermissions(
                arrayOf(Manifest.permission.READ_EXTERNAL_STORAGE),
                REQ_STORAGE
            )
            return
        }

        val dir = File(path)
        if (!dir.isDirectory) {
            binding.tvStatus.text = getString(R.string.import_permission_denied)
            return
        }

        sourceDir = dir
        treeUri = null
        setBusy(true, getString(R.string.import_scanning))

        Thread {
            val files = dir.listFiles()
                ?.filter { it.isFile && (it.name.lowercase().endsWith(".dfs") ||
                    !DataImporter.isSplitName(it.name)) }
                ?.sortedBy { it.name.lowercase() }
                .orEmpty()

            val entries = files.map {
                DataImporter.SafEntry(
                    uri = Uri.fromFile(it),
                    name = it.name,
                    size = it.length(),
                    isArchive = it.name.lowercase().endsWith(".dfs"),
                    isSplit = false
                )
            }

            // With a real path we can ask the native reader what is inside.
            val archives = ArchiveScanner.scan(path)

            runOnUiThread {
                setBusy(false, null)
                if (entries.isEmpty()) {
                    binding.tvStatus.text = getString(R.string.import_no_archives)
                } else {
                    val totalFiles = archives.sumOf { it.files }
                    binding.tvStatus.text = getString(
                        R.string.import_found,
                        entries.count { it.isArchive },
                        Formatter.formatFileSize(this, entries.sumOf { it.size })
                    ) + "  ·  $totalFiles files inside"
                }
                adapter.submit(entries)
                updateImportButton()
            }
        }.start()
    }

    private fun receiveUris(uris: List<Uri>) {
        val entries = uris.map { uri ->
            val name = DataImporter.queryDisplayName(this, uri) ?: "shared_file"
            DataImporter.SafEntry(
                uri = uri,
                name = name,
                size = 0L,
                isArchive = name.lowercase().endsWith(".dfs"),
                isSplit = false
            )
        }
        adapter.submit(entries)
        binding.tvStatus.text = getString(R.string.import_shared_received, entries.size)
        updateImportButton()
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == REQ_STORAGE) openManualPath()
    }

    // ------------------------------------------------------------------
    //  Importing
    // ------------------------------------------------------------------

    private fun updateImportButton() {
        val selected = adapter.selectedEntries()
        binding.btnImport.isEnabled = selected.isNotEmpty() && !busy
    }

    private fun startImport() {
        val selected = adapter.selectedEntries()
        if (selected.isEmpty()) return

        val name = binding.etName.text?.toString()?.trim()?.takeIf { it.isNotEmpty() }
            ?: "Area 51 ${System.currentTimeMillis() / 1000}"
        val id = "g" + System.currentTimeMillis().toString(36)
        val dest = GameLibrary.dirFor(id)

        val needed = adapter.totalSelectedBytes()
        val free = dest.parentFile?.usableSpace ?: 0L
        if (needed > 0 && free in 1 until needed) {
            binding.tvStatus.text = getString(
                R.string.import_no_space,
                Formatter.formatFileSize(this, needed),
                Formatter.formatFileSize(this, free)
            )
            return
        }

        cancelled = false
        setBusy(true, getString(R.string.import_copying, ""))

        val dir = sourceDir

        Thread {
            val result = if (dir != null) {
                val files = selected.map { File(dir, it.name) }.filter { it.exists() }
                DataImporter.importPaths(files, dest, { cancelled }) { p -> postProgress(p) }
            } else {
                DataImporter.importEntries(this, selected, dest, { cancelled }) { p ->
                    postProgress(p)
                }
            }

            // Whatever landed on disk is the truth from now on.
            val archives = ArchiveScanner.scan(dest.absolutePath)

            runOnUiThread {
                setBusy(false, null)
                when (result) {
                    is DataImporter.Result.Ok -> {
                        val profile = GameProfile(
                            id = id,
                            name = name,
                            dir = dest.absolutePath,
                            sizeBytes = result.bytes,
                            archiveCount = archives.size,
                            fileCount = archives.sumOf { it.files },
                            createdAt = System.currentTimeMillis(),
                            lastPlayedAt = 0L
                        )
                        GameLibrary.add(profile)
                        Toast.makeText(
                            this,
                            getString(
                                R.string.import_done,
                                profile.name,
                                Formatter.formatFileSize(this, result.bytes)
                            ),
                            Toast.LENGTH_LONG
                        ).show()
                        finish()
                    }
                    is DataImporter.Result.Failed ->
                        binding.tvStatus.text = getString(R.string.import_failed, result.message)
                    DataImporter.Result.Cancelled ->
                        binding.tvStatus.text = getString(R.string.action_cancel)
                }
                updateImportButton()
            }
        }.start()
    }

    private fun postProgress(p: DataImporter.Progress) {
        runOnUiThread {
            binding.progress.visibility = View.VISIBLE
            if (p.totalBytes > 0) {
                val percent = (p.doneBytes * 100 / p.totalBytes).toInt().coerceIn(0, 100)
                binding.progress.setProgressCompat(percent, true)
            }
            binding.tvStatus.text = getString(R.string.import_copying, p.currentName)
        }
    }

    private fun setBusy(busy: Boolean, message: String?) {
        this.busy = busy
        binding.btnPickFolder.isEnabled = !busy
        binding.btnPickFiles.isEnabled = !busy
        binding.btnOpenPath.isEnabled = !busy
        binding.btnImport.isEnabled = !busy && adapter.selectedEntries().isNotEmpty()
        binding.progress.visibility = if (busy) View.VISIBLE else View.GONE
        if (message != null) binding.tvStatus.text = message
    }

    override fun onDestroy() {
        cancelled = true
        super.onDestroy()
    }
}
