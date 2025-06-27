#include <jni.h>
#include <vector>
#include <android/log.h>

#include "essentia/essentia.h"
#include "essentia/algorithmfactory.h"

using namespace std;
using namespace essentia;
using namespace essentia::standard;

const char* TAG = "VoiceAnalysisEngine";

// --- GLOBAL VARIABLES ---
// We will hold our algorithms here so they are not recreated for every frame.
Algorithm* pitch_algo = nullptr;
Algorithm* lowpass_algo = nullptr;
Algorithm* windowing_algo = nullptr;
Algorithm* spectrum_algo = nullptr;

// --- INITIALIZE FUNCTION ---
// This will be called only ONCE.
extern "C" JNIEXPORT void JNICALL
Java_com_juliejohnson_voicegenderpavlok_audio_VoiceAnalysisEngine_initialize(
        JNIEnv* env,
        jobject /* this */,
        jint sampleRate,
        jint frameSize) {

    __android_log_print(ANDROID_LOG_INFO, TAG, "Initializing Essentia Engine...");
    essentia::init();
    AlgorithmFactory& factory = AlgorithmFactory::instance();

    // Create our algorithms ONCE and store them.
    lowpass_algo = factory.create("LowPass", "sampleRate", sampleRate, "cutoffFrequency", 1500);
    windowing_algo = factory.create("Windowing", "type", "hann");
    spectrum_algo = factory.create("Spectrum");
    pitch_algo = factory.create("PitchYinFFT",
                                "sampleRate", sampleRate,
                                "frameSize", frameSize);

    if (!lowpass_algo || !windowing_algo || !spectrum_algo || !pitch_algo) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Error: could not create all analysis algorithms.");
    }
}

// --- SHUTDOWN FUNCTION ---
// This will be called only ONCE when the app closes.
extern "C" JNIEXPORT void JNICALL
Java_com_juliejohnson_voicegenderpavlok_audio_VoiceAnalysisEngine_shutdown(
        JNIEnv* env,
        jobject /* this */) {

    __android_log_print(ANDROID_LOG_INFO, TAG, "Shutting Down Essentia Engine...");
    // Delete the algorithms to free memory
    delete lowpass_algo;
    delete windowing_algo;
    delete spectrum_algo;
    delete pitch_algo;
    lowpass_algo = nullptr;
    windowing_algo = nullptr;
    spectrum_algo = nullptr;
    pitch_algo = nullptr;

    // Shutdown the Essentia library
    essentia::shutdown();
}


// --- GETPITCH FUNCTION ---
// This is now very lightweight and fast.
extern "C" JNIEXPORT jfloat JNICALL
Java_com_juliejohnson_voicegenderpavlok_audio_VoiceAnalysisEngine_getPitch(
        JNIEnv* env,
        jobject /* this */,
        jfloatArray audioBuffer) {

    // Check if the engine is initialized
    if (!pitch_algo) {
        return -1.0f;
    }

    // Convert audio data from Java to C++
    jsize buffer_size = env->GetArrayLength(audioBuffer);
    if (buffer_size == 0) return -1.0f;
    jfloat* audio_ptr = env->GetFloatArrayElements(audioBuffer, 0);
    vector<float> audio_vector(audio_ptr, audio_ptr + buffer_size);
    env->ReleaseFloatArrayElements(audioBuffer, audio_ptr, 0);

    // --- Setup the processing chain for this frame ---
    vector<float> filtered_audio, windowed_frame, audio_spectrum;
    Real pitch = 0.0, pitchConfidence = 0.0;

    // --- Run the processing chain using our existing algorithms ---
    lowpass_algo->input("signal").set(audio_vector);
    lowpass_algo->output("signal").set(filtered_audio);
    lowpass_algo->compute();

    windowing_algo->input("frame").set(filtered_audio);
    windowing_algo->output("frame").set(windowed_frame);
    windowing_algo->compute();

    spectrum_algo->input("frame").set(windowed_frame);
    spectrum_algo->output("spectrum").set(audio_spectrum);
    spectrum_algo->compute();

    pitch_algo->input("spectrum").set(audio_spectrum);
    pitch_algo->output("pitch").set(pitch);
    pitch_algo->output("pitchConfidence").set(pitchConfidence);
    pitch_algo->compute();

    // --- THE FIX for HIGH PITCH ---
    // Now that the algorithm is stateful, it will be much less prone to octave errors.
    // We only return the pitch if the algorithm is confident.
    if (pitchConfidence > 0.90) {
        return pitch;
    } else {
        return -1.0f;
    }
}