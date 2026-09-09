package com.a51.android.data

import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * JVM tests for the persistence format.  These run on the CI machine before the APK
 * is built, so a broken save format fails the pipeline instead of a phone.
 */
class GameProfileTest {

    private fun sample() = GameProfile(
        id = "g1",
        name = "Area 51 (PC DVD)",
        dir = "/data/data/com.a51.android/files/games/g1",
        sizeBytes = 123_456_789L,
        archiveCount = 6,
        fileCount = 12_345,
        createdAt = 1700000000L,
        lastPlayedAt = 1700000123L
    )

    @Test
    fun profile_round_trips_through_json() {
        val original = sample()
        val restored = GameProfile.fromJson(original.toJson())

        assertEquals(original.id, restored?.id)
        assertEquals(original.name, restored?.name)
        assertEquals(original.dir, restored?.dir)
        assertEquals(original.sizeBytes, restored?.sizeBytes)
        assertEquals(original.archiveCount, restored?.archiveCount)
        assertEquals(original.fileCount, restored?.fileCount)
        assertEquals(original.createdAt, restored?.createdAt)
        assertEquals(original.lastPlayedAt, restored?.lastPlayedAt)
    }

    @Test
    fun list_round_trips() {
        val list = listOf(sample(), sample().copy(id = "g2", name = "second"))
        val json = GameProfile.listToJson(list)
        val restored = GameProfile.listFromJson(json)

        assertEquals(2, restored.size)
        assertEquals("g1", restored[0].id)
        assertEquals("g2", restored[1].id)
    }

    @Test
    fun rejects_garbage() {
        assertNull(GameProfile.fromJson(JSONObject()))
        assertTrue(GameProfile.listFromJson(null).isEmpty())
        assertTrue(GameProfile.listFromJson("").isEmpty())
        assertTrue(GameProfile.listFromJson("not json at all").isEmpty())
        assertTrue(GameProfile.listFromJson("{\"profiles\": 5}").isEmpty())
    }

    @Test
    fun tolerates_missing_fields() {
        val minimal = JSONObject().apply {
            put("id", "only-id")
            put("dir", "/x")
        }
        val restored = GameProfile.fromJson(minimal)
        assertEquals("only-id", restored?.id)
        assertEquals("only-id", restored?.name)   // falls back to the id
        assertEquals(0L, restored?.sizeBytes)
    }
}
