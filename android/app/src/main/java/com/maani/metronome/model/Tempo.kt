package com.maani.metronome.model

import kotlin.math.roundToInt
import kotlin.math.sqrt

/**
 * Pure tempo mathematics for the metronome.
 *
 * Everything here is plain Kotlin (no Android dependencies) so it can be
 * exhaustively unit-tested on the JVM.
 */
object Tempo {

    const val MIN_BPM: Int = 40
    const val MAX_BPM: Int = 208

    fun clamp(bpm: Int): Int = bpm.coerceIn(MIN_BPM, MAX_BPM)

    /**
     * Interval between two quarter-note beats in milliseconds.
     *
     * At tempo T:  beatIntervalMs = 60000.0 / T
     */
    fun beatIntervalMs(bpm: Int): Double {
        require(bpm > 0) { "bpm must be positive, was $bpm" }
        return 60_000.0 / bpm
    }

    /** Interval between two quarter-note beats in seconds. */
    fun beatIntervalSeconds(bpm: Int): Double = beatIntervalMs(bpm) / 1000.0

    /**
     * Traditional Italian tempo marking for a BPM value.
     * Ranges follow the common conservatory convention:
     *
     *  - Largo     40–59
     *  - Adagio    60–75
     *  - Andante   76–107
     *  - Moderato  108–119
     *  - Allegro   120–155
     *  - Vivace    156–175
     *  - Presto    176–208
     */
    fun marking(bpm: Int): String = when (clamp(bpm)) {
        in MIN_BPM..59 -> "Largo"
        in 60..75 -> "Adagio"
        in 76..107 -> "Andante"
        in 108..119 -> "Moderato"
        in 120..155 -> "Allegro"
        in 156..175 -> "Vivace"
        else -> "Presto"
    }

    /**
     * The engraved scale of a classic mechanical metronome (Wittner-style),
     * 40 through 208 BPM with the traditional non-linear spacing.
     */
    val scaleValues: List<Int> = listOf(
        40, 42, 44, 46, 48, 50, 52, 54, 56, 58,
        60, 63, 66, 69, 72, 76, 80, 84, 88, 92,
        96, 100, 104, 108, 112, 116, 120, 126, 132, 138,
        144, 152, 160, 168, 176, 184, 192, 200, 208,
    )

    /** Scale values that get a printed number on the engraved scale. */
    val labeledScaleValues: Set<Int> = setOf(
        40, 50, 60, 72, 84, 96, 108, 120, 132, 144, 160, 176, 192, 208,
    )

    /**
     * Physical position of the pendulum weight for a tempo, in `0.0..1.0`
     * (0 = bottom of the rod = 40 BPM, 1 = top = 208 BPM).
     *
     * A real metronome is a compound pendulum: with the sliding weight at
     * distance d from the pivot, the beat frequency obeys f ∝ 1/√d, i.e.
     * d ∝ 1/f². That is why the engraved numbers crowd together towards the
     * top of the scale — and this function reproduces exactly that law so the
     * brass weight always sits opposite its correct marking.
     */
    fun weightPosition(bpm: Int): Double {
        val b = clamp(bpm).toDouble()
        val invMin = 1.0 / (MIN_BPM.toDouble() * MIN_BPM.toDouble())   // 1/40²
        val invMax = 1.0 / (MAX_BPM.toDouble() * MAX_BPM.toDouble())   // 1/208²
        return (invMin - 1.0 / (b * b)) / (invMin - invMax)
    }

    /** Inverse of [weightPosition]: the tempo engraved at a rod position. */
    fun bpmAtPosition(position: Double): Int {
        val p = position.coerceIn(0.0, 1.0)
        val invMin = 1.0 / (MIN_BPM.toDouble() * MIN_BPM.toDouble())
        val invMax = 1.0 / (MAX_BPM.toDouble() * MAX_BPM.toDouble())
        val inv = invMin - p * (invMin - invMax)
        return clamp((1.0 / sqrt(inv)).roundToInt())
    }
}
