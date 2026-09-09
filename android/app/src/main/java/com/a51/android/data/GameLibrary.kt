package com.a51.android.data

import android.content.Context
import java.io.File

/**
 * The list of imported game data sets, stored as JSON in the app private
 * storage.  Tiny on purpose - the game files themselves live next to it.
 */
object GameLibrary {

    private lateinit var appContext: Context
    private val profiles = ArrayList<GameProfile>()

    @JvmStatic
    fun init(context: Context) {
        if (!::appContext.isInitialized) {
            appContext = context.applicationContext
            load()
        }
    }

    private fun libraryFile(): File = File(appContext.filesDir, "library.json")

    /** Where a profile's game files live. */
    @JvmStatic
    fun dirFor(id: String): File = File(File(appContext.filesDir, "games"), id)

    @JvmStatic
    fun all(): List<GameProfile> = synchronized(profiles) { ArrayList(profiles) }

    @JvmStatic
    fun find(id: String): GameProfile? =
        synchronized(profiles) { profiles.firstOrNull { it.id == id } }

    @JvmStatic
    fun add(profile: GameProfile) {
        synchronized(profiles) {
            profiles.removeAll { it.id == profile.id }
            profiles.add(profile)
        }
        save()
    }

    @JvmStatic
    fun touch(id: String) {
        synchronized(profiles) {
            profiles.firstOrNull { it.id == id }?.lastPlayedAt = System.currentTimeMillis()
        }
        save()
    }

    @JvmStatic
    fun remove(id: String): Boolean {
        val profile = find(id) ?: return false
        synchronized(profiles) { profiles.removeAll { it.id == id } }
        save()
        deleteRecursive(File(profile.dir))
        return true
    }

    private fun load() {
        val file = libraryFile()
        if (!file.exists()) return
        val loaded = try {
            GameProfile.listFromJson(file.readText())
        } catch (e: Exception) {
            emptyList()
        }
        synchronized(profiles) {
            profiles.clear()
            // Drop entries whose folder is gone (cleared cache, moved files).
            profiles.addAll(loaded.filter { File(it.dir).isDirectory })
        }
    }

    private fun save() {
        try {
            libraryFile().writeText(GameProfile.listToJson(all()))
        } catch (e: Exception) {
            android.util.Log.e("A51", "could not save the library", e)
        }
    }

    private fun deleteRecursive(file: File) {
        if (!file.exists()) return
        if (file.isDirectory) {
            file.listFiles()?.forEach { deleteRecursive(it) }
        }
        @Suppress("UNUSED_EXPRESSION")
        file.delete()
    }
}
