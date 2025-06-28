package com.juliejohnson.voicegenderpavlok.ui

import android.Manifest
import android.content.pm.PackageManager
import android.os.Bundle
import android.util.Log
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import com.juliejohnson.voicegenderpavlok.R
import com.juliejohnson.voicegenderpavlok.audio.AudioFeatures
import com.juliejohnson.voicegenderpavlok.audio.EssentiaAnalyzer
import com.juliejohnson.voicegenderpavlok.utils.VADManager // Import your existing VADManager
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch

class AnalysisActivity : AppCompatActivity() {

    private lateinit var pitchTextView: TextView
    private lateinit var formant1TextView: TextView
    private lateinit var formant2TextView: TextView

    // We will use a CoroutineScope for background tasks
    private val analysisScope = CoroutineScope(Dispatchers.Default)

    private val sampleRate = 44100 // Essentia is initialized with this rate
    private val permissionsRequestCode = 101

    private val essentiaAnalyzer = EssentiaAnalyzer()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_analysis)

        pitchTextView = findViewById(R.id.pitch_text)
        formant1TextView = findViewById(R.id.formant1_text)
        formant2TextView = findViewById(R.id.formant2_text)

        // Initialize your existing VADManager when the activity is created
        VADManager.initialize(applicationContext)

        // Initialize the Essentia streaming engine when the activity is created
        if (!essentiaAnalyzer.initialize(sampleRate)) {
            // Handle initialization failure
            Log.e("AudioProcessor", "Failed to initialize Essentia")
        }
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
        // Stop the VAD when the activity is not in the foreground to save resources
        VADManager.stop()
    }

    private fun startVADListening() {
        // Use the VADManager to start listening for speech.
        // The block of code inside this lambda will ONLY be executed when speech is detected.
        VADManager.startListening {
            // Speech has been detected, now we perform our analysis.
            // We launch a coroutine to do this work in the background.
            analysisScope.launch {
                // Get the chunk of audio that contains the detected speech.
                val audioBuffer = VADManager.getRecentAudio()

                if (audioBuffer.isNotEmpty()) {
                    // Call our Essentia engine with the captured audio.
                    // This now only runs when you are actually speaking.
                    try {
                        val features = essentiaAnalyzer.analyzeFrame(audioBuffer)
                        features?.let {
                            // Use the extracted features
                            processSpeechFeatures(it)
                        }
                    } catch (e: Exception) {
                        Log.e("AudioProcessor", "Analysis failed", e)
                    }
                }
            }
        }
    }

    private fun processSpeechFeatures(features: AudioFeatures) {
        if (features.isValid) {
            Log.d("AudioFeatures", "Pitch: ${features.pitch} Hz")
            Log.d("AudioFeatures", "Brightness: ${features.brightness}")
            Log.d("AudioFeatures", "Resonance: ${features.resonance}")
            Log.d("AudioFeatures", "Centroid: ${features.centroid} Hz")
            Log.d("AudioFeatures", "Formants: ${features.formants.joinToString(", ")} Hz")

            // Your processing logic here
            runOnUiThread {
                if (features.pitch > 0) {
                    pitchTextView.text = "%.2f Hz".format(features.pitch)
                }

                // NEW: Update the formant text views
                if (features.formants.size >= 2) {
                    formant1TextView.text = "F1: %.0f Hz".format(features.formants[0])
                    formant2TextView.text = "F2: %.0f Hz".format(features.formants[1])
                }
            }
        }
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == permissionsRequestCode && grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
            startVADListening()
        }
    }
}