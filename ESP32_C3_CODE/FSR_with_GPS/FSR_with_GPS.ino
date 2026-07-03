#include <TinyGPS++.h>
#include <HardwareSerial.h>

// ---------------- GPS ----------------
TinyGPSPlus gps;

// RX, TX for GPS
HardwareSerial gpsSerial(1);

// ---------------- FSR ----------------
#define FSR_PIN 34

// ---------------- SETTINGS ----------------
int threshold = 2000;

const unsigned long tapGapMax = 700;
const unsigned long requiredPause = 500;
const unsigned long resetTimeout = 2000;

// ---------------- VARIABLES ----------------
bool tapDetected = false;

int stage = 0;
int tapCount = 0;

unsigned long lastTapTime = 0;

// Filtering Variable
int filteredValue = 0;

// ---------------- READ FSR ----------------
int readFSR()
{
    long sum = 0;

    // Average readings
    for(int i = 0; i < 10; i++)
    {
        sum += analogRead(FSR_PIN);
        delay(2);
    }

    int rawValue = sum / 10;

    // -------- SMOOTHING FILTER --------
    filteredValue = (0.7 * filteredValue) + (0.3 * rawValue);

    return filteredValue;
}

// ---------------- RESET PATTERN ----------------
void resetPattern()
{
    stage = 0;
    tapCount = 0;

    Serial.println("Pattern Reset");
}

// ---------------- PRINT GPS ----------------
void printGPSLocation()
{
    Serial.println("");
    Serial.println("========= GPS STATUS =========");

    // -------- CASE 1 : GPS NOT CONNECTED --------
    // No GPS characters received
    if(gps.charsProcessed() < 10)
    {
        Serial.println("GPS MODULE NOT DETECTED");
        Serial.println("Check Wiring / Power");

        Serial.println("================================");
        Serial.println("");

        return;
    }

    // -------- CASE 2 : GPS CONNECTED BUT NO LOCK --------
    if(!gps.location.isValid())
    {
        Serial.println("GPS CONNECTED");
        Serial.println("WAITING FOR SATELLITE LOCK");

        Serial.print("Satellites Detected: ");

        if(gps.satellites.isValid())
        {
            Serial.println(gps.satellites.value());
        }
        else
        {
            Serial.println("Unknown");
        }

        Serial.println("Move Outdoors For Better Signal");

        Serial.println("================================");
        Serial.println("");

        return;
    }

    // -------- CASE 3 : GPS LOCK SUCCESS --------
    Serial.println("GPS LOCK SUCCESSFUL");

    Serial.print("Latitude: ");
    Serial.println(gps.location.lat(), 6);

    Serial.print("Longitude: ");
    Serial.println(gps.location.lng(), 6);

    Serial.print("Satellites: ");

    if(gps.satellites.isValid())
    {
        Serial.println(gps.satellites.value());
    }
    else
    {
        Serial.println("Unknown");
    }

    Serial.println("");

    Serial.println("Google Maps Link:");
    Serial.print("https://maps.google.com/?q=");
    Serial.print(gps.location.lat(), 6);
    Serial.print(",");
    Serial.println(gps.location.lng(), 6);

    Serial.println("================================");
    Serial.println("");
}

// ---------------- SETUP ----------------
void setup()
{
    Serial.begin(115200);

    analogReadResolution(12);

    // GPS Serial
    // RX = GPIO16
    // TX = GPIO17
    gpsSerial.begin(9600, SERIAL_8N1, 16, 17);

    Serial.println("==================================");
    Serial.println(" Smart Women Safety Insole ");
    Serial.println(" GPS + 3-2 SOS Detection ");
    Serial.println("==================================");
}

// ---------------- MAIN LOOP ----------------
void loop()
{
    // -------- READ GPS DATA --------
    while(gpsSerial.available() > 0)
    {
        gps.encode(gpsSerial.read());
    }

    int fsrValue = readFSR();

    unsigned long currentTime = millis();

    // -------- SERIAL OUTPUT --------
    // Serial.print("FSR Value: ");
    // Serial.println(fsrValue);

    // -------- TAP DETECTION --------
    if(fsrValue > threshold && !tapDetected)
    {
        tapDetected = true;

        Serial.println("TAP DETECTED");

        // Reset if too much time between taps
        if(currentTime - lastTapTime > resetTimeout)
        {
            resetPattern();
        }

        // -------- STAGE 0 --------
        if(stage == 0)
        {
            tapCount++;

            Serial.print("Stage 0 Tap Count: ");
            Serial.println(tapCount);

            if(tapCount == 3)
            {
                stage = 1;

                Serial.println("3 Taps Completed");
                Serial.println("Waiting For Pause...");
            }
        }

        // -------- STAGE 1 --------
        else if(stage == 1)
        {
            unsigned long pauseTime = currentTime - lastTapTime;

            if(pauseTime > requiredPause)
            {
                stage = 2;
                tapCount = 1;

                Serial.println("Pause Accepted");
                Serial.println("Stage 2 Started");
            }
            else
            {
                Serial.println("Pause Too Short");

                resetPattern();
            }
        }

        // -------- STAGE 2 --------
        else if(stage == 2)
        {
            if(currentTime - lastTapTime < tapGapMax)
            {
                tapCount++;

                Serial.print("Stage 2 Tap Count: ");
                Serial.println(tapCount);

                if(tapCount == 2)
                {
                    Serial.println("");
                    Serial.println("################################");
                    Serial.println("######## SOS DETECTED #########");
                    Serial.println("################################");

                    // -------- PRINT GPS --------
                    printGPSLocation();

                    delay(2000);
                    
                    resetPattern();
                }
            }
            else
            {
                Serial.println("Tap Gap Too Long");

                resetPattern();
            }
        }

        // Store latest tap time
        lastTapTime = currentTime;
    }

    // -------- RELEASE DETECTION --------
    if(fsrValue < 1200 && (millis() - lastTapTime > 80))
    {
        tapDetected = false;
    }

    delay(20);
}