package com.juliejohnson.voicegenderpavlok.ml

data class AudioBuffer(
    val samples: FloatArray,
    val sampleRate: Int = 16000,
    val channels: Int = 1  // 1 = mono, 2 = stereo
)