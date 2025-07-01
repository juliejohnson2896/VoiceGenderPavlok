package com.juliejohnson.voicegenderpavlok.storage

import android.os.Environment
import android.util.Log
import com.juliejohnson.voicegenderpavlok.ml.VoiceProfile
import java.io.File
import java.io.FileOutputStream
import java.text.SimpleDateFormat
import java.util.*

object SessionStorage {

    private fun getSessionDirectory(): File {
        val dir = File(Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS), "VoiceAnalysisSessions")
        if (!dir.exists()) {
            dir.mkdirs()
        }
        return dir
    }

    fun saveSession(
        audioData: ByteArray,
        analysisData: List<VoiceProfile>,
        sampleRate: Int
    ) {
        val timestamp = System.currentTimeMillis()
        val dateFormat = SimpleDateFormat("yyyyMMdd_HHmmss", Locale.getDefault())
        val baseFilename = "session_${dateFormat.format(Date(timestamp))}"
        val sessionDir = getSessionDirectory()

        // Save the WAV file using your existing FileUtils
        val wavFile = File(sessionDir, "$baseFilename.wav")
        val shortArray = ShortArray(audioData.size / 2) { i ->
            ((audioData[i * 2 + 1].toInt() shl 8) or (audioData[i * 2].toInt() and 0xFF)).toShort()
        }
        FileUtils.writeWavFile(shortArray, sampleRate, wavFile)

        // Save the CSV analysis file
        val csvFile = File(sessionDir, "$baseFilename.csv")
        try {
            FileOutputStream(csvFile).use { fos ->
                fos.write("session_time_ms,pitch_hz,f1_hz,f2_hz\n".toByteArray())
                analysisData.forEach { profile ->
                    val line = "${profile.timestamp},${profile.pitch},${profile.formant1},${profile.formant2}\n"
                    fos.write(line.toByteArray())
                }
            }
            Log.d("SessionStorage", "Session saved to ${csvFile.absolutePath}")
        } catch (e: Exception) {
            Log.e("SessionStorage", "Failed to save CSV file", e)
        }
    }
}