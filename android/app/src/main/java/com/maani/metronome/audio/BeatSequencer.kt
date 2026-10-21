package com.maani.metronome.audio

import com.maani.metronome.model.TimeSignature

/** Pure beat-grid calculations shared by the scheduler and local JVM tests. */
object BeatSequencer {
    /** Zero-based beat in the measure, always in 0 until beatsPerMeasure. */
    fun beatInMeasure(absoluteBeat: Long, beatsPerMeasure: Int): Int {
        require(beatsPerMeasure in 2..7) { "Unsupported meter: $beatsPerMeasure" }
        return Math.floorMod(absoluteBeat, beatsPerMeasure.toLong()).toInt()
    }

    fun beatInMeasure(absoluteBeat: Long, signature: TimeSignature): Int =
        beatInMeasure(absoluteBeat, signature.beatsPerMeasure)

    fun isAccent(absoluteBeat: Long, signature: TimeSignature): Boolean =
        beatInMeasure(absoluteBeat, signature) == 0

    /** Next nominal beat deadline. Addition rather than now+interval avoids accumulated drift. */
    fun nextDeadline(previousDeadlineNanos: Long, beatIntervalNanos: Long): Long {
        require(beatIntervalNanos > 0)
        return previousDeadlineNanos + beatIntervalNanos
    }
}
