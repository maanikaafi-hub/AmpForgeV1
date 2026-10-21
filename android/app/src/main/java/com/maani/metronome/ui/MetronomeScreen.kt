package com.maani.metronome.ui

import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.asPaddingValues
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBars
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBars
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Pause
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Remove
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.VolumeDown
import androidx.compose.material.icons.filled.VolumeUp
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.Slider
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.maani.metronome.model.Tempo
import com.maani.metronome.model.TickStyle
import com.maani.metronome.model.TimeSignature
import com.maani.metronome.viewmodel.MetronomeViewModel

@Composable
fun MetronomeScreen(
    state: MetronomeViewModel.UiState,
    onTempo: (Int) -> Unit,
    onAdjustTempo: (Int) -> Unit,
    onPlayPause: () -> Unit,
    onVolume: (Float) -> Unit,
    onSignature: (TimeSignature) -> Unit,
    onTickStyle: (TickStyle) -> Unit,
    onErrorDismiss: () -> Unit,
) {
    var settingsOpen by remember { mutableStateOf(false) }
    val swing by animateFloatAsState(
        targetValue = if (state.isPlaying && state.absoluteBeat >= 0) {
            if (state.absoluteBeat % 2L == 0L) 0.52f else -0.52f
        } else 0f,
        animationSpec = tween(
            durationMillis = (Tempo.beatIntervalMs(state.bpm).toInt()).coerceIn(140, 1_500),
        ),
        label = "audioBeatPendulumSwing",
    )

    BoxWithConstraints(
        modifier = Modifier
            .fillMaxSize()
            .background(
                Brush.verticalGradient(
                    0f to Color(0xFF17120D),
                    0.45f to Color(0xFF100D09),
                    1f to Color(0xFF080706),
                ),
            ),
    ) {
        val compact = maxHeight < 790.dp
        Column(
            modifier = Modifier
                .fillMaxSize()
                .verticalScroll(rememberScrollState())
                .padding(top = WindowInsets.statusBars.asPaddingValues().calculateTopPadding() + 7.dp)
                .padding(bottom = WindowInsets.navigationBars.asPaddingValues().calculateBottomPadding() + 16.dp)
                .padding(horizontal = 22.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            Header(onSettings = { settingsOpen = true })
            Spacer(Modifier.height(if (compact) 4.dp else 9.dp))

            MechanicalMetronome(
                bpm = state.bpm,
                swingAngle = swing,
                isPlaying = state.isPlaying,
                onTempo = onTempo,
                modifier = Modifier
                    .fillMaxWidth()
                    .height(if (compact) 310.dp else 350.dp),
            )

            TempoReadout(
                bpm = state.bpm,
                modifier = Modifier.fillMaxWidth(),
            )
            Spacer(Modifier.height(7.dp))

            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.Center,
                verticalAlignment = Alignment.CenterVertically,
            ) {
                TactileButton(
                    label = "Decrease tempo",
                    onClick = { onAdjustTempo(-1) },
                    modifier = Modifier.size(54.dp),
                ) { Icon(Icons.Filled.Remove, contentDescription = null, modifier = Modifier.size(23.dp)) }
                Spacer(Modifier.width(16.dp))
                TactileButton(
                    label = "Play metronome",
                    onClick = onPlayPause,
                    primary = true,
                    modifier = Modifier.size(72.dp),
                ) {
                    Icon(
                        if (state.isPlaying) Icons.Filled.Pause else Icons.Filled.PlayArrow,
                        contentDescription = null,
                        modifier = Modifier.size(34.dp),
                    )
                }
                Spacer(Modifier.width(16.dp))
                TactileButton(
                    label = "Increase tempo",
                    onClick = { onAdjustTempo(1) },
                    modifier = Modifier.size(54.dp),
                ) { Icon(Icons.Filled.Add, contentDescription = null, modifier = Modifier.size(23.dp)) }
            }

            Spacer(Modifier.height(16.dp))
            SignaturePicker(state.signature, onSignature)
            Spacer(Modifier.height(10.dp))
            VolumeControl(state.volume, onVolume)
            Spacer(Modifier.height(15.dp))
            Footer(isPlaying = state.isPlaying)
        }
    }

    if (settingsOpen) {
        SettingsDialog(
            selected = state.tickStyle,
            onChoose = onTickStyle,
            onDismiss = { settingsOpen = false },
        )
    }
    state.errorMessage?.let { message ->
        AlertDialog(
            onDismissRequest = onErrorDismiss,
            title = { Text("Audio unavailable") },
            text = { Text(message) },
            confirmButton = { TextButton(onClick = onErrorDismiss) { Text("OK") } },
            containerColor = MetronomePalette.Panel,
            titleContentColor = MetronomePalette.Cream,
            textContentColor = MetronomePalette.Muted,
        )
    }
}

@Composable
private fun Header(onSettings: () -> Unit) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Column(Modifier.weight(1f)) {
            Text(
                "MAANI",
                color = MetronomePalette.BrassLight,
                fontSize = 12.sp,
                fontWeight = FontWeight.Bold,
                letterSpacing = 4.sp,
            )
            Text(
                "MECHANICAL METRONOME",
                color = MetronomePalette.Muted,
                fontSize = 9.sp,
                fontWeight = FontWeight.Medium,
                letterSpacing = 2.1.sp,
            )
        }
        Surface(
            modifier = Modifier.size(40.dp).clickable(role = Role.Button, onClick = onSettings),
            shape = CircleShape,
            color = Color(0xFF251C13),
            border = BorderStroke(1.dp, Color(0xFF6E542E)),
        ) {
            Box(contentAlignment = Alignment.Center) {
                Icon(
                    Icons.Filled.Settings,
                    contentDescription = "Sound settings",
                    tint = MetronomePalette.BrassLight,
                    modifier = Modifier.size(18.dp),
                )
            }
        }
    }
}

@Composable
private fun TempoReadout(bpm: Int, modifier: Modifier = Modifier) {
    Row(
        modifier = modifier.padding(horizontal = 5.dp),
        horizontalArrangement = Arrangement.Center,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            Row(verticalAlignment = Alignment.Bottom) {
                Text(
                    bpm.toString(),
                    color = MetronomePalette.Cream,
                    fontSize = 51.sp,
                    lineHeight = 53.sp,
                    fontWeight = FontWeight.Light,
                    fontFamily = FontFamily.Serif,
                    letterSpacing = (-1.5).sp,
                )
                Text(
                    " BPM",
                    color = MetronomePalette.Brass,
                    fontSize = 12.sp,
                    fontWeight = FontWeight.SemiBold,
                    letterSpacing = 1.5.sp,
                    modifier = Modifier.padding(bottom = 9.dp),
                )
            }
            Text(
                Tempo.marking(bpm).uppercase(),
                color = MetronomePalette.BrassLight,
                fontSize = 10.sp,
                letterSpacing = 2.9.sp,
                fontWeight = FontWeight.Medium,
            )
        }
    }
}

@Composable
private fun TactileButton(
    label: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    primary: Boolean = false,
    content: @Composable () -> Unit,
) {
    Surface(
        modifier = modifier
            .shadow(if (primary) 10.dp else 5.dp, CircleShape, ambientColor = Color.Black)
            .clip(CircleShape)
            .clickable(role = Role.Button, onClickLabel = label, onClick = onClick),
        shape = CircleShape,
        color = if (primary) MetronomePalette.Brass else Color(0xFF281F16),
        border = BorderStroke(
            if (primary) 1.5.dp else 1.dp,
            if (primary) MetronomePalette.BrassLight else Color(0xFF705532),
        ),
    ) {
        Box(contentAlignment = Alignment.Center) {
            if (primary) {
                Box(
                    Modifier
                        .fillMaxSize()
                        .padding(4.dp)
                        .clip(CircleShape)
                        .background(Brush.verticalGradient(listOf(Color(0xFFE1BD69), Color(0xFFAD7D32)))),
                )
            }
            Box(contentAlignment = Alignment.Center) {
                // Content receives the button's foreground tint through LocalContentColor.
                androidx.compose.runtime.CompositionLocalProvider(
                    androidx.compose.material3.LocalContentColor provides
                        if (primary) Color(0xFF271B0E) else MetronomePalette.BrassLight,
                    content = content,
                )
            }
        }
    }
}

@Composable
private fun SignaturePicker(signature: TimeSignature, onSelect: (TimeSignature) -> Unit) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(15.dp))
            .background(Color(0xFF18130E))
            .padding(5.dp)
            .horizontalScroll(rememberScrollState()),
        horizontalArrangement = Arrangement.Center,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        TimeSignature.entries.forEach { option ->
            val selected = option == signature
            Box(
                modifier = Modifier
                    .clip(RoundedCornerShape(11.dp))
                    .background(if (selected) Color(0xFF594127) else Color.Transparent)
                    .clickable(role = Role.RadioButton, onClick = { onSelect(option) })
                    .padding(horizontal = 10.dp, vertical = 9.dp),
                contentAlignment = Alignment.Center,
            ) {
                Text(
                    option.label,
                    color = if (selected) MetronomePalette.BrassLight else MetronomePalette.Muted,
                    fontSize = 12.sp,
                    fontWeight = if (selected) FontWeight.SemiBold else FontWeight.Normal,
                )
            }
        }
    }
}

@Composable
private fun VolumeControl(volume: Float, onVolume: (Float) -> Unit) {
    Row(
        modifier = Modifier.fillMaxWidth().padding(horizontal = 3.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Icon(Icons.Filled.VolumeDown, contentDescription = "Volume", tint = MetronomePalette.Muted, modifier = Modifier.size(19.dp))
        Slider(
            value = volume,
            onValueChange = onVolume,
            modifier = Modifier.weight(1f).height(36.dp).padding(horizontal = 9.dp),
            colors = androidx.compose.material3.SliderDefaults.colors(
                thumbColor = MetronomePalette.BrassLight,
                activeTrackColor = MetronomePalette.Brass,
                inactiveTrackColor = Color(0xFF403326),
            ),
        )
        Icon(Icons.Filled.VolumeUp, contentDescription = null, tint = MetronomePalette.Brass, modifier = Modifier.size(19.dp))
    }
}

@Composable
private fun Footer(isPlaying: Boolean) {
    Row(verticalAlignment = Alignment.CenterVertically) {
        Box(
            Modifier
                .size(6.dp)
                .clip(CircleShape)
                .background(if (isPlaying) Color(0xFF91B57A) else Color(0xFF665E4F)),
        )
        Spacer(Modifier.width(7.dp))
        Text(
            if (isPlaying) "READY TO PRACTISE" else "CRAFTED FOR YOUR PRACTICE",
            color = MetronomePalette.Muted.copy(alpha = 0.76f),
            fontSize = 8.sp,
            letterSpacing = 1.6.sp,
        )
    }
}

@Composable
private fun SettingsDialog(
    selected: TickStyle,
    onChoose: (TickStyle) -> Unit,
    onDismiss: () -> Unit,
) {
    AlertDialog(
        onDismissRequest = onDismiss,
        containerColor = Color(0xFF211910),
        titleContentColor = MetronomePalette.Cream,
        textContentColor = MetronomePalette.Muted,
        title = {
            Column {
                Text("The voice of the instrument", fontFamily = FontFamily.Serif, fontSize = 21.sp)
                Text("MECHANISM VOICING", color = MetronomePalette.Brass, fontSize = 9.sp, letterSpacing = 2.sp)
            }
        },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                TickStyle.entries.forEach { style ->
                    val active = style == selected
                    Card(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clickable(role = Role.RadioButton) { onChoose(style); onDismiss() },
                        colors = CardDefaults.cardColors(
                            containerColor = if (active) Color(0xFF41301C) else Color(0xFF2A2117),
                        ),
                        border = if (active) BorderStroke(1.dp, MetronomePalette.Brass) else null,
                    ) {
                        Column(Modifier.padding(12.dp)) {
                            Text(style.displayName, color = MetronomePalette.Cream, fontWeight = FontWeight.SemiBold)
                            Text(style.description, color = MetronomePalette.Muted, fontSize = 11.sp, lineHeight = 15.sp)
                        }
                    }
                }
            }
        },
        confirmButton = {
            TextButton(onClick = onDismiss) { Text("DONE", color = MetronomePalette.BrassLight, letterSpacing = 1.5.sp) }
        },
    )
}
