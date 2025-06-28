#include "essentia_wrapper.h"
#include <android/log.h>
#include <memory>
#include <algorithm>
#include <cmath>

// Include Essentia headers
#include <essentia/essentia.h>
#include <essentia/algorithmfactory.h>
#include <essentia/essentiamath.h>
#include <essentia/pool.h>

#define LOG_TAG "EssentiaWrapper"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

using namespace essentia;
using namespace essentia::standard;

// Global instance
std::unique_ptr<EssentiaWrapper> g_essentiaWrapper = nullptr;

EssentiaWrapper::EssentiaWrapper()
        : sampleRate(44100)
        , frameSize(1024)
        , hopSize(512)
        , initialized(false) {
}

EssentiaWrapper::~EssentiaWrapper() {
    cleanup();
}

bool EssentiaWrapper::initialize(int sr, int fs, int hs) {
    if (initialized) {
        LOGD("EssentiaWrapper already initialized");
        return true;
    }

    try {
        LOGI("Initializing Essentia with sampleRate=%d, frameSize=%d, hopSize=%d", sr, fs, hs);

        // Initialize Essentia
        essentia::init();

        sampleRate = sr;
        frameSize = fs;
        hopSize = hs;

        // Create algorithm factory
        AlgorithmFactory& factory = AlgorithmFactory::instance();

        // Initialize algorithms
        pitchYin.reset(factory.create("PitchYin",
                                      "frameSize", frameSize,
                                      "sampleRate", sampleRate));

        centroidAlg.reset(factory.create("Centroid"));

        mfccAlg.reset(factory.create("MFCC",
                                     "inputSize", frameSize/2 + 1,
                                     "numberCoefficients", 13));

        windowAlg.reset(factory.create("Windowing",
                                       "type", "hann"));

        spectrumAlg.reset(factory.create("Spectrum"));

        spectralPeaksAlg.reset(factory.create("SpectralPeaks",
                                              "magnitudeThreshold", 0.00001,
                                              "minFrequency", 40,
                                              "maxFrequency", sampleRate/2,
                                              "maxPeaks", 100));

        initialized = true;
        LOGI("Essentia initialization completed successfully");
        return true;

    } catch (const EssentiaException& e) {
        LOGE("Essentia exception during initialization: %s", e.what());
        cleanup();
        return false;
    } catch (const std::exception& e) {
        LOGE("Standard exception during initialization: %s", e.what());
        cleanup();
        return false;
    } catch (...) {
        LOGE("Unknown exception during initialization");
        cleanup();
        return false;
    }
}

AudioFeatures EssentiaWrapper::analyzeFrame(const float* audioData, int length) {
    if (!initialized) {
        LOGE("EssentiaWrapper not initialized");
        return AudioFeatures();
    }

    if (audioData == nullptr || length < frameSize) {
        LOGE("Invalid audio data: data=%p, length=%d, required=%d", audioData, length, frameSize);
        return AudioFeatures();
    }

    try {
        // Preprocess audio data
        std::vector<float> audioFrame = preprocessAudio(audioData, std::min(length, frameSize));

        AudioFeatures features;

        // Apply windowing
        std::vector<float> windowedFrame;
        windowAlg->input("frame").set(audioFrame);
        windowAlg->output("frame").set(windowedFrame);
        windowAlg->compute();

        // Compute spectrum
        std::vector<float> spectrum;
        spectrumAlg->input("frame").set(windowedFrame);
        spectrumAlg->output("spectrum").set(spectrum);
        spectrumAlg->compute();

        // Extract pitch using YIN algorithm
        float pitch, pitchConfidence;
        pitchYin->input("signal").set(audioFrame);
        pitchYin->output("pitch").set(pitch);
        pitchYin->output("pitchConfidence").set(pitchConfidence);
        pitchYin->compute();

        // Only use pitch if confidence is reasonable
        features.pitch = (pitchConfidence > 0.8) ? pitch : 0.0f;

        // Compute spectral centroid
        float centroid;
        centroidAlg->input("array").set(spectrum);
        centroidAlg->output("centroid").set(centroid);
        centroidAlg->compute();
        features.centroid = centroid;

        // Compute MFCC
        std::vector<float> mfccBands, mfccCoeffs;
        mfccAlg->input("spectrum").set(spectrum);
        mfccAlg->output("bands").set(mfccBands);
        mfccAlg->output("mfcc").set(mfccCoeffs);
        mfccAlg->compute();
        features.mfcc = mfccCoeffs;

        // Calculate brightness (high frequency energy ratio)
        features.brightness = calculateBrightness(spectrum);

        // Calculate resonance (simplified)
        features.resonance = calculateResonance(spectrum, features.pitch);

        features.isValid = true;

        LOGD("Analysis complete: pitch=%.2f, centroid=%.2f, brightness=%.3f",
             features.pitch, features.centroid, features.brightness);

        return features;

    } catch (const EssentiaException& e) {
        LOGE("Essentia exception during analysis: %s", e.what());
        return AudioFeatures();
    } catch (const std::exception& e) {
        LOGE("Standard exception during analysis: %s", e.what());
        return AudioFeatures();
    } catch (...) {
        LOGE("Unknown exception during analysis");
        return AudioFeatures();
    }
}

std::vector<AudioFeatures> EssentiaWrapper::analyzeBuffer(const float* audioBuffer, int bufferLength, int hopSize) {
    std::vector<AudioFeatures> results;

    if (!initialized || audioBuffer == nullptr || bufferLength < frameSize) {
        LOGE("Invalid parameters for buffer analysis");
        return results;
    }

    // Process buffer with sliding window
    for (int i = 0; i <= bufferLength - frameSize; i += hopSize) {
        AudioFeatures features = analyzeFrame(audioBuffer + i, frameSize);
        if (features.isValid) {
            results.push_back(features);
        }
    }

    LOGD("Buffer analysis complete: %zu frames processed", results.size());
    return results;
}

void EssentiaWrapper::cleanup() {
    if (initialized) {
        LOGI("Cleaning up Essentia resources");

        // Reset all algorithm pointers
        pitchYin.reset();
        centroidAlg.reset();
        mfccAlg.reset();
        windowAlg.reset();
        spectrumAlg.reset();
        spectralPeaksAlg.reset();

        // Shutdown Essentia
        try {
            essentia::shutdown();
        } catch (...) {
            LOGE("Exception during Essentia shutdown");
        }

        initialized = false;
        LOGI("Essentia cleanup completed");
    }
}

float EssentiaWrapper::calculateBrightness(const std::vector<float>& spectrum) {
    if (spectrum.empty()) return 0.0f;

    // Calculate brightness as ratio of high frequency energy to total energy
    const size_t cutoffBin = spectrum.size() / 4; // Rough cutoff at 1/4 of Nyquist

    float totalEnergy = 0.0f;
    float highFreqEnergy = 0.0f;

    for (size_t i = 0; i < spectrum.size(); ++i) {
        float energy = spectrum[i] * spectrum[i];
        totalEnergy += energy;
        if (i >= cutoffBin) {
            highFreqEnergy += energy;
        }
    }

    return (totalEnergy > 0.0f) ? (highFreqEnergy / totalEnergy) : 0.0f;
}

float EssentiaWrapper::calculateResonance(const std::vector<float>& spectrum, float pitch) {
    if (spectrum.empty() || pitch <= 0.0f) return 0.0f;

    // Simplified resonance calculation based on harmonic strength
    const float binWidth = static_cast<float>(sampleRate) / (2.0f * spectrum.size());
    float resonance = 0.0f;

    // Look for harmonics
    for (int harmonic = 1; harmonic <= 5; ++harmonic) {
        float harmonicFreq = pitch * harmonic;
        if (harmonicFreq >= sampleRate / 2.0f) break;

        int bin = static_cast<int>(harmonicFreq / binWidth);
        if (bin < static_cast<int>(spectrum.size())) {
            resonance += spectrum[bin] / harmonic; // Weight by inverse harmonic number
        }
    }

    return resonance;
}

std::vector<float> EssentiaWrapper::preprocessAudio(const float* audioData, int length) {
    std::vector<float> processed(audioData, audioData + length);

    // Ensure we have the right frame size
    if (processed.size() < static_cast<size_t>(frameSize)) {
        processed.resize(frameSize, 0.0f); // Zero-pad if necessary
    } else if (processed.size() > static_cast<size_t>(frameSize)) {
        processed.resize(frameSize); // Truncate if necessary
    }

    // Simple DC removal
    float mean = 0.0f;
    for (float sample : processed) {
        mean += sample;
    }
    mean /= processed.size();

    for (float& sample : processed) {
        sample -= mean;
    }

    return processed;
}

// C-style functions for JNI
extern "C" {
bool initEssentia(int sampleRate) {
    try {
        if (!g_essentiaWrapper) {
            g_essentiaWrapper = std::make_unique<EssentiaWrapper>();
        }
        return g_essentiaWrapper->initialize(sampleRate);
    } catch (...) {
        LOGE("Exception in initEssentia");
        return false;
    }
}

AudioFeatures analyzeAudioFrame(const float* audioData, int length) {
    if (g_essentiaWrapper && g_essentiaWrapper->isReady()) {
        return g_essentiaWrapper->analyzeFrame(audioData, length);
    }
    LOGE("EssentiaWrapper not initialized in analyzeAudioFrame");
    return AudioFeatures();
}

std::vector<AudioFeatures> analyzeAudioBuffer(const float* audioBuffer, int bufferLength, int hopSize) {
    if (g_essentiaWrapper && g_essentiaWrapper->isReady()) {
        return g_essentiaWrapper->analyzeBuffer(audioBuffer, bufferLength, hopSize);
    }
    LOGE("EssentiaWrapper not initialized in analyzeAudioBuffer");
    return std::vector<AudioFeatures>();
}

void cleanupEssentia() {
    if (g_essentiaWrapper) {
        g_essentiaWrapper->cleanup();
        g_essentiaWrapper.reset();
    }
}
}