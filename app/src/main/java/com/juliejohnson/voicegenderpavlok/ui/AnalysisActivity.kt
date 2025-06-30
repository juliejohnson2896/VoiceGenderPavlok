package com.juliejohnson.voicegenderpavlok.ui

import android.Manifest
import android.content.pm.PackageManager
import android.os.Bundle
import android.widget.Button
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import com.juliejohnson.voicegenderpavlok.R
import com.juliejohnson.voicegenderpavlok.audio.AudioFeatures
import com.juliejohnson.voicegenderpavlok.audio.EssentiaAnalyzer
import com.juliejohnson.voicegenderpavlok.ml.VoiceProfile
import com.juliejohnson.voicegenderpavlok.storage.SessionStorage
import com.juliejohnson.voicegenderpavlok.utils.VADManager
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import java.io.ByteArrayOutputStream

class AnalysisActivity : AppCompatActivity() {

    // UI Elements
    private lateinit var pitchTextView: TextView
    private lateinit var formant1TextView: TextView
    private lateinit var formant2TextView: TextView
    private lateinit var startRecButton: Button
    private lateinit var stopRecButton: Button
    private lateinit var hnrTextView: TextView

    // Analysis Engine
    private lateinit var essentiaAnalyzer: EssentiaAnalyzer
    private val analysisScope = CoroutineScope(Dispatchers.Default)

    // Session Recording State
    private var isSessionRecording = false
    private lateinit var audioSessionStream: ByteArrayOutputStream
    private val analysisSessionData = mutableListOf<VoiceProfile>()
    // --- NEW: Add a variable to store the session start time ---
    private var sessionStartTime: Long = 0L

    private val permissionsRequestCode = 101

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_analysis)

        // Initialize UI
        pitchTextView = findViewById(R.id.pitch_text)
        formant1TextView = findViewById(R.id.formant1_text)
        formant2TextView = findViewById(R.id.formant2_text)
        startRecButton = findViewById(R.id.button_start_recording)
        stopRecButton = findViewById(R.id.button_stop_recording)
        hnrTextView = findViewById(R.id.hnr_text)

        startRecButton.setOnClickListener { startSessionRecording() }
        stopRecButton.setOnClickListener { stopSessionRecording() }

        essentiaAnalyzer = EssentiaAnalyzer()
        essentiaAnalyzer.initialize()
        VADManager.initialize(applicationContext)
    }

    override fun onResume() {
        super.onResume()
        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this, arrayOf(Manifest.permission.RECORD_AUDIO), permissionsRequestCode)
        } else {
            startVADListening()
        }
    }

    override fun onPause() {
        super.onPause()
        VADManager.stop()
    }

    private fun startVADListening() {
        VADManager.startListening(
            onSpeechDetected = {
                analysisScope.launch {
                    val speechChunk = VADManager.getRecentAudio()
                    if (speechChunk.isNotEmpty()) {
                        analyzeChunk(speechChunk)
                    }
                }
            },
            onRawAudio = { rawChunk ->
                if (isSessionRecording) {
                    val byteBuffer = ByteArray(rawChunk.size * 2)
                    for (i in rawChunk.indices) {
                        byteBuffer[2 * i] = (rawChunk[i].toInt() and 0xFF).toByte()
                        byteBuffer[2 * i + 1] = (rawChunk[i].toInt() shr 8 and 0xFF).toByte()
                    }
                    audioSessionStream.write(byteBuffer)
                }
            }
        )
    }

    private fun analyzeChunk(audioBuffer: FloatArray) {
        val features = essentiaAnalyzer.analyzeFrame(audioBuffer)
        features?.let {

            if (isSessionRecording) {
                // --- MODIFIED: Calculate elapsed time relative to the session start ---
                val elapsedTimeMs = System.currentTimeMillis() - sessionStartTime
                analysisSessionData.add(
                    VoiceProfile(
                        elapsedTimeMs, // Use the new relative timestamp
                        features.pitch,
                        features.formants.getOrNull(0) ?: 0f,
                        features.formants.getOrNull(1) ?: 0f
                    )
                )
            }
            updateUI(features)
        }
    }

    private fun updateUI(features: AudioFeatures) {
        // Update UI
        runOnUiThread {
            pitchTextView.text = if (features.pitch > 0) "%.2f Hz".format(features.pitch) else "..."
            formant1TextView.text =
                if (features.formants.isNotEmpty()) "F1: %.0f Hz".format(features.formants[0]) else "F1: ..."
            formant2TextView.text =
                if (features.formants.size >= 2) "F2: %.0f Hz".format(features.formants[1]) else "F2: ..."

            // Update the HNR text view with our new, meaningful description
            if (features.hnr > 0) { // HNR can be negative, so we check if it's a valid calculation
                // --- NEW: Interpret the HNR value ---
                val hnrDescription = when {
                    features.hnr < 15.0f -> "Breathy"
                    features.hnr < 25.0f -> "Clear"
                    else -> "Resonant"
                }

                hnrTextView.text = "Clarity: $hnrDescription (%.1f dB)".format(features.hnr)
            } else {
                hnrTextView.text = "Clarity (HNR): ..."
            }
        }
    }

    private fun startSessionRecording() {
        // --- NEW: Capture the start time when recording begins ---
        sessionStartTime = System.currentTimeMillis()
        audioSessionStream = ByteArrayOutputStream()
        analysisSessionData.clear()
        isSessionRecording = true
        startRecButton.isEnabled = false
        stopRecButton.isEnabled = true
        Toast.makeText(this, "Session recording started...", Toast.LENGTH_SHORT).show()
    }

    private fun stopSessionRecording() {
        if (!isSessionRecording) return
        isSessionRecording = false
        startRecButton.isEnabled = true
        stopRecButton.isEnabled = false

        val sampleRate = VADManager.getSampleRate()
        SessionStorage.saveSession(audioSessionStream.toByteArray(), analysisSessionData, sampleRate)
        Toast.makeText(this, "Session saved to Downloads folder.", Toast.LENGTH_LONG).show()
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == permissionsRequestCode && grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
            startVADListening()
        }
    }
}