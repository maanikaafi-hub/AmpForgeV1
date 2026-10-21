package com.maani.metronome

import android.content.Context
import android.media.AudioAttributes
import android.media.AudioFocusRequest
import android.media.AudioManager
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.WindowManager
import androidx.activity.compose.setContent
import androidx.activity.viewModels
import androidx.compose.runtime.SideEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsControllerCompat
import com.maani.metronome.ui.MetronomeScreen
import com.maani.metronome.ui.MetronomeTheme
import com.maani.metronome.viewmodel.MetronomeViewModel

/** Activity owns platform audio focus and screen-awake policy; timing lives in the engine. */
class MainActivity : androidx.activity.ComponentActivity() {
    private val metronome: MetronomeViewModel by viewModels {
        MetronomeViewModel.Factory(application)
    }

    private lateinit var audioManager: AudioManager
    private lateinit var focusRequest: AudioFocusRequest
    private var ownsAudioFocus = false

    private val focusListener = AudioManager.OnAudioFocusChangeListener { change ->
        when (change) {
            AudioManager.AUDIOFOCUS_LOSS,
            AudioManager.AUDIOFOCUS_LOSS_TRANSIENT,
            AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK -> {
                // A clock that keeps ticking over navigation, calls or another
                // app is surprising. Pause on every loss; never auto-resume.
                runOnUiThread { metronome.setPlaying(false) }
                ownsAudioFocus = false
            }
            AudioManager.AUDIOFOCUS_GAIN -> Unit // deliberate resume: user presses play
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        WindowCompat.setDecorFitsSystemWindows(window, false)
        window.statusBarColor = android.graphics.Color.TRANSPARENT
        window.navigationBarColor = android.graphics.Color.TRANSPARENT
        WindowInsetsControllerCompat(window, window.decorView).apply {
            isAppearanceLightStatusBars = false
            isAppearanceLightNavigationBars = false
        }
        audioManager = getSystemService(Context.AUDIO_SERVICE) as AudioManager
        focusRequest = AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK)
            .setAudioAttributes(
                AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_MEDIA)
                    .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                    .build(),
            )
            .setAcceptsDelayedFocusGain(false)
            .setWillPauseWhenDucked(true)
            .setOnAudioFocusChangeListener(focusListener, Handler(Looper.getMainLooper()))
            .build()

        setContent {
            val state by metronome.state.collectAsState()
            SideEffect {
                if (state.isPlaying) {
                    window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
                } else {
                    window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
                    abandonAudioFocus()
                }
            }
            MetronomeTheme {
                MetronomeScreen(
                    state = state,
                    onTempo = metronome::setTempo,
                    onAdjustTempo = metronome::adjustTempo,
                    onPlayPause = { togglePlayback() },
                    onVolume = metronome::setVolume,
                    onSignature = metronome::setSignature,
                    onTickStyle = metronome::setTickStyle,
                    onErrorDismiss = metronome::dismissError,
                )
            }
        }
    }

    private fun togglePlayback() {
        if (metronome.state.value.isPlaying) {
            metronome.setPlaying(false)
            abandonAudioFocus()
            return
        }
        val result = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            audioManager.requestAudioFocus(focusRequest)
        } else {
            @Suppress("DEPRECATION")
            audioManager.requestAudioFocus(focusListener, AudioManager.STREAM_MUSIC, AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK)
        }
        if (result == AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
            ownsAudioFocus = true
            metronome.setPlaying(true)
        } else {
            metronome.setPlaying(false)
        }
    }

    private fun abandonAudioFocus() {
        if (!ownsAudioFocus) return
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            audioManager.abandonAudioFocusRequest(focusRequest)
        } else {
            @Suppress("DEPRECATION")
            audioManager.abandonAudioFocus(focusListener)
        }
        ownsAudioFocus = false
    }

    override fun onStop() {
        // Predictable background behavior: silence immediately, release focus,
        // and reset the pendulum when the user returns.
        metronome.setPlaying(false)
        abandonAudioFocus()
        window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        super.onStop()
    }

    override fun onDestroy() {
        window.decorView.keepScreenOn = false
        super.onDestroy()
    }
}
