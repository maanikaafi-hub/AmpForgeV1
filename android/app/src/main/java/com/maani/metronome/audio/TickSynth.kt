package com.maani.metronome.audio

import com.maani.metronome.model.TickStyle
import kotlin.math.PI
import kotlin.math.ceil
import kotlin.math.cos
import kotlin.math.exp
import kotlin.math.max
import kotlin.math.min
import kotlin.math.roundToInt
import kotlin.math.sin
import kotlin.random.Random

/**
 * Offline synthesizer for the mechanical metronome tick.
 *
 * The goal is the sound of a real wooden metronome escapement, not an
 * electronic beep. Each tick is a physically-motivated sum of components:
 *
 *  1. **Click** — a few milliseconds of white noise driven through a resonant
 *     2nd-order bandpass. This is the pallet hitting the escape wheel:
 *     a sharp, slightly noisy "clack" with a woody center frequency.
 *  2. **Snap** — a raw broadband noise burst with a sub-millisecond decay,
 *     the very first transient edge of the impact.
 *  3. **Wood body** — two damped sinusoids (~620 Hz and ~1180 Hz), the
 *     resonance of the wooden case being struck.
 *  4. **Metallic ring** — three *inharmonic* partials (ratios 1 : 1.31 : 1.91)
 *     with different decay times: the faint brass/steel shimmer of the
 *     mechanism. Inharmonicity plus staggered decays is what makes it read as
 *     metal rather than as a tone.
 *  5. **Thunk** — two very low, very short sinusoids (~165/92 Hz): the mass of
 *     the pendulum weight landing.
 *
 * The accented downbeat uses the same model with the whole spectrum shifted
 * down (pitchScale < 1 — a physically larger/heavier strike), a louder level,
 * a longer tail and an extra bell partial.
 *
 * Everything is pure Kotlin/JVM with a fixed PRNG seed: the output is
 * deterministic, unit-testable, and rendered once at engine start (each tick
 * is ~200 µs of CPU). Decay envelopes use per-sample multiplication instead of
 * `exp()` per sample, oscillators use direct `sin(i·ω)` so there is no
 * recursion drift even on long tails.
 */
object TickSynth {

    const val SAMPLE_RATE: Int = 48_000

    /** Fixed seed → identical samples on every device and in every test. */
    private const val SEED: Long = 20_240_921L

    /** Output scaling of the resonant bandpass (≈ 1/Q) so the click sits
     *  at a musical level relative to the resonances instead of swamping them. */
    private const val RESONATOR_OUT_SCALE: Float = 0.15f

    /** Amplitude of the raw broadband snap layered over the resonant click. */
    private const val SNAP_GAIN: Float = 0.22f
    private const val SNAP_TAU: Float = 0.00055f

    data class Component(val freqHz: Float, val tauSeconds: Float, val gain: Float)

    data class TickDesign(
        val durationSeconds: Double,
        val targetPeak: Float,
        /** Multiplies every frequency; < 1 = heavier, lower strike (downbeat). */
        val pitchScale: Float,
        val noiseBurstSeconds: Double,
        val noiseResonanceHz: Float,
        val noiseBandwidthHz: Float,
        val noiseGain: Float,
        val wood: List<Component>,
        val metal: List<Component>,
        val thunk: List<Component>,
        /** Extra shimmer partial, used on the accented beat only. */
        val bell: Component?,
    )

    /** Full parameter set for one voicing. */
    fun designFor(style: TickStyle, accent: Boolean): TickDesign {
        val accentPitch = if (accent) 0.90f else 1.00f
        val accentLevel = if (accent) 1.22f else 1.00f
        val accentTail = if (accent) 1.30f else 1.00f

        val (stylePitch, styleNoiseF, styleNoiseBw, styleWood, styleMetal, styleThunk, styleDur, stylePeak) =
            when (style) {
                TickStyle.CLASSIC -> StyleVoice(
                    pitch = 1.00f, noiseF = 3300f, noiseBw = 2600f,
                    wood = 1.00f, metal = 1.00f, thunk = 1.00f,
                    duration = 1.00f, peak = 0.85f,
                )
                TickStyle.BRIGHT -> StyleVoice(
                    pitch = 1.06f, noiseF = 4300f, noiseBw = 3200f,
                    wood = 0.78f, metal = 1.55f, thunk = 0.88f,
                    duration = 0.95f, peak = 0.88f,
                )
                TickStyle.SOFT -> StyleVoice(
                    pitch = 0.94f, noiseF = 2450f, noiseBw = 1750f,
                    wood = 1.08f, metal = 0.42f, thunk = 1.15f,
                    duration = 0.78f, peak = 0.70f,
                )
            }

        val p = stylePitch * accentPitch
        return TickDesign(
            durationSeconds = (if (accent) 0.140 else 0.100) * styleDur,
            targetPeak = if (accent) min(0.97f, stylePeak + 0.12f) else stylePeak,
            pitchScale = p,
            noiseBurstSeconds = if (accent) 0.0050 else 0.0035,
            noiseResonanceHz = styleNoiseF,
            noiseBandwidthHz = styleNoiseBw,
            noiseGain = if (accent) 1.15f else 1.00f,
            wood = listOf(
                Component(620f * p, 0.030f * accentTail, 0.55f * styleWood * accentLevel),
                Component(1180f * p, 0.016f * accentTail, 0.30f * styleWood * accentLevel),
            ),
            metal = listOf(
                Component(2370f * p, 0.050f * accentTail, 0.100f * styleMetal * accentLevel),
                Component(3113f * p, 0.038f * accentTail, 0.070f * styleMetal * accentLevel),
                Component(4521f * p, 0.026f * accentTail, 0.045f * styleMetal * accentLevel),
            ),
            thunk = listOf(
                Component(165f * p, 0.012f * accentTail, 0.34f * styleThunk * accentLevel),
                Component(92f * p, 0.009f * accentTail, 0.20f * styleThunk * accentLevel),
            ),
            bell = if (accent) Component(5060f * p, 0.085f, 0.05f * styleMetal) else null,
        )
    }

    private data class StyleVoice(
        val pitch: Float,
        val noiseF: Float,
        val noiseBw: Float,
        val wood: Float,
        val metal: Float,
        val thunk: Float,
        val duration: Float,
        val peak: Float,
    )

    /** Render one tick as mono PCM-16 at [SAMPLE_RATE]. */
    fun render(style: TickStyle, accent: Boolean): ShortArray =
        toPcm16(renderFloat(style, accent))

    /** Render one tick as mono float samples in roughly -1..1. */
    fun renderFloat(style: TickStyle, accent: Boolean): FloatArray {
        val d = designFor(style, accent)
        val sr = SAMPLE_RATE
        val n = max(64, ceil(d.durationSeconds * sr).toInt())
        val out = FloatArray(n)
        val rand = Random(SEED)

        // --- 1 + snap: noise transient through a resonant bandpass ----------
        val burstN = min(n, (d.noiseBurstSeconds * sr).toInt())
        val theta = 2.0 * PI * d.noiseResonanceHz * d.pitchScale / sr
        val radius = exp(-PI * d.noiseBandwidthHz / sr)
        val c1 = (2.0 * radius * cos(theta)).toFloat()
        val c2 = (-radius * radius).toFloat()
        val snapDecay = exp(-1.0 / (SNAP_TAU * sr)).toFloat()
        var y1 = 0f
        var y2 = 0f
        var snapEnv = SNAP_GAIN * d.noiseGain
        for (i in 0 until n) {
            val white = if (i < burstN) rand.nextFloat() * 2f - 1f else 0f
            // raw snap: broadband, sub-millisecond decay — the impact edge
            if (snapEnv > 1e-5f) {
                out[i] += white * snapEnv + (if (i >= burstN) 0f else 0f)
                snapEnv *= snapDecay
            }
            // resonant clack: the same noise exciting the wood/steel resonance
            if (i < burstN || kotlin.math.abs(y1) + kotlin.math.abs(y2) > 1e-5f) {
                val y = white + c1 * y1 + c2 * y2
                y2 = y1
                y1 = y
                out[i] += y * RESONATOR_OUT_SCALE * d.noiseGain
            } else {
                y1 = 0f
                y2 = 0f
            }
        }

        // --- 2: resonant bodies ----------------------------------------------
        for (c in d.wood) addTone(out, c.freqHz, c.tauSeconds, c.gain)
        for (c in d.metal) addTone(out, c.freqHz, c.tauSeconds, c.gain)
        for (c in d.thunk) addTone(out, c.freqHz, c.tauSeconds, c.gain)
        d.bell?.let { addTone(out, it.freqHz, it.tauSeconds, it.gain) }

        // --- normalize to the design peak (headroom, no clipping) ------------
        var peak = 0f
        for (v in out) if (kotlin.math.abs(v) > peak) peak = kotlin.math.abs(v)
        if (peak > 1e-9f) {
            val g = d.targetPeak / peak
            for (i in out.indices) out[i] *= g
        }

        // --- 2 ms tail fade so the truncated tail can never click ------------
        val fadeN = min(n / 2, (0.002 * sr).toInt())
        for (k in 0 until fadeN) {
            out[n - fadeN + k] *= (1f - k.toFloat() / fadeN)
        }
        return out
    }

    /** One exponentially-decaying sinusoid added into [out] (phase 0 → no attack click). */
    private fun addTone(out: FloatArray, freqHz: Float, tauSeconds: Float, gain: Float) {
        val sr = SAMPLE_RATE
        val decay = exp(-1.0 / (tauSeconds * sr)).toFloat()
        val w = (2.0 * PI * freqHz / sr).toFloat()
        var env = gain
        for (i in out.indices) {
            if (env < 1e-6f) break
            out[i] += (sin(i * w) * env).toFloat()
            env *= decay
        }
    }

    fun toPcm16(samples: FloatArray): ShortArray {
        val out = ShortArray(samples.size)
        for (i in samples.indices) {
            val x = samples[i].coerceIn(-1f, 1f)
            out[i] = (x * 32767f).roundToInt().toShort()
        }
        return out
    }
}
