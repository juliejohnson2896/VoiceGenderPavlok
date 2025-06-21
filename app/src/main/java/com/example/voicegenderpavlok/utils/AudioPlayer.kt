package com.example.voicegenderpavlok.utils

import android.media.MediaPlayer
import android.util.Log
import java.io.File

object AudioPlayer {

    private var mediaPlayer: MediaPlayer? = null

    fun play(audioPath: String) {
        stop() // Stop current if already playing

        try {
            val file = File(audioPath)
            if (!file.exists()) {
                Log.e("AudioPlayer", "File does not exist: $audioPath")
                return
            }

            // Release if a player exists
            mediaPlayer?.let {
                if (it.isPlaying) {
                    it.stop()
                }
                it.release()
            }

            mediaPlayer = MediaPlayer().apply {
                setDataSource(file.absolutePath)
                prepare()
                start()

                setOnCompletionListener {
                    // Automatically release after playback finishes
                    release()
                    mediaPlayer = null
                }
            }

            Log.d("AudioPlayer", "Playing audio: $audioPath")

        } catch (e: Exception) {
            Log.e("AudioPlayer", "Failed to play audio", e)
        }
    }

    fun pause() {
        mediaPlayer?.takeIf { it.isPlaying }?.pause()
    }

    fun resume() {
        mediaPlayer?.start()
    }

    fun stop() {
        mediaPlayer?.let {
            try {
                if (it.isPlaying) it.stop()
            } catch (e: IllegalStateException) {
                Log.w("AudioPlayer", "Tried to stop mediaPlayer in invalid state", e)
            } finally {
                it.release()
            }
        }
        mediaPlayer = null
    }


    fun isPlaying(): Boolean {
        return mediaPlayer?.isPlaying ?: false
    }
}
