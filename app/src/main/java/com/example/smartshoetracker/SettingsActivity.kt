package com.example.smartshoetracker // UPDATE PACKAGE NAME

import android.Manifest
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Bundle
import android.telephony.SmsManager
import android.widget.Button
import android.widget.EditText
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat

class SettingsActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_settings)

        val etShoeSimNumber = findViewById<EditText>(R.id.etShoeSimNumber)
        val etEmergency1 = findViewById<EditText>(R.id.etEmergency1)
        val etEmergency2 = findViewById<EditText>(R.id.etEmergency2)
        val etEmergency3 = findViewById<EditText>(R.id.etEmergency3)
        val btnSaveConfig = findViewById<Button>(R.id.btnSaveConfig)
        val btnCalibrate = findViewById<Button>(R.id.btnCalibrate)
        val btnDebug = findViewById<Button>(R.id.btnDebug) // NEW DEBUG BUTTON

        val prefs = getSharedPreferences("ShoeAppPrefs", Context.MODE_PRIVATE)
        etShoeSimNumber.setText(prefs.getString("SHOE_SIM", ""))
        etEmergency1.setText(prefs.getString("EMG1", ""))
        etEmergency2.setText(prefs.getString("EMG2", ""))
        etEmergency3.setText(prefs.getString("EMG3", ""))

        btnSaveConfig.setOnClickListener {
            val shoeNumber = etShoeSimNumber.text.toString().replace(" ", "")
            val num1 = etEmergency1.text.toString().replace(" ", "")
            val num2 = etEmergency2.text.toString().replace(" ", "")
            val num3 = etEmergency3.text.toString().replace(" ", "")

            if (shoeNumber.isNotEmpty() && num1.isNotEmpty()) {
                prefs.edit()
                    .putString("SHOE_SIM", shoeNumber)
                    .putString("EMG1", num1)
                    .putString("EMG2", num2)
                    .putString("EMG3", num3)
                    .apply()

                val contactList = listOf(num1, num2, num3).filter { it.isNotEmpty() }.joinToString(",")

                val successMessage = "Contacts Sent! The shoe will update shortly."
                sendSmsCommand(shoeNumber, "SET_EMERGENCY:$contactList", successMessage)
            } else {
                Toast.makeText(this, "Shoe SIM and Contact 1 are required!", Toast.LENGTH_SHORT).show()
            }
        }

        btnCalibrate.setOnClickListener {
            val shoeNumber = etShoeSimNumber.text.toString().replace(" ", "")
            if (shoeNumber.isNotEmpty()) {
                val successMessage = "Calibration command sent! Please stand still on the shoe for 10 seconds."
                sendSmsCommand(shoeNumber, "CALIBRATE_FSR", successMessage)
            } else {
                Toast.makeText(this, "Please enter the Shoe SIM Number first.", Toast.LENGTH_SHORT).show()
            }
        }

        // NEW: Open BLE Monitor
        btnDebug.setOnClickListener {
            startActivity(Intent(this, DebugActivity::class.java))
        }
    }

    private fun sendSmsCommand(shoeNumber: String, rawCommand: String, toastMessage: String) {
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.SEND_SMS) != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this, arrayOf(Manifest.permission.SEND_SMS), 1)
            return
        }

        try {
            val smsManager = SmsManager.getDefault()
            smsManager.sendTextMessage(shoeNumber, null, rawCommand, null, null)
            Toast.makeText(this, toastMessage, Toast.LENGTH_LONG).show()
        } catch (e: Exception) {
            Toast.makeText(this, "Failed to send SMS", Toast.LENGTH_SHORT).show()
            e.printStackTrace()
        }
    }
}