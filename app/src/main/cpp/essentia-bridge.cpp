#include <jni.h>
#include <vector>
#include <android/log.h> // For logging from C++ to Android's Logcat

// Include the main Essentia headers
#include "include/essentia/essentia.h"
#include "include/essentia/algorithmfactory.h"
#include "include/essentia/pool.h"

using namespace std;
using namespace essentia;
using namespace essentia::standard;

const char* TAG = "VoiceAnalysisEngine";

extern "C" JNIEXPORT jfloat JNICALL
Java_com_juliejohnson_voicegenderpavlok_audio_VoiceAnalysisEngine_getPitch(
        JNIEnv* env,
        jobject /* this */,
        jfloatArray audioBuffer,
        jint sampleRate) {

    // Convert the audio data from a Java FloatArray to a C++ vector<float>
    jsize buffer_size = env->GetArrayLength(audioBuffer);
    if (buffer_size == 0) return -1.0f;

    jfloat* audio_ptr = env->GetFloatArrayElements(audioBuffer, 0);
    vector<float> audio_vector(audio_ptr, audio_ptr + buffer_size);
    env->ReleaseFloatArrayElements(audioBuffer, audio_ptr, 0);

    // Initialize the Essentia library
    essentia::init();
    AlgorithmFactory& factory = AlgorithmFactory::instance();

    // Create the PitchYinFFT algorithm
    Algorithm* pitch_algo = factory.create("PitchYinFFT", "sampleRate", sampleRate);

    // Set up a "Pool" to hold the input audio and receive the output pitch
    Pool pool;
    Real pitch = 0.0;
    Real pitchConfidence = 0.0;

    // Run the algorithm
    pitch_algo->input("audio").set(audio_vector);
    pitch_algo->output("pitch").set(pitch);
    pitch_algo->output("pitchConfidence").set(pitchConfidence);
    pitch_algo->compute();

    // Clean up Essentia resources
    delete pitch_algo;
    essentia::shutdown();

    // Only return a valid pitch if the confidence is high enough
    if (pool.value<Real>("pitchConfidence") > 0.8) {
        return pool.value<Real>("pitch");
    } else {
        return -1.0f;
    }
}