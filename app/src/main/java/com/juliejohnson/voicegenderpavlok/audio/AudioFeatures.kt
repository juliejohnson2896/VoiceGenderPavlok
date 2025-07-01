package com.juliejohnson.voicegenderpavlok.audio

data class AudioFeatures(
    val pitch: Float = 0f,              // Fundamental frequency in Hz
    val brightness: Float = 0f,         // Spectral brightness (0-1)
    val resonance: Float = 0f,          // Vocal tract resonance
    val centroid: Float = 0f,   // centroid in Hz
    val mfcc: FloatArray = floatArrayOf(), // MFCC coefficients
    val formants: FloatArray = floatArrayOf(), // Formant frequencies
    val hnr: Float = 0f,                // Harmonic-to-noise ratio
    val isValid: Boolean = false        // Whether analysis was successful
) {
    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (javaClass != other?.javaClass) return false

        other as AudioFeatures

        if (pitch != other.pitch) return false
        if (brightness != other.brightness) return false
        if (resonance != other.resonance) return false
        if (centroid != other.centroid) return false
        if (!mfcc.contentEquals(other.mfcc)) return false
        if (!formants.contentEquals(other.formants)) return false
        if (hnr != other.hnr) return false
        if (isValid != other.isValid) return false

        return true
    }

    override fun hashCode(): Int {
        var result = pitch.hashCode()
        result = 31 * result + brightness.hashCode()
        result = 31 * result + resonance.hashCode()
        result = 31 * result + centroid.hashCode()
        result = 31 * result + mfcc.contentHashCode()
        result = 31 * result + formants.contentHashCode()
        result = 31 * result + hnr.hashCode()
        result = 31 * result + isValid.hashCode()
        return result
    }
}

