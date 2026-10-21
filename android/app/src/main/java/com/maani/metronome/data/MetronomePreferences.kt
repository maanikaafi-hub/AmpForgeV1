package com.maani.metronome.data

import android.content.Context
import com.maani.metronome.model.Tempo
import com.maani.metronome.model.TickStyle
import com.maani.metronome.model.TimeSignature

/** Small, versioned SharedPreferences store; settings never leave the device. */
class MetronomePreferences(context: Context) {
    private val prefs = context.applicationContext.getSharedPreferences("metronome_settings_v1", Context.MODE_PRIVATE)

    data class Values(
        val bpm: Int = 96,
        val signature: TimeSignature = TimeSignature.DEFAULT,
        val volume: Float = 0.72f,
        val tickStyle: TickStyle = TickStyle.DEFAULT,
    )

    fun load(): Values = Values(
        bpm = Tempo.clamp(prefs.getInt(KEY_BPM, 96)),
        signature = TimeSignature.fromBeatsPerMeasure(prefs.getInt(KEY_METER, TimeSignature.DEFAULT.beatsPerMeasure)),
        volume = prefs.getFloat(KEY_VOLUME, 0.72f).coerceIn(0f, 1f),
        tickStyle = TickStyle.fromName(prefs.getString(KEY_STYLE, TickStyle.DEFAULT.name)),
    )

    fun saveBpm(value: Int) { prefs.edit().putInt(KEY_BPM, Tempo.clamp(value)).apply() }
    fun saveSignature(value: TimeSignature) { prefs.edit().putInt(KEY_METER, value.beatsPerMeasure).apply() }
    fun saveVolume(value: Float) { prefs.edit().putFloat(KEY_VOLUME, value.coerceIn(0f, 1f)).apply() }
    fun saveTickStyle(value: TickStyle) { prefs.edit().putString(KEY_STYLE, value.name).apply() }

    private companion object {
        const val KEY_BPM = "bpm"
        const val KEY_METER = "meter"
        const val KEY_VOLUME = "volume"
        const val KEY_STYLE = "tick_style"
    }
}
