package com.juliejohnson.voicegenderpavlok.ui

import android.Manifest
import android.annotation.SuppressLint
import android.content.pm.PackageManager
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import android.os.Bundle
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import com.juliejohnson.voicegenderpavlok.R
import com.juliejohnson.voicegenderpavlok.audio.VoiceAnalysisEngine // Import our new engine
import kotlin.concurrent.thread

class AnalysisActivity : AppCompatActivity() {

    private lateinit var pitchTextView: TextView
    private var audioRecord: AudioRecord? = null
    private var isRecording = false
    private var recordThread: Thread? = null

    // Using a standard sample rate that works well for voice
    private val sampleRate = 44100
    private val bufferSize = 2048 // A good buffer size for real-time processing

    private val permissionsRequestCode = 101

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_analysis)
        pitchTextView = findViewById(R.id.pitch_text)

        // Make sure our native library is loaded when the activity is created
        VoiceAnalysisEngine
    }

    override fun onResume() {
        super.onResume()
        // Check for microphone permission and start processing
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
                    // Convert the Short audio data from AudioRecord to the Float data Essentia needs
                    for (i in 0 until readSize) {
                        floatBuffer[i] = shortBuffer[i] / 32768.0f
                    }

                    // Call our C++ function via the Kotlin Engine!
                    val pitch = VoiceAnalysisEngine.getPitch(floatBuffer, sampleRate)

                    runOnUiThread {
                        if (pitch > 0) {
                            pitchTextView.text = "%.2f Hz".format(pitch)
                        }
                    }
                }
            }
        }
    }

    private fun stopAudioProcessing() {
        isRecording = false
        recordThread?.join() // Wait for the processing thread to finish
        audioRecord?.stop()
        audioRecord?.release()
        audioRecord = null
        recordThread = null
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == permissionsRequestCode && grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
            startAudioProcessing()
        }
    }
}