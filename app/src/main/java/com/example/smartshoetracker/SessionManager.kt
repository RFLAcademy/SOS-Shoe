package com.example.smartshoetracker // UPDATE PACKAGE NAME

import android.content.Context
import android.content.SharedPreferences

class SessionManager(context: Context) {
    private var prefs: SharedPreferences = context.getSharedPreferences("ShoeAppPrefs", Context.MODE_PRIVATE)

    fun saveLogin(email: String, pass: String, deviceId: String) {
        prefs.edit()
            .putString("EMAIL", email)
            .putString("PASS", pass)
            .putString("DEVICE_ID", deviceId)
            .apply()
    }

    fun getEmail(): String? = prefs.getString("EMAIL", null)
    fun getPass(): String? = prefs.getString("PASS", null)
    fun getDeviceId(): String? = prefs.getString("DEVICE_ID", null)

    fun isLoggedIn(): Boolean = getEmail() != null

    fun logout() {
        prefs.edit().clear().apply()
    }
}