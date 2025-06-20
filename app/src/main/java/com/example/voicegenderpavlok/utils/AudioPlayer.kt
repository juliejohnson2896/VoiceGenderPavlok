package com.example.voicegenderpavlok.utils

import android.media.MediaPlayer
import java.io.File

object AudioPlayer {
    private var player: MediaPlayer? = null

    fun play(file: File) {
        stop()
        player = MediaPlayer().apply {
            setDataSource(file.absolutePath)
            prepare()
            start()
        }
    }

    fun stop() {
        player?.stop()
        player?.release()
        player = null
    }

    fun isPlaying(): Boolean = player?.isPlaying == true
}
