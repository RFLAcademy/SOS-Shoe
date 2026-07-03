package com.example.smartshoetracker // UPDATE PACKAGE NAME
import com.google.gson.annotations.SerializedName

data class TraccarPosition(
    @SerializedName("deviceId") val deviceId: Int,
    @SerializedName("latitude") val latitude: Double,
    @SerializedName("longitude") val longitude: Double,
    @SerializedName("speed") val speed: Double,
    @SerializedName("fixTime") val fixTime: String,
    @SerializedName("attributes") val attributes: Map<String, String>?
)

data class TraccarDevice(
    @SerializedName("id") val id: Int,
    @SerializedName("name") val name: String
)
