Smart Women Safety Insole - Companion App 🥿🚨

This is the official Android companion application for the Smart Women Safety Insole capstone project.

The system consists of a custom hardware insole (built with an ESP32-C3, SIM800C GPRS/GSM module, GPS, and an FSR sensor) and this Kotlin-based Android application. The app connects to a Traccar open-source backend to provide real-time location monitoring and emergency alert management.

🌟 Key Features

Live GPS Tracking: Integrates with the Google Maps SDK and Traccar API to display the real-time location of the shoe.

Route History: Automatically fetches and draws a polyline of the user's route history for the last 2 hours.

Instant SOS Alerts: If the user triggers the FSR sensor with a specific 3-taps, pause, 2-taps pattern, the app receives an immediate SOS flag and triggers a visual and auditory alarm.

Dynamic Hardware Configuration: Users can update the shoe's emergency contacts directly from the app. The app uses Android's native SmsManager to send hidden configuration commands (e.g., SET_EMERGENCY:9999999999) to the shoe, which saves the new numbers to its non-volatile flash memory.

Live Telemetry: Displays real-time speed (in knots) and UTC timestamps of the wearer's movements.

🛠️ Tech Stack

Language: Kotlin

Architecture: Android Views / XML Layouts

Maps: Google Maps API (play-services-maps)

Networking: Retrofit2 & OkHttp (for Traccar REST API communication)

Hardware Interfacing: SMS-based command transmission

🚀 How It Works

The Hardware: An ESP32 reads an FSR sensor. Upon detecting the SOS pattern, it sends an SMS to emergency contacts and pushes an &alarm=sos flag to the Traccar server via GPRS.

The Server: Traccar logs the GPS coordinates and the SOS flag.

The App: This Android app constantly polls the Traccar API using Retrofit. It updates the Google Map UI with the latest coordinates and triggers an alarm screen if the SOS flag is detected.

🔒 Setup Note

To run this project locally, you must provide your own MAPS_API_KEY in the local.properties file and set up a Traccar server instance.
