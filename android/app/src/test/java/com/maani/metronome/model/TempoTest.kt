package com.maani.metronome.model

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class TempoTest {
    @Test fun quarterNoteIntervalsMatchSpecification() {
        assertEquals(1_500.0, Tempo.beatIntervalMs(40), 1e-9)
        assertEquals(1_000.0, Tempo.beatIntervalMs(60), 1e-9)
        assertEquals(500.0, Tempo.beatIntervalMs(120), 1e-9)
        assertEquals(60_000.0 / 208.0, Tempo.beatIntervalMs(208), 1e-9)
    }

    @Test fun clampsToSupportedInstrumentRange() {
        assertEquals(40, Tempo.clamp(0))
        assertEquals(40, Tempo.clamp(40))
        assertEquals(208, Tempo.clamp(208))
        assertEquals(208, Tempo.clamp(999))
    }

    @Test fun weightScaleIsMonotoneAndRoundTrips() {
        var last = -1.0
        for (bpm in Tempo.MIN_BPM..Tempo.MAX_BPM) {
            val position = Tempo.weightPosition(bpm)
            assertTrue("weight should move upward as tempo increases", position > last)
            assertTrue(kotlin.math.abs(bpm - Tempo.bpmAtPosition(position)) <= 1)
            last = position
        }
        assertEquals(0.0, Tempo.weightPosition(40), 1e-9)
        assertEquals(1.0, Tempo.weightPosition(208), 1e-9)
    }

    @Test fun traditionalTempoMarkings() {
        assertEquals("Largo", Tempo.marking(40))
        assertEquals("Adagio", Tempo.marking(60))
        assertEquals("Andante", Tempo.marking(90))
        assertEquals("Moderato", Tempo.marking(112))
        assertEquals("Allegro", Tempo.marking(120))
        assertEquals("Vivace", Tempo.marking(160))
        assertEquals("Presto", Tempo.marking(200))
    }

    @Test(expected = IllegalArgumentException::class)
    fun rejectsZeroTempoInterval() { Tempo.beatIntervalMs(0) }
}
