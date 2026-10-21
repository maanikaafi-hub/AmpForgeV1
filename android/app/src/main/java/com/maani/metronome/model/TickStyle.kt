package com.maani.metronome.model

/**
 * Voicings of the synthesized mechanical tick. They are subtle EQ/material
 * variations of the same physical model (see `TickSynth`), not different
 * samples.
 */
enum class TickStyle(val displayName: String, val description: String) {
    CLASSIC(
        displayName = "Classic walnut",
        description = "Warm woody knock with a quiet brass ring — the house sound.",
    ),
    BRIGHT(
        displayName = "Bright brass",
        description = "Sharper attack and more metallic resonance; cuts through loud rooms.",
    ),
    SOFT(
        displayName = "Soft practice",
        description = "Gentler transient and shorter ring for long, quiet sessions.",
    );

    companion object {
        val DEFAULT: TickStyle = CLASSIC

        fun fromName(name: String?): TickStyle =
            entries.firstOrNull { it.name == name } ?: DEFAULT
    }
}
