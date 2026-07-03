#include <TinyGPS++.h>
#include <HardwareSerial.h>

// =====================================================
//                    GPS SETUP
// =====================================================

TinyGPSPlus gps;

// GPS UART1
HardwareSerial gpsSerial(1);

// GPS Pins
#define GPS_RX 16
#define GPS_TX 17

// =====================================================
//                   SIM800C SETUP
// =====================================================

// SIM800 UART2
HardwareSerial sim800(2);

// SIM800 Pins
#define SIM800_RX 26
#define SIM800_TX 27

// Emergency Number
String emergencyNumber = "9372978724";

// =====================================================
//                    FSR SETUP
// =====================================================

#define FSR_PIN 34

// =====================================================
//                    SETTINGS
// =====================================================

int threshold = 2000;

const unsigned long tapGapMax = 700;
const unsigned long requiredPause = 500;
const unsigned long resetTimeout = 2000;

// =====================================================
//                    VARIABLES
// =====================================================

bool tapDetected = false;

int stage = 0;
int tapCount = 0;

unsigned long lastTapTime = 0;

// Filtering Variable
int filteredValue = 0;

// =====================================================
//                   READ FSR
// =====================================================

int readFSR()
{
    long sum = 0;

    for(int i = 0; i < 10; i++)
    {
        sum += analogRead(FSR_PIN);
        delay(2);
    }

    int rawValue = sum / 10;

    // Smoothing Filter
    filteredValue = (0.7 * filteredValue) + (0.3 * rawValue);

    return filteredValue;
}

// =====================================================
//                 RESET PATTERN
// =====================================================

void resetPattern()
{
    stage = 0;
    tapCount = 0;

    Serial.println("Pattern Reset");
}

// =====================================================
//                  SEND SMS
// =====================================================

void sendSMS(String message)
{
    Serial.println("Sending SMS...");

    sim800.println("AT");
    delay(1000);

    sim800.println("AT+CMGF=1");
    delay(1000);

    sim800.print("AT+CMGS=\"");
    sim800.print(emergencyNumber);
    sim800.println("\"");

    delay(1000);

    sim800.print(message);

    delay(500);

    // CTRL + Z
    sim800.write(26);

    delay(5000);

    Serial.println("SMS SENT");
}

// =====================================================
//               GPS + SMS FUNCTION
// =====================================================

void handleSOS()
{
    Serial.println("");
    Serial.println("################################");
    Serial.println("######## SOS DETECTED #########");
    Serial.println("################################");

    // ----------------------------------------
    // CASE 1 : GPS MODULE NOT DETECTED
    // ----------------------------------------
    if(gps.charsProcessed() < 10)
    {
        Serial.println("GPS MODULE NOT DETECTED");

        sendSMS(
            "EMERGENCY ALERT!\n"
            "GPS module not detected."
        );

        return;
    }

    // ----------------------------------------
    // CASE 2 : GPS CONNECTED BUT NO LOCK
    // ----------------------------------------
    if(!gps.location.isValid())
    {
        Serial.println("GPS CONNECTED BUT NO LOCK");

        sendSMS(
            "EMERGENCY ALERT!\n"
            "GPS connected but no satellite lock."
        );

        return;
    }

    // ----------------------------------------
    // CASE 3 : GPS LOCK SUCCESS
    // ----------------------------------------

    double latitude = gps.location.lat();
    double longitude = gps.location.lng();

    Serial.println("GPS LOCK SUCCESS");

    Serial.print("Latitude: ");
    Serial.println(latitude, 6);

    Serial.print("Longitude: ");
    Serial.println(longitude, 6);

    String mapsLink =
        "https://maps.google.com/?q=" +
        String(latitude, 6) +
        "," +
        String(longitude, 6);

    Serial.println(mapsLink);

    String smsMessage =
        "EMERGENCY ALERT!\n\n"
        "Location:\n" +
        mapsLink;

    sendSMS(smsMessage);
}

// =====================================================
//                      SETUP
// =====================================================

void setup()
{
    Serial.begin(115200);

    analogReadResolution(12);

    // GPS Serial
    gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);

    // SIM800 Serial
    sim800.begin(9600, SERIAL_8N1, SIM800_RX, SIM800_TX);

    delay(3000);

    Serial.println("==================================");
    Serial.println(" Smart Women Safety Insole ");
    Serial.println(" GPS + GSM + SOS Detection ");
    Serial.println("==================================");

    // Test SIM800
    sim800.println("AT");
}

// =====================================================
//                       LOOP
// =====================================================

void loop()
{
    // ----------------------------------------
    // READ GPS DATA
    // ----------------------------------------

    while(gpsSerial.available() > 0)
    {
        gps.encode(gpsSerial.read());
    }

    // ----------------------------------------
    // READ SIM800 RESPONSES
    // ----------------------------------------

    while(sim800.available())
    {
        Serial.write(sim800.read());
    }

    int fsrValue = readFSR();

    unsigned long currentTime = millis();

    // ----------------------------------------
    // TAP DETECTION
    // ----------------------------------------

    if(fsrValue > threshold && !tapDetected)
    {
        tapDetected = true;

        Serial.println("TAP DETECTED");

        // Reset if timeout exceeded
        if(currentTime - lastTapTime > resetTimeout)
        {
            resetPattern();
        }

        // ----------------------------------------
        // STAGE 0
        // ----------------------------------------

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

        // ----------------------------------------
        // STAGE 1
        // ----------------------------------------

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

        // ----------------------------------------
        // STAGE 2
        // ----------------------------------------

        else if(stage == 2)
        {
            if(currentTime - lastTapTime < tapGapMax)
            {
                tapCount++;

                Serial.print("Stage 2 Tap Count: ");
                Serial.println(tapCount);

                if(tapCount == 2)
                {
                    // ----------------------------------------
                    // SOS TRIGGER
                    // ----------------------------------------

                    handleSOS();

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

    // ----------------------------------------
    // RELEASE DETECTION
    // ----------------------------------------

    if(fsrValue < 1200 && (millis() - lastTapTime > 80))
    {
        tapDetected = false;
    }

    delay(20);
}