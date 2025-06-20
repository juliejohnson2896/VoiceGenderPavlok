package com.example.voicegenderpavlok.ui

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import com.example.voicegenderpavlok.R
import com.example.voicegenderpavlok.storage.EnrollmentSample
import java.text.SimpleDateFormat
import java.util.*

class EnrollmentAdapter(
    private val items: List<EnrollmentSample>,
    private val onPlay: (EnrollmentSample) -> Unit,
    private val onDelete: (EnrollmentSample) -> Unit
) : RecyclerView.Adapter<EnrollmentAdapter.ViewHolder>() {

    private val formatter = SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.getDefault())

    inner class ViewHolder(view: View) : RecyclerView.ViewHolder(view) {
        val label: TextView = view.findViewById(R.id.sample_label)
        val playButton: Button = view.findViewById(R.id.play_button)
        val deleteButton: Button = view.findViewById(R.id.delete_button)
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(R.layout.item_enrollment_sample, parent, false)
        return ViewHolder(view)
    }

    override fun getItemCount(): Int = items.size

    override fun onBindViewHolder(holder: ViewHolder, position: Int) {
        val sample = items[position]
        holder.label.text = sample.metadata.label ?: formatter.format(Date(sample.metadata.timestamp))
        holder.playButton.setOnClickListener { onPlay(sample) }
        holder.deleteButton.setOnClickListener { onDelete(sample) }
    }
}
