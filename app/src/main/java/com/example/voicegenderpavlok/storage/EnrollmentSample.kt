package com.example.voicegenderpavlok.storage

import com.example.voicegenderpavlok.ml.EmbeddingMetadata

data class EnrollmentSample(
    val id: String,
    val audioPath: String,
    val embeddingPath: String,
    val metadata: EmbeddingMetadata
)
