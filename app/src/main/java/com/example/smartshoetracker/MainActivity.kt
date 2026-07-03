package com.example.smartshoetracker // UPDATE PACKAGE NAME

import android.content.Intent
import android.graphics.Color
import android.media.RingtoneManager
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import com.google.android.gms.maps.CameraUpdateFactory
import com.google.android.gms.maps.GoogleMap
import com.google.android.gms.maps.OnMapReadyCallback
import com.google.android.gms.maps.SupportMapFragment
import com.google.android.gms.maps.model.LatLng
import com.google.android.gms.maps.model.Marker
import com.google.android.gms.maps.model.MarkerOptions
import com.google.android.gms.maps.model.Polyline
import com.google.android.gms.maps.model.PolylineOptions
import okhttp3.Credentials
import okhttp3.OkHttpClient
import retrofit2.Call
import retrofit2.Callback
import retrofit2.Response
import retrofit2.Retrofit
import retrofit2.converter.gson.GsonConverterFactory
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.TimeZone

class MainActivity : AppCompatActivity(), OnMapReadyCallback {

    private lateinit var mMap: GoogleMap
    private var shoeMarker: Marker? = null

    // Route History components
    private var routePolyline: Polyline? = null
    private val routePoints = mutableListOf<LatLng>()

    private val handler = Handler(Looper.getMainLooper())
    private val updateInterval = 10000L

    private lateinit var traccarApi: TraccarApi
    private lateinit var session: SessionManager

    private lateinit var tvSpeed: TextView
    private lateinit var tvTime: TextView
    private var isAlarmActive = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        session = SessionManager(this)
        tvSpeed = findViewById(R.id.tvSpeed)
        tvTime = findViewById(R.id.tvTime)

        findViewById<Button>(R.id.btnLogout).setOnClickListener {
            session.logout()
            startActivity(Intent(this, LoginActivity::class.java))
            finish()
        }
        findViewById<Button>(R.id.btnSettings).setOnClickListener {
            startActivity(Intent(this, SettingsActivity::class.java))
        }

        setupNetworkClient()

        val mapFragment = supportFragmentManager.findFragmentById(R.id.map) as SupportMapFragment
        mapFragment.getMapAsync(this)
    }

    private fun setupNetworkClient() {
        val email = session.getEmail() ?: ""
        val pass = session.getPass() ?: ""

        val client = OkHttpClient.Builder().addInterceptor { chain ->
            val request = chain.request().newBuilder()
                .header("Authorization", Credentials.basic(email, pass))
                .header("Accept", "application/json")
                .build()
            chain.proceed(request)
        }.build()

        val retrofit = Retrofit.Builder()
            .baseUrl("https://demo2.traccar.org")
            .client(client)
            .addConverterFactory(GsonConverterFactory.create())
            .build()

        traccarApi = retrofit.create(TraccarApi::class.java)
    }

    override fun onMapReady(googleMap: GoogleMap) {
        mMap = googleMap

        // Fetch the user's registered device to get its ID, then fetch history!
        fetchDeviceAndHistory()
    }

    private fun fetchDeviceAndHistory() {
        traccarApi.getDevices().enqueue(object : Callback<List<TraccarDevice>> {
            override fun onResponse(call: Call<List<TraccarDevice>>, response: Response<List<TraccarDevice>>) {
                if (response.isSuccessful) {
                    val devices = response.body()
                    if (!devices.isNullOrEmpty()) {
                        val deviceId = devices[0].id
                        loadRouteHistory(deviceId)
                    } else {
                        // NEW USER: No devices found! Let's automatically link the shoe.
                        linkUserShoe()
                    }
                }
            }
            override fun onFailure(call: Call<List<TraccarDevice>>, t: Throwable) {
                Log.e("Traccar", "Failed to get devices")
            }
        })
    }

    // --- UPDATED: DYNAMIC AUTO LINK SHOE FUNCTION ---
    private fun linkUserShoe() {
        val userDeviceId = session.getDeviceId() ?: "000000"

        Toast.makeText(this, "Linking Shoe ID $userDeviceId to account...", Toast.LENGTH_SHORT).show()
        val newDevice = HashMap<String, String>()
        newDevice["name"] = "My Smart Shoe"
        newDevice["uniqueId"] = userDeviceId

        traccarApi.addDevice(newDevice).enqueue(object : Callback<TraccarDevice> {
            override fun onResponse(call: Call<TraccarDevice>, response: Response<TraccarDevice>) {
                if (response.isSuccessful && response.body() != null) {
                    Toast.makeText(this@MainActivity, "Shoe Linked Successfully!", Toast.LENGTH_SHORT).show()
                    val newDeviceId = response.body()!!.id
                    loadRouteHistory(newDeviceId)
                } else {
                    Toast.makeText(this@MainActivity, "Failed to link. ID $userDeviceId might be taken by another account!", Toast.LENGTH_LONG).show()
                }
            }
            override fun onFailure(call: Call<TraccarDevice>, t: Throwable) {
                Toast.makeText(this@MainActivity, "Network error while linking", Toast.LENGTH_SHORT).show()
            }
        })
    }

    private fun loadRouteHistory(deviceId: Int) {
        // Fetch history for the last 2 hours to draw the initial route
        val sdf = SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'", Locale.US)
        sdf.timeZone = TimeZone.getTimeZone("UTC")
        val toStr = sdf.format(Date())
        val fromStr = sdf.format(Date(System.currentTimeMillis() - 2 * 60 * 60 * 1000))

        traccarApi.getHistory(deviceId, fromStr, toStr).enqueue(object : Callback<List<TraccarPosition>> {
            override fun onResponse(call: Call<List<TraccarPosition>>, response: Response<List<TraccarPosition>>) {
                if (response.isSuccessful && response.body() != null) {
                    val history = response.body()!!
                    routePoints.clear()

                    for (pos in history) {
                        routePoints.add(LatLng(pos.latitude, pos.longitude))
                    }

                    // Draw the blue route line
                    if (routePoints.isNotEmpty()) {
                        routePolyline = mMap.addPolyline(PolylineOptions().addAll(routePoints).color(Color.BLUE).width(12f))

                        // Set the pin to the last known position
                        val latest = history.last()
                        updateMapAndTelemetry(latest)
                    }
                }
                // Once history is loaded, start the 10-second live updates
                startLiveUpdates()
            }
            override fun onFailure(call: Call<List<TraccarPosition>>, t: Throwable) {
                startLiveUpdates() // start anyway if history fails
            }
        })
    }

    private fun startLiveUpdates() {
        val runnable = object : Runnable {
            override fun run() {
                traccarApi.getLatestPositions().enqueue(object : Callback<List<TraccarPosition>> {
                    override fun onResponse(call: Call<List<TraccarPosition>>, response: Response<List<TraccarPosition>>) {
                        if (response.isSuccessful && !response.body().isNullOrEmpty()) {
                            val latestPos = response.body()!![0]
                            updateMapAndTelemetry(latestPos)

                            // Add new point to our polyline trail
                            routePoints.add(LatLng(latestPos.latitude, latestPos.longitude))
                            routePolyline?.points = routePoints
                        }
                    }
                    override fun onFailure(call: Call<List<TraccarPosition>>, t: Throwable) {}
                })
                handler.postDelayed(this, updateInterval)
            }
        }
        handler.post(runnable)
    }

    private fun updateMapAndTelemetry(pos: TraccarPosition) {
        val shoeLocation = LatLng(pos.latitude, pos.longitude)
        // Check for SOS Alarm Flag from Traccar
        val alarm = pos.attributes?.get("alarm")
        if (alarm == "sos" && !isAlarmActive) {
            isAlarmActive = true
            triggerRedAlertScreen()
        }
        // Update UI Text
        tvSpeed.text = "Speed: ${String.format("%.1f", pos.speed)} knots"

        // Clean up the timestamp format
        val cleanTime = pos.fixTime.substringBefore(".").replace("T", " ")
        tvTime.text = "Last Update: $cleanTime (UTC)"

        // Update Map Marker
        if (shoeMarker == null) {
            shoeMarker = mMap.addMarker(MarkerOptions().position(shoeLocation).title("Shoe Live"))
            mMap.animateCamera(CameraUpdateFactory.newLatLngZoom(shoeLocation, 17f))
        } else {
            shoeMarker?.position = shoeLocation
            mMap.animateCamera(CameraUpdateFactory.newLatLng(shoeLocation))
        }
    }
    private fun triggerRedAlertScreen() {
        // Change card background to red
        findViewById<LinearLayout>(R.id.telemetryLayout)?.setBackgroundColor(Color.RED)
        tvSpeed.setTextColor(Color.WHITE)
        tvTime.setTextColor(Color.WHITE)
        Toast.makeText(this, "🚨 EMERGENCY SOS TRIGGERED! 🚨", Toast.LENGTH_LONG).show()

        // Play default android alarm sound
        try {
            val notification = RingtoneManager.getDefaultUri(RingtoneManager.TYPE_ALARM)
            val r = RingtoneManager.getRingtone(applicationContext, notification)
            r.play()
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }
}
