package com.example.voicegenderpavlok.storage

import android.content.Context
import android.util.Log
import com.example.voicegenderpavlok.ml.EmbeddingMetadata
import kotlinx.serialization.encodeToString
import kotlinx.serialization.json.Json
import java.io.File

object EnrollmentStorage {

    private lateinit var appContext: Context

    private val json = Json {
        prettyPrint = true
        ignoreUnknownKeys = true
    }

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
                    val metadata = json.decodeFromString<EmbeddingMetadata>(jsonFile.readText())
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
        val dir = getEnrollmentDir()

        val jsonFile = File(dir, "${sample.id}.json")
        val metadata = sample.metadata
        jsonFile.writeText(json.encodeToString(metadata))
    }

    fun deleteSample(sampleId: String) {
        val dir = getEnrollmentDir()
        File(dir, "$sampleId.json").delete()
        File(dir, "$sampleId.wav").delete()
        File(dir, "$sampleId.embedding").delete()
    }

    fun clearAllSamples() {
        val dir = getEnrollmentDir()
        dir.listFiles()?.forEach { it.delete() }
    }
}
