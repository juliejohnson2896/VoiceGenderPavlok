package com.example.voicegenderpavlok.utils

import android.content.Context
import android.media.*
import android.util.Log
import com.example.voicegenderpavlok.ui.CircularShortBuffer
import com.konovalov.vad.silero.Vad
import com.konovalov.vad.silero.VadSilero
import com.konovalov.vad.silero.config.FrameSize
import com.konovalov.vad.silero.config.Mode
import com.konovalov.vad.silero.config.SampleRate
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlin.math.max

class VADUtils(context: Context) {

    companion object {
        private val SAMPLE_RATE = SampleRate.SAMPLE_RATE_16K
        private const val SAMPLE_RATE_INT = 16000
        private const val CHUNK_SIZE = 512 // SileroVAD performs well with small chunks (e.g., 10ms = 160 samples)

    }

    private val vad: VadSilero = Vad.builder()
        .setContext(context)
        .setSampleRate(SAMPLE_RATE)
        .setFrameSize(FrameSize.FRAME_SIZE_512)
        .setMode(Mode.NORMAL)
        .setSilenceDurationMs(300)
        .setSpeechDurationMs(50)
        .build();
    private var audioRecord: AudioRecord? = null
    private val coroutineScope = CoroutineScope(Dispatchers.Default + SupervisorJob())

    private val minBufferSize = AudioRecord.getMinBufferSize(
        SAMPLE_RATE_INT,
        AudioFormat.CHANNEL_IN_MONO,
        AudioFormat.ENCODING_PCM_16BIT
    )

    private val buffer = ShortArray(CHUNK_SIZE)
    private val audioHistory = CircularShortBuffer(16000) // ~1 sec of history

    private val _vadStatus = MutableStateFlow(false)
    val vadStatus: StateFlow<Boolean> get() = _vadStatus

    fun initializeAudioRecord(): Boolean {
        return try {
            audioRecord = AudioRecord(
                MediaRecorder.AudioSource.MIC,
                SAMPLE_RATE_INT,
                AudioFormat.CHANNEL_IN_MONO,
                AudioFormat.ENCODING_PCM_16BIT,
                max(minBufferSize, CHUNK_SIZE * 2)
            )
            true
        } catch (e: SecurityException) {
            Log.e("VADUtils", "Permission denied: ${e.message}")
            false
        } catch (e: Exception) {
            Log.e("VADUtils", "Failed to initialize AudioRecord: ${e.message}")
            false
        }
    }

    fun startListening(onSpeechDetected: () -> Unit) {
        val recorder = audioRecord ?: return

        recorder.startRecording()

        coroutineScope.launch {
            while (isActive) {
                val read = recorder.read(buffer, 0, buffer.size)
                if (read > 0) {
                    audioHistory.append(buffer.copyOf(read))
                    val isSpeech = vad.isSpeech(buffer)
                    _vadStatus.value = isSpeech
                    if (isSpeech) {
                        onSpeechDetected()
                    }
                }
                delay(10)
            }
        }
    }

    fun getRecentAudio(): FloatArray {
        return audioHistory.toArray().map { it.toFloat() / Short.MAX_VALUE }.toFloatArray()
    }

    fun stop() {
        coroutineScope.cancel()
        audioRecord?.apply {
            stop()
            release()
        }
        audioRecord = null
        vad.close()
    }
}
