package com.maani.metronome.viewmodel

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.viewModelScope
import com.maani.metronome.audio.MetronomeAudioEngine
import com.maani.metronome.data.MetronomePreferences
import com.maani.metronome.model.Tempo
import com.maani.metronome.model.TickStyle
import com.maani.metronome.model.TimeSignature
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch

/** Presentation state and commands. The audio engine has no UI dependency. */
class MetronomeViewModel(application: Application) : AndroidViewModel(application) {
    data class UiState(
        val bpm: Int = 96,
        val signature: TimeSignature = TimeSignature.DEFAULT,
        val volume: Float = 0.72f,
        val tickStyle: TickStyle = TickStyle.DEFAULT,
        val isPlaying: Boolean = false,
        val absoluteBeat: Long = -1L,
        val beatInMeasure: Int = 0,
        val lastBeatNanos: Long = 0L,
        val errorMessage: String? = null,
    )

    private val preferences = MetronomePreferences(application)
    private val initial = preferences.load()
    private val mutableState = MutableStateFlow(
        UiState(
            bpm = initial.bpm,
            signature = initial.signature,
            volume = initial.volume,
            tickStyle = initial.tickStyle,
        ),
    )
    val state: StateFlow<UiState> = mutableState.asStateFlow()

    private val engine = MetronomeAudioEngine(
        onBeat = { beat ->
            mutableState.update {
                it.copy(
                    absoluteBeat = beat.absoluteBeat,
                    beatInMeasure = beat.beatInMeasure,
                    lastBeatNanos = beat.scheduledAtNanos,
                )
            }
        },
        onError = { message ->
            mutableState.update { it.copy(isPlaying = false, errorMessage = message) }
        },
    ).apply {
        setTempo(initial.bpm)
        setVolume(initial.volume)
        setTimeSignature(initial.signature)
        setTickStyle(initial.tickStyle)
    }

    fun setPlaying(playing: Boolean) {
        if (playing) {
            mutableState.update { it.copy(errorMessage = null, isPlaying = true) }
            try {
                engine.start()
            } catch (error: Exception) {
                mutableState.update { it.copy(isPlaying = false, errorMessage = error.message ?: "Could not start audio") }
            }
        } else {
            engine.stop()
            mutableState.update { it.copy(isPlaying = false, absoluteBeat = -1L) }
        }
    }

    fun togglePlaying() = setPlaying(!mutableState.value.isPlaying)

    fun setTempo(bpm: Int) {
        val value = Tempo.clamp(bpm)
        mutableState.update { it.copy(bpm = value) }
        engine.setTempo(value)
        viewModelScope.launch { preferences.saveBpm(value) }
    }

    fun adjustTempo(delta: Int) = setTempo(mutableState.value.bpm + delta)

    fun setSignature(signature: TimeSignature) {
        mutableState.update { it.copy(signature = signature, beatInMeasure = 0) }
        engine.setTimeSignature(signature)
        viewModelScope.launch { preferences.saveSignature(signature) }
    }

    fun setVolume(volume: Float) {
        val value = volume.coerceIn(0f, 1f)
        mutableState.update { it.copy(volume = value) }
        engine.setVolume(value)
        viewModelScope.launch { preferences.saveVolume(value) }
    }

    fun setTickStyle(style: TickStyle) {
        mutableState.update { it.copy(tickStyle = style) }
        engine.setTickStyle(style)
        viewModelScope.launch { preferences.saveTickStyle(style) }
    }

    fun dismissError() { mutableState.update { it.copy(errorMessage = null) } }

    override fun onCleared() {
        engine.close()
        super.onCleared()
    }

    class Factory(private val application: Application) : ViewModelProvider.Factory {
        @Suppress("UNCHECKED_CAST")
        override fun <T : androidx.lifecycle.ViewModel> create(modelClass: Class<T>): T {
            if (modelClass.isAssignableFrom(MetronomeViewModel::class.java)) {
                return MetronomeViewModel(application) as T
            }
            throw IllegalArgumentException("Unknown ViewModel class: ${modelClass.name}")
        }
    }
}
