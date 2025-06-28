#include <jni.h>
#include <android/log.h>
#include <vector>
#include <string>
#include "essentia_wrapper.h"

#define LOG_TAG "EssentiaJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Helper function to create Java AudioFeatures object
jobject createAudioFeaturesObject(JNIEnv* env, const AudioFeatures& features) {
    // Find the AudioFeatures class
    jclass audioFeaturesClass = env->FindClass("com/juliejohnson/voicegenderpavlok/audio/AudioFeatures");
    if (audioFeaturesClass == nullptr) {
        LOGE("Failed to find AudioFeatures class");
        return nullptr;
    }

    // Get constructor method ID
    jmethodID constructor = env->GetMethodID(audioFeaturesClass, "<init>", "(FFFF[FZ)V");
    if (constructor == nullptr) {
        LOGE("Failed to find AudioFeatures constructor");
        return nullptr;
    }

    // Convert MFCC vector to Java float array
    jfloatArray mfccArray = env->NewFloatArray(features.mfcc.size());
    if (mfccArray != nullptr && !features.mfcc.empty()) {
        env->SetFloatArrayRegion(mfccArray, 0, features.mfcc.size(), features.mfcc.data());
    }

    // Create the AudioFeatures object
    jobject audioFeaturesObj = env->NewObject(audioFeaturesClass, constructor,
                                              features.pitch,
                                              features.brightness,
                                              features.resonance,
                                              features.centroid,
                                              mfccArray,
                                              features.isValid);

    // Clean up local references
    env->DeleteLocalRef(audioFeaturesClass);
    if (mfccArray != nullptr) {
        env->DeleteLocalRef(mfccArray);
    }

    return audioFeaturesObj;
}

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_juliejohnson_voicegenderpavlok_audio_EssentiaAnalyzer_nativeInitialize(JNIEnv *env, jobject thiz, jint sampleRate) {
    LOGI("Initializing Essentia with sample rate: %d", sampleRate);

    try {
        bool success = initEssentia(static_cast<int>(sampleRate));
        if (success) {
            LOGI("Essentia initialized successfully");
        } else {
            LOGE("Failed to initialize Essentia");
        }
        return static_cast<jboolean>(success);
    } catch (const std::exception& e) {
        LOGE("Exception during Essentia initialization: %s", e.what());
        return JNI_FALSE;
    }
}

JNIEXPORT jobject JNICALL
Java_com_juliejohnson_voicegenderpavlok_audio_EssentiaAnalyzer_nativeAnalyzeFrame(JNIEnv *env, jobject thiz,
                                                               jfloatArray audioData, jint frameSize) {
    if (audioData == nullptr) {
        LOGE("Audio data is null");
        return nullptr;
    }

    // Get array length and data
    jsize arrayLength = env->GetArrayLength(audioData);
    if (arrayLength < frameSize) {
        LOGE("Audio data length (%d) is less than frame size (%d)", arrayLength, frameSize);
        return nullptr;
    }

    // Get audio data from Java array
    jfloat* audioBuffer = env->GetFloatArrayElements(audioData, nullptr);
    if (audioBuffer == nullptr) {
        LOGE("Failed to get audio buffer");
        return nullptr;
    }

    try {
        // Analyze the audio frame
        AudioFeatures features = analyzeAudioFrame(audioBuffer, frameSize);

        // Release the audio buffer
        env->ReleaseFloatArrayElements(audioData, audioBuffer, JNI_ABORT);

        // Create and return Java AudioFeatures object
        return createAudioFeaturesObject(env, features);

    } catch (const std::exception& e) {
        LOGE("Exception during audio analysis: %s", e.what());
        env->ReleaseFloatArrayElements(audioData, audioBuffer, JNI_ABORT);
        return nullptr;
    }
}

JNIEXPORT jobjectArray JNICALL
Java_com_juliejohnson_voicegenderpavlok_audio_EssentiaAnalyzer_nativeAnalyzeBuffer(JNIEnv *env, jobject thiz,
                                                                jfloatArray audioBuffer, jint hopSize) {
    if (audioBuffer == nullptr) {
        LOGE("Audio buffer is null");
        return nullptr;
    }

    // Get array length and data
    jsize bufferLength = env->GetArrayLength(audioBuffer);
    jfloat* buffer = env->GetFloatArrayElements(audioBuffer, nullptr);
    if (buffer == nullptr) {
        LOGE("Failed to get audio buffer");
        return nullptr;
    }

    try {
        // Analyze the audio buffer
        std::vector<AudioFeatures> featuresList = analyzeAudioBuffer(buffer, bufferLength, hopSize);

        // Release the audio buffer
        env->ReleaseFloatArrayElements(audioBuffer, buffer, JNI_ABORT);

        // Create Java object array
        jclass audioFeaturesClass = env->FindClass("com/juliejohnson/voicegenderpavlok/audio/AudioFeatures");
        if (audioFeaturesClass == nullptr) {
            LOGE("Failed to find AudioFeatures class");
            return nullptr;
        }

        jobjectArray resultArray = env->NewObjectArray(featuresList.size(), audioFeaturesClass, nullptr);
        if (resultArray == nullptr) {
            LOGE("Failed to create result array");
            env->DeleteLocalRef(audioFeaturesClass);
            return nullptr;
        }

        // Fill the array with AudioFeatures objects
        for (size_t i = 0; i < featuresList.size(); ++i) {
            jobject featuresObj = createAudioFeaturesObject(env, featuresList[i]);
            if (featuresObj != nullptr) {
                env->SetObjectArrayElement(resultArray, i, featuresObj);
                env->DeleteLocalRef(featuresObj);
            }
        }

        env->DeleteLocalRef(audioFeaturesClass);
        return resultArray;

    } catch (const std::exception& e) {
        LOGE("Exception during buffer analysis: %s", e.what());
        env->ReleaseFloatArrayElements(audioBuffer, buffer, JNI_ABORT);
        return nullptr;
    }
}

JNIEXPORT void JNICALL
Java_com_juliejohnson_voicegenderpavlok_audio_EssentiaAnalyzer_nativeCleanup(JNIEnv *env, jobject thiz) {
    LOGI("Cleaning up Essentia");
    try {
        cleanupEssentia();
        LOGI("Essentia cleanup completed");
    } catch (const std::exception& e) {
        LOGE("Exception during cleanup: %s", e.what());
    }
}
}