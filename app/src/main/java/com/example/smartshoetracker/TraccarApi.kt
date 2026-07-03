package com.example.smartshoetracker // UPDATE PACKAGE NAME

import retrofit2.Call
import retrofit2.http.Body
import retrofit2.http.GET
import retrofit2.http.POST
import retrofit2.http.Query

interface TraccarApi {
    // 1. Get User's Devices
    @GET("/api/devices")
    fun getDevices(): Call<List<TraccarDevice>>

    // 2. Get Live Latest Position
    @GET("/api/positions")
    fun getLatestPositions(): Call<List<TraccarPosition>>

    // 3. Get Route History for a specific time window
    @GET("/api/positions")
    fun getHistory(
        @Query("deviceId") deviceId: Int,
        @Query("from") from: String,
        @Query("to") to: String
    ): Call<List<TraccarPosition>>

    // 4. Register a new User
    @POST("/api/users")
    fun registerUser(@Body userData: HashMap<String, String>): Call<Void>

    // 5. Add a Device to Account
    @POST("/api/devices")
    fun addDevice(@Body deviceData: HashMap<String, String>): Call<TraccarDevice>
}