package com.juliejohnson.voicegenderpavlok

import retrofit2.Call
import retrofit2.http.Header
import retrofit2.http.POST

interface ApiService {
    @POST("trigger")
    fun triggerShock(@Header("Authorization") token: String): Call<Unit>
}
