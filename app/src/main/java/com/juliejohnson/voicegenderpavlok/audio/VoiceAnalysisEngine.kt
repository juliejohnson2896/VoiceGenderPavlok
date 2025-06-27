package com.juliejohnson.voicegenderpavlok.audio

import android.util.Log

object VoiceAnalysisEngine {

    init {
        try {
            System.loadLibrary("voice_analysis_engine")
            Log.d("VoiceAnalysisEngine", "Native Essentia library loaded.")
        } catch (e: UnsatisfiedLinkError) {
            Log.e("VoiceAnalysisEngine", "Failed to load native Essentia library", e)
        }
    }

    // --- NEW: Declare all our JNI functions ---
    external fun initialize(sampleRate: Int, frameSize: Int)
    external fun shutdown()
    external fun getPitch(audioBuffer: FloatArray): Float

    // We will add more analysis functions here later (e.g., getFormants)
}