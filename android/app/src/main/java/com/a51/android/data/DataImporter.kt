package com.a51.android.data

import android.content.Context
import android.net.Uri
import android.provider.OpenableColumns
import androidx.documentfile.provider.DocumentFile
import java.io.File
import java.io.FileOutputStream

/**
 * Copies game files into the app private storage.
 *
 * Three ways in, one way out:
 *   * a SAF folder (ACTION_OPEN_DOCUMENT_TREE) - the normal path on any phone;
 *   * files shared into the app (ACTION_SEND / SEND_MULTIPLE);
 *   * a plain path typed by the user, for phones where the files already sit on
 *     readable storage (Termux, a rooted device, an SD card on older Android).
 *
 * Everything ends up as ordinary files under files/games/<id>/, which is what
 * the native DFS reader needs - it works on file descriptors, not on content
 * URIs.
 */
object DataImporter {

    data class SafEntry(
        val uri: Uri,
        val name: String,
        val size: Long,
        val isArchive: Boolean,
        val isSplit: Boolean
    )

    data class Progress(val currentName: String, val doneBytes: Long, val totalBytes: Long)

    sealed class Result {
        data class Ok(val copiedFiles: Int, val bytes: Long) : Result()
        data class Failed(val message: String) : Result()
        object Cancelled : Result()
    }

    private const val BUFFER = 512 * 1024

    // ------------------------------------------------------------------
    //  SAF folder
    // ------------------------------------------------------------------

    /** Lists the files of a picked folder, newest Android friendly. */
    @JvmStatic
    fun listFolder(context: Context, treeUri: Uri, maxDepth: Int = 4): List<SafEntry> {
        val out = ArrayList<SafEntry>()
        val root = DocumentFile.fromTreeUri(context, treeUri) ?: return out
        walk(context, root, 0, maxDepth, out)
        out.sortBy { it.name.lowercase() }
        return out
    }

    private fun walk(
        context: Context,
        dir: DocumentFile,
        depth: Int,
        maxDepth: Int,
        out: MutableList<SafEntry>
    ) {
        if (depth > maxDepth) return
        val children = try {
            dir.listFiles()
        } catch (e: SecurityException) {
            return
        }
        for (child in children) {
            if (child.isDirectory) {
                walk(context, child, depth + 1, maxDepth, out)
            } else if (child.isFile) {
                val name = child.name ?: continue
                out.add(
                    SafEntry(
                        uri = child.uri,
                        name = name,
                        size = child.length(),
                        isArchive = name.lowercase().endsWith(".dfs"),
                        isSplit = SPLIT_REGEX.matches(name)
                    )
                )
            }
        }
    }

    private val SPLIT_REGEX = Regex(".*\\.\\d{3}$")

    // ------------------------------------------------------------------
    //  Copying
    // ------------------------------------------------------------------

    /**
     * Copies [entries] into [destDir].  When an archive is selected its data
     * splits (.000, .001, ...) come along automatically - without them the
     * archive is useless.
     */
    @JvmStatic
    fun importEntries(
        context: Context,
        entries: List<SafEntry>,
        destDir: File,
        cancelled: () -> Boolean,
        onProgress: (Progress) -> Unit
    ): Result {
        if (entries.isEmpty()) return Result.Failed("nothing selected")
        if (!destDir.exists() && !destDir.mkdirs()) {
            return Result.Failed("cannot create ${destDir.absolutePath}")
        }

        // Work out which splits belong to the selected archives: "music.dfs"
        // needs "music.000", "music.001", ... to be of any use at all.
        val wantedNames = HashSet<String>()
        val wantedBases = HashSet<String>()
        for (e in entries) {
            val n = e.name.lowercase()
            if (e.isArchive) {
                wantedNames.add(n)
                wantedBases.add(baseName(n))
            } else if (!isSplitName(n)) {
                wantedNames.add(n)
            }
        }

        val toCopy = entries.filter { e ->
            val n = e.name.lowercase()
            when {
                n in wantedNames -> true
                isSplitName(n) -> baseName(n) in wantedBases
                else -> false
            }
        }
        if (toCopy.isEmpty()) return Result.Failed("nothing to copy")

        val total = toCopy.sumOf { it.size.coerceAtLeast(0L) }
        var done = 0L
        var copied = 0

        for (entry in toCopy) {
            if (cancelled()) return Result.Cancelled
            onProgress(Progress(entry.name, done, total))

            val target = File(destDir, entry.name)
            val ok = try {
                val input = context.contentResolver.openInputStream(entry.uri)
                if (input == null) {
                    false
                } else {
                    input.use { src ->
                        FileOutputStream(target).use { output ->
                            val buffer = ByteArray(BUFFER)
                            while (true) {
                                if (cancelled()) {
                                    target.delete()
                                    return Result.Cancelled
                                }
                                val read = src.read(buffer)
                                if (read < 0) break
                                output.write(buffer, 0, read)
                                done += read
                                onProgress(Progress(entry.name, done, total))
                            }
                        }
                    }
                    true
                }
            } catch (e: Exception) {
                android.util.Log.e("A51", "copy failed for ${entry.name}", e)
                false
            }

            if (!ok) {
                target.delete()
                return Result.Failed(entry.name)
            }
            copied++
        }

        onProgress(Progress("", total, total))
        return Result.Ok(copied, done)
    }

    /** Copies plain paths (the "typed a folder" flow). */
    @JvmStatic
    fun importPaths(
        sources: List<File>,
        destDir: File,
        cancelled: () -> Boolean,
        onProgress: (Progress) -> Unit
    ): Result {
        if (sources.isEmpty()) return Result.Failed("nothing selected")
        if (!destDir.exists() && !destDir.mkdirs()) {
            return Result.Failed("cannot create ${destDir.absolutePath}")
        }

        val total = sources.sumOf { it.length() }
        var done = 0L
        var copied = 0

        for (src in sources) {
            if (cancelled()) return Result.Cancelled
            onProgress(Progress(src.name, done, total))

            val target = File(destDir, src.name)
            val ok = try {
                src.inputStream().use { input ->
                    FileOutputStream(target).use { output ->
                        val buffer = ByteArray(BUFFER)
                        while (true) {
                            if (cancelled()) {
                                target.delete()
                                return Result.Cancelled
                            }
                            val read = input.read(buffer)
                            if (read < 0) break
                            output.write(buffer, 0, read)
                            done += read
                            onProgress(Progress(src.name, done, total))
                        }
                    }
                }
                true
            } catch (e: Exception) {
                android.util.Log.e("A51", "copy failed for ${src.name}", e)
                false
            }

            if (!ok) {
                target.delete()
                return Result.Failed(src.name)
            }
            copied++
        }

        onProgress(Progress("", total, total))
        return Result.Ok(copied, done)
    }

    /** Copies files that were shared into the app. */
    @JvmStatic
    fun importUris(
        context: Context,
        uris: List<Uri>,
        destDir: File,
        cancelled: () -> Boolean,
        onProgress: (Progress) -> Unit
    ): Result {
        val entries = uris.map { uri ->
            val name = queryDisplayName(context, uri) ?: "shared_${System.nanoTime()}.bin"
            SafEntry(uri, name, querySize(context, uri), false, false)
        }
        return importEntries(context, entries, destDir, cancelled, onProgress)
    }

    // ------------------------------------------------------------------
    //  Helpers
    // ------------------------------------------------------------------

    @JvmStatic
    fun queryDisplayName(context: Context, uri: Uri): String? {
        return try {
            context.contentResolver.query(uri, null, null, null, null)?.use { c ->
                val idx = c.getColumnIndex(OpenableColumns.DISPLAY_NAME)
                if (idx >= 0 && c.moveToFirst()) c.getString(idx) else null
            }
        } catch (e: Exception) {
            uri.lastPathSegment
        } ?: uri.lastPathSegment
    }

    private fun querySize(context: Context, uri: Uri): Long {
        return try {
            context.contentResolver.query(uri, null, null, null, null)?.use { c ->
                val idx = c.getColumnIndex(OpenableColumns.SIZE)
                if (idx >= 0 && c.moveToFirst() && !c.isNull(idx)) c.getLong(idx) else 0L
            } ?: 0L
        } catch (e: Exception) {
            0L
        }
    }

    /** "audio" for "audio.dfs", used to find the matching .000/.001 files. */
    @JvmStatic
    fun baseName(name: String): String = name.substringBeforeLast('.', name)

    @JvmStatic
    fun isSplitName(name: String): Boolean = SPLIT_REGEX.matches(name)
}
