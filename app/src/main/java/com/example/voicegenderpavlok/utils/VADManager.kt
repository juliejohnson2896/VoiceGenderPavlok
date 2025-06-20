package com.example.voicegenderpavlok.utils

import android.content.Context
import kotlinx.coroutines.flow.StateFlow

object VADManager {
    private var vadUtils: VADUtils? = null

    val vadStatus: StateFlow<Boolean>
        get() = vadUtils?.vadStatus ?: error("VADUtils not initialized")

    fun initialize(context: Context): Boolean {
        if (vadUtils == null) {
            vadUtils = VADUtils(context.applicationContext)
        }
        return vadUtils?.initializeAudioRecord() ?: false
    }

    fun startListening(onSpeechDetected: () -> Unit) {
        vadUtils?.startListening(onSpeechDetected)
    }

    fun getRecentAudio(): FloatArray {
        return vadUtils?.getRecentAudio() ?: FloatArray(0)
    }

    fun stop() {
        vadUtils?.stop()
        vadUtils = null
    }
}
