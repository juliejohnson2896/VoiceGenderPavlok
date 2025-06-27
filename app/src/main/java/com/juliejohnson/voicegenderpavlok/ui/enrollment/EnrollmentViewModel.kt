package com.juliejohnson.voicegenderpavlok.ui.enrollment

import android.Manifest
import android.content.Context
import android.util.Log
import androidx.annotation.RequiresPermission
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.juliejohnson.voicegenderpavlok.ml.AudioBuffer
import com.juliejohnson.voicegenderpavlok.ml.Gender
import com.juliejohnson.voicegenderpavlok.ml.MLUtils
import com.juliejohnson.voicegenderpavlok.ml.VoiceProfile
import com.juliejohnson.voicegenderpavlok.storage.EnrollmentStorage
import com.juliejohnson.voicegenderpavlok.ui.EnrollmentUiState
import com.juliejohnson.voicegenderpavlok.utils.VADRecorder
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class EnrollmentViewModel : ViewModel() {

    private val MIN_SAMPLES = 1
    private val MAX_SAMPLES = 20

    private val _uiState = MutableStateFlow(EnrollmentUiState())
    val uiState: StateFlow<EnrollmentUiState> = _uiState.asStateFlow()

    // CORRECTED: VADRecorder is no longer a property of the ViewModel.
    // It will be created on-demand for each recording to ensure a fresh state.

    fun initialize(context: Context) {
        MLUtils.initialize(context)
        EnrollmentStorage.initialize(context)
    }

    @RequiresPermission(Manifest.permission.RECORD_AUDIO)
    fun startEnrollmentFlow(context: Context, voiceProfile: VoiceProfile) {
        viewModelScope.launch {
            for (sampleNumber in MIN_SAMPLES..MAX_SAMPLES) {
                // Pass context down to the recording function so it can create the recorder.
                val audioBuffer = recordSingleSample(context, sampleNumber)

                if (audioBuffer != null) {
                    _uiState.update { it.copy(statusText = "Processing sample $sampleNumber...") }
                    val success = withContext(Dispatchers.IO) {
                        processAndSaveSample(context, audioBuffer, voiceProfile)
                    }
                    if (!success) {
                        _uiState.update {
                            it.copy(
                                finalResult = "Error processing sample $sampleNumber. Please try again.",
                                isButtonEnabled = true
                            )
                        }
                        return@launch
                    }
                    _uiState.update { it.copy(samplesCollected = sampleNumber) }
                } else {
                    _uiState.update {
                        it.copy(
                            finalResult = "Sample $sampleNumber failed to record. Please try again.",
                            isButtonEnabled = true
                        )
                    }
                    return@launch
                }
            }

            _uiState.update {
                it.copy(
                    finalResult = "Enrollment Complete!",
                    isButtonEnabled = true
                )
            }
        }
    }

    @RequiresPermission(Manifest.permission.RECORD_AUDIO)
    private suspend fun recordSingleSample(context: Context, sampleNumber: Int): AudioBuffer? {
        _uiState.update {
            it.copy(
                statusText = "Say something for sample $sampleNumber...",
                isButtonEnabled = false,
                isRecording = true,
                finalResult = null
            )
        }

        // --- CORRECTED LOGIC ---
        // Create a new VADRecorder instance here. This is the key to resetting the state.
        val vadRecorder = VADRecorder(context)
        // -----------------------

        val buffer = vadRecorder.recordUntilSpeechDetected(
            onAmplitude = { amplitude ->
                _uiState.update { it.copy(latestAmplitude = amplitude) }
            }
        )

        _uiState.update { it.copy(isRecording = false) }
        return buffer
    }

    private fun processAndSaveSample(context: Context, buffer: AudioBuffer, voiceProfile: VoiceProfile): Boolean {
        return try {
            val embedding = MLUtils.generateEmbedding(buffer)
            // Pass the profile tag to the updated saveSample function
            EnrollmentStorage.saveSample(context, buffer.samples, embedding, "Untitled Sample", voiceProfile, false)
            true
        } catch (e: Exception) {
            false
        }
    }
}