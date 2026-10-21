package com.maani.metronome.audio

import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioTrack
import android.os.Process
import android.os.SystemClock
import com.maani.metronome.model.Tempo
import com.maani.metronome.model.TickStyle
import com.maani.metronome.model.TimeSignature
import java.io.Closeable
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicLong
import java.util.concurrent.atomic.AtomicReference
import kotlin.math.max

/**
 * Standalone, UI-independent metronome engine.
 *
 * A single priority audio worker owns its `AudioTrack`, mixes pre-rendered mono
 * mechanical tick samples into short PCM-16 buffers, and schedules beat edges
 * against `System.nanoTime()`. The next deadline is advanced from the previous
 * deadline (not from the current wake-up time), so thread scheduling jitter
 * does not accumulate into tempo drift. When the worker falls more than one
 * interval behind (e.g. device slept), it resynchronizes instead of producing
 * a burst of catch-up ticks.
 *
 * At 208 BPM the beat period is ~288 ms; the maximum tick is 140 ms, so beats
 * cannot overlap. At 48 kHz the 5 ms output blocks are small enough for
 * low-latency playback on modern devices, and the configured track buffer is
 * bounded to the platform-reported minimum. Tick samples are synthesized once
 * per voicing when playback starts, not allocated on every beat.
 *
 * Listener callbacks are delivered from the audio worker and MUST be light.
 * [onBeat] includes the audio scheduler's monotonic target time in nanoseconds;
 * the UI can use that timestamp as an animation phase anchor.
 */
class MetronomeAudioEngine(
    private val onBeat: (BeatEvent) -> Unit,
    private val onError: (String) -> Unit = {},
) : Closeable {

    data class BeatEvent(
        val absoluteBeat: Long,
        val beatInMeasure: Int,
        val accented: Boolean,
        val scheduledAtNanos: Long,
        val tempoBpm: Int,
    )

    private val playing = AtomicBoolean(false)
    private val closed = AtomicBoolean(false)
    private val tempo = AtomicLong(96L)
    private val volumeBits = AtomicLong(java.lang.Float.floatToRawIntBits(0.72f).toLong())
    private val meter = AtomicReference(TimeSignature.DEFAULT)
    private val style = AtomicReference(TickStyle.DEFAULT)
    private val workerRef = AtomicReference<Thread?>(null)

    @Volatile private var generation = 0L

    val isPlaying: Boolean get() = playing.get()

    fun setTempo(bpm: Int) {
        tempo.set(Tempo.clamp(bpm).toLong())
    }

    fun setVolume(volume: Float) {
        volumeBits.set(java.lang.Float.floatToRawIntBits(volume.coerceIn(0f, 1f)).toLong())
    }

    fun setTimeSignature(signature: TimeSignature) {
        meter.set(signature)
    }

    fun setTickStyle(tickStyle: TickStyle) {
        style.set(tickStyle)
    }

    /** Start with an immediate downbeat. Repeated start calls are harmless. */
    @Synchronized
    fun start() {
        check(!closed.get()) { "Audio engine is closed" }
        if (!playing.compareAndSet(false, true)) return
        val runGeneration = ++generation
        val worker = Thread({ runAudio(runGeneration) }, "MaaniMetronomeAudio").apply {
            priority = Thread.MAX_PRIORITY - 1
            isDaemon = true
        }
        workerRef.set(worker)
        worker.start()
    }

    /** Stop and join the worker before returning; no duplicate audio loops. */
    @Synchronized
    fun stop() {
        if (!playing.compareAndSet(true, false)) return
        generation++
        val worker = workerRef.getAndSet(null)
        worker?.let {
            it.interrupt()
            if (it !== Thread.currentThread()) {
                try {
                    it.join(700)
                } catch (_: InterruptedException) {
                    Thread.currentThread().interrupt()
                }
            }
        }
    }

    private fun runAudio(runGeneration: Long) {
        var track: AudioTrack? = null
        try {
            Process.setThreadPriority(Process.THREAD_PRIORITY_AUDIO)
            val soundCache = TickStyle.entries.associateWith { tickStyle ->
                arrayOf(
                    TickSynth.render(tickStyle, accent = false),
                    TickSynth.render(tickStyle, accent = true),
                )
            }
            val minBytes = AudioTrack.getMinBufferSize(
                TickSynth.SAMPLE_RATE,
                AudioFormat.CHANNEL_OUT_MONO,
                AudioFormat.ENCODING_PCM_16BIT,
            )
            check(minBytes > 0) { "AudioTrack is not supported on this device" }
            // Keep the hardware queue at the platform-reported minimum to avoid
            // adding a large software latency behind the beat scheduler.
            val bufferBytes = max(minBytes, 1_024)
            val attributes = AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_MEDIA)
                .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                .build()
            val format = AudioFormat.Builder()
                .setSampleRate(TickSynth.SAMPLE_RATE)
                .setChannelMask(AudioFormat.CHANNEL_OUT_MONO)
                .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                .build()
            val created = AudioTrack.Builder()
                .setAudioAttributes(attributes)
                .setAudioFormat(format)
                .setBufferSizeInBytes(bufferBytes)
                .setTransferMode(AudioTrack.MODE_STREAM)
                .setPerformanceMode(AudioTrack.PERFORMANCE_MODE_LOW_LATENCY)
                .build()
            check(created.state == AudioTrack.STATE_INITIALIZED) { "Unable to initialize audio output" }
            track = created
            created.setVolume(volume())
            created.play()

            val blockFrames = 240 // 5 ms at 48 kHz
            val buffer = ShortArray(blockFrames)
            var tick: ShortArray? = null
            var tickOffset = 0
            var absoluteBeat = 0L
            var nextBeatNanos = SystemClock.elapsedRealtimeNanos()
            var activeTempo = tempo.get().toInt()

            while (playing.get() && !closed.get() && generation == runGeneration) {
                val now = SystemClock.elapsedRealtimeNanos()
                val bpm = tempo.get().toInt().coerceIn(Tempo.MIN_BPM, Tempo.MAX_BPM)
                if (bpm != activeTempo) {
                    // Begin the new tempo at the next clean beat edge. This avoids
                    // a sudden partial interval and keeps the audio transient intact.
                    activeTempo = bpm
                    nextBeatNanos = now + (Tempo.beatIntervalMs(bpm) * 1_000_000.0).toLong()
                }
                created.setVolume(volume())

                if (now >= nextBeatNanos) {
                    if (now - nextBeatNanos > (Tempo.beatIntervalMs(bpm) * 2_000_000.0).toLong()) {
                        nextBeatNanos = now
                    }
                    val signature = meter.get()
                    val inMeasure = BeatSequencer.beatInMeasure(absoluteBeat, signature)
                    val accent = inMeasure == 0
                    tick = soundCache.getValue(style.get())[if (accent) 1 else 0]
                    tickOffset = 0
                    val event = BeatEvent(absoluteBeat, inMeasure, accent, nextBeatNanos, bpm)
                    try {
                        onBeat(event)
                    } catch (_: RuntimeException) {
                        // A UI listener must never terminate audio.
                    }
                    absoluteBeat++
                    nextBeatNanos = BeatSequencer.nextDeadline(
                        nextBeatNanos,
                        (60_000_000_000.0 / bpm).toLong(),
                    )
                }

                buffer.fill(0)
                val currentTick = tick
                if (currentTick != null && tickOffset < currentTick.size) {
                    val count = minOf(blockFrames, currentTick.size - tickOffset)
                    val vol = volume()
                    for (i in 0 until count) {
                        val scaled = currentTick[tickOffset + i] * vol
                        buffer[i] = scaled.toInt().coerceIn(Short.MIN_VALUE.toInt(), Short.MAX_VALUE.toInt()).toShort()
                    }
                    tickOffset += count
                }
                var frameOffset = 0
                while (frameOffset < blockFrames && playing.get() && generation == runGeneration) {
                    val written = created.write(
                        buffer,
                        frameOffset,
                        blockFrames - frameOffset,
                        AudioTrack.WRITE_BLOCKING,
                    )
                    if (written <= 0) error("AudioTrack write failed ($written)")
                    frameOffset += written
                }
            }
        } catch (e: Exception) {
            if (playing.get() && generation == runGeneration) {
                playing.set(false)
                onError(e.message ?: "Audio playback failed")
            }
        } finally {
            try {
                track?.pause()
                track?.flush()
                track?.release()
            } catch (_: RuntimeException) {
                // AudioTrack can already be released after an output-device change.
            }
            if (generation == runGeneration) {
                playing.set(false)
                workerRef.compareAndSet(Thread.currentThread(), null)
            }
        }
    }

    private fun volume(): Float = java.lang.Float.intBitsToFloat(volumeBits.get().toInt()).coerceIn(0f, 1f)

    override fun close() {
        if (closed.compareAndSet(false, true)) stop()
    }
}
