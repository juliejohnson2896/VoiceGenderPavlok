package com.juliejohnson.voicegenderpavlok.ui

import android.Manifest
import android.content.pm.PackageManager
import android.os.Bundle
import android.view.View
import android.widget.Button
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import be.tarsos.dsp.AudioDispatcher
import be.tarsos.dsp.AudioEvent
import be.tarsos.dsp.AudioProcessor
import be.tarsos.dsp.io.android.AudioDispatcherFactory
import be.tarsos.dsp.pitch.PitchDetectionHandler
import be.tarsos.dsp.pitch.PitchDetectionResult
import be.tarsos.dsp.pitch.PitchProcessor
import com.juliejohnson.voicegenderpavlok.R
import kotlin.math.ln

class PitchDetectionActivity : AppCompatActivity() {
    private var dispatcher: AudioDispatcher? = null
    private var audioThread: Thread? = null
    private var isListening = false

    private var pitchTextView: TextView? = null
    private var frequencyTextView: TextView? = null
    private var noteTextView: TextView? = null
    private var startStopButton: Button? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_pitch_detection)

        initializeViews()
        setupClickListeners()


        // Check for microphone permission
        if (checkMicrophonePermission()) {
            setupAudioProcessing()
        } else {
            requestMicrophonePermission()
        }
    }

    private fun initializeViews() {
        pitchTextView = findViewById<TextView?>(R.id.pitch_text_view)
        frequencyTextView = findViewById<TextView?>(R.id.frequency_text_view)
        noteTextView = findViewById<TextView?>(R.id.note_text_view)
        startStopButton = findViewById<Button?>(R.id.start_stop_button)


        // Initialize with default values
        pitchTextView!!.setText("Pitch: --")
        frequencyTextView!!.setText("Frequency: -- Hz")
        noteTextView!!.setText("Note: --")
        startStopButton!!.setText("Start Listening")
    }

    private fun setupClickListeners() {
        startStopButton!!.setOnClickListener(View.OnClickListener { v: View? ->
            if (isListening) {
                stopListening()
            } else {
                startListening()
            }
        })
    }

    private fun checkMicrophonePermission(): Boolean {
        return (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO)
                == PackageManager.PERMISSION_GRANTED)
    }

    private fun requestMicrophonePermission() {
        ActivityCompat.requestPermissions(
            this,
            arrayOf<String>(Manifest.permission.RECORD_AUDIO),
            PERMISSION_REQUEST_CODE
        )
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(
            requestCode,
            permissions as Array<out String>,
            grantResults
        )

        if (requestCode == PERMISSION_REQUEST_CODE) {
            if (grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                setupAudioProcessing()
            } else {
                Toast.makeText(
                    this, "Microphone permission required for pitch detection",
                    Toast.LENGTH_LONG
                ).show()
            }
        }
    }

    private fun setupAudioProcessing() {
        // Create pitch detection handler
        val pdh: PitchDetectionHandler = object : PitchDetectionHandler {
            override fun handlePitch(result: PitchDetectionResult, e: AudioEvent?) {
                val pitchInHz = result.getPitch()
                val probability = result.getProbability()

//                if (probability > 0 && e != null && e.floatBuffer != null) {
//                    val formantFrames: Array<Array<Formant>> = FormantAnalyzer.analyze(
//                        audioData = e.floatBuffer,
//                        sampleRate = SAMPLE_RATE.toDouble(),
//                        formantCeiling = 5500.0,
//                        numFormants = 5,
//                        windowLength = 0.025,
//                        preEmphasisFreq = 50.0
//                    )
//
////                    // Process the results
////                    formantFrames.forEachIndexed { frameIndex, formantsInFrame ->
////                        println("--- Frame ${frameIndex + 1} ---")
////                        formantsInFrame.forEach { formant ->
////                            println("  Frequency: ${"%.2f".format(formant.frequencyHz)} Hz, Bandwidth: ${"%.2f".format(formant.bandwidthHz)} Hz")
////                        }
////                    }
//
//                    // Now, iterate and find the stable vowel segments
//                    for (frame in formantFrames) {
//                        // Is this frame a candidate for a vowel?
//                        // It should contain at least two formants (F1 and F2).
//                        if (frame.size >= 2) {
//                            val f1 = frame[0].frequencyHz
//                            val f2 = frame[1].frequencyHz
//
//                            // Here you would add logic to see if f1 and f2 are stable
//                            // compared to the previous valid frame.
//                            // For now, we'll just print them.
//                            println("Potential Vowel Frame - F1: ${"%.0f".format(f1)} Hz, F2: ${"%.0f".format(f2)} Hz")
//                        } else {
//                            // This is likely a consonant or silence. Ignore it.
//                            println("Non-Vowel Frame (Silence/Consonant)")
//                        }
//                    }
//                }

                runOnUiThread(Runnable {
                    if (pitchInHz != -1f && probability > 0.9) {
                        // Update pitch display
                        pitchTextView!!.setText(String.format("Pitch: %.2f", pitchInHz))
                        frequencyTextView!!.setText(String.format("Frequency: %.2f Hz", pitchInHz))


                        // Convert frequency to musical note
                        val note = frequencyToNote(pitchInHz)
                        noteTextView!!.setText("Note: " + note)


                        // Change text color based on pitch detection confidence
                        if (probability > 0.95) {
                            pitchTextView!!.setTextColor(getResources().getColor(R.color.holo_green_dark))
                        } else {
                            pitchTextView!!.setTextColor(getResources().getColor(R.color.holo_orange_dark))
                        }
                    } else {
                        // No clear pitch detected
                        pitchTextView!!.setText("Pitch: --")
                        pitchTextView!!.setTextColor(getResources().getColor(R.color.darker_gray))
                        frequencyTextView!!.setText("Frequency: -- Hz")
                        noteTextView!!.setText("Note: --")
                    }
                })
            }
        }


        // Create audio dispatcher from microphone
        dispatcher = AudioDispatcherFactory.fromDefaultMicrophone(SAMPLE_RATE, BUFFER_SIZE, 0)


        // Create pitch processor with YIN algorithm
        val pitchProcessor: AudioProcessor = PitchProcessor(
            PitchProcessor.PitchEstimationAlgorithm.YIN,
            SAMPLE_RATE.toFloat(),
            BUFFER_SIZE,
            pdh
        )

        dispatcher!!.addAudioProcessor(pitchProcessor)
    }

    private fun startListening() {
        if (dispatcher != null && !isListening) {
            isListening = true
            startStopButton!!.setText("Stop Listening")

            audioThread = Thread(dispatcher, "Audio Thread")
            audioThread!!.start()
        }
    }

    private fun stopListening() {
        if (dispatcher != null && isListening) {
            isListening = false
            startStopButton!!.setText("Start Listening")

            dispatcher!!.stop()


            // Reset display
            pitchTextView!!.setText("Pitch: --")
            pitchTextView!!.setTextColor(getResources().getColor(R.color.darker_gray))
            frequencyTextView!!.setText("Frequency: -- Hz")
            noteTextView!!.setText("Note: --")
        }
    }

    private fun frequencyToNote(frequency: Float): String {
        // Standard musical note frequencies (A4 = 440 Hz)
        val noteNames =
            arrayOf<String>("C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B")


        // Calculate the number of semitones from A4
        val semitones = 12 * ln(frequency / 440.0) / ln(2.0)
        var noteIndex = Math.round(semitones).toInt() % 12


        // Handle negative indices
        if (noteIndex < 0) {
            noteIndex += 12
        }


        // Calculate octave
        val octave = 4 + Math.round(semitones).toInt() / 12

        return noteNames[noteIndex] + octave
    }

    override fun onDestroy() {
        super.onDestroy()
        stopListening()
    }

    override fun onPause() {
        super.onPause()
        stopListening()
    }

    companion object {
        private const val PERMISSION_REQUEST_CODE = 1000
        private const val SAMPLE_RATE = 22050
        private const val BUFFER_SIZE = 1024
    }
}