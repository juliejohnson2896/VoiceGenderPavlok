package com.juliejohnson.voicegenderpavlok.ui

import com.juliejohnson.voicegenderpavlok.ml.AudioBuffer

data class EnrollmentUiState(
    val statusText: String = "Press button to begin enrollment",
    val isButtonEnabled: Boolean = true,
    val isRecording: Boolean = false,
    val latestAmplitude: Float = 0f,
    val samplesCollected: Int = 0,
    val finalResult: String? = null // To show "Enrollment Complete" or "Failed"
)