package com.example.voicegenderpavlok

class AudioChunker(val frameSize: Int = 1600) {
    private val buffer = mutableListOf<Float>()

    fun addSamples(samples: FloatArray): List<FloatArray> {
        buffer.addAll(samples.toList())
        val chunks = mutableListOf<FloatArray>()

        while (buffer.size >= frameSize) {
            chunks.add(buffer.subList(0, frameSize).toFloatArray())
            buffer.subList(0, frameSize).clear()
        }
        return chunks
    }
}

