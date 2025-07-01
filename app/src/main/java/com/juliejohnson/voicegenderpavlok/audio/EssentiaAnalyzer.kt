package com.juliejohnson.voicegenderpavlok.audio

/**
 * JNI wrapper for Essentia audio analysis library
 */
class EssentiaAnalyzer {

    companion object {
        init {
            System.loadLibrary("essentia_wrapper")
        }

        private const val TAG = "EssentiaAnalyzer"
    }

    private var isInitialized = false

    /**
     * Initialize Essentia library and algorithms
     * Call this once before using analysis functions
     */
    fun initialize(sampleRate: Int = 44100): Boolean {
        isInitialized = nativeInitialize(sampleRate)
        return isInitialized
    }

    /**
     * Analyze audio frame and extract features
     * @param audioData Float array containing audio samples
     * @param frameSize Size of the audio frame (should match Essentia requirements)
     * @return AudioFeatures object with extracted features
     */
    fun analyzeFrame(audioData: FloatArray, frameSize: Int = 1024): AudioFeatures? {
        if (!isInitialized) {
            throw IllegalStateException("EssentiaAnalyzer not initialized. Call initialize() first.")
        }

        if (audioData.size < frameSize) {
            return null
        }

        return nativeAnalyzeFrame(audioData, frameSize)
    }

    /**
     * Analyze audio buffer with automatic windowing
     * @param audioBuffer Complete audio buffer
     * @param hopSize Hop size for windowing
     * @return List of AudioFeatures for each frame
     */
    fun analyzeBuffer(audioBuffer: FloatArray, hopSize: Int = 512): List<AudioFeatures> {
        if (!isInitialized) {
            throw IllegalStateException("EssentiaAnalyzer not initialized. Call initialize() first.")
        }

        return nativeAnalyzeBuffer(audioBuffer, hopSize).filterNotNull()
    }

    /**
     * Clean up resources
     * Call this when done with analysis
     */
    fun cleanup() {
        if (isInitialized) {
            nativeCleanup()
            isInitialized = false
        }
    }

    /**
     * Check if analyzer is ready for use
     */
    fun isReady(): Boolean = isInitialized

    // Native method declarations
    private external fun nativeInitialize(sampleRate: Int): Boolean
    private external fun nativeAnalyzeFrame(audioData: FloatArray, frameSize: Int): AudioFeatures?
    private external fun nativeAnalyzeBuffer(audioBuffer: FloatArray, hopSize: Int): Array<AudioFeatures?>
    private external fun nativeCleanup()
}