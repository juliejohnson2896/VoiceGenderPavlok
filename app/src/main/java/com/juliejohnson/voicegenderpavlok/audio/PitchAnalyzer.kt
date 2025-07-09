package com.juliejohnson.voicegenderpavlok.audio

import android.content.Context
import android.util.Log
import com.chaquo.python.PyObject
import com.chaquo.python.Python
import com.chaquo.python.android.AndroidPlatform
import org.json.JSONArray

class PitchAnalyzer (context: Context) {

    private lateinit var py: Python
    private val pitchModule: PyObject

    init {
        // Start Python if it hasn't been started
        if (!Python.isStarted()) {
            Python.start(AndroidPlatform(context))
        }
        py = Python.getInstance()
        pitchModule = py.getModule("pitch_analyzer") // The name of your Python file
    }

    fun analyze(audioBuffer: FloatArray, sampleRate: Int): AudioFeatures? {
        try {

            // With confidence (still fast)
            val result = pitchModule.callAttr("analyze_pitch_fast", audioBuffer)
            val resultMap: Map<PyObject, PyObject> = result.asMap()
            val pitch = resultMap[PyObject.fromJava("pitch_hz")]?.toDouble() ?: 0.0
            val confidence = resultMap[PyObject.fromJava("confidence")]?.toDouble() ?: 0.0


//            // Or get full analysis
//            val fullResult = pitchModule.callAttr("process_audio_chunk", audioBuffer)
//            // Get the map from the PyObject result
//            val resultMap: Map<PyObject, PyObject> = fullResult.asMap()
//
//            // Safely extract values using PyObject keys
//            val pitch = resultMap[PyObject.fromJava("pitch_hz")]?.toDouble() ?: 0.0
//            val confidence = resultMap[PyObject.fromJava("confidence")]?.toDouble() ?: 0.0
//            val isVoiced = resultMap[PyObject.fromJava("is_voiced")]?.toBoolean() ?: false
//            val pitchMidi = resultMap[PyObject.fromJava("pitch_midi")]?.toDouble() ?: 0.0
//            val noteName = resultMap[PyObject.fromJava("note_name")]?.toString() ?: "N/A"

//
//            // Parse the JSON string into a map
//            return parseJsonToAudioFeatures(resultJson)
            return AudioFeatures(pitch = pitch.toFloat(), confidence = confidence.toFloat())

        } catch (e: Exception) {
            Log.e("ChaquopyAnalyzer", "Error analyzing audio", e)
            // Handle exceptions
            return AudioFeatures()
        }
    }

    private fun parseJsonToAudioFeatures(json: String): AudioFeatures {
        val jsonObject = org.json.JSONObject(json)

        val audioFeatures = AudioFeatures(
            pitch = jsonObject.getDouble("pitch_hz").toFloat(),
            formants = jsonArrayToFloatArray(jsonObject.getJSONArray("formants")),
            hnr = jsonObject.getDouble("hnr_db").toFloat(),
        )

        return audioFeatures
    }

    private fun jsonArrayToFloatArray(jsonArray: JSONArray): FloatArray {
        val floatArray = FloatArray(jsonArray.length())
        for (i in 0 until jsonArray.length()) {
            try {
                // Attempt to get the element as a Double and cast to Float
                floatArray[i] = jsonArray.getDouble(i).toFloat()
            } catch (e: Exception) {
                // Handle parsing errors, e.g., if an element is not a valid number
                e.printStackTrace()
                // You might want to assign a default value or throw a specific exception
                // For example, assigning 0f if conversion fails:
                floatArray[i] = 0f
            }
        }
        return floatArray
    }
}