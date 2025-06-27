#include <jni.h>
#include <vector>
#include <android/log.h>

#include <essentia/essentia.h>
#include <essentia/algorithmfactory.h>

using namespace essentia;
using namespace essentia::standard;

#define LOG_TAG "VoiceAnalysisEngine"

// Called when .so loads
extern "C" JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* /*reserved*/) {
    essentia::init();
    __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, "Essentia initialized");
    return JNI_VERSION_1_6;
}

// Called when .so unloads
extern "C" JNIEXPORT void JNICALL
JNI_OnUnload(JavaVM* vm, void* /*reserved*/) {
essentia::shutdown();
__android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, "Essentia shutdown");
}

/*
 * Class:     com_juliejohnson_voicegenderpavlok_audio_VoiceAnalysisEngine
 * Method:    getPitch
 * Signature: ([FI)F
 */
extern "C" JNIEXPORT jfloat JNICALL
Java_com_juliejohnson_voicegenderpavlok_audio_VoiceAnalysisEngine_getPitch(
        JNIEnv* env,
        jobject /* this */,
        jfloatArray audioBuffer,
        jint sampleRate) {

    // 1. FloatArray → std::vector<Real>
    jsize len = env->GetArrayLength(audioBuffer);
    std::vector<Real> audio(len);
    env->GetFloatArrayRegion(audioBuffer, 0, len,
                             reinterpret_cast<jfloat*>(audio.data()));

    // 2. Create Pitch algorithm (Yin)
    AlgorithmFactory& factory = standard::AlgorithmFactory::instance();
    Algorithm* pitchAlg = factory.create(
            "PitchYin",            // choose "PitchYin" or another PitchXXX
            "frameSize", (int)len,
            "sampleRate", (int)sampleRate
    );

    Real pitch = 0.0, confidence = 0.0;
    pitchAlg->input("signal").set(audio);
    pitchAlg->output("pitch").set(pitch);
    pitchAlg->output("pitchConfidence").set(confidence);
    pitchAlg->compute();
    delete pitchAlg;

    __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG,
                        "Pitch: %f Hz, conf: %f", (double)pitch, (double)confidence);

    // 3. Return pitch in Hz
    return static_cast<jfloat>(pitch);
}
