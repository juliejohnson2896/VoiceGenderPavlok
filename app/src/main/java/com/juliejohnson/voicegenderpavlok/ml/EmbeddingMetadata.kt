package com.juliejohnson.voicegenderpavlok.ml

import com.juliejohnson.voicegenderpavlok.data.VoiceProfile

@kotlinx.serialization.Serializable
data class EmbeddingMetadata(
    val timestamp: Long,
    val label: String? = null,
    val audioFile: String,
    val embeddingFile: String,
    val voiceProfile: VoiceProfile,
    val autoEnrolled: Boolean = false
)
