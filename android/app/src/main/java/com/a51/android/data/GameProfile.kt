package com.a51.android.data

import org.json.JSONArray
import org.json.JSONObject

/**
 * One imported set of game files.  The launcher shows a card per profile and
 * the game mounts every archive inside [dir].
 */
data class GameProfile(
    val id: String,
    val name: String,
    val dir: String,
    val sizeBytes: Long,
    val archiveCount: Int,
    val fileCount: Int,
    val createdAt: Long,
    var lastPlayedAt: Long
) {

    fun toJson(): JSONObject = JSONObject().apply {
        put("id", id)
        put("name", name)
        put("dir", dir)
        put("size", sizeBytes)
        put("archives", archiveCount)
        put("files", fileCount)
        put("created", createdAt)
        put("lastPlayed", lastPlayedAt)
    }

    companion object {
        fun fromJson(o: JSONObject): GameProfile? {
            val id = o.optString("id", "")
            val dir = o.optString("dir", "")
            if (id.isEmpty() || dir.isEmpty()) return null
            return GameProfile(
                id = id,
                name = o.optString("name", id),
                dir = dir,
                sizeBytes = o.optLong("size", 0L),
                archiveCount = o.optInt("archives", 0),
                fileCount = o.optInt("files", 0),
                createdAt = o.optLong("created", 0L),
                lastPlayedAt = o.optLong("lastPlayed", 0L)
            )
        }

        fun listToJson(list: List<GameProfile>): String {
            val arr = JSONArray()
            list.forEach { arr.put(it.toJson()) }
            return JSONObject().put("version", 1).put("profiles", arr).toString()
        }

        fun listFromJson(text: String?): List<GameProfile> {
            if (text.isNullOrEmpty()) return emptyList()
            return try {
                val root = JSONObject(text)
                val arr = root.optJSONArray("profiles") ?: return emptyList()
                val out = ArrayList<GameProfile>(arr.length())
                for (i in 0 until arr.length()) {
                    fromJson(arr.optJSONObject(i) ?: continue)?.let { out.add(it) }
                }
                out
            } catch (e: Exception) {
                emptyList()
            }
        }
    }
}
