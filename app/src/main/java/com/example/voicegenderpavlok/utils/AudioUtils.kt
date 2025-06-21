package com.example.voicegenderpavlok.utils

import android.util.Log
import com.example.voicegenderpavlok.ml.AudioBuffer
import kotlin.math.abs

object AudioUtils {

    fun stereoToMono(interleaved: FloatArray): FloatArray {
        val mono = FloatArray(interleaved.size / 2)
        for (i in mono.indices) {
            mono[i] = (interleaved[2 * i] + interleaved[2 * i + 1]) / 2f
        }
        Log.d("AudioUtils", "Mono size: ${mono.size}")
        return mono
    }

    fun ensureMono(buffer: AudioBuffer): FloatArray {
        return if (buffer.channels == 2) {
            Log.d("AudioUtils", "Converting stereo to mono (actual stereo)")
            stereoToMono(buffer.samples)
        } else {
            Log.d("AudioUtils", "Already mono, no conversion needed")
            buffer.samples
        }
    }

    fun prepareAudio(input: FloatArray, length: Int = 16000): FloatArray {
        return if (input.size >= length) {
            input.take(length).toFloatArray()
        } else {
            FloatArray(length) { i -> if (i < input.size) input[i] else 0f }
        }
    }

    fun ensureMonoAndFixedLength(buffer: AudioBuffer, length: Int = 16000): FloatArray {
        val mono = ensureMono(buffer)
        val normalized = normalizeVolume(mono)
        return prepareAudio(normalized, length)
    }

    fun normalizeVolume(input: FloatArray): FloatArray {
        val max = input.maxOfOrNull { abs(it) } ?: 1f
        return if (max < 0.01f) input else input.map { it / max }.toFloatArray()
    }

}