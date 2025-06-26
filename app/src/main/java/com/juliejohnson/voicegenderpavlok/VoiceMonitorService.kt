package com.juliejohnson.voicegenderpavlok

import android.app.*
import android.content.Intent
import android.os.Build
import android.os.IBinder
import android.util.Log
import androidx.core.app.NotificationCompat
import com.juliejohnson.voicegenderpavlok.ml.AudioBuffer
import com.juliejohnson.voicegenderpavlok.ml.Gender
import com.juliejohnson.voicegenderpavlok.ml.MLUtils
import com.juliejohnson.voicegenderpavlok.network.RetrofitClient
import com.juliejohnson.voicegenderpavlok.utils.*
import kotlinx.coroutines.*

class VoiceMonitorService : Service() {

    private val serviceScope = CoroutineScope(Dispatchers.Default + SupervisorJob())

    private var lastTriggered = 0L
    private val cooldownMs = 5000L

    override fun onCreate() {
        super.onCreate()

        createNotificationChannel()
        startForeground(1, buildNotification())

        if (VADManager.initialize(applicationContext)) {
            VADManager.startListening {
                onSpeechDetected()
            }
        } else {
            Log.e("VoiceMonitorService", "Failed to initialize audio input for VAD.")
            stopSelf()
        }
    }

    private fun onSpeechDetected() {
        val now = System.currentTimeMillis()
        if (now - lastTriggered < cooldownMs) {
            Log.d("VoiceMonitorService", "Cooldown active, skipping trigger.")
            return
        }
        lastTriggered = now

        serviceScope.launch {
            val audio = VADManager.getRecentAudio()
            val buffer = AudioBuffer(audio, 16000, 1)

            try {
                if (MLUtils.verifySpeaker(applicationContext, buffer)) {
                    val gender = MLUtils.classifyGender(MLUtils.generateEmbedding(buffer))
                    if (gender == Gender.MALE) {
                        Log.d("VoiceMonitorService", "Triggering shock for male speaker.")
                        val token = AuthUtils.getAuthToken(applicationContext)
                        RetrofitClient.instance.triggerShock("Bearer $token")
                    } else {
                        Log.d("VoiceMonitorService", "Speaker is not male.")
                    }
                } else {
                    Log.d("VoiceMonitorService", "Speaker not verified.")
                }
            } catch (e: Exception) {
                Log.e("VoiceMonitorService", "Error during speech processing: ${e.message}")
            }
        }
    }

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                "VoiceMonitor",
                "Voice Monitor",
                NotificationManager.IMPORTANCE_LOW
            )
            getSystemService(NotificationManager::class.java)
                .createNotificationChannel(channel)
        }
    }

    private fun buildNotification(): Notification {
        return NotificationCompat.Builder(this, "VoiceMonitor")
            .setContentTitle("Voice Monitoring Active")
            .setSmallIcon(R.drawable.ic_mic)
            .setOngoing(true)
            .build()
    }

    override fun onDestroy() {
        super.onDestroy()
        VADManager.stop()
        serviceScope.cancel()
    }

    override fun onBind(intent: Intent?): IBinder? = null
}
