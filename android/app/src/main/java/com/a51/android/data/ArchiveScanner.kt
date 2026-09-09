package com.a51.android.data

import com.a51.android.core.NativeBridge
import org.json.JSONObject

/**
 * What the native DFS reader found in a folder.
 */
data class ArchiveInfo(
    val path: String,
    val valid: Boolean,
    val error: String,
    val totalSize: Long,
    val dfsSize: Long,
    val files: Int,
    val splits: Int,
    val version: Int
) {
    val displayName: String
        get() {
            val idx = path.lastIndexOf('/')
            return if (idx >= 0 && idx < path.length - 1) path.substring(idx + 1) else path
        }

    companion object {
        fun fromJson(o: JSONObject): ArchiveInfo = ArchiveInfo(
            path = o.optString("path", ""),
            valid = o.optBoolean("valid", false),
            error = o.optString("error", ""),
            totalSize = o.optDouble("size", 0.0).toLong(),
            dfsSize = o.optDouble("dfsSize", 0.0).toLong(),
            files = o.optInt("files", 0),
            splits = o.optInt("splits", 0),
            version = o.optInt("version", 0)
        )
    }
}

/**
 * Scans a folder for .dfs archives using the native reader.
 *
 * Only works on real file system paths (that is: not on SAF content URIs - for
 * those the launcher lists the folder first and scans after the import).
 */
object ArchiveScanner {

    @JvmStatic
    fun scan(path: String, maxDepth: Int = 6): List<ArchiveInfo> {
        if (!NativeBridge.available) return emptyList()
        return try {
            val json = NativeBridge.nativeScanFolder(path, maxDepth)
            val arr = org.json.JSONArray(json)
            val out = ArrayList<ArchiveInfo>(arr.length())
            for (i in 0 until arr.length()) {
                val o = arr.optJSONObject(i) ?: continue
                val info = ArchiveInfo.fromJson(o)
                if (info.path.isNotEmpty()) out.add(info)
            }
            out.sortedBy { it.path.lowercase() }
        } catch (e: Exception) {
            android.util.Log.e("A51", "scan failed for $path", e)
            emptyList()
        }
    }

    @JvmStatic
    fun folderSize(path: String, maxDepth: Int = 6): Long {
        if (!NativeBridge.available) return 0L
        return try {
            NativeBridge.nativeFolderSize(path, maxDepth)
        } catch (e: Exception) {
            0L
        }
    }
}
