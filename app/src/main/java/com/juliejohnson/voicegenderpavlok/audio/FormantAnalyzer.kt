package com.juliejohnson.voicegenderpavlok.audio

object FormantAnalyzer {

    // Load the native library when this object is first accessed.
    // The library name must match the one defined in your CMakeLists.txt.
    init {
        System.loadLibrary("essentia_wrapper")
    }

    /**
     * Calls the native C++ function to perform formant analysis on an audio buffer.
     *
     * @param audioData The raw audio samples.
     * @param sampleRate The sample rate of the audio data.
     * @param formantCeiling The maximum frequency to search for formants (e.g., 5500.0 for female voice).
     * @param numFormants The number of formants to find.
     * @param windowLength The duration of the analysis window in seconds (e.g., 0.025).
     * @param timeStep The time step between analysis frames in seconds.
     * @param preEmphasisFreq The pre-emphasis cutoff frequency (e.g., 50.0).
     * @return An array where each element is an array of formants found in that analysis frame.
     */
    external fun analyze(
        audioData: FloatArray,
        sampleRate: Double,
        formantCeiling: Double,
        numFormants: Int,
        windowLength: Double,
        preEmphasisFreq: Double
    ): Array<Array<Formant>>
}