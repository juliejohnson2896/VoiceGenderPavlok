package com.example.voicegenderpavlok.utils

import android.app.Application
import android.content.pm.PackageManager
import android.Manifest
import androidx.core.content.ContextCompat
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow

object AudioPermissionManager {
    private val _hasPermission = MutableStateFlow(false)
    val hasPermission: StateFlow<Boolean> = _hasPermission

    fun checkPermission(application: Application) {
        val granted = ContextCompat.checkSelfPermission(
            application,
            Manifest.permission.RECORD_AUDIO
        ) == PackageManager.PERMISSION_GRANTED
        _hasPermission.value = granted
    }

    fun updatePermission(granted: Boolean) {
        _hasPermission.value = granted
    }
}
