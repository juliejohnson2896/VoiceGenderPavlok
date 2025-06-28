#include <jni.h>
#include <vector>
#include <android/log.h>

#include "essentia/essentia.h"
#include "essentia/algorithmfactory.h"
#include "essentia/streaming/streamingalgorithm.h"

using namespace std;
using namespace essentia;
using namespace essentia::streaming;

const char* TAG = "VoiceAnalysisEngine";

// --- GLOBAL POINTERS for our persistent "Assembly Line" ---
Algorithm* audio_source = nullptr;
Algorithm* pitch_algo = nullptr;

// --- Global vectors to hold the streaming results ---
// This is the simplest and most direct way to get the output.
vector<Real> pitch_output;
vector<Real> pitch_confidence_output;


// --- INITIALIZE FUNCTION ---
// This builds the entire processing pipeline once.
extern "C" JNIEXPORT void JNICALL
Java_com_juliejohnson_voicegenderpavlok_audio_VoiceAnalysisEngine_initialize(
        JNIEnv* env,
        jobject /* this */,
        jint sampleRate,
        jint frameSize) {

    __android_log_print(ANDROID_LOG_INFO, TAG, "Initializing Essentia Streaming Engine...");
    essentia::init();
    AlgorithmFactory& factory = AlgorithmFactory::instance();

    // --- Create the streaming algorithms ---
    audio_source = factory.create("VectorInput", "frameSize", frameSize);
    pitch_algo = factory.create("PitchYinFFT",
                                "sampleRate", sampleRate,
                                "frameSize", frameSize);

    // --- Connect the pipeline using the '>>' operator ---
    // 1. The audio source's output is connected to the pitch algorithm's input.
    audio_source->output("data") >> pitch_algo->input("signal");

    // 2. THE FIX: Connect the algorithm's outputs directly to our C++ vectors.
    //    Essentia will automatically push the results into these vectors on every frame.
    pitch_algo->output("pitch") >> pitch_output;
    pitch_algo->output("pitchConfidence") >> pitch_confidence_output;
}

// --- SHUTDOWN FUNCTION ---
// This cleans up all resources once when the app is done.
extern "C" JNIEXPORT void JNICALL
Java_com_juliejohnson_voicegenderpavlok_audio_VoiceAnalysisEngine_shutdown(
        JNIEnv* env,
        jobject /* this */) {

    __android_log_print(ANDROID_LOG_INFO, TAG, "Shutting Down Essentia Engine...");
    // The C++ "delete" keyword will automatically handle de-allocating memory
    delete audio_source;
    delete pitch_algo;
    // Reset pointers to null to prevent accidental use after shutdown
    audio_source = nullptr;
    pitch_algo = nullptr;

    essentia::shutdown();
}


// --- PROCESS FUNCTION ---
// This is now extremely fast. It just pushes a new audio frame into the pipeline.
extern "C" JNIEXPORT void JNICALL
Java_com_juliejohnson_voicegenderpavlok_audio_VoiceAnalysisEngine_process(
        JNIEnv* env,
        jobject /* this */,
        jfloatArray audioBuffer) {

    if (!audio_source) return; // Safety check

    jsize buffer_size = env->GetArrayLength(audioBuffer);
    jfloat* audio_ptr = env->GetFloatArrayElements(audioBuffer, 0);
    vector<float> audio_vector(audio_ptr, audio_ptr + buffer_size);
    env->ReleaseFloatArrayElements(audioBuffer, audio_ptr, 0);

    // Feed the new audio frame into the start of the pipeline.
    audio_source->input("frame").set(audio_vector);
    audio_source->compute();
}


// --- GETPITCH FUNCTION ---
// This is also now extremely fast. It just reads the last result from our vectors.
extern "C" JNIEXPORT jfloat JNICALL
Java_com_juliejohnson_voicegenderpavlok_audio_VoiceAnalysisEngine_getPitch(
        JNIEnv* env,
        jobject /* this */) {

    // Check if we have received any results yet
    if (pitch_output.empty() || pitch_confidence_output.empty()) {
        return -1.0f;
    }

    // Return the last calculated pitch if its confidence is high enough
    if (pitch_confidence_output.back() > 0.85) {
        return pitch_output.back();
    } else {
        return -1.0f;
    }
}