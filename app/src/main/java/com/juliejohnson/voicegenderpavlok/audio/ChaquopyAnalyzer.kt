package com.juliejohnson.voicegenderpavlok.audio

// In your Kotlin analyzer class
import android.content.Context
import android.util.Log
import com.chaquo.python.PyObject
import com.chaquo.python.Python
import com.chaquo.python.android.AndroidPlatform
import org.json.JSONArray
import java.nio.ByteBuffer
import java.nio.ByteOrder

class ChaquopyAnalyzer(context: Context) {

    private lateinit var py: Python
    private val analyzerModule: PyObject

    init {
        // Start Python if it hasn't been started
        if (!Python.isStarted()) {
            Python.start(AndroidPlatform(context))
        }
        py = Python.getInstance()
        analyzerModule = py.getModule("voice_analyzer") // The name of your Python file
    }

    fun analyze(audioBuffer: FloatArray, sampleRate: Int): AudioFeatures? {
        try {
            // --- THE FIX ---
            // 1. Detect the native byte order of the Android device.
            val nativeByteOrder = ByteOrder.nativeOrder()
            val byteOrderString = if (nativeByteOrder == ByteOrder.LITTLE_ENDIAN) "little" else "big"

            // 2. Convert the FloatArray to a ByteArray using the detected native order.
            val byteBuffer = ByteBuffer.allocate(audioBuffer.size * 4).order(nativeByteOrder)
            for (value in audioBuffer) {
                byteBuffer.putFloat(value)
            }
            val audioBytes = byteBuffer.array()

            // 3. Pass the raw bytes AND the byte order string to the Python function.
            val resultJson = analyzerModule.callAttr(
                "analyze_voice_features",
                audioBytes,
                44100,      // your sample rate
                this,
                false,       // save_audio flag
                byteOrderString // NEW: Pass the detected byte order
            ).toString()

            // Parse the JSON string into a map
            return parseJsonToAudioFeatures(resultJson)

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