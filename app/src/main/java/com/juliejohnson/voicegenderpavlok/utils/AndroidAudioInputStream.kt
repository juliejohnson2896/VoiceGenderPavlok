package com.juliejohnson.voicegenderpavlok.utils

import android.media.AudioRecord
import be.tarsos.dsp.io.TarsosDSPAudioFormat
import be.tarsos.dsp.io.TarsosDSPAudioInputStream
import java.io.IOException

/**
 * This is our concrete implementation of the TarsosDSPAudioInputStream interface.
 * It acts as a bridge between Android's AudioRecord and TarsosDSP's processing pipeline.
 */
class AndroidAudioInputStream(
    private val audioRecord: AudioRecord,
    private val format: TarsosDSPAudioFormat
) : TarsosDSPAudioInputStream {

    // This method is called by TarsosDSP to get raw audio bytes.
    // We simply pass the request along to our AudioRecord instance.
    override fun read(b: ByteArray, off: Int, len: Int): Int {
        return try {
            audioRecord.read(b, off, len)
        } catch (e: Exception) {
            // To prevent crashes if the AudioRecord state is invalid
            -1
        }
    }

    // TarsosDSP asks for the audio format, and we provide it.
    override fun getFormat(): TarsosDSPAudioFormat {
        return format
    }

    // This method is required by the interface. For a live stream,
    // the total length is unknown, so we return -1.
    override fun getFrameLength(): Long {
        return -1L
    }

    override fun skip(n: Long): Long {
        // Skip is not supported in this implementation.
        return 0
    }

    // When TarsosDSP is finished, it closes the stream, and we release the AudioRecord resources.
    @Throws(IOException::class)
    override fun close() {
        audioRecord.release()
    }
}