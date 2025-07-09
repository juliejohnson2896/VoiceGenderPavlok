#include <jni.h>
#include <vector>
#include <android/log.h>
#include "formant_analyzer.h" // <-- Matches your header name

// Helper to convert a Java float array to a C++ vector of doubles
// This function does not need any changes.
std::vector<double> jfloatArrayToDoubleVector(JNIEnv *env, jfloatArray array) {
    if (array == nullptr) {
        return {};
    }
    jsize len = env->GetArrayLength(array);
    if (len == 0) {
        return {};
    }
    std::vector<double> vec(len);
    jfloat* body = env->GetFloatArrayElements(array, 0);
    for (int i = 0; i < len; i++) {
        vec[i] = static_cast<double>(body[i]);
    }
    env->ReleaseFloatArrayElements(array, body, 0);
    return vec;
}

extern "C" JNIEXPORT jobjectArray JNICALL
// IMPORTANT: Make sure this path matches your Java package structure exactly.
Java_com_juliejohnson_voicegenderpavlok_audio_FormantAnalyzer_analyze(
        JNIEnv *env,
        jobject /* this */,
        jfloatArray audioData,
        jdouble sampleRate,
        jdouble formantCeiling,
        jint numFormants,
        jdouble windowLength,
        jdouble preEmphasisFreq)
{
    // --- 1. Convert Java input array to C++ vector ---
    std::vector<double> audio_vector = jfloatArrayToDoubleVector(env, audioData);
    if (audio_vector.empty()) {
        __android_log_print(ANDROID_LOG_ERROR, "FormantAnalyzerJNI", "Input audio data is empty.");
        return nullptr;
    }

    // --- 2. Call the C++ formant analysis pipeline ---
    // This now calls the static method from your `formant_analyzer` class.
    std::vector<std::vector<Formant>> all_formants = formant_analyzer::process(
            audio_vector,
            static_cast<double>(sampleRate),
            static_cast<double>(formantCeiling),
            static_cast<int>(numFormants),
            static_cast<double>(windowLength),
            static_cast<double>(preEmphasisFreq)
    );

    // --- 3. Create and populate the Java return array ---
    // This logic remains the same.
    // IMPORTANT: Make sure this path matches your Formant data class in Java.
    jclass formantClass = env->FindClass("com/juliejohnson/voicegenderpavlok/audio/Formant");
    if (formantClass == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, "FormantAnalyzerJNI", "Could not find Formant class.");
        return nullptr;
    }
    jmethodID formantConstructor = env->GetMethodID(formantClass, "<init>", "(DD)V");
    if (formantConstructor == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, "FormantAnalyzerJNI", "Could not find Formant constructor.");
        return nullptr;
    }

    // Create an array of Formant arrays.
    jobjectArray resultOuterArray = env->NewObjectArray(all_formants.size(), env->FindClass("[Lcom/juliejohnson/voicegenderpavlok/audio/Formant;"), nullptr);

    for(size_t i = 0; i < all_formants.size(); ++i) {
        const auto& frame_formants = all_formants[i];
        jobjectArray resultInnerArray = env->NewObjectArray(frame_formants.size(), formantClass, nullptr);
        for(size_t j = 0; j < frame_formants.size(); ++j) {
            jobject formantObj = env->NewObject(formantClass, formantConstructor, frame_formants[j].frequency_hz, frame_formants[j].bandwidth_hz);
            env->SetObjectArrayElement(resultInnerArray, j, formantObj);
            env->DeleteLocalRef(formantObj);
        }
        env->SetObjectArrayElement(resultOuterArray, i, resultInnerArray);
        env->DeleteLocalRef(resultInnerArray);
    }

    return resultOuterArray;
}