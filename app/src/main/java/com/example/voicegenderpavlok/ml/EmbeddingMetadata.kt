package com.example.voicegenderpavlok.ml

import kotlinx.serialization.Serializable

@kotlinx.serialization.Serializable
data class EmbeddingMetadata(
    val timestamp: Long,
    val label: String? = null,
    val audioFile: String,
    val embeddingFile: String,
    val autoEnrolled: Boolean = false    // NEW
)
