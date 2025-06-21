package com.example.voicegenderpavlok.ui

import android.Manifest
import android.annotation.SuppressLint
import android.content.pm.PackageManager
import android.os.Bundle
import android.util.Log
import android.widget.Button
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.annotation.RequiresPermission
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import com.example.voicegenderpavlok.R
import com.example.voicegenderpavlok.ml.AudioBuffer
import com.example.voicegenderpavlok.ml.MLUtils
import com.example.voicegenderpavlok.storage.EnrollmentStorage
import com.example.voicegenderpavlok.utils.AudioUtils
import com.example.voicegenderpavlok.utils.VADRecorder
import kotlinx.coroutines.*
import kotlinx.coroutines.channels.Channel
import kotlin.math.absoluteValue

class EnrollmentActivity : AppCompatActivity() {

    private lateinit var recordButton: Button
    private lateinit var statusText: TextView
    private lateinit var waveformView: WaveformView
    private lateinit var vadRecorder: VADRecorder

    private val sampleRate = 16000
    private val numSamples = 3

    private val amplitudeChannel = Channel<Float>(capacity = Channel.UNLIMITED)
    private val uiScope = CoroutineScope(Dispatchers.Main + SupervisorJob())

    @SuppressLint("MissingPermission")
    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestPermission()
    ) { granted ->
        if (granted) startEnrollmentFlow()
        else Toast.makeText(this, "Microphone permission denied", Toast.LENGTH_SHORT).show()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_enrollment)

        recordButton = findViewById(R.id.record_button)
        statusText = findViewById(R.id.status_text)
        waveformView = findViewById(R.id.waveform_view)
        vadRecorder = VADRecorder(this)

        recordButton.setOnClickListener {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO)
                == PackageManager.PERMISSION_GRANTED
            ) {
                startEnrollmentFlow()
            } else {
                permissionLauncher.launch(Manifest.permission.RECORD_AUDIO)
            }
        }
    }

    @OptIn(ExperimentalCoroutinesApi::class)
    @RequiresPermission(Manifest.permission.RECORD_AUDIO)
    private fun startEnrollmentFlow() {
        recordButton.isEnabled = false
        statusText.text = "Starting enrollment…"

        CoroutineScope(Dispatchers.IO).launch {
            for (i in 1..numSamples) {
                withContext(Dispatchers.Main) {
                    statusText.text = "Waiting for speech ($i of $numSamples)…"
                    waveformView.reset()
                }

                val buffer = vadRecorder.recordUntilSpeechDetected { amp -> amplitudeChannel.trySend(amp) }

                if (buffer == null || buffer.samples.isEmpty()) {
                    withContext(Dispatchers.Main) {
                        statusText.text = "Sample $i failed"
                        recordButton.isEnabled = true
                        Toast.makeText(this@EnrollmentActivity, "Recording failed.", Toast.LENGTH_SHORT).show()
                    }
                    continue // skip this sample and try the next
                }

                val fixed = AudioUtils.prepareAudio(AudioUtils.ensureMono(buffer))
                val embedding = MLUtils.generateEmbedding(AudioBuffer(fixed, 1))

                EnrollmentStorage.saveSample(applicationContext, fixed, embedding)

                withContext(Dispatchers.Main) {
                    statusText.text = "Saved sample $i"
                    waveformView.reset()
                }

                delay(1000)
            }


            withContext(Dispatchers.Main) {
                statusText.text = "Enrollment complete!"
                Toast.makeText(this@EnrollmentActivity, "Enrollment saved successfully.", Toast.LENGTH_LONG).show()
                recordButton.isEnabled = true
            }
        }

        uiScope.launch {
            for (amp in amplitudeChannel) {
                waveformView.addAmplitude(amp)
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        amplitudeChannel.close()
        uiScope.cancel()
    }
}
