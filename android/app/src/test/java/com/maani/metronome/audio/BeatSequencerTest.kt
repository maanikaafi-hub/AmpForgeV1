package com.maani.metronome.audio

import com.maani.metronome.model.TimeSignature
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class BeatSequencerTest {
    @Test fun accentsExactlyTheFirstBeatInEveryMeter() {
        TimeSignature.entries.forEach { signature ->
            val sequence = (0L until (signature.beatsPerMeasure * 3L)).map {
                BeatSequencer.beatInMeasure(it, signature)
            }
            assertEquals((0 until signature.beatsPerMeasure).toList().let { it + it + it }, sequence)
            for (beat in sequence.indices) {
                val expected = beat % signature.beatsPerMeasure == 0
                assertEquals(expected, BeatSequencer.isAccent(beat.toLong(), signature))
            }
        }
    }

    @Test fun fourFourDownbeatsAreNotConfusedWithBeatOneBasedLabel() {
        assertTrue(BeatSequencer.isAccent(0, TimeSignature.FOUR_FOUR))
        assertFalse(BeatSequencer.isAccent(1, TimeSignature.FOUR_FOUR))
        assertFalse(BeatSequencer.isAccent(2, TimeSignature.FOUR_FOUR))
        assertFalse(BeatSequencer.isAccent(3, TimeSignature.FOUR_FOUR))
        assertTrue(BeatSequencer.isAccent(4, TimeSignature.FOUR_FOUR))
    }

    @Test fun invalidMeterIsRejected() {
        org.junit.Assert.assertThrows(IllegalArgumentException::class.java) {
            BeatSequencer.beatInMeasure(0, 1)
        }
    }

    @Test fun nominalDeadlinesAccumulateFromPreviousEdge() {
        val interval = 500_000_000L
        assertEquals(2_000_000_000L, BeatSequencer.nextDeadline(1_500_000_000L, interval))
        assertEquals(1_000_000_000L, BeatSequencer.nextDeadline(500_000_000L, interval))
    }
}
