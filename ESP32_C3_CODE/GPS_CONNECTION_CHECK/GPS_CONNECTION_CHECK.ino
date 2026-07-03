#include <TinyGPSPlus.h>

TinyGPSPlus gps;

// UART2 on ESP32
HardwareSerial gpsSerial(1);

void setup()
{
  Serial.begin(115200);

  // RX = GPIO16
  // TX = GPIO17
  gpsSerial.begin(9600, SERIAL_8N1, 7, 6);

  Serial.println();
  Serial.println("ESP32 Neo-6M Connection Test");
  Serial.println("Waiting for GPS serial data...");
}

void loop()
{
  while (gpsSerial.available())
  {
    char c = gpsSerial.read();

    // Print raw GPS data
    Serial.print(c);

    // Feed TinyGPS++
    gps.encode(c);
  }

  // Every 3 sec print status
  static unsigned long lastCheck = 0;

  if (millis() - lastCheck > 3000)
  {
    lastCheck = millis();

    Serial.println();
    Serial.println("----------- STATUS -----------");

    Serial.print("Characters Received: ");
    Serial.println(gps.charsProcessed());

    Serial.print("Sentences With Fix: ");
    Serial.println(gps.sentencesWithFix());

    Serial.print("Checksum Failed: ");
    Serial.println(gps.failedChecksum());

    if (gps.charsProcessed() > 10)
    {
      Serial.println("GPS MODULE CONNECTED ✔");
    }
    else
    {
      Serial.println("NO GPS DATA RECEIVED ✖");
      Serial.println("Check wiring / baud rate");
    }

    if (gps.location.isValid())
    {
      Serial.println("GPS FIX ACQUIRED ✔");
    }
    else
    {
      Serial.println("NO GPS FIX YET");
      Serial.println("(This is normal indoors)");
    }

    Serial.println("------------------------------");
  }
}