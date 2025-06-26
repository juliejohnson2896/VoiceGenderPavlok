package com.juliejohnson.voicegenderpavlok.storage

import android.content.Context
import android.util.Log
import com.juliejohnson.voicegenderpavlok.data.VoiceProfile
import com.juliejohnson.voicegenderpavlok.ml.EmbeddingMetadata
import com.juliejohnson.voicegenderpavlok.ml.Gender
import java.io.File

object EnrollmentStorage {

    private lateinit var appContext: Context

    fun initialize(context: Context) {
        appContext = context.applicationContext
    }

    fun getEnrollmentDir(): File {
        val dir = File(appContext.filesDir, "enrollments")
        if (!dir.exists()) dir.mkdirs()
        return dir
    }

    fun listSamples(): List<EnrollmentSample> {
        val dir = getEnrollmentDir()
        return dir.listFiles { file -> file.extension == "json" }
            ?.mapNotNull { jsonFile ->
                try {
                    val metadata = FileUtils.readMetadataFile(jsonFile)
                    val id = jsonFile.nameWithoutExtension
                    val audioPath = File(dir, metadata.audioFile).absolutePath
                    val embeddingPath = File(dir, metadata.embeddingFile).absolutePath

                    EnrollmentSample(
                        id = id,
                        audioPath = audioPath,
                        embeddingPath = embeddingPath,
                        metadata = metadata
                    )
                } catch (e: Exception) {
                    Log.w("EnrollmentStorage", "Failed to read ${jsonFile.name}: ${e.message}")
                    null
                }
            } ?: emptyList()
    }

    fun saveSample(sample: EnrollmentSample) {
        val jsonFile = File(getEnrollmentDir(), "${sample.id}.json")
        FileUtils.writeMetadataFile(jsonFile, sample.metadata)
    }

    fun saveSample(
        context: Context,
        rawAudio: FloatArray,
        embedding: FloatArray,
        label: String? = null,
        voiceProfile: VoiceProfile,
        autoEnrolled: Boolean = false
    ) {
        val timestamp = System.currentTimeMillis()
        val id = "sample_$timestamp"

        val audioFileName = "$id.wav"
        val audioFile = File(getEnrollmentDir(), audioFileName)
        val shortBuffer = rawAudio.map { (it * 32767).toInt().coerceIn(-32768, 32767).toShort() }.toShortArray()
        FileUtils.writeWavFile(shortBuffer, 16000, audioFile)

        val embeddingFileName = "$id.embedding"
        val embeddingFile = File(getEnrollmentDir(), embeddingFileName)
        FileUtils.writeEmbeddingFile(embeddingFile, embedding)

        val metadata = EmbeddingMetadata(
            timestamp = timestamp,
            label = label,
            audioFile = audioFileName,
            embeddingFile = embeddingFileName,
            voiceProfile = voiceProfile,
            autoEnrolled = autoEnrolled
        )

        val sample = EnrollmentSample(
            id = id,
            audioPath = audioFile.absolutePath,
            embeddingPath = embeddingFile.absolutePath,
            metadata = metadata
        )

        saveSample(sample)
    }


    fun deleteSample(sampleId: String) {
        val dir = getEnrollmentDir()
        val files = listOf(
            File(dir, "$sampleId.json"),
            File(dir, "$sampleId.wav"),
            File(dir, "$sampleId.embedding")
        )
        files.forEach { file ->
            if (file.exists()) {
                if (!file.delete()) {
                    Log.w("EnrollmentStorage", "Failed to delete ${file.name}")
                }
            }
        }
    }


    fun clearAllSamples() {
        getEnrollmentDir().listFiles()?.forEach { file ->
            if (file.extension in listOf("json", "wav", "embedding")) {
                file.delete()
            }
        }
    }

    fun listAllEmbeddings(): List<FloatArray> {
        return listSamples().mapNotNull { sample ->
            try {
                FileUtils.readEmbedding(File(sample.embeddingPath))
            } catch (e: Exception) {
                Log.e("EnrollmentStorage", "Failed to read embedding: ${sample.embeddingPath}", e)
                null
            }
        }
    }
}