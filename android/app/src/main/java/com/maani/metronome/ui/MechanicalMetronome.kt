package com.maani.metronome.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.drawscope.clipPath
import androidx.compose.ui.graphics.drawscope.rotate
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.graphics.nativeCanvas
import android.graphics.Paint
import android.graphics.Typeface
import com.maani.metronome.model.Tempo
import kotlin.math.sin
import kotlin.math.cos
import kotlin.random.Random

/**
 * Custom Canvas illustration of a classic walnut-case mechanical metronome.
 * All dimensions are proportional to the measured canvas. Drag/tap on the
 * engraved scale to set tempo. The pendulum itself follows audio beat events,
 * not animation frame time.
 */
@Composable
fun MechanicalMetronome(
    bpm: Int,
    swingAngle: Float,
    isPlaying: Boolean,
    onTempo: (Int) -> Unit,
    modifier: Modifier = Modifier,
) {
    Box(modifier = modifier) {
        Canvas(
            modifier = Modifier
                .fillMaxSize()
                .pointerInput(Unit) {
                    detectTapGestures { point ->
                        val scalePosition = 1.0 - ((point.y / size.height) - 0.205) / 0.54
                        onTempo(Tempo.bpmAtPosition(scalePosition))
                    }
                }
                .pointerInput(Unit) {
                    detectDragGestures { change, _ ->
                        change.consume()
                        val scalePosition = 1.0 - ((change.position.y / size.height) - 0.205) / 0.54
                        onTempo(Tempo.bpmAtPosition(scalePosition))
                    }
                },
        ) {
            val w = size.width
            val h = size.height
            val cx = w * 0.535f
            val bodyW = w * 0.73f
            val bodyLeft = cx - bodyW / 2f
            val bodyRight = cx + bodyW / 2f
            val bodyTop = h * 0.055f
            val bodyBottom = h * 0.925f
            val panelTop = h * 0.17f
            val panelBottom = h * 0.79f
            // soft instrument halo
            drawOval(
                brush = Brush.radialGradient(
                    listOf(Color(0x2ACB9B54), Color(0x0C8D5E2E), Color.Transparent),
                    center = Offset(cx, h * 0.52f),
                    radius = h * 0.52f,
                ),
                topLeft = Offset(cx - w * 0.48f, h * 0.05f),
                size = Size(w * 0.96f, h * 0.91f),
            )

            // Main walnut silhouette with subtly shouldered top and broad plinth.
            val body = Path().apply {
                moveTo(cx - bodyW * 0.18f, bodyTop)
                lineTo(cx + bodyW * 0.18f, bodyTop)
                cubicTo(cx + bodyW * 0.24f, bodyTop, cx + bodyW * 0.235f, bodyTop + h * 0.045f, cx + bodyW * 0.275f, bodyTop + h * 0.105f)
                lineTo(bodyRight, bodyBottom - h * 0.045f)
                quadraticTo(bodyRight, bodyBottom, bodyRight - bodyW * 0.07f, bodyBottom)
                lineTo(bodyLeft + bodyW * 0.07f, bodyBottom)
                quadraticTo(bodyLeft, bodyBottom, bodyLeft, bodyBottom - h * 0.045f)
                lineTo(cx - bodyW * 0.275f, bodyTop + h * 0.105f)
                cubicTo(cx - bodyW * 0.235f, bodyTop + h * 0.045f, cx - bodyW * 0.24f, bodyTop, cx - bodyW * 0.18f, bodyTop)
                close()
            }
            drawPath(body, Color(0xFF070605), style = Stroke(width = w * 0.045f))
            drawPath(
                body,
                brush = Brush.linearGradient(
                    listOf(Color(0xFF86572E), Color(0xFF4B2C1B), Color(0xFF704426), Color(0xFF321D13)),
                    start = Offset(bodyLeft, 0f),
                    end = Offset(bodyRight, h),
                ),
            )

            // Fine vertical satin sheen on the case for carved-wood depth.
            val grainRandom = Random(814)
            clipPath(body) {
                repeat(34) { index ->
                    val x = bodyLeft + bodyW * (index + 1) / 36f
                    val wobble = (grainRandom.nextFloat() - 0.5f) * bodyW * 0.018f
                    val alpha = if (index % 4 == 0) 0.12f else 0.045f
                    val grain = Path().apply {
                        moveTo(x, bodyTop + h * 0.08f)
                        cubicTo(x + wobble, h * 0.36f, x - wobble * 0.7f, h * 0.62f, x + wobble * 0.3f, bodyBottom - h * 0.02f)
                    }
                    drawPath(grain, Color(0xFFFFD194).copy(alpha = alpha), style = Stroke(width = w * 0.0026f))
                }
                // darker cathedral figuring, a few broad flowing ribbons
                repeat(5) { i ->
                    val y = bodyTop + h * (0.20f + i * 0.145f)
                    val grain = Path().apply {
                        moveTo(bodyLeft + bodyW * 0.05f, y)
                        cubicTo(cx - bodyW * 0.14f, y - h * 0.035f, cx + bodyW * 0.10f, y + h * 0.045f, bodyRight - bodyW * 0.05f, y - h * 0.012f)
                    }
                    drawPath(grain, Color(0xFF1D100A).copy(alpha = 0.16f), style = Stroke(width = w * 0.0045f))
                }
            }

            // Gold pinstripe follows the case moulding.
            drawPath(body, Color(0xFFDBB56D).copy(alpha = 0.76f), style = Stroke(width = w * 0.006f))

            // The dark, inset scale and mechanism board.
            val panel = Path().apply {
                moveTo(cx - bodyW * 0.205f, panelTop)
                lineTo(cx + bodyW * 0.205f, panelTop)
                lineTo(cx + bodyW * 0.265f, panelBottom)
                lineTo(cx - bodyW * 0.265f, panelBottom)
                close()
            }
            drawPath(panel, Color(0xFF100D09))
            drawPath(panel, Color(0xFFC39A55).copy(alpha = 0.85f), style = Stroke(width = w * 0.004f))

            // Brass screws in the inner bezel.
            val screwX = listOf(cx - bodyW * 0.238f, cx + bodyW * 0.238f)
            listOf(panelTop + h * 0.025f, panelBottom - h * 0.025f).forEach { y ->
                screwX.forEach { x ->
                    drawCircle(Color(0xFF85632F), radius = w * 0.012f, center = Offset(x, y))
                    drawLine(Color(0xFFFFD98A), Offset(x - w * 0.006f, y), Offset(x + w * 0.006f, y), w * 0.002f)
                }
            }

            // Engraved BPM scale, non-linear like a compound pendulum.
            val scaleX = cx - bodyW * 0.125f
            val scaleTop = h * 0.295f
            val scaleBottom = h * 0.700f
            val labelPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
                color = android.graphics.Color.rgb(216, 197, 157)
                textAlign = Paint.Align.RIGHT
                textSize = w * 0.026f
                typeface = Typeface.create("serif", Typeface.NORMAL)
                alpha = 220
            }
            Tempo.scaleValues.forEach { value ->
                val pos = Tempo.weightPosition(value).toFloat()
                val y = scaleBottom - pos * (scaleBottom - scaleTop)
                val major = value in Tempo.labeledScaleValues
                val length = if (major) bodyW * 0.085f else bodyW * 0.040f
                drawLine(
                    color = Color(0xFFD4AD63).copy(alpha = if (major) 0.95f else 0.55f),
                    start = Offset(scaleX - length * 0.45f, y),
                    end = Offset(scaleX + length * 0.55f, y),
                    strokeWidth = if (major) w * 0.004f else w * 0.0022f,
                )
                if (major) {
                    drawContext.canvas.nativeCanvas.drawText(
                        value.toString(), scaleX - length * 0.63f, y + w * 0.009f, labelPaint,
                    )
                }
            }
            // tempo pointer / engraved numeral location
            val bpmY = scaleBottom - Tempo.weightPosition(bpm).toFloat() * (scaleBottom - scaleTop)
            drawLine(
                Color(0xFFE9C66E),
                Offset(scaleX + bodyW * 0.018f, bpmY),
                Offset(scaleX + bodyW * 0.11f, bpmY),
                w * 0.006f,
                cap = StrokeCap.Round,
            )
            drawCircle(Color(0xFFE8C16A), w * 0.007f, Offset(scaleX + bodyW * 0.115f, bpmY))

            // Maker's small engraved nameplate, above the pivot.
            val namePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
                color = android.graphics.Color.rgb(215, 178, 105)
                textAlign = Paint.Align.CENTER
                textSize = w * 0.022f
                typeface = Typeface.create("serif", Typeface.BOLD)
            }
            drawContext.canvas.nativeCanvas.drawText("MAANI", cx + bodyW * 0.02f, panelTop + h * 0.047f, namePaint)
            val smallPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
                color = android.graphics.Color.rgb(156, 129, 82)
                textAlign = Paint.Align.CENTER
                textSize = w * 0.0145f
                typeface = Typeface.create("sans-serif", Typeface.NORMAL)
            }
            drawContext.canvas.nativeCanvas.drawText("MADE FOR MUSIC", cx + bodyW * 0.02f, panelTop + h * 0.070f, smallPaint)

            // Pendulum rod. Its angular motion is driven by the audio beat state.
            val pivot = Offset(cx + bodyW * 0.045f, panelTop + h * 0.11f)
            val rodLength = h * 0.49f
            val angle = if (isPlaying) swingAngle else 0f
            val tip = Offset(
                pivot.x + sin(angle) * rodLength,
                pivot.y + cos(angle) * rodLength,
            )
            drawLine(Color(0xFF030303), pivot, tip, w * 0.023f, cap = StrokeCap.Round)
            drawLine(
                Brush.horizontalGradient(listOf(Color(0xFF655A45), Color(0xFFF0E3C4), Color(0xFF8B7A5D))),
                pivot,
                tip,
                w * 0.011f,
                cap = StrokeCap.Round,
            )

            // Sliding brass weight position follows the traditional 1 / BPM² law.
            val weightFraction = 0.23f + (1f - Tempo.weightPosition(bpm).toFloat()) * 0.66f
            val weightCenter = Offset(
                pivot.x + sin(angle) * rodLength * weightFraction,
                pivot.y + cos(angle) * rodLength * weightFraction,
            )
            val weightW = bodyW * 0.155f
            val weightH = h * 0.052f
            rotate(degrees = Math.toDegrees(angle.toDouble()).toFloat(), pivot = pivot) {
                drawRoundRect(
                    Color(0xFF4B310F),
                    topLeft = Offset(weightCenter.x - weightW / 2f + w * 0.007f, weightCenter.y - weightH / 2f + w * 0.007f),
                    size = Size(weightW, weightH),
                    cornerRadius = androidx.compose.ui.geometry.CornerRadius(w * 0.018f),
                )
                drawRoundRect(
                    brush = Brush.verticalGradient(
                        listOf(Color(0xFFFFE9A2), Color(0xFFD6AD58), Color(0xFF9E712E), Color(0xFFE0BF73)),
                        startY = weightCenter.y - weightH / 2f,
                        endY = weightCenter.y + weightH / 2f,
                    ),
                    topLeft = Offset(weightCenter.x - weightW / 2f, weightCenter.y - weightH / 2f),
                    size = Size(weightW, weightH),
                    cornerRadius = androidx.compose.ui.geometry.CornerRadius(w * 0.018f),
                )
                drawRoundRect(
                    Color(0xFFFFF0B6).copy(alpha = 0.66f),
                    topLeft = Offset(weightCenter.x - weightW * 0.39f, weightCenter.y - weightH * 0.34f),
                    size = Size(weightW * 0.78f, weightH * 0.13f),
                    cornerRadius = androidx.compose.ui.geometry.CornerRadius(w * 0.008f),
                )
                drawLine(
                    Color(0xFF6D4B20),
                    Offset(weightCenter.x, weightCenter.y - weightH * 0.33f),
                    Offset(weightCenter.x, weightCenter.y + weightH * 0.33f),
                    w * 0.002f,
                )
            }

            // Polished pivot cap.
            drawCircle(Color(0xFF372512), radius = w * 0.038f, center = pivot)
            drawCircle(
                Brush.radialGradient(listOf(Color(0xFFFFE6A0), Color(0xFFB27D32), Color(0xFF65431E)), center = pivot, radius = w * 0.031f),
                radius = w * 0.030f,
                center = pivot,
            )
            drawCircle(Color(0xFFFFF2C6), radius = w * 0.007f, center = Offset(pivot.x - w * 0.009f, pivot.y - w * 0.010f))

            // The little mechanical foot at the very bottom of the scale window.
            val footY = panelBottom - h * 0.018f
            drawRoundRect(
                Brush.verticalGradient(listOf(Color(0xFFE0BE74), Color(0xFF86602C))),
                topLeft = Offset(cx - bodyW * 0.125f, footY),
                size = Size(bodyW * 0.25f, h * 0.013f),
                cornerRadius = androidx.compose.ui.geometry.CornerRadius(w * 0.006f),
            )

            // Layered cabinet base moulding, brass-capped.
            drawRoundRect(
                Color(0xFF24150D),
                topLeft = Offset(bodyLeft - w * 0.008f, bodyBottom - h * 0.045f),
                size = Size(bodyW + w * 0.016f, h * 0.067f),
                cornerRadius = androidx.compose.ui.geometry.CornerRadius(w * 0.018f),
            )
            drawRoundRect(
                Brush.verticalGradient(listOf(Color(0xFF9B6A3B), Color(0xFF5A351D), Color(0xFF2D1A11))),
                topLeft = Offset(bodyLeft + w * 0.004f, bodyBottom - h * 0.047f),
                size = Size(bodyW - w * 0.008f, h * 0.052f),
                cornerRadius = androidx.compose.ui.geometry.CornerRadius(w * 0.012f),
            )
            drawLine(
                Color(0xFFE1BD73),
                Offset(bodyLeft + w * 0.018f, bodyBottom - h * 0.042f),
                Offset(bodyRight - w * 0.018f, bodyBottom - h * 0.042f),
                w * 0.004f,
                cap = StrokeCap.Round,
            )

            // Lower highlight only while playing: a quiet indication of movement.
            if (isPlaying) {
                drawLine(
                    Color(0xFFEBBE63).copy(alpha = 0.36f),
                    Offset(cx - w * 0.07f, bodyBottom + h * 0.026f),
                    Offset(cx + w * 0.07f, bodyBottom + h * 0.026f),
                    w * 0.003f,
                    cap = StrokeCap.Round,
                )
            }
        }
    }
}
