#ifndef ESSENTIA_WRAPPER_H
#define ESSENTIA_WRAPPER_H

#include <vector>
#include <memory>

// Forward declarations for Essentia classes
namespace essentia {
    namespace standard {
        class Algorithm;
    }
    namespace streaming {
        class Algorithm;
    }
}

/**
 * Struct to hold extracted audio features
 */
struct AudioFeatures {
    float pitch = 0.0f;
    float brightness = 0.0f;
    float resonance = 0.0f;
    float centroid  = 0.0f;
    std::vector<float> mfcc;
    std::vector<float> formants;
    bool isValid = false;

    AudioFeatures() = default;

    AudioFeatures(float p, float b, float r, float sc, const std::vector<float>& m, const std::vector<float>& f, bool valid)
            : pitch(p), brightness(b), resonance(r), centroid (sc), mfcc(m), formants(f), isValid(valid) {}
};

/**
 * Wrapper class for Essentia audio analysis
 */
class EssentiaWrapper {
private:
    // Essentia algorithms
    std::unique_ptr<essentia::standard::Algorithm> energyAlg;
    std::unique_ptr<essentia::standard::Algorithm> pitchYin;
    std::unique_ptr<essentia::standard::Algorithm> centroidAlg;
    std::unique_ptr<essentia::standard::Algorithm> mfccAlg;
    std::unique_ptr<essentia::standard::Algorithm> windowAlg;
    std::unique_ptr<essentia::standard::Algorithm> spectrumAlg;
    std::unique_ptr<essentia::standard::Algorithm> spectralPeaksAlg;
    std::unique_ptr<essentia::standard::Algorithm> lpcAlg;

    // Analysis parameters
    int sampleRate;
    int frameSize;
    int hopSize;
    bool initialized;

    // Helper methods
    float calculateBrightness(const std::vector<float>& spectrum);
    float calculateResonance(const std::vector<float>& spectrum, float pitch);
    std::vector<float> calculateFormants(std::vector<float> lpcCoeffs);
    std::vector<float> preprocessAudio(const float* audioData, int length);

public:
    EssentiaWrapper();
    ~EssentiaWrapper();

    /**
     * Initialize Essentia and create algorithms
     */
    bool initialize(int sampleRate = 44100, int frameSize = 1024, int hopSize = 512);

    /**
     * Analyze a single audio frame
     */
    AudioFeatures analyzeFrame(const float* audioData, int length);

    /**
     * Analyze audio buffer with windowing
     */
    std::vector<AudioFeatures> analyzeBuffer(const float* audioBuffer, int bufferLength, int hopSize);

    /**
     * Clean up resources
     */
    void cleanup();

    /**
     * Check if wrapper is initialized
     */
    bool isReady() const { return initialized; }

    /**
     * Get current sample rate
     */
    int getSampleRate() const { return sampleRate; }

    /**
     * Get current frame size
     */
    int getFrameSize() const { return frameSize; }
};

// Global instance for JNI access
extern std::unique_ptr<EssentiaWrapper> g_essentiaWrapper;

// C-style functions for JNI
extern "C" {
bool initEssentia(int sampleRate);
AudioFeatures analyzeAudioFrame(const float* audioData, int length);
std::vector<AudioFeatures> analyzeAudioBuffer(const float* audioBuffer, int bufferLength, int hopSize);
void cleanupEssentia();
}

#endif // ESSENTIA_WRAPPER_H