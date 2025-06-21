package com.example.voicegenderpavlok.ml

import android.content.Context
import android.util.Log
import com.example.voicegenderpavlok.audio.AudioFeatureExtractor
import com.example.voicegenderpavlok.storage.EnrollmentStorage
import com.example.voicegenderpavlok.storage.FileUtils
import com.example.voicegenderpavlok.utils.AudioUtils
import org.tensorflow.lite.Interpreter
import java.io.File
import kotlin.math.sqrt

object MLUtils {

    private lateinit var speakerInterpreter: Interpreter
    private lateinit var genderInterpreter: Interpreter

    fun initialize(context: Context) {
        speakerInterpreter = Interpreter(FileUtils.loadModelFile(context, "ecapa_tdnn_model_opset11.tflite"))
        genderInterpreter = Interpreter(FileUtils.loadModelFile(context, "gender_classifier.tflite"))
        logModelInfo(speakerInterpreter, "SpeakerModelInfo")
        logModelInfo(genderInterpreter, "GenderModelInfo")

    }

    fun logModelInfo(interpreter: Interpreter, tag: String) {
        val inputCount = interpreter.inputTensorCount
        for (i in 0 until inputCount) {
            val shape = interpreter.getInputTensor(i).shape()
            val dtype = interpreter.getInputTensor(i).dataType()
            Log.d(tag, "Input tensor[$i] shape: ${shape.contentToString()}, dtype: $dtype")
        }

        val outputCount = interpreter.outputTensorCount
        for (i in 0 until outputCount) {
            val shape = interpreter.getOutputTensor(i).shape()
            val dtype = interpreter.getOutputTensor(i).dataType()
            Log.d(tag, "Output tensor[$i] shape: ${shape.contentToString()}, dtype: $dtype")
        }
    }

    fun generateEmbedding(buffer: AudioBuffer): FloatArray {
        val mono = AudioUtils.ensureMono(buffer)
        Log.d("MLUtils", "Raw audio samples size: ${mono.size}")

        val features: Array<FloatArray> = AudioFeatureExtractor.extractLogMelSpectrogram(mono)
        val inputTensor = arrayOf(features) // shape: [1][100][80]
        val inputLengths = floatArrayOf(100f) // shape: [1]

        val inputs = arrayOf(inputTensor, inputLengths)
        val outputTensor = Array(1) { Array(1) { FloatArray(192) } }
        Log.d("MLUtils", "Embedding input shape: [${inputTensor.size}, ${inputTensor[0].size}, ${inputTensor[0][0].size}]")

        try {
            speakerInterpreter.runForMultipleInputsOutputs(inputs, mapOf(0 to outputTensor))
        } catch (e: Exception) {
            Log.e("MLUtils", "Error running speakerInterpreter", e)
        }

        Log.d("MLUtils", "Embedding shape: [1][1][192], output: ${outputTensor[0][0].take(5)}…")
        return outputTensor[0][0]
    }

    fun classifyGender(embedding: FloatArray): Gender {
        // Ensure input is [1, 1024]
        val input = arrayOf(embedding.copyOf())
        val output = Array(1) { FloatArray(3) }

        Log.d("MLUtils", "Gender classification input shape: [${input.size}, ${input[0].size}]")

        try {
            genderInterpreter.run(input, output)

            Log.d("MLUtils", "Gender classification output: ${output[0].joinToString()}")

            return when (output[0].indices.maxByOrNull { output[0][it] }) {
                0 -> Gender.MALE
                1 -> Gender.FEMALE
                else -> Gender.ANDROGYNOUS
            }

        } catch (e: Exception) {
            Log.e("MLUtils", "Error during gender classification", e)
            return Gender.ANDROGYNOUS
        }
    }

    fun verifySpeaker(context: Context, buffer: AudioBuffer): Boolean {
        val rawAudio = AudioUtils.ensureMonoAndFixedLength(buffer)
        FileUtils.writeDebugWav(context, rawAudio) // 🔍 save for inspection

        val inputEmbedding = generateEmbedding(AudioBuffer(rawAudio, 1))
        Log.d("Debug", "Raw audio size: ${rawAudio.size}, Embedding size: ${inputEmbedding.size}")

        val storedSamples = EnrollmentStorage.listSamples()

        if (storedSamples.isEmpty()) return false

        var maxSim = 0f
        storedSamples.forEachIndexed { index, sample ->
            try {
                val storedEmbedding = FileUtils.readEmbedding(File(sample.embeddingPath))
                val sim = cosineSimilarity(inputEmbedding, storedEmbedding)
                maxSim = maxOf(maxSim, sim)
                Log.d("SpeakerSim", "To sample $index (${sample.id}): $sim")
            } catch (e: Exception) {
                Log.w("SpeakerSim", "Failed to read embedding ${sample.id}: ${e.message}")
            }
        }

        Log.d("SpeakerSim", "Max similarity: $maxSim")

        if (maxSim > 0.92f) {
            Log.d("MLUtils", "Auto-appending verified sample.")
            EnrollmentStorage.saveSample(
                context = context,
                rawAudio = rawAudio,
                embedding = inputEmbedding,
                label = "auto-verified",
                autoEnrolled = true
            )
        }

        return maxSim > 0.85f
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
