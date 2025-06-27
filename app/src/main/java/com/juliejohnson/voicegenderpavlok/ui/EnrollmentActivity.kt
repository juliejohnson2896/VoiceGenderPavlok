package com.juliejohnson.voicegenderpavlok.ui

import android.os.Bundle
import androidx.activity.viewModels
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import com.juliejohnson.voicegenderpavlok.databinding.ActivityEnrollmentBinding
import com.juliejohnson.voicegenderpavlok.ml.VoiceProfile
import com.juliejohnson.voicegenderpavlok.ui.enrollment.EnrollmentViewModel
import kotlinx.coroutines.launch

class EnrollmentActivity : AppCompatActivity() {

    private lateinit var binding: ActivityEnrollmentBinding
    private val viewModel: EnrollmentViewModel by viewModels()

    @androidx.annotation.RequiresPermission(android.Manifest.permission.RECORD_AUDIO)
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityEnrollmentBinding.inflate(layoutInflater)
        setContentView(binding.root)

        // Prepare the ViewModel and its components
        viewModel.initialize(this)

        val voiceProfile = VoiceProfile()

        binding.recordMasculineButton.setOnClickListener {
            viewModel.startEnrollmentFlow(this, voiceProfile)
        }
        binding.recordFeminineButton.setOnClickListener {
            viewModel.startEnrollmentFlow(this, voiceProfile)
        }
        binding.recordAndrogynousButton.setOnClickListener {
            viewModel.startEnrollmentFlow(this, voiceProfile)
        }

        // This coroutine observes the UI state and updates the screen
        lifecycleScope.launch {
            repeatOnLifecycle(Lifecycle.State.STARTED) {
                viewModel.uiState.collect { state ->
                    binding.statusText.text = state.statusText
                    binding.recordFeminineButton.isEnabled = state.isButtonEnabled
                    binding.recordMasculineButton.isEnabled = state.isButtonEnabled
                    binding.recordAndrogynousButton.isEnabled = state.isButtonEnabled

                    // Only update the waveform when actively recording
                    if (state.isRecording) {
                        binding.waveformView.addAmplitude(state.latestAmplitude)
                    }

                    // Handle the final result message
                    state.finalResult?.let {
                        binding.statusText.text = it
                        // You could also show a Toast or navigate away
                    }
                }
            }
        }
    }
}