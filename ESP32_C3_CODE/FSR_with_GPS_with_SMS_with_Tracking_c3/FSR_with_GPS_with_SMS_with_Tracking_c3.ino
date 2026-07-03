#include <TinyGPS++.h>
#include <HardwareSerial.h>

// =====================================================
//                    GPS SETUP (UART1)
// =====================================================
TinyGPSPlus gps;
HardwareSerial gpsSerial(1);  // Use UART1 for GPS
#define GPS_RX 7  // XIAO Pin D5 (GPIO 7) - Connect to GPS TX
#define GPS_TX 6  // XIAO Pin D4 (GPIO 6) - Connect to GPS RX

// =====================================================
//                  SIM800C SETUP (UART0)
// =====================================================
HardwareSerial sim800(0);     // Use UART0 for SIM800
#define SIM800_RX 20 // XIAO Pin D7 (Labeled RX) - Connect to SIM800 TX
#define SIM800_TX 21 // XIAO Pin D6 (Labeled TX) - Connect to SIM800 RX

// --- UPDATE THESE DETAILS ---
String emergencyNumber = "9372978724";
String apn = "airtelgprs.com";  // Change based on your SIM (e.g., "www" for Vi, "jionet" for Jio)
String deviceID = "545454";     // A unique 6-digit number you will register on Traccar

// =====================================================
//                    FSR SETUP
// =====================================================
#define FSR_PIN 3 // XIAO Pin D1 (GPIO 3)

int threshold = 2000;
const unsigned long tapGapMax = 700;
const unsigned long requiredPause = 500;
const unsigned long resetTimeout = 2000;

bool tapDetected = false;
int stage = 0;
int tapCount = 0;
unsigned long lastTapTime = 0;
int filteredValue = 0;

// =====================================================
//                 CLOUD TRACKING SETUP
// =====================================================
unsigned long lastCloudUpload = 0;
const unsigned long uploadInterval = 30000;  // Upload every 30 seconds (30,000 ms)

// =====================================================
//                    FUNCTIONS
// =====================================================

int readFSR() {
  long sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(FSR_PIN);
    delay(2);
  }
  int rawValue = sum / 10;
  filteredValue = (0.7 * filteredValue) + (0.3 * rawValue);
  return filteredValue;
}

void resetPattern() {
  stage = 0;
  tapCount = 0;
  Serial.println("Pattern Reset");
}

void sendSMS(String message) {
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
  sim800.write(26);  // CTRL + Z
  delay(5000);
  Serial.println("SMS SENT");
}

void handleSOS() {
  Serial.println("\n################################");
  Serial.println("######## SOS DETECTED #########");
  Serial.println("################################");

  if (gps.charsProcessed() < 10) {
    Serial.println("GPS MODULE NOT DETECTED");
    sendSMS("EMERGENCY ALERT!\nGPS module not detected.");
    return;
  }

  if (!gps.location.isValid()) {
    Serial.println("GPS CONNECTED BUT NO LOCK");
    sendSMS("EMERGENCY ALERT!\nGPS connected but no satellite lock.");
    return;
  }

  double latitude = gps.location.lat();
  double longitude = gps.location.lng();

  String mapsLink = "https://maps.google.com/?q=" + String(latitude, 6) + "," + String(longitude, 6);
  Serial.println(mapsLink);

  String smsMessage = "EMERGENCY ALERT!\n\nLocation:\n" + mapsLink;
  sendSMS(smsMessage);
}

// --- UPLOAD TO TRACCAR ---
void uploadToCloud(double lat, double lng) {
  Serial.println("Uploading location to Cloud...");

  // Format the OsmAnd protocol URL for Traccar
  String url = "http://demo2.traccar.org:5055/?id=" + deviceID + "&lat=" + String(lat, 6) + "&lon=" + String(lng, 6);

  // Initialize GPRS
  sim800.println("AT+SAPBR=3,1,\"Contype\",\"GPRS\"");
  delay(500);

  // Set APN
  sim800.print("AT+SAPBR=3,1,\"APN\",\"");
  sim800.print(apn);
  sim800.println("\"");
  delay(500);

  // Open GPRS context
  sim800.println("AT+SAPBR=1,1");
  delay(2000);  // Give it time to connect to network

  // Initialize HTTP
  sim800.println("AT+HTTPINIT");
  delay(500);
  sim800.println("AT+HTTPPARA=\"CID\",1");
  delay(500);

  // Set URL
  sim800.print("AT+HTTPPARA=\"URL\",\"");
  sim800.print(url);
  sim800.println("\"");
  delay(500);

  // Execute HTTP GET
  sim800.println("AT+HTTPACTION=0");
  delay(3000);  // Wait for server response

  // Terminate HTTP and GPRS
  sim800.println("AT+HTTPTERM");
  delay(500);
  sim800.println("AT+SAPBR=0,1");
  delay(500);

  Serial.println("Cloud Upload Complete.");
}

// =====================================================
//                      SETUP
// =====================================================

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  
  // HardwareSerial mappings for XIAO ESP32-C3
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
  sim800.begin(9600, SERIAL_8N1, SIM800_RX, SIM800_TX);

  delay(3000);
  Serial.println("==================================");
  Serial.println(" Smart Women Safety Insole ");
  Serial.println(" GPS + GSM + Cloud Tracking ");
  Serial.println("        (ESP32-C3)          ");
  Serial.println("==================================");

  sim800.println("AT");
}

// =====================================================
//                      LOOP
// =====================================================

void loop() {
  // 1. Read GPS Data
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }

  // 2. Read SIM800 Responses (for debugging)
  while (sim800.available()) {
    Serial.write(sim800.read());
  }

  unsigned long currentTime = millis();
  int fsrValue = readFSR();

  // 3. CLOUD TRACKING LOGIC (Runs every 30 seconds)
  if (currentTime - lastCloudUpload > uploadInterval) {
    if (gps.location.isValid()) {
      uploadToCloud(gps.location.lat(), gps.location.lng());
    } else {
      Serial.println("Skipping cloud upload: No GPS lock yet.");
    }
    lastCloudUpload = currentTime;  // Reset timer regardless of success to prevent spamming
  }

  // 4. TAP DETECTION LOGIC
  if (fsrValue > threshold && !tapDetected) {
    tapDetected = true;
    Serial.println("TAP DETECTED");

    if (currentTime - lastTapTime > resetTimeout) {
      resetPattern();
    }

    if (stage == 0) {
      tapCount++;
      Serial.print("Stage 0 Tap Count: ");
      Serial.println(tapCount);
      if (tapCount == 3) {
        stage = 1;
        Serial.println("3 Taps Completed\nWaiting For Pause...");
      }
    } else if (stage == 1) {
      unsigned long pauseTime = currentTime - lastTapTime;
      if (pauseTime > requiredPause) {
        stage = 2;
        tapCount = 1;
        Serial.println("Pause Accepted\nStage 2 Started");
      } else {
        Serial.println("Pause Too Short");
        resetPattern();
      }
    } else if (stage == 2) {
      if (currentTime - lastTapTime < tapGapMax) {
        tapCount++;
        Serial.print("Stage 2 Tap Count: ");
        Serial.println(tapCount);
        if (tapCount == 2) {
          handleSOS();
          resetPattern();
        }
      } else {
        Serial.println("Tap Gap Too Long");
        resetPattern();
      }
    }
    lastTapTime = currentTime;
  }

  // 5. RELEASE DETECTION
  if (fsrValue < 1200 && (millis() - lastTapTime > 80)) {
    tapDetected = false;
  }

  delay(20);
}