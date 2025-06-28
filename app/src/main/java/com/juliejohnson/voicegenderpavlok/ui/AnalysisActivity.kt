package com.juliejohnson.voicegenderpavlok.ui

import android.Manifest
import android.annotation.SuppressLint
import android.content.pm.PackageManager
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import android.os.Bundle
import android.util.Log
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import com.juliejohnson.voicegenderpavlok.R
import com.juliejohnson.voicegenderpavlok.audio.AudioFeatures
import com.juliejohnson.voicegenderpavlok.audio.EssentiaAnalyzer
import kotlin.concurrent.thread

class AnalysisActivity : AppCompatActivity() {

    private lateinit var pitchTextView: TextView
    private var audioRecord: AudioRecord? = null
    private var isRecording = false
    private var recordThread: Thread? = null

    private val sampleRate = 44100
    private val bufferSize = 2048

    private val permissionsRequestCode = 101

    private val essentiaAnalyzer = EssentiaAnalyzer()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_analysis)
        pitchTextView = findViewById(R.id.pitch_text)

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
            startAudioProcessing()
        }
    }

    override fun onPause() {
        super.onPause()
        stopAudioProcessing()
    }

    override fun onDestroy() {
        super.onDestroy()
        // Properly shut down the Essentia engine when the app is destroyed
        essentiaAnalyzer.cleanup()
    }

    @SuppressLint("MissingPermission")
    private fun startAudioProcessing() {
        val minBufferSize = AudioRecord.getMinBufferSize(sampleRate, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT)
        audioRecord = AudioRecord(MediaRecorder.AudioSource.MIC, sampleRate, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT, minBufferSize)

        audioRecord?.startRecording()
        isRecording = true

        recordThread = thread(start = true) {
            val shortBuffer = ShortArray(bufferSize)
            val floatBuffer = FloatArray(bufferSize)

            while (isRecording) {
                val readSize = audioRecord?.read(shortBuffer, 0, bufferSize) ?: -1
                if (readSize > 0) {
                    for (i in 0 until readSize) {
                        floatBuffer[i] = shortBuffer[i] / 32768.0f
                    }

                    try {
                        val features = essentiaAnalyzer.analyzeFrame(floatBuffer)
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

            // Your processing logic here
            runOnUiThread {
                if (features.pitch > 0) {
                    pitchTextView.text = "%.2f Hz".format(features.pitch)
                }
            }
        }
    }

    private fun stopAudioProcessing() {
        isRecording = false
        recordThread?.join()
        audioRecord?.stop()
        audioRecord?.release()
        audioRecord = null
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == permissionsRequestCode && grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
            startAudioProcessing()
        }
    }
}