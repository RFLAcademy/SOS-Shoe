package com.example.smartshoetracker // UPDATE PACKAGE NAME

import android.content.Intent
import android.os.Bundle
import android.widget.Button
import android.widget.EditText
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import okhttp3.OkHttpClient
import retrofit2.Call
import retrofit2.Callback
import retrofit2.Response
import retrofit2.Retrofit
import retrofit2.converter.gson.GsonConverterFactory

class LoginActivity : AppCompatActivity() {

    private lateinit var session: SessionManager

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        session = SessionManager(this)

        // If already logged in, skip straight to Map!
        if (session.isLoggedIn()) {
            startActivity(Intent(this, MainActivity::class.java))
            finish()
            return
        }

        setContentView(R.layout.activity_login)

        val etEmail = findViewById<EditText>(R.id.etEmail)
        val etPassword = findViewById<EditText>(R.id.etPassword)
        val etDeviceId = findViewById<EditText>(R.id.etDeviceId)

        val btnLogin = findViewById<Button>(R.id.btnLogin)
        val btnRegister = findViewById<Button>(R.id.btnRegister)

        btnLogin.setOnClickListener {
            val email = etEmail.text.toString()
            val pass = etPassword.text.toString()
            val deviceId = etDeviceId.text.toString()

            if (email.isNotEmpty() && pass.isNotEmpty() && deviceId.isNotEmpty()) {
                session.saveLogin(email, pass, deviceId)
                Toast.makeText(this, "Logged In!", Toast.LENGTH_SHORT).show()
                startActivity(Intent(this, MainActivity::class.java))
                finish()
            } else {
                Toast.makeText(this, "Please enter email, password, and Device ID", Toast.LENGTH_SHORT).show()
            }
        }

        btnRegister.setOnClickListener {
            val email = etEmail.text.toString()
            val pass = etPassword.text.toString()
            val deviceId = etDeviceId.text.toString()

            if (email.isNotEmpty() && pass.isNotEmpty() && deviceId.isNotEmpty()) {
                // Save credentials to session immediately so auto-link has access to Device ID
                session.saveLogin(email, pass, deviceId)
                registerOnTraccar(email, pass)
            } else {
                Toast.makeText(this, "Enter email, password, and Device ID to sign up", Toast.LENGTH_LONG).show()
            }
        }
    }

    private fun registerOnTraccar(email: String, pass: String) {
        // Open Registration requires NO authentication headers
        val retrofit = Retrofit.Builder()
            .baseUrl("https://demo2.traccar.org")
            .addConverterFactory(GsonConverterFactory.create())
            .build()
        val api = retrofit.create(TraccarApi::class.java)

        val userData = HashMap<String, String>()
        userData["name"] = "Shoe User"
        userData["email"] = email
        userData["password"] = pass

        api.registerUser(userData).enqueue(object : Callback<Void> {
            override fun onResponse(call: Call<Void>, response: Response<Void>) {
                if (response.isSuccessful) {
                    Toast.makeText(this@LoginActivity, "Registered! You can now login.", Toast.LENGTH_LONG).show()
                } else {
                    Toast.makeText(this@LoginActivity, "Registration failed or email taken.", Toast.LENGTH_LONG).show()
                }
            }
            override fun onFailure(call: Call<Void>, t: Throwable) {
                Toast.makeText(this@LoginActivity, "Network Error", Toast.LENGTH_SHORT).show()
            }
        })
    }
}