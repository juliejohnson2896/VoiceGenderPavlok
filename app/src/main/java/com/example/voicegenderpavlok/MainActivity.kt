package com.example.voicegenderpavlok

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.widget.Button
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import com.example.voicegenderpavlok.ml.MLUtils
import com.example.voicegenderpavlok.storage.EnrollmentStorage
import com.example.voicegenderpavlok.ui.EnrollmentActivity
import com.example.voicegenderpavlok.ui.enrollment.EnrollmentHistoryActivity
import com.example.voicegenderpavlok.ui.SettingsActivity
import com.example.voicegenderpavlok.ui.SpeakerTestActivity
import com.example.voicegenderpavlok.utils.*
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch

class MainActivity : AppCompatActivity() {

    private lateinit var startButton: Button
    private lateinit var stopButton: Button
    private lateinit var settingsButton: Button
    private lateinit var enrollmentButton: Button
    private lateinit var logsButton: Button
    private lateinit var permissionStatusText: TextView
    private lateinit var vadRunningText: TextView
    private lateinit var vadStatusText: TextView
    private lateinit var speakerTestButton: Button

    private val micPermissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestPermission()
    ) { isGranted ->
        if (isGranted) {
            startMonitoringService()
        } else {
            Toast.makeText(this, "Microphone permission denied", Toast.LENGTH_SHORT).show()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        EnrollmentStorage.initialize(applicationContext)
        MLUtils.initialize(this)

        startButton = findViewById(R.id.button_start)
        stopButton = findViewById(R.id.button_stop)
        settingsButton = findViewById(R.id.button_settings)
        enrollmentButton = findViewById(R.id.button_enroll)
        logsButton = findViewById(R.id.button_logs)
        vadStatusText = findViewById(R.id.text_vad_status)
        speakerTestButton = findViewById<Button>(R.id.open_speaker_test_button)

        startButton.setOnClickListener {
            checkMicPermissionAndStart()
        }

        stopButton.setOnClickListener {
            stopService(Intent(this, VoiceMonitorService::class.java))
            Toast.makeText(this, "Monitoring stopped", Toast.LENGTH_SHORT).show()

            // ✅ Update VAD + mic UI status
            updateMicPermissionStatus()
        }

        settingsButton.setOnClickListener {
            startActivity(Intent(this, SettingsActivity::class.java))
        }

        enrollmentButton.setOnClickListener {
            startActivity(Intent(this, EnrollmentActivity::class.java))
        }

        logsButton.setOnClickListener {
            startActivity(Intent(this, EnrollmentHistoryActivity::class.java))
        }

        speakerTestButton.setOnClickListener {
            val intent = Intent(this, SpeakerTestActivity::class.java)
            startActivity(intent)
        }

        permissionStatusText = findViewById(R.id.text_permission_status)
        vadRunningText = findViewById(R.id.text_vad_running)
        vadStatusText = findViewById(R.id.text_vad_status)

        updateMicPermissionStatus()
        observeVADStatus()
    }

    private fun checkMicPermissionAndStart() {
        when {
            ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED -> {
                startMonitoringService()
            }
            else -> {
                micPermissionLauncher.launch(Manifest.permission.RECORD_AUDIO)
            }
        }
    }

    private fun startMonitoringService() {
        val intent = Intent(this, VoiceMonitorService::class.java)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            startForegroundService(intent)
        } else {
            startService(intent)
        }
        Toast.makeText(this, "Monitoring started", Toast.LENGTH_SHORT).show()

        // ✅ Update VAD + mic UI status
        updateMicPermissionStatus()
    }

    private fun updateMicPermissionStatus() {
        val hasPermission = ContextCompat.checkSelfPermission(
            this,
            Manifest.permission.RECORD_AUDIO
        ) == PackageManager.PERMISSION_GRANTED

        permissionStatusText.text = if (hasPermission) {
            "Mic Permission: Granted ✅"
        } else {
            "Mic Permission: Denied ❌"
        }

        vadRunningText.text = if (VADManagerInitialized()) {
            "VAD Status: Active 🎤"
        } else {
            "VAD Status: Idle ⏹"
        }
    }

    private fun VADManagerInitialized(): Boolean {
        return try {
            VADManager.vadStatus.value != null
        } catch (e: Exception) {
            false
        }
    }

    private fun observeVADStatus() {
        lifecycleScope.launch {
            try {
                VADManager.vadStatus.collectLatest { isSpeaking ->
                    runOnUiThread {
                        vadStatusText.text = if (isSpeaking) "Speech: Detected 🗣️" else "Speech: None 🤐"
                    }
                }
            } catch (e: Exception) {
                vadStatusText.text = "Speech: N/A (VAD not running)"
            }
        }
    }

}
