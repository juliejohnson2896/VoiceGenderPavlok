package com.example.voicegenderpavlok.ui

import android.media.MediaPlayer
import android.os.Bundle
import android.widget.Button
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.example.voicegenderpavlok.R
import com.example.voicegenderpavlok.storage.EnrollmentSample
import com.example.voicegenderpavlok.storage.EnrollmentStorage
import java.io.File

class EnrollmentHistoryActivity : AppCompatActivity() {

    private lateinit var recyclerView: RecyclerView
    private lateinit var adapter: EnrollmentAdapter
    private lateinit var clearAllButton: Button

    private val samples = mutableListOf<EnrollmentSample>()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_enrollment_history)

        recyclerView = findViewById(R.id.recycler_view)
        clearAllButton = findViewById(R.id.clear_all_button)

        adapter = EnrollmentAdapter(
            items = samples,
            onPlay = { sample -> playSample(sample) },
            onDelete = { sample -> deleteSample(sample) }
        )

        recyclerView.layoutManager = LinearLayoutManager(this)
        recyclerView.adapter = adapter

        clearAllButton.setOnClickListener {
            EnrollmentStorage.clearAllSamples()
            refreshSamples()
            Toast.makeText(this, "All voice samples cleared.", Toast.LENGTH_SHORT).show()
        }

        refreshSamples()
    }

    private fun refreshSamples() {
        samples.clear()
        samples.addAll(EnrollmentStorage.listSamples())
        adapter.notifyDataSetChanged()
    }

    private fun playSample(sample: EnrollmentSample) {
        try {
            val player = MediaPlayer().apply {
                setDataSource(sample.audioPath) // <== Use audioPath
                prepare()
                start()
            }
        } catch (e: Exception) {
            Toast.makeText(this, "Failed to play sample.", Toast.LENGTH_SHORT).show()
        }
    }

    private fun deleteSample(sample: EnrollmentSample) {
        EnrollmentStorage.deleteSample(sample.id)
        refreshSamples()
        Toast.makeText(this, "Sample deleted.", Toast.LENGTH_SHORT).show()
    }
}
