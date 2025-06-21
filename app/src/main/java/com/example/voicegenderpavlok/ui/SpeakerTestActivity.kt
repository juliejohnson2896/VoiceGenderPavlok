package com.example.voicegenderpavlok.ui

import android.os.Bundle
import android.widget.Button
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import com.example.voicegenderpavlok.R
import com.example.voicegenderpavlok.ml.AudioBuffer
import com.example.voicegenderpavlok.ml.MLUtils
import com.example.voicegenderpavlok.ml.Gender
import com.example.voicegenderpavlok.utils.VADRecorder
import kotlinx.coroutines.*
import kotlinx.coroutines.channels.Channel

class SpeakerTestActivity : AppCompatActivity() {

    private lateinit var recordButton: Button
    private lateinit var resultText: TextView
    private lateinit var waveformView: WaveformView
    private lateinit var vadRecorder: VADRecorder

    private val amplitudeChannel = Channel<Float>(capacity = Channel.UNLIMITED)
    private val uiScope = CoroutineScope(Dispatchers.Main + SupervisorJob())

    @androidx.annotation.RequiresPermission(android.Manifest.permission.RECORD_AUDIO)
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_speaker_test)

        recordButton = findViewById(R.id.record_button)
        resultText = findViewById(R.id.similarity_results)
        waveformView = findViewById(R.id.waveform_view)

        vadRecorder = VADRecorder(this)

        recordButton.setOnClickListener {
            recordButton.isEnabled = false
            resultText.text = "Listening…"
            waveformView.reset()

            CoroutineScope(Dispatchers.IO).launch {
                val buffer = vadRecorder.recordUntilSpeechDetected { amp -> amplitudeChannel.trySend(amp) }

                if (buffer == null || buffer.samples.isEmpty()) {
                    withContext(Dispatchers.Main) {
                        resultText.text = "Recording failed"
                        Toast.makeText(this@SpeakerTestActivity, "Failed to detect speech", Toast.LENGTH_SHORT).show()
                        recordButton.isEnabled = true
                    }
                    return@launch
                }

                val verified = MLUtils.verifySpeaker(this@SpeakerTestActivity, buffer)
                val gender = MLUtils.classifyGender(MLUtils.generateEmbedding(buffer))

                withContext(Dispatchers.Main) {
                    resultText.text = "Verified: $verified\nGender: $gender"
                    Toast.makeText(this@SpeakerTestActivity, "Result: $verified", Toast.LENGTH_SHORT).show()
                    waveformView.reset()
                    recordButton.isEnabled = true
                }
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
