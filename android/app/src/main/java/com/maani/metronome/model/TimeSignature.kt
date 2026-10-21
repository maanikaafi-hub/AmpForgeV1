package com.maani.metronome.model

/**
 * x/4 time signatures supported by the metronome: 2/4 through 7/4.
 * Each beat of the grid is a quarter note, so the denominator never changes.
 */
enum class TimeSignature(val beatsPerMeasure: Int) {
    TWO_FOUR(2),
    THREE_FOUR(3),
    FOUR_FOUR(4),
    FIVE_FOUR(5),
    SIX_FOUR(6),
    SEVEN_FOUR(7);


    /** Human readable label, e.g. "3/4". */
    val label: String get() = "$beatsPerMeasure/4"

    /** The first beat of every measure is accented (audible + visual downbeat). */
    fun isAccentBeat(zeroBasedBeatInMeasure: Int): Boolean = zeroBasedBeatInMeasure == 0

    companion object {
        val DEFAULT: TimeSignature = FOUR_FOUR

        fun fromBeatsPerMeasure(beats: Int): TimeSignature =
            entries.firstOrNull { it.beatsPerMeasure == beats } ?: DEFAULT
    }
}
