package com.juliejohnson.voicegenderpavlok.ui

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import com.juliejohnson.voicegenderpavlok.R
import com.juliejohnson.voicegenderpavlok.storage.EnrollmentSample
import java.text.SimpleDateFormat
import java.util.*

class EnrollmentAdapter(
    private var samples: List<EnrollmentSample>,
    private val listener: Listener
) : RecyclerView.Adapter<EnrollmentAdapter.ViewHolder>() {

    interface Listener {
        fun onPlay(sample: EnrollmentSample, position: Int)
        fun onDelete(sample: EnrollmentSample, position: Int)
    }

    class ViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
        val labelText: TextView = itemView.findViewById(R.id.label_text)
        val timestampText: TextView = itemView.findViewById(R.id.timestamp_text)
        val autoEnrolledTag: TextView = itemView.findViewById(R.id.auto_enrolled_tag)
        val playButton: Button = itemView.findViewById(R.id.play_button)
        val deleteButton: Button = itemView.findViewById(R.id.delete_button)
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(R.layout.item_enrollment_sample, parent, false)
        return ViewHolder(view)
    }

    override fun onBindViewHolder(holder: ViewHolder, position: Int) {
        val sample = samples[position]
        val metadata = sample.metadata

        // Format timestamp
        val timestampFormatted = SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.getDefault())
            .format(Date(metadata.timestamp))

        holder.labelText.text = metadata.label ?: "Unnamed Sample"
        holder.timestampText.text = timestampFormatted

        // Show/hide auto-enrolled tag
        holder.autoEnrolledTag.visibility =
            if (metadata.autoEnrolled) View.VISIBLE else View.GONE

        holder.playButton.setOnClickListener {
            listener.onPlay(sample, holder.bindingAdapterPosition)
        }

        holder.deleteButton.setOnClickListener {
            listener.onDelete(sample, holder.bindingAdapterPosition)
        }
    }

    override fun getItemCount(): Int = samples.size

    fun updateSamples(newSamples: List<EnrollmentSample>) {
        this.samples = newSamples
        notifyDataSetChanged()
    }
}