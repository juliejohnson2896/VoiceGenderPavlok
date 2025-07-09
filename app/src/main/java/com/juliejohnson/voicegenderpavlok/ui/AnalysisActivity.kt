package com.juliejohnson.voicegenderpavlok.ui

import android.Manifest
import android.content.pm.PackageManager
import android.os.Bundle
import android.widget.Button
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import be.tarsos.dsp.AudioDispatcher
import be.tarsos.dsp.AudioEvent
import be.tarsos.dsp.AudioProcessor
import be.tarsos.dsp.io.jvm.AudioDispatcherFactory
import be.tarsos.dsp.pitch.PitchDetectionHandler
import be.tarsos.dsp.pitch.PitchDetectionResult
import be.tarsos.dsp.pitch.PitchProcessor
import be.tarsos.dsp.pitch.PitchProcessor.PitchEstimationAlgorithm
import com.juliejohnson.voicegenderpavlok.R
import com.juliejohnson.voicegenderpavlok.audio.AudioFeatures
import com.juliejohnson.voicegenderpavlok.audio.PitchAnalyzer
import com.juliejohnson.voicegenderpavlok.ml.VoiceProfile
import com.juliejohnson.voicegenderpavlok.utils.VADManager
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import java.io.ByteArrayOutputStream


class AnalysisActivity : AppCompatActivity() {

    // UI Elements
    private lateinit var pitchTextView: TextView
    private lateinit var confidenceTextView: TextView
    private lateinit var isVoicedTextView: TextView
    private lateinit var pitchMidiTextView: TextView
    private lateinit var noteNameTextView: TextView
//    private lateinit var formant1TextView: TextView
//    private lateinit var formant2TextView: TextView
    private lateinit var startRecButton: Button
    private lateinit var stopRecButton: Button
//    private lateinit var hnrTextView: TextView

    // Analysis Engine
    private lateinit var chaquopyAnalyzer: PitchAnalyzer
    private val analysisScope = CoroutineScope(Dispatchers.Default)

    // Session Recording State
    private var isSessionRecording = false
    private lateinit var audioSessionStream: ByteArrayOutputStream
    private val analysisSessionData = mutableListOf<VoiceProfile>()
    // --- NEW: Add a variable to store the session start time ---
    private var sessionStartTime: Long = 0L

    private val permissionsRequestCode = 101

    private val dispatcher: AudioDispatcher = AudioDispatcherFactory.fromDefaultMicrophone(22050, 1024, 0)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_analysis)

        // Initialize UI
        pitchTextView = findViewById(R.id.pitch_text)
        confidenceTextView = findViewById(R.id.confidence_text)
        isVoicedTextView = findViewById(R.id.is_voiced_text)
        pitchMidiTextView = findViewById(R.id.pitch_midi_text)
        noteNameTextView = findViewById(R.id.note_name_text)
//        formant1TextView = findViewById(R.id.formant1_text)
//        formant2TextView = findViewById(R.id.formant2_text)
        startRecButton = findViewById(R.id.button_start_recording)
        stopRecButton = findViewById(R.id.button_stop_recording)
//        hnrTextView = findViewById(R.id.hnr_text)

//        startRecButton.setOnClickListener { startSessionRecording() }
//        stopRecButton.setOnClickListener { stopSessionRecording() }

        chaquopyAnalyzer = PitchAnalyzer(applicationContext)

//        VADManager.initialize(applicationContext)


        val pdh: PitchDetectionHandler = object : PitchDetectionHandler {
            override fun handlePitch(res: PitchDetectionResult, e: AudioEvent?) {
                val pitchInHz = res.getPitch()
                runOnUiThread(object : Runnable {
                    override fun run() {
                        processPitch(pitchInHz)
                    }
                })
            }
        }
        val pitchProcessor: AudioProcessor =
            PitchProcessor(PitchEstimationAlgorithm.FFT_YIN, 22050f, 1024, pdh)
        dispatcher.addAudioProcessor(pitchProcessor)

        val audioThread: Thread = Thread(dispatcher, "Audio Thread")
        audioThread.start()
    }

    override fun onResume() {
        super.onResume()
        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this, arrayOf(Manifest.permission.RECORD_AUDIO), permissionsRequestCode)
        } else {
//            startVADListening()
        }
    }

    override fun onPause() {
        super.onPause()
//        VADManager.stop()
    }

    private fun processPitch(pitchInHz: Float) {
        runOnUiThread {
            pitchTextView.text = "Pitch: %.2f Hz".format(pitchInHz)
            if(pitchInHz >= 110 && pitchInHz < 123.47) {
                //A
                noteNameTextView.text  ="A"
            }
            else if(pitchInHz >= 123.47 && pitchInHz < 130.81) {
                //B
                noteNameTextView.text  ="B"
            }
            else if(pitchInHz >= 130.81 && pitchInHz < 146.83) {
                //C
                noteNameTextView.text  ="C"
            }
            else if(pitchInHz >= 146.83 && pitchInHz < 164.81) {
                //D
                noteNameTextView.text  ="D"
            }
            else if(pitchInHz >= 164.81 && pitchInHz <= 174.61) {
                //E
                noteNameTextView.text  ="E"
            }
            else if(pitchInHz >= 174.61 && pitchInHz < 185) {
                //F
                noteNameTextView.text  ="F"
            }
            else if(pitchInHz >= 185 && pitchInHz < 196) {
                //G
                noteNameTextView.text  ="G"
            }
        }
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


        val pdh: PitchDetectionHandler = object : PitchDetectionHandler {
            override fun handlePitch(res: PitchDetectionResult, e: AudioEvent?) {
                val pitchInHz = res.getPitch()
                runOnUiThread(object : Runnable {
                    override fun run() {
                        processPitch(pitchInHz)
                    }
                })
            }
        }
        val pitchProcessor: AudioProcessor =
            PitchProcessor(PitchEstimationAlgorithm.FFT_YIN, 22050f, 1024, pdh)
        dispatcher.addAudioProcessor(pitchProcessor)



        //val features = essentiaAnalyzer.analyzeFrame(audioBuffer)
//        val features = chaquopyAnalyzer.analyze(audioBuffer, VADManager.getSampleRate())

//        features?.let {
//
//            if (isSessionRecording) {
//                // --- MODIFIED: Calculate elapsed time relative to the session start ---
//                val elapsedTimeMs = System.currentTimeMillis() - sessionStartTime
//                analysisSessionData.add(
//                    VoiceProfile(
//                        elapsedTimeMs, // Use the new relative timestamp
//                        features.pitch,
//                        features.formants.getOrNull(0) ?: 0f,
//                        features.formants.getOrNull(1) ?: 0f
//                    )
//                )
//            }
//            updateUI(features)
//        }
    }

    private fun updateUI(features: AudioFeatures) {
        // Update UI
        runOnUiThread {
            if (features.confidence >= 0.8) {
                if (features.pitch > 0) {
                    pitchTextView.text = "%.2f Hz".format(features.pitch)
                }

                confidenceTextView.text = "Confidence: %.2f".format(features.confidence)
                isVoicedTextView.text = "Is Voiced?: ${features.isVoiced}"
                pitchMidiTextView.text = "Pitch MIDI: %.1f".format(features.pitchMidi)
                noteNameTextView.text = "Note Name: ${features.noteName}"
            }
//
//            if (features.formants.isNotEmpty()) {
//                formant1TextView.text = "F1: %.0f Hz".format(features.formants[0])
//            }
//
//            if (features.formants.size >= 2) {
//                formant2TextView.text = "F2: %.0f Hz".format(features.formants[1])
//            }

//            // Update the HNR text view with our new, meaningful description
//            if (features.hnr > 0) { // HNR can be negative, so we check if it's a valid calculation
//                // --- NEW: Interpret the HNR value ---
//                val hnrDescription = when {
//                    features.hnr < 15.0f -> "Breathy"
//                    features.hnr < 25.0f -> "Clear"
//                    else -> "Resonant"
//                }
//
//                hnrTextView.text = "Clarity: $hnrDescription (%.1f dB)".format(features.hnr)
//            }
        }
    }

//    private fun startSessionRecording() {
//        // --- NEW: Capture the start time when recording begins ---
//        sessionStartTime = System.currentTimeMillis()
//        audioSessionStream = ByteArrayOutputStream()
//        analysisSessionData.clear()
//        isSessionRecording = true
//        startRecButton.isEnabled = false
//        stopRecButton.isEnabled = true
//        Toast.makeText(this, "Session recording started...", Toast.LENGTH_SHORT).show()
//    }
//
//    private fun stopSessionRecording() {
//        if (!isSessionRecording) return
//        isSessionRecording = false
//        startRecButton.isEnabled = true
//        stopRecButton.isEnabled = false
//
//        val sampleRate = VADManager.getSampleRate()
//        SessionStorage.saveSession(audioSessionStream.toByteArray(), analysisSessionData, sampleRate)
//        Toast.makeText(this, "Session saved to Downloads folder.", Toast.LENGTH_LONG).show()
//    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == permissionsRequestCode && grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
            startVADListening()
        }
    }
}