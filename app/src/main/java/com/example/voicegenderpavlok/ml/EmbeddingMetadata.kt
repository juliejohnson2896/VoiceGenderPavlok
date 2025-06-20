package com.example.voicegenderpavlok.ml

data class EmbeddingMetadata(
    val timestamp: Long,
    val label: String? = null,
    val audioFile: String,
    val embeddingFile: String
)
