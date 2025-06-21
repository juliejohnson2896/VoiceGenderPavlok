package com.example.voicegenderpavlok.storage

import android.content.Context
import android.util.Log
import com.example.voicegenderpavlok.ml.EmbeddingMetadata
import kotlinx.serialization.decodeFromString
import kotlinx.serialization.encodeToString
import kotlinx.serialization.json.Json
import java.io.File
import java.io.FileInputStream
import java.io.IOException
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.channels.FileChannel

object FileUtils {

    private const val ENROLLMENT_DIR = "enrollments"

    fun getEnrollmentDir(context: Context): File {
        val dir = File(context.filesDir, ENROLLMENT_DIR)
        if (!dir.exists()) dir.mkdirs()
        return dir
    }

    fun listEmbeddingFiles(context: Context): List<File> {
        return getEnrollmentDir(context).listFiles { file ->
            file.isFile && file.extension == "dat"
        }?.toList() ?: emptyList()
    }

    fun writeEmbedding(context: Context, embedding: FloatArray, filename: String) {
        val file = File(getEnrollmentDir(context), filename)
        val buffer = ByteBuffer.allocate(embedding.size * 4).order(ByteOrder.LITTLE_ENDIAN)
        embedding.forEach { buffer.putFloat(it) }
        file.writeBytes(buffer.array())
    }

    fun readEmbedding(file: File): FloatArray {
        val buffer = ByteBuffer.wrap(file.readBytes()).order(ByteOrder.LITTLE_ENDIAN)
        val floats = FloatArray(buffer.remaining() / 4)
        for (i in floats.indices) {
            floats[i] = buffer.float
        }
        return floats
    }

    fun writeEmbeddingFile(file: File, embedding: FloatArray) {
        val buffer = ByteBuffer.allocate(embedding.size * 4).order(ByteOrder.LITTLE_ENDIAN)
        embedding.forEach { buffer.putFloat(it) }
        file.writeBytes(buffer.array())
    }

    fun readEmbeddingFile(file: File): FloatArray {
        val buffer = ByteBuffer.wrap(file.readBytes()).order(ByteOrder.LITTLE_ENDIAN)
        val floats = FloatArray(buffer.remaining() / 4)
        for (i in floats.indices) {
            floats[i] = buffer.float
        }
        return floats
    }


    fun deleteEmbedding(file: File): Boolean {
        return file.delete()
    }

    fun clearAllEmbeddings(context: Context) {
        listEmbeddingFiles(context).forEach { it.delete() }
    }

    /**
     * Loads a TensorFlow Lite model file from the assets folder into a MappedByteBuffer.
     * This is required to initialize a TFLite Interpreter.
     */
    fun loadModelFile(context: Context, filename: String): ByteBuffer {
        return try {
            context.assets.openFd(filename).use { fileDescriptor ->
                FileInputStream(fileDescriptor.fileDescriptor).use { inputStream ->
                    val fileChannel = inputStream.channel
                    val startOffset = fileDescriptor.startOffset
                    val declaredLength = fileDescriptor.declaredLength
                    fileChannel.map(FileChannel.MapMode.READ_ONLY, startOffset, declaredLength)
                }
            }
        } catch (e: IOException) {
            Log.e("FileUtils", "Failed to load model file: $filename", e)
            throw RuntimeException("Could not load model file $filename", e)
        }
    }

    fun writeWavFile(pcmData: ShortArray, sampleRate: Int, file: File) {
        val byteRate = 16 * sampleRate / 8
        val dataSize = pcmData.size * 2
        val totalDataLen = 36 + dataSize

        file.outputStream().use { out ->
            // Write RIFF header
            out.write("RIFF".toByteArray(Charsets.US_ASCII))
            out.write(intToLittleEndian(totalDataLen))
            out.write("WAVE".toByteArray(Charsets.US_ASCII))

            // fmt subchunk
            out.write("fmt ".toByteArray(Charsets.US_ASCII))
            out.write(intToLittleEndian(16)) // PCM = 16
            out.write(shortToLittleEndian(1)) // Audio format (1 = PCM)
            out.write(shortToLittleEndian(1)) // Num channels
            out.write(intToLittleEndian(sampleRate))
            out.write(intToLittleEndian(sampleRate * 2)) // Byte rate
            out.write(shortToLittleEndian(2)) // Block align
            out.write(shortToLittleEndian(16)) // Bits per sample

            // data subchunk
            out.write("data".toByteArray(Charsets.US_ASCII))
            out.write(intToLittleEndian(dataSize))

            // Write PCM audio data
            for (sample in pcmData) {
                out.write(shortToLittleEndian(sample))
            }
        }
    }

    private fun intToLittleEndian(value: Int): ByteArray {
        return byteArrayOf(
            (value and 0xFF).toByte(),
            ((value shr 8) and 0xFF).toByte(),
            ((value shr 16) and 0xFF).toByte(),
            ((value shr 24) and 0xFF).toByte()
        )
    }

    private fun shortToLittleEndian(value: Short): ByteArray {
        return byteArrayOf(
            (value.toInt() and 0xFF).toByte(),
            ((value.toInt() shr 8) and 0xFF).toByte()
        )
    }


    private val json = Json {
        prettyPrint = true
        ignoreUnknownKeys = true
    }

    fun writeMetadataFile(file: File, metadata: EmbeddingMetadata) {
        file.writeText(json.encodeToString(metadata))
    }

    fun readMetadataFile(file: File): EmbeddingMetadata {
        return json.decodeFromString(file.readText())
    }

    fun getDebugDir(context: Context): File {
        val dir = File(context.filesDir, "debug")
        if (!dir.exists()) dir.mkdirs()
        return dir
    }

    fun writeDebugWav(context: Context, samples: FloatArray, sampleRate: Int = 16000) {
        val file = File(getDebugDir(context), "last_input.wav")
        val shortBuffer = samples.map { (it * 32767).toInt().coerceIn(-32768, 32767).toShort() }.toShortArray()
        writeWavFile(shortBuffer, sampleRate, file)
    }
}
