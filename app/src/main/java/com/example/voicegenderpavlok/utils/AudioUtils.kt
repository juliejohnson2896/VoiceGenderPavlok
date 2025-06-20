package com.example.voicegenderpavlok.utils

import android.util.Log
import com.example.voicegenderpavlok.ml.AudioBuffer

object AudioUtils {

    fun stereoToMono(interleaved: FloatArray): FloatArray {
        val mono = FloatArray(interleaved.size / 2)
        for (i in mono.indices) {
            mono[i] = (interleaved[2 * i] + interleaved[2 * i + 1]) / 2f
        }
        return mono
    }

    fun ensureMono(buffer: AudioBuffer): FloatArray {
        val isLikelyStereo = buffer.samples.size % 2 == 0 && buffer.samples.size >= 2
        Log.d("AudioUtils", "Samples size: ${buffer.samples.size}, Likely stereo: $isLikelyStereo")
        return if (buffer.channels == 2 || isLikelyStereo) {
            stereoToMono(buffer.samples)
        } else {
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


}