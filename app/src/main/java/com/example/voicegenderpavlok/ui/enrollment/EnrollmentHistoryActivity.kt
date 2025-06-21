package com.example.voicegenderpavlok.ui.enrollment

import android.os.Bundle
import android.util.Log
import android.view.Menu
import android.view.MenuItem
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.recyclerview.widget.LinearLayoutManager
import com.example.voicegenderpavlok.R
import com.example.voicegenderpavlok.databinding.ActivityEnrollmentHistoryBinding
import com.example.voicegenderpavlok.storage.EnrollmentSample
import com.example.voicegenderpavlok.storage.EnrollmentStorage
import com.example.voicegenderpavlok.ui.EnrollmentAdapter
import com.example.voicegenderpavlok.utils.AudioPlayer

class EnrollmentHistoryActivity : AppCompatActivity(), EnrollmentAdapter.Listener {

    private lateinit var binding: ActivityEnrollmentHistoryBinding
    private lateinit var enrollmentAdapter: EnrollmentAdapter
    private val samples = mutableListOf<EnrollmentSample>()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityEnrollmentHistoryBinding.inflate(layoutInflater)
        setContentView(binding.root)
        setSupportActionBar(binding.toolbar)

        // Initialize EnrollmentStorage if needed
        EnrollmentStorage.initialize(applicationContext)

        // Load samples
        samples.addAll(EnrollmentStorage.listSamples())

        // Setup RecyclerView & Adapter
        enrollmentAdapter = EnrollmentAdapter(samples, this)
        binding.recyclerViewEnrollment.apply {
            layoutManager = LinearLayoutManager(this@EnrollmentHistoryActivity)
            adapter = enrollmentAdapter
        }
    }

    // Inflate toolbar menu with "Clear All"
    override fun onCreateOptionsMenu(menu: Menu?): Boolean {
        menuInflater.inflate(R.menu.menu_enrollment, menu)
        return true
    }

    // Handle toolbar item clicks
    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            R.id.action_clear_all -> {
                showClearAllConfirmationDialog()
                true
            }
            else -> super.onOptionsItemSelected(item)
        }
    }

    // Show confirmation dialog before clearing
    private fun showClearAllConfirmationDialog() {
        AlertDialog.Builder(this)
            .setTitle("Clear All Enrollment Samples")
            .setMessage("Are you sure you want to delete all enrolled voice samples? This action cannot be undone.")
            .setPositiveButton("Clear All") { _, _ ->
                clearAllEnrollmentSamples()
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    // Clear all samples from storage and UI
    private fun clearAllEnrollmentSamples() {
        EnrollmentStorage.clearAllSamples()  // Delete all files

        samples.clear()                      // Clear adapter data
        enrollmentAdapter.notifyDataSetChanged() // Refresh UI

        Toast.makeText(this, "All enrollment samples cleared", Toast.LENGTH_SHORT).show()
    }

    // EnrollmentAdapter.Listener implementation

    override fun onPlay(sample: EnrollmentSample, position: Int) {
        Log.d("EnrollmentHistory", "Play pressed for sample: ${sample.audioPath}")
        AudioPlayer.play(sample.audioPath)
    }

    override fun onDelete(sample: EnrollmentSample, position: Int) {
        // Delete sample files
        EnrollmentStorage.deleteSample(sample.id)
        // Remove from list and notify adapter
        samples.removeAt(position)
        enrollmentAdapter.notifyItemRemoved(position)
    }
}