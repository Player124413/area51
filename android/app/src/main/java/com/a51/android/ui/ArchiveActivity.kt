package com.a51.android.ui

import android.os.Bundle
import android.text.format.Formatter
import androidx.appcompat.app.AppCompatActivity
import androidx.recyclerview.widget.LinearLayoutManager
import com.a51.android.R
import com.a51.android.core.NativeBridge
import com.a51.android.data.ArchiveScanner
import com.a51.android.data.ArchiveInfo
import com.a51.android.databinding.ActivityArchiveBinding
import org.json.JSONObject

/**
 * Reads a .dfs archive with the native reader and shows what is inside.
 *
 * This is also the fastest way to check that a dump is healthy: the verify
 * button runs the engine's own CRC16 over the 32k chunks.
 */
class ArchiveActivity : AppCompatActivity() {

    companion object {
        const val EXTRA_DIR = "extra_dir"
        private const val PAGE = 200
    }

    private lateinit var binding: ActivityArchiveBinding
    private val adapter = FileAdapter { showPreview(it) }

    private var archives: List<ArchiveInfo> = emptyList()
    private var current = -1
    private var loaded = 0
    private var totalFiles = 0

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityArchiveBinding.inflate(layoutInflater)
        setContentView(binding.root)

        binding.rvFiles.layoutManager = LinearLayoutManager(this)
        binding.rvFiles.adapter = adapter

        binding.btnMore.setOnClickListener { loadPage() }
        binding.btnVerify.setOnClickListener { verify() }
        binding.tvArchive.setOnClickListener { nextArchive() }

        val dir = intent?.getStringExtra(EXTRA_DIR).orEmpty()

        Thread {
            val found = ArchiveScanner.scan(dir)
            runOnUiThread {
                archives = found
                if (found.isEmpty()) {
                    binding.tvInfo.text = getString(R.string.import_no_archives)
                    binding.btnMore.isEnabled = false
                    binding.btnVerify.isEnabled = false
                } else {
                    openArchive(0)
                }
            }
        }.start()
    }

    private fun nextArchive() {
        if (archives.size < 2) return
        openArchive((current + 1) % archives.size)
    }

    private fun openArchive(index: Int) {
        if (index !in archives.indices) return
        current = index
        val info = archives[index]

        adapter.clear()
        loaded = 0
        totalFiles = 0

        if (!NativeBridge.available) return

        if (!NativeBridge.nativeBrowseOpen(info.path)) {
            binding.tvArchive.text = info.displayName
            binding.tvInfo.text = getString(R.string.archive_open_failed, NativeBridge.let {
                try {
                    JSONObject(NativeBridge.nativeBrowseInfo()).optString("error", "?")
                } catch (e: Exception) {
                    "?"
                }
            })
            binding.btnMore.isEnabled = false
            binding.btnVerify.isEnabled = false
            return
        }

        try {
            val o = JSONObject(NativeBridge.nativeBrowseInfo())
            totalFiles = o.optInt("files", 0)
            binding.tvArchive.text = "${info.displayName}   (${index + 1}/${archives.size})"
            binding.tvInfo.text = getString(
                R.string.archive_info,
                totalFiles,
                o.optInt("splits", 0),
                o.optInt("version", 0)
            ) + "  ·  " + Formatter.formatFileSize(this, o.optDouble("dataSize", 0.0).toLong())
        } catch (e: Exception) {
            binding.tvInfo.text = getString(R.string.error_generic)
        }

        binding.btnMore.isEnabled = true
        binding.btnVerify.isEnabled = true
        binding.tvVerifyResult.text = ""
        loadPage()
    }

    private fun loadPage() {
        if (current < 0 || !NativeBridge.available) return

        Thread {
            val json = try {
                NativeBridge.nativeBrowseList(loaded, PAGE)
            } catch (e: Exception) {
                null
            }

            val entries = ArrayList<FileAdapter.FileEntry>()
            if (json != null) {
                try {
                    val o = JSONObject(json)
                    val arr = o.optJSONArray("entries")
                    totalFiles = o.optInt("total", totalFiles)
                    if (arr != null) {
                        for (i in 0 until arr.length()) {
                            val e = arr.optJSONObject(i) ?: continue
                            entries.add(
                                FileAdapter.FileEntry(
                                    index = e.optInt("i", -1),
                                    name = e.optString("name", "?"),
                                    size = e.optDouble("size", 0.0).toLong(),
                                    offset = e.optDouble("offset", 0.0).toLong()
                                )
                            )
                        }
                    }
                } catch (e: Exception) {
                    android.util.Log.e("A51", "could not parse the archive listing", e)
                }
            }

            runOnUiThread {
                adapter.addAll(entries)
                loaded += entries.size
                binding.btnMore.isEnabled = loaded < totalFiles
            }
        }.start()
    }

    private fun verify() {
        if (!NativeBridge.available) return
        binding.btnVerify.isEnabled = false
        binding.tvVerifyResult.text = getString(R.string.import_scanning)

        Thread {
            // 0 = every chunk.  A full disc takes a while, so this runs off the
            // UI thread and the result is posted back.
            val bad = try {
                NativeBridge.nativeBrowseVerify(0)
            } catch (e: Exception) {
                -2
            }
            val checked = totalFiles

            runOnUiThread {
                binding.btnVerify.isEnabled = true
                binding.tvVerifyResult.text = when {
                    bad == -1 -> getString(R.string.archive_verify_none)
                    bad < 0 -> getString(R.string.error_generic)
                    bad == 0 -> getString(R.string.archive_verify_ok, checked)
                    else -> getString(R.string.archive_verify_bad, bad, checked)
                }
            }
        }.start()
    }

    private fun showPreview(entry: FileAdapter.FileEntry) {
        if (!NativeBridge.available) return

        Thread {
            val bytes = try {
                NativeBridge.nativeBrowseRead(entry.index, 0, 4096)
            } catch (e: Exception) {
                null
            }

            val text = bytes?.let {
                val printable = it.count { b -> b in 32..126 || b == 10 || b == 13 }
                if (printable > it.size * 3 / 4) String(it, Charsets.UTF_8)
                else hexDump(it)
            } ?: getString(R.string.error_generic)

            runOnUiThread {
                com.google.android.material.dialog.MaterialAlertDialogBuilder(this)
                    .setTitle(entry.name)
                    .setMessage(text)
                    .setPositiveButton(R.string.ok, null)
                    .show()
            }
        }.start()
    }

    private fun hexDump(bytes: ByteArray): String {
        val sb = StringBuilder()
        var i = 0
        while (i < bytes.size && sb.length < 4000) {
            sb.append(String.format("%08X  ", i))
            for (j in 0 until 8) {
                if (i + j < bytes.size) sb.append(String.format("%02X ", bytes[i + j]))
                else sb.append("   ")
            }
            sb.append('\n')
            i += 8
        }
        return sb.toString()
    }

    override fun onDestroy() {
        if (NativeBridge.available) {
            try {
                NativeBridge.nativeBrowseClose()
            } catch (e: Exception) {
                // shutting down anyway
            }
        }
        super.onDestroy()
    }
}
