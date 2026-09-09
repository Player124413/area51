package com.a51.android.data

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * Pure string logic that decides which files belong together, so it is safe to
 * test on the JVM.
 */
class DataImporterTest {

    @Test
    fun split_names_are_recognised() {
        assertTrue(DataImporter.isSplitName("music.000"))
        assertTrue(DataImporter.isSplitName("music.001"))
        assertTrue(DataImporter.isSplitName("a.b.999"))
        assertFalse(DataImporter.isSplitName("music.dfs"))
        assertFalse(DataImporter.isSplitName("music.0000"))   // 4 digits is not a split
        assertFalse(DataImporter.isSplitName("music.00"))
        assertFalse(DataImporter.isSplitName("music"))
    }

    @Test
    fun base_name_is_stripped() {
        assertEquals2("music", DataImporter.baseName("music.dfs"))
        assertEquals2("music", DataImporter.baseName("music.000"))
        assertEquals2("a.b", DataImporter.baseName("a.b.000"))
        assertEquals2("noext", DataImporter.baseName("noext"))
    }

    private fun assertEquals2(expected: String, actual: String) {
        org.junit.Assert.assertEquals(expected, actual)
    }
}
