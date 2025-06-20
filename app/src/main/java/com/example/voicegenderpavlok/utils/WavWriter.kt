package com.example.voicegenderpavlok.utils

import android.util.Log
import java.io.File
import java.io.FileOutputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder

object WavWriter {

    fun writeWav(file: File, audioData: ShortArray, sampleRate: Int) {
        try {
            val numChannels = 1
            val bitsPerSample = 16
            val byteRate = sampleRate * numChannels * bitsPerSample / 8
            val dataSize = audioData.size * 2
            val totalSize = 36 + dataSize

            val header = ByteBuffer.allocate(44).order(ByteOrder.LITTLE_ENDIAN)
            header.put("RIFF".toByteArray())
            header.putInt(totalSize)
            header.put("WAVE".toByteArray())
            header.put("fmt ".toByteArray())
            header.putInt(16) // Subchunk1Size (16 for PCM)
            header.putShort(1) // AudioFormat (1 = PCM)
            header.putShort(numChannels.toShort())
            header.putInt(sampleRate)
            header.putInt(byteRate)
            header.putShort((numChannels * bitsPerSample / 8).toShort())
            header.putShort(bitsPerSample.toShort())
            header.put("data".toByteArray())
            header.putInt(dataSize)

            val audioBytes = ByteBuffer.allocate(dataSize).order(ByteOrder.LITTLE_ENDIAN)
            audioData.forEach { audioBytes.putShort(it) }

            FileOutputStream(file).use {
                it.write(header.array())
                it.write(audioBytes.array())
            }
        } catch (e: Exception) {
            Log.d("WavWriter", "Failed to write WAV file", e)
        }
    }
}
