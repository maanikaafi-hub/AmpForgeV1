package com.maani.metronome.audio

import com.maani.metronome.model.TickStyle
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import kotlin.math.abs

class TickSynthTest {
    @Test fun rendersDeterministicNonSilentFinitePcm() {
        val first = TickSynth.render(TickStyle.CLASSIC, accent = false)
        val second = TickSynth.render(TickStyle.CLASSIC, accent = false)
        assertArrayEquals(first, second)
        assertTrue(first.isNotEmpty())
        assertTrue(first.any { it != 0.toShort() })
        assertTrue(first.all { it != Short.MIN_VALUE })
    }

    @Test fun accentHasMoreEnergyThanRegularTick() {
        val regular = TickSynth.renderFloat(TickStyle.CLASSIC, accent = false)
        val accent = TickSynth.renderFloat(TickStyle.CLASSIC, accent = true)
        assertTrue(rms(accent) > rms(regular))
        assertTrue(maxAbs(accent) <= 0.98f)
        assertTrue(maxAbs(regular) <= 0.86f)
    }

    @Test fun tickVoicingsAreDistinctAndBounded() {
        val classic = TickSynth.render(TickStyle.CLASSIC, false)
        val bright = TickSynth.render(TickStyle.BRIGHT, false)
        val soft = TickSynth.render(TickStyle.SOFT, false)
        assertFalse(classic.contentEquals(bright))
        assertFalse(classic.contentEquals(soft))
        assertEquals(TickSynth.SAMPLE_RATE, 48_000)
        listOf(classic, bright, soft).forEach { samples ->
            assertTrue(samples.all { abs(it.toInt()) < Short.MAX_VALUE })
        }
    }

    private fun rms(samples: FloatArray): Double =
        kotlin.math.sqrt(samples.sumOf { it.toDouble() * it.toDouble() } / samples.size)

    private fun maxAbs(samples: FloatArray): Float = samples.maxOf { abs(it) }
}
