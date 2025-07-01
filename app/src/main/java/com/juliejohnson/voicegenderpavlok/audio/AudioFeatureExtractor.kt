package com.juliejohnson.voicegenderpavlok.audio

import org.jtransforms.fft.FloatFFT_1D
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.floor
import kotlin.math.ln
import kotlin.math.log10
import kotlin.math.min
import kotlin.math.pow
import kotlin.math.sqrt

object AudioFeatureExtractor {

    // Configurable parameters
    private const val sampleRate = 16000
    private const val fftSize = 512
    private const val hopSize = 160 // 10ms
    private const val melBands = 80
    private const val melMinHz = 20.0
    private const val melMaxHz = 7600.0

    fun extractLogMelSpectrogram(waveform: FloatArray): Array<FloatArray> {
        val numFrames = 100
        val paddedLength = fftSize + (numFrames - 1) * hopSize
        val padded = FloatArray(paddedLength)
        System.arraycopy(waveform, 0, padded, 0, min(waveform.size, padded.size))

        val spectrogram = Array(numFrames) { FloatArray(fftSize / 2 + 1) }

        val window = hammingWindow(fftSize)
        val fft = FloatFFT_1D(fftSize.toLong())

        for (i in 0 until numFrames) {
            val start = i * hopSize
            val frame = FloatArray(fftSize) { j ->
                padded.getOrElse(start + j) { 0f } * window[j]
            }

            val complex = FloatArray(fftSize * 2) // real + imag
            for (j in frame.indices) {
                complex[2 * j] = frame[j]     // real
                complex[2 * j + 1] = 0f        // imag
            }

            fft.complexForward(complex)

            for (j in 0 until fftSize / 2 + 1) {
                val real = complex[2 * j]
                val imag = complex[2 * j + 1]
                val magnitude = sqrt(real * real + imag * imag)
                spectrogram[i][j] = magnitude * magnitude // power
            }
        }

        val melFilterbank = createMelFilterBank()
        val melSpectrogram = Array(numFrames) { FloatArray(melBands) }

        for (t in 0 until numFrames) {
            for (m in 0 until melBands) {
                for (k in 0 until fftSize / 2 + 1) {
                    melSpectrogram[t][m] += spectrogram[t][k] * melFilterbank[m][k]
                }
                melSpectrogram[t][m] = ln(melSpectrogram[t][m] + 1e-6f)
            }
        }

        return melSpectrogram
    }

    private fun hammingWindow(size: Int): FloatArray {
        return FloatArray(size) { i ->
            (0.54f - 0.46f * cos(2.0 * PI * i / (size - 1))).toFloat()
        }
    }

    private fun hzToMel(hz: Double): Double = 2595.0 * log10(1 + hz / 700.0)
    private fun melToHz(mel: Double): Double = 700.0 * (10.0.pow(mel / 2595.0) - 1)

    private fun createMelFilterBank(): Array<FloatArray> {
        val melMin = hzToMel(melMinHz)
        val melMax = hzToMel(melMaxHz)
        val melPoints = DoubleArray(melBands + 2) { i ->
            melMin + i * (melMax - melMin) / (melBands + 1)
        }
        val hzPoints = melPoints.map(::melToHz)
        val binPoints = hzPoints.map { floor((fftSize + 1) * it / sampleRate).toInt() }

        val filterbank = Array(melBands) { FloatArray(fftSize / 2 + 1) }

        for (m in 1 until melBands + 1) {
            val f0 = binPoints[m - 1]
            val f1 = binPoints[m]
            val f2 = binPoints[m + 1]

            for (k in f0 until f1) {
                if (k in 0 until fftSize / 2 + 1)
                    filterbank[m - 1][k] = ((k - f0).toFloat() / (f1 - f0))
            }
            for (k in f1 until f2) {
                if (k in 0 until fftSize / 2 + 1)
                    filterbank[m - 1][k] = ((f2 - k).toFloat() / (f2 - f1))
            }
        }

        return filterbank
    }
}