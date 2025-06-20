package com.example.voicegenderpavlok.ml

import android.content.Context
import android.util.Log
import com.example.voicegenderpavlok.utils.AudioUtils
import com.example.voicegenderpavlok.utils.FileUtils
import org.tensorflow.lite.Interpreter
import java.io.File
import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlin.math.sqrt

object MLUtils {

    private lateinit var speakerInterpreter: Interpreter
    private lateinit var genderInterpreter: Interpreter

    fun initialize(context: Context) {
        speakerInterpreter = Interpreter(FileUtils.loadModelFile(context, "speaker_embedder.tflite"))
        genderInterpreter = Interpreter(FileUtils.loadModelFile(context, "gender_classifier.tflite"))
    }

    fun generateEmbedding(buffer: AudioBuffer): FloatArray {
        val mono = AudioUtils.ensureMono(buffer)
        val input = AudioUtils.prepareAudio(mono)
        val inputBuffer = ByteBuffer.allocateDirect(input.size * 4).order(ByteOrder.nativeOrder())
        input.forEach { inputBuffer.putFloat(it) }
        inputBuffer.rewind()

        val output = Array(2) { FloatArray(1024) } // Change 2 -> 1 if your model outputs [1,1024]
        speakerInterpreter.run(inputBuffer, output)
        return output[0]
    }

    fun classifyGender(embedding: FloatArray): Gender {
        val input = arrayOf(embedding)
        val output = Array(1) { FloatArray(3) }
        genderInterpreter.run(input, output)

        return when (output[0].indices.maxByOrNull { output[0][it] }) {
            0 -> Gender.MALE
            1 -> Gender.FEMALE
            else -> Gender.ANDROGYNOUS
        }
    }

    fun verifySpeaker(context: Context, buffer: AudioBuffer): Boolean {
        val inputEmbedding = generateEmbedding(buffer)
        val storedEmbeddings = loadAllEmbeddings(context)

        if (storedEmbeddings.isEmpty()) return false

        val similarities = storedEmbeddings.map { cosineSimilarity(inputEmbedding, it) }
        val maxSim = similarities.maxOrNull() ?: 0f

        Log.d("MLUtils", "Speaker similarity scores: $similarities")
        Log.d("MLUtils", "Max similarity: $maxSim")

        if (maxSim > 0.92f) {
            Log.d("MLUtils", "Auto-appending verified sample.")
            saveNewEmbedding(context, inputEmbedding)
        }

        return maxSim > 0.85f
    }

    fun loadAllEmbeddings(context: Context): List<FloatArray> {
        return FileUtils.listEmbeddingFiles(context).mapNotNull { file ->
            try {
                FileUtils.readEmbedding(file)
            } catch (e: Exception) {
                Log.e("MLUtils", "Failed reading ${file.name}", e)
                null
            }
        }
    }

    fun saveNewEmbedding(context: Context, embedding: FloatArray) {
        val timestamp = System.currentTimeMillis()
        val filename = "embedding_$timestamp.dat"
        FileUtils.writeEmbedding(context, embedding, filename)
    }

    fun cosineSimilarity(a: FloatArray, b: FloatArray): Float {
        var dot = 0f
        var normA = 0f
        var normB = 0f
        for (i in a.indices) {
            dot += a[i] * b[i]
            normA += a[i] * a[i]
            normB += b[i] * b[i]
        }
        return (dot / (sqrt(normA.toDouble()) * sqrt(normB.toDouble()))).toFloat()
    }
}
