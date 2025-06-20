package com.example.voicegenderpavlok.ui

import android.Manifest
import android.annotation.SuppressLint
import android.content.pm.PackageManager
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
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
import com.example.voicegenderpavlok.ml.EmbeddingMetadata
import com.example.voicegenderpavlok.ml.MLUtils
import com.example.voicegenderpavlok.storage.EnrollmentSample
import com.example.voicegenderpavlok.storage.EnrollmentStorage
import com.example.voicegenderpavlok.utils.FileUtils
import kotlinx.coroutines.*
import kotlinx.coroutines.channels.Channel
import java.io.File
import kotlin.math.absoluteValue

class EnrollmentActivity : AppCompatActivity() {

    private lateinit var recordButton: Button
    private lateinit var statusText: TextView
    private lateinit var waveformView: WaveformView

    private val sampleRate = 16000
    private val channels = AudioFormat.CHANNEL_IN_MONO
    private val encoding = AudioFormat.ENCODING_PCM_16BIT
    private val durationSeconds = 2
    private val numSamples = 3

    private val bufferSize = AudioRecord.getMinBufferSize(sampleRate, channels, encoding)
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

    @RequiresPermission(Manifest.permission.RECORD_AUDIO)
    private fun startEnrollmentFlow() {
        recordButton.isEnabled = false
        statusText.text = "Starting enrollment…"

        CoroutineScope(Dispatchers.IO).launch {
            for (i in 1..numSamples) {
                withContext(Dispatchers.Main) {
                    statusText.text = "Recording sample $i of $numSamples…"
                }

                val rawAudio = recordSample() ?: run {
                    withContext(Dispatchers.Main) {
                        statusText.text = "Sample $i failed"
                        recordButton.isEnabled = true
                        Toast.makeText(this@EnrollmentActivity, "Recording failed.", Toast.LENGTH_SHORT).show()
                    }
                    return@launch
                }

                val audioBuffer = AudioBuffer(rawAudio, sampleRate, 1)
                val embedding = MLUtils.generateEmbedding(audioBuffer)

                EnrollmentStorage.saveSample(applicationContext, rawAudio, embedding)
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

    @RequiresPermission(Manifest.permission.RECORD_AUDIO)
    private suspend fun recordAndProcessSample(): EnrollmentSample? {
        val recorder = AudioRecord(
            MediaRecorder.AudioSource.MIC,
            sampleRate,
            channels,
            encoding,
            bufferSize
        )

        if (recorder.state != AudioRecord.STATE_INITIALIZED) return null

        val shortBuffer = ShortArray(sampleRate * durationSeconds)
        val timestamp = System.currentTimeMillis()
        val id = "sample_$timestamp"

        return try {
            recorder.startRecording()
            val read = recorder.read(shortBuffer, 0, shortBuffer.size)

            for (i in 0 until shortBuffer.size step 256) {
                val chunk = shortBuffer.slice(i until minOf(i + 256, shortBuffer.size))
                val amplitude = chunk.map { it.toInt().absoluteValue }.average().toFloat()
                amplitudeChannel.trySend(amplitude)
                delay(16)
            }

            recorder.stop()
            recorder.release()

            withContext(Dispatchers.Main) {
                waveformView.reset()
            }

            if (read <= 0) return null

            val floatAudio = shortBuffer.map { it / 32768.0f }.toFloatArray()
            val audioBuffer = AudioBuffer(floatAudio, sampleRate, 1)
            val embedding = MLUtils.generateEmbedding(audioBuffer)

            // Save audio file
            val audioFileName = "$id.wav"
            val audioFile = File(EnrollmentStorage.getEnrollmentDir(), audioFileName)
            FileUtils.writeWavFile(audioFile, shortBuffer, sampleRate)

            // Save embedding file
            val embeddingFileName = "$id.embedding"
            val embeddingFile = File(EnrollmentStorage.getEnrollmentDir(), embeddingFileName)
            FileUtils.writeEmbeddingFile(embeddingFile, embedding)

            // Build metadata and sample
            val metadata = EmbeddingMetadata(
                timestamp = timestamp,
                label = null,
                audioFile = audioFileName,
                embeddingFile = embeddingFileName
            )

            return EnrollmentSample(
                id = id,
                audioPath = audioFile.absolutePath,
                embeddingPath = embeddingFile.absolutePath,
                metadata = metadata
            )

        } catch (e: Exception) {
            Log.e("EnrollmentActivity", "Failed to record sample", e)
            recorder.release()
            null
        }
    }


    override fun onDestroy() {
        super.onDestroy()
        amplitudeChannel.close()
        uiScope.cancel()
    }
}
