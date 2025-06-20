package com.example.voicegenderpavlok.ui

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.util.AttributeSet
import android.view.View
import java.util.*

class WaveformView @JvmOverloads constructor(
    context: Context, attrs: AttributeSet? = null
) : View(context, attrs) {

    private val paint = Paint().apply {
        color = Color.GREEN
        strokeWidth = 4f
        isAntiAlias = true
    }

    private val amplitudes: LinkedList<Float> = LinkedList()
    private val maxAmplitudes = 100

    fun addAmplitude(value: Float) {
        val normalized = (value.coerceIn(0f, 32768f)) / 32768f
        amplitudes.add(normalized)
        if (amplitudes.size > maxAmplitudes) amplitudes.removeFirst()
        invalidate()
    }

    fun reset() {
        amplitudes.clear()
        invalidate()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        val centerY = height / 2f
        val spacing = width.toFloat() / maxAmplitudes

        amplitudes.forEachIndexed { i, amp ->
            val heightFactor = amp * height / 2
            canvas.drawLine(
                i * spacing,
                centerY - heightFactor,
                i * spacing,
                centerY + heightFactor,
                paint
            )
        }
    }
}
