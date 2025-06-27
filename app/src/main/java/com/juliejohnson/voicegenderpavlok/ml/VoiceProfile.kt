package com.juliejohnson.voicegenderpavlok.ml

@kotlinx.serialization.Serializable
data class VoiceProfile(
    val pitch: Float = 0f,
    val formant1: Float = 0f,
    val formant2: Float = 0f,
    val loudness: Float = 0f
    // We will add more features like HNR, Jitter, etc., here later.
)
