package com.juliejohnson.voicegenderpavlok.utils

import android.content.Context
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import android.util.Log
import com.juliejohnson.voicegenderpavlok.ml.AudioBuffer
import com.konovalov.vad.silero.VadSilero
import com.konovalov.vad.silero.config.FrameSize
import com.konovalov.vad.silero.config.Mode
import com.konovalov.vad.silero.config.SampleRate
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.isActive
import kotlinx.coroutines.withContext
import kotlin.math.absoluteValue

class VADRecorder(
    private val context: Context,
    private val sampleRateInt: Int = 16000,
    private val frameSizeInt: Int = 512,
    private val speechDurationMs: Int = 1000,
    private val silenceDurationMs: Int = 1000,
    private val mode: Mode = Mode.AGGRESSIVE,
    private val sampleRate: SampleRate = SampleRate.SAMPLE_RATE_16K,
    private val frameSize: FrameSize = FrameSize.FRAME_SIZE_512
) {
    private val vad = VadSilero(
        context = context,
        sampleRate = sampleRate,
        frameSize = frameSize,
        mode = mode,
        speechDurationMs = speechDurationMs,
        silenceDurationMs = silenceDurationMs
    )

    @androidx.annotation.RequiresPermission(android.Manifest.permission.RECORD_AUDIO)
    suspend fun recordUntilSpeechDetected(
        onAmplitude: ((Float) -> Unit)? = null
    ): AudioBuffer? = withContext(Dispatchers.IO) {
        val buffer = ShortArray(frameSizeInt)
        val audioData = mutableListOf<Short>()

        val recorder = AudioRecord(
            MediaRecorder.AudioSource.MIC,
            sampleRateInt,
            AudioFormat.CHANNEL_IN_MONO,
            AudioFormat.ENCODING_PCM_16BIT,
            AudioRecord.getMinBufferSize(sampleRateInt, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT)
        )

        try {
            recorder.startRecording()

            while (isActive) {
                val read = recorder.read(buffer, 0, buffer.size)
                if (read > 0) {
                    onAmplitude?.invoke(buffer.take(read).map { it.toInt().absoluteValue }.average().toFloat())
                    val isSpeech = vad.isSpeech(buffer)
                    Log.d("VADRecorder", "Amplitude: $onAmplitude, Speech: $isSpeech")
                    audioData.addAll(buffer.take(read))

                    if (isSpeech) {
                        recorder.stop()
                        recorder.release()

                        Log.d("VADRecorder", "Speech detected — samples: ${audioData.size}")
                        val floatData = audioData.map { it / 32768.0f }.toFloatArray()
                        return@withContext AudioBuffer(floatData, sampleRateInt, 1)
                    }
                }
            }

            null
        } catch (e: Exception) {
            Log.e("VADRecorder", "Failed to record", e)
            recorder.release()
            null
        }
    }

}
