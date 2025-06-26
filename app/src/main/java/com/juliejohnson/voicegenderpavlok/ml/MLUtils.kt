package com.juliejohnson.voicegenderpavlok.ml

import android.content.Context
import android.util.Log
import org.tensorflow.lite.Interpreter
import java.io.File
import kotlin.math.sqrt

// --- New Imports for ONNX Runtime ---
import ai.onnxruntime.OnnxTensor
import ai.onnxruntime.OrtEnvironment
import ai.onnxruntime.OrtSession
import com.juliejohnson.voicegenderpavlok.audio.AudioFeatureExtractor
import com.juliejohnson.voicegenderpavlok.storage.EnrollmentStorage
import com.juliejohnson.voicegenderpavlok.storage.FileUtils
import com.juliejohnson.voicegenderpavlok.utils.AudioUtils
import java.nio.FloatBuffer

object MLUtils {

    // --- MODIFIED: Replaced TFLite Interpreter with ONNX OrtSession ---
    private lateinit var speakerSession: OrtSession
    private lateinit var genderInterpreter: Interpreter

    fun initialize(context: Context) {
        // --- MODIFIED: Initialize the ONNX Runtime for the speaker model ---
        val env = OrtEnvironment.getEnvironment()
        val speakerModelBytes = FileUtils.loadOnnxModelFile(context, "speaker_embedding_model.onnx")
        speakerSession = env.createSession(speakerModelBytes, OrtSession.SessionOptions())

        // --- UNCHANGED: Keep the gender classifier as is ---
        genderInterpreter = Interpreter(FileUtils.loadModelFile(context, "gender_classifier.tflite"))

        logOnnxModelInfo(speakerSession, "SpeakerModelInfo")
        logTfliteModelInfo(genderInterpreter, "GenderModelInfo")
    }

    // --- NEW: A logging function specifically for ONNX models ---
    fun logOnnxModelInfo(session: OrtSession, tag: String) {
        val inputNames = session.inputNames.joinToString()
        val outputNames = session.outputNames.joinToString()
        Log.d(tag, "ONNX Model Inputs: $inputNames, Outputs: $outputNames")
        session.inputInfo.forEach { (name, info) ->
            Log.d(tag, "Input '$name': ${info.info.toString()}")
        }
    }

    // Renamed the original logModelInfo to be specific to TFLite
    fun logTfliteModelInfo(interpreter: Interpreter, tag: String) {
        val inputCount = interpreter.inputTensorCount
        for (i in 0 until inputCount) {
            val shape = interpreter.getInputTensor(i).shape()
            val dtype = interpreter.getInputTensor(i).dataType()
            Log.d(tag, "TFLite Input tensor[$i] shape: ${shape.contentToString()}, dtype: $dtype")
        }

        val outputCount = interpreter.outputTensorCount
        for (i in 0 until outputCount) {
            val shape = interpreter.getOutputTensor(i).shape()
            val dtype = interpreter.getOutputTensor(i).dataType()
            Log.d(tag, "TFLite Output tensor[$i] shape: ${shape.contentToString()}, dtype: $dtype")
        }
    }

    // --- MODIFIED: This function now uses the ONNX model ---
    fun generateEmbedding(buffer: AudioBuffer): FloatArray {
        val env = OrtEnvironment.getEnvironment()

        // 1. UNCHANGED: Get the raw audio and create the spectrogram. This is our model's input.
        val mono = AudioUtils.ensureMono(buffer)
        val features: Array<FloatArray> = AudioFeatureExtractor.extractLogMelSpectrogram(mono)

        // 2. MODIFIED: Prepare the input for the ONNX model.
        //    We flatten the 2D spectrogram into a 1D array for the FloatBuffer.
        val modelInput = features.flatMap { it.asIterable() }.toFloatArray()
        val inputBuffer = FloatBuffer.wrap(modelInput)

        // The shape must match what the model was exported with: [batch_size, time_steps, num_mels]
        // Our Android code uses 100 frames and 80 mels, which is perfect.
        val inputShape = longArrayOf(1, features.size.toLong(), features[0].size.toLong())

        // Create the OnnxTensor. "input" is the name we gave it in the export script.
        val inputTensor = OnnxTensor.createTensor(env, inputBuffer, inputShape)

        // 3. MODIFIED: Run inference using the ONNX session.
        val results = speakerSession.run(mapOf("input" to inputTensor))

        // 4. MODIFIED: Extract the output embedding.
        val outputTensor = results.get(0) as OnnxTensor
        val embedding = outputTensor.floatBuffer.array().copyOf()
        Log.d("MLUtils", "ONNX Embedding generated with size: ${embedding.size}")

        // 5. MODIFIED: Clean up the ONNX tensors to prevent memory leaks.
        inputTensor.close()
        results.close()

        return embedding
    }

    // --- UNCHANGED: This function remains exactly the same ---
    fun classifyGender(embedding: FloatArray): Gender {
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

    // --- UNCHANGED: This function remains exactly the same ---
    fun verifySpeaker(context: Context, buffer: AudioBuffer): Boolean {
        val rawAudio = AudioUtils.ensureMonoAndFixedLength(buffer)
        FileUtils.writeDebugWav(context, rawAudio)

        // This now calls our new ONNX-powered generateEmbedding function
        val inputEmbedding = generateEmbedding(AudioBuffer(rawAudio, 16000))
        Log.d("Debug", "Raw audio size: ${rawAudio.size}, Embedding size: ${inputEmbedding.size}")

        val storedSamples = EnrollmentStorage.listSamples()

        if (storedSamples.isEmpty()) return false

        var maxSim = 0f
        storedSamples.forEachIndexed { index, sample ->
            try {
                val storedEmbedding = FileUtils.readEmbedding(File(sample.embeddingPath))
                val sim = cosineSimilarity(inputEmbedding, storedEmbedding)
                maxSim = maxOf(maxSim, sim)
                Log.d("SpeakerSim", "Similarity to sample $index (${sample.id}): $sim")
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
                voiceProfile = storedSamples[0].metadata.voiceProfile,
                autoEnrolled = true
            )
        }

        return maxSim > 0.85f
    }

    // --- UNCHANGED: This function remains exactly the same ---
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