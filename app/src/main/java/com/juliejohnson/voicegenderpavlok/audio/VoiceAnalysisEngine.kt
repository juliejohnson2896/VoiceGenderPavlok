package com.juliejohnson.voicegenderpavlok.audio

object VoiceAnalysisEngine {
    init {
        System.loadLibrary("voice_analysis_engine")
    }

    /**
     * Compute pitch in Hz for the given PCM float buffer.
     * @param audioBuffer PCM samples (e.g. normalized -1..1)
     * @param sampleRate sample rate in Hz (e.g. 44100)
     * @return pitch in Hz (0.0 if unvoiced or on error)
     */
    external fun getPitch(audioBuffer: FloatArray, sampleRate: Int): Float
}