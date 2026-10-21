package com.maani.metronome.ui

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

object MetronomePalette {
    val Ink = Color(0xFF100D09)
    val InkSoft = Color(0xFF1A1510)
    val Panel = Color(0xFF201910)
    val Walnut = Color(0xFF4C2E1E)
    val WalnutLight = Color(0xFF825032)
    val Brass = Color(0xFFC59A4A)
    val BrassLight = Color(0xFFE5C477)
    val Cream = Color(0xFFF2E7CE)
    val Muted = Color(0xFFAA9B80)
    val Red = Color(0xFFA64530)
}

private val MaaniDarkColors = darkColorScheme(
    primary = MetronomePalette.BrassLight,
    onPrimary = MetronomePalette.Ink,
    secondary = MetronomePalette.Brass,
    onSecondary = MetronomePalette.Ink,
    background = MetronomePalette.Ink,
    onBackground = MetronomePalette.Cream,
    surface = MetronomePalette.Panel,
    onSurface = MetronomePalette.Cream,
    surfaceVariant = Color(0xFF2B2118),
    onSurfaceVariant = MetronomePalette.Muted,
    error = Color(0xFFFFB4A9),
)

@Composable
fun MetronomeTheme(content: @Composable () -> Unit) {
    MaterialTheme(colorScheme = MaaniDarkColors, content = content)
}
