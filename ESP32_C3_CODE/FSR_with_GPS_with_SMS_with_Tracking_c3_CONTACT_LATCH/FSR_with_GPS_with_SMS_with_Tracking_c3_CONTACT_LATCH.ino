#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <Preferences.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// =====================================================
//                 POWER, CHARGING & MOTOR SETUP
// =====================================================
#define RELAY_PIN 2
#define VBUS_PIN 4
#define MOTOR_PIN 5

unsigned long lastActivityTime = 0;
int lastActivityFsr = 0;
const unsigned long autoOffTimeout = 120000; // 2 minutes (120,000 ms)

// Motor Timer Variables
bool motorActive = false;
unsigned long motorStartTime = 0;
const unsigned long motorDuration = 5000; // 5 seconds

// =====================================================
//                    BLE UART SETUP
// =====================================================
BLEServer *pServer = NULL;
BLECharacteristic * pTxCharacteristic;
bool deviceConnected = false;

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) { deviceConnected = true; };
    void onDisconnect(BLEServer* pServer) { deviceConnected = false; }
};

// Custom Print Function - Sends to USB Serial AND Bluetooth!
void blePrintln(String msg) {
  Serial.println(msg);
  if (deviceConnected) {
    pTxCharacteristic->setValue(msg.c_str());
    pTxCharacteristic->notify();
    delay(10); // Small delay to prevent Bluetooth congestion
  }
}

// =====================================================
//                    PREFERENCES (EEPROM)
// =====================================================
Preferences preferences;

// =====================================================
//                    GPS & SIM800 SETUP
// =====================================================
TinyGPSPlus gps;
HardwareSerial gpsSerial(1);
#define GPS_RX 7
#define GPS_TX 6

HardwareSerial sim800(0);
#define SIM800_RX 20
#define SIM800_TX 21

#define MAX_CONTACTS 3
String emergencyContacts[MAX_CONTACTS] = {"9372978724", "", ""};
String apn = "airtelgprs.com";
String deviceID = "545454";

// =====================================================
//                    ADAPTIVE FSR SETUP
// =====================================================
#define FSR_PIN 3
int threshold = 2000; 

// Adaptive Tap Variables
unsigned long tapTimes[5];
unsigned long avgTapInterval = 0;
const unsigned long MAX_INITIAL_GAP = 1500;  // Max gap between regular taps
const unsigned long SEQUENCE_TIMEOUT = 3000; // Reset everything if sequence takes too long

bool tapDetected = false;
int stage = 0;
int tapCount = 0;
unsigned long lastTapTime = 0;
int filteredValue = 0;

unsigned long lastCloudUpload = 0;
const unsigned long uploadInterval = 30000;
String incomingSMS = "";

// =====================================================
//                    FUNCTIONS
// =====================================================

int readFSR() {
  int rawValue = analogRead(FSR_PIN);
  filteredValue = rawValue;
  return filteredValue;
}

void resetPattern() {
  stage = 0;
  tapCount = 0;
  avgTapInterval = 0;
  blePrintln("Pattern Reset");
}

void sendSMS(String message, String number) {
  if (number.length() < 10) return; 

  blePrintln("Sending SMS to: " + number);
  sim800.println("AT");
  delay(1000);
  sim800.println("AT+CMGF=1");
  delay(1000);
  sim800.print("AT+CMGS=\"");
  sim800.print(number);
  sim800.println("\"");
  delay(1000);
  sim800.print(message);
  delay(500);
  sim800.write(26);  
  delay(5000);
  blePrintln("SMS SENT");
}

void uploadToCloud(double lat, double lng, bool isSOS) {
  blePrintln("Uploading location to Cloud...");
  String url = "http://demo2.traccar.org:5055/?id=" + deviceID + "&lat=" + String(lat, 6) + "&lon=" + String(lng, 6);
  if (isSOS) url += "&alarm=sos";

  sim800.println("AT+SAPBR=3,1,\"Contype\",\"GPRS\"");
  delay(500);
  sim800.print("AT+SAPBR=3,1,\"APN\",\"");
  sim800.print(apn);
  sim800.println("\"");
  delay(500);
  sim800.println("AT+SAPBR=1,1");
  delay(2000);
  sim800.println("AT+HTTPINIT");
  delay(500);
  sim800.println("AT+HTTPPARA=\"CID\",1");
  delay(500);
  sim800.print("AT+HTTPPARA=\"URL\",\"");
  sim800.print(url);
  sim800.println("\"");
  delay(500);
  sim800.println("AT+HTTPACTION=0");
  delay(3000);
  sim800.println("AT+HTTPTERM");
  delay(500);
  sim800.println("AT+SAPBR=0,1");
  delay(500);
  blePrintln("Cloud Upload Complete.");
}

void handleSOS() {
  blePrintln("\n################################");
  blePrintln("######## SOS DETECTED #########");
  blePrintln("################################");

  // --- START VIBRATION MOTOR INSTANTLY ---
  digitalWrite(MOTOR_PIN, HIGH);
  motorActive = true;
  motorStartTime = millis();

  if (gps.charsProcessed() < 10) {
    blePrintln("GPS MODULE NOT DETECTED");
    for (int i = 0; i < MAX_CONTACTS; i++) {
      if (emergencyContacts[i].length() >= 10) {
          sendSMS("EMERGENCY ALERT!\nGPS module not detected.", emergencyContacts[i]);
          delay(1000);
      }
    }
    return;
  }

  if (!gps.location.isValid()) {
    blePrintln("GPS CONNECTED BUT NO LOCK");
    for (int i = 0; i < MAX_CONTACTS; i++) {
      if (emergencyContacts[i].length() >= 10) {
          sendSMS("EMERGENCY ALERT!\nGPS connected but no satellite lock.", emergencyContacts[i]);
          delay(1000);
      }
    }
    return;
  }

  double latitude = gps.location.lat();
  double longitude = gps.location.lng();
  String mapsLink = "https://maps.google.com/?q=" + String(latitude, 6) + "," + String(longitude, 6);
  blePrintln(mapsLink);

  String smsMessage = "EMERGENCY ALERT!\n\nLocation:\n" + mapsLink;
  for (int i = 0; i < MAX_CONTACTS; i++) {
    if (emergencyContacts[i].length() >= 10) {
        sendSMS(smsMessage, emergencyContacts[i]);
        delay(1000); 
    }
  }
  
  uploadToCloud(latitude, longitude, true);
}

// =====================================================
//                      SETUP
// =====================================================
void setup() {
  // --- LATCH POWER FIRST ---
  // Do this immediately so the limit switch can be released
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); 
  
  // Setup Charger Detection Pin
  pinMode(VBUS_PIN, INPUT);

  // Setup Vibration Motor Pin
  pinMode(MOTOR_PIN, OUTPUT);
  digitalWrite(MOTOR_PIN, LOW); // Ensure it is off on boot

  Serial.begin(115200);
  analogReadResolution(12);

  // --- BLE SETUP ---
  BLEDevice::init("SmartShoe_BLE");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
  pTxCharacteristic->addDescriptor(new BLE2902());
  pService->start();
  pServer->getAdvertising()->start();
  Serial.println("BLE Started. Waiting for connections...");

  // --- STANDARD SETUP ---
  preferences.begin("shoe_config", false);
  threshold = preferences.getInt("fsr_thresh", 2000);
  
  blePrintln("==================================");
  blePrintln(" Smart Women Safety Insole ");
  blePrintln("==================================");
  blePrintln("FSR Threshold: " + String(threshold));
  
  for (int i = 0; i < MAX_CONTACTS; i++) {
      String key = "em_contact" + String(i);
      emergencyContacts[i] = preferences.getString(key.c_str(), emergencyContacts[i]);
      if (emergencyContacts[i].length() > 0) {
          blePrintln("Loaded Emergency #" + String(i+1) + ": " + emergencyContacts[i]);
      }
  }
  
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
  sim800.begin(9600, SERIAL_8N1, SIM800_RX, SIM800_TX);

  delay(3000);
  sim800.println("AT");
  delay(1000);
  sim800.println("AT+CMGF=1"); 
  delay(1000);
  sim800.println("AT+CNMI=1,2,0,0,0"); 
  delay(1000);

  // Initialize inactivity timer
  lastActivityTime = millis();
}

// =====================================================
//                      LOOP
// =====================================================
void loop() {
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }

  while (sim800.available()) {
    char c = sim800.read();
    Serial.write(c); 
    incomingSMS += c;
    
    if (c == '\n') {
        // --- 1. SET EMERGENCY CONTACTS ---
        int cmdIndex = incomingSMS.indexOf("SET_EMERGENCY:");
        if (cmdIndex != -1) {
            String payload = incomingSMS.substring(cmdIndex + 14); 
            payload.trim();
            payload.replace(" ", ""); 
            
            for(int i = 0; i < MAX_CONTACTS; i++) emergencyContacts[i] = "";
            
            int contactIndex = 0;
            int startIndex = 0;
            int commaIndex = payload.indexOf(',');

            while (commaIndex != -1 && contactIndex < MAX_CONTACTS) {
                emergencyContacts[contactIndex] = payload.substring(startIndex, commaIndex);
                startIndex = commaIndex + 1;
                commaIndex = payload.indexOf(',', startIndex);
                contactIndex++;
            }
            if (contactIndex < MAX_CONTACTS) {
                emergencyContacts[contactIndex] = payload.substring(startIndex);
            }
            
            blePrintln("\n*** EMERGENCY NUMBERS UPDATED ***");
            for (int i = 0; i < MAX_CONTACTS; i++) {
                String key = "em_contact" + String(i);
                if (emergencyContacts[i].length() >= 10) {
                    emergencyContacts[i] = emergencyContacts[i].substring(0, 10);
                    preferences.putString(key.c_str(), emergencyContacts[i]);
                    blePrintln("Contact " + String(i+1) + ": " + emergencyContacts[i]);
                } else {
                    preferences.putString(key.c_str(), ""); 
                }
            }
        }

        // --- 2. CALIBRATE FSR COMMAND ---
        if (incomingSMS.indexOf("CALIBRATE_FSR") != -1) {
            blePrintln("\n*** STARTING CALIBRATION IN 3 SECONDS... ***");
            delay(3000); 
            
            long sum = 0;
            int samples = 50;
            for(int i=0; i<samples; i++) {
                sum += readFSR();
                delay(40); 
            }
            
            int baseline = sum / samples;
            threshold = baseline + 800; 
            
            if (threshold > 4000) threshold = 4000; 
            if (threshold < 1000) threshold = 1000; 
            
            preferences.putInt("fsr_thresh", threshold);
            blePrintln("Calibration Complete! Baseline: " + String(baseline) + " | New Threshold: " + String(threshold));
            
            if (emergencyContacts[0].length() >= 10) {
                sendSMS("Shoe Calibrated!\nNew Tap Threshold: " + String(threshold), emergencyContacts[0]);
            }
        }
        
        incomingSMS = ""; 
    }

    if (incomingSMS.length() > 200) {
        incomingSMS = "";
    }
  }

  unsigned long currentTime = millis();
  int fsrValue = readFSR();

  // --- MOTOR NON-BLOCKING TIMER LOGIC ---
  if (motorActive && (currentTime - motorStartTime >= motorDuration)) {
      digitalWrite(MOTOR_PIN, LOW);
      motorActive = false;
      blePrintln("Vibration Motor Stopped.");
  }

  // --- AUTO OFF & CHARGING LOGIC ---
  bool isCharging = digitalRead(VBUS_PIN);

  if (abs(fsrValue - lastActivityFsr) > 50) {
      lastActivityTime = currentTime;
      lastActivityFsr = fsrValue;
  }

  if (!isCharging && (currentTime - lastActivityTime > autoOffTimeout)) {
      blePrintln("Shoe inactive for 2 minutes. Turning OFF Power...");
      delay(500); 
      digitalWrite(RELAY_PIN, LOW); 
      while (true); 
  }

  // --- CLOUD TRACKING LOGIC ---
  if (currentTime - lastCloudUpload > uploadInterval) {
    if (gps.location.isValid()) {
      uploadToCloud(gps.location.lat(), gps.location.lng(), false);
    }
    lastCloudUpload = currentTime;
  }

  // --- TRAILING TAP VERIFICATION (STAGE 3) ---
  if (stage == 3) {
      // We wait for 2.5x the average tap interval. If no extra tap happens, we trigger SOS!
      if (currentTime - lastTapTime > (avgTapInterval * 2.5)) {
          blePrintln("Pattern Verified: No trailing taps detected. Triggering SOS!");
          handleSOS();
          resetPattern();
      }
  }

  // --- GENERIC SEQUENCE TIMEOUT ---
  // If the user stops tapping halfway through the pattern, reset it.
  if (tapCount > 0 && stage < 3 && (currentTime - lastTapTime > SEQUENCE_TIMEOUT)) {
      resetPattern();
  }

  // --- ADAPTIVE TAP DETECTION LOGIC ---
  if (fsrValue > threshold && !tapDetected) {
    tapDetected = true;
    lastActivityTime = currentTime; 
    
    if (stage == 0) {
      // If it's been too long since the last tap, assume this is Tap 1
      if (tapCount > 0 && (currentTime - lastTapTime > MAX_INITIAL_GAP)) {
          tapCount = 0; 
      }
      tapTimes[tapCount] = currentTime;
      tapCount++;
      blePrintln("Tap " + String(tapCount));

      if (tapCount == 3) {
         // Calculate user's natural tapping speed
         avgTapInterval = ((tapTimes[1] - tapTimes[0]) + (tapTimes[2] - tapTimes[1])) / 2;
         
         if (avgTapInterval < 100) { 
             resetPattern(); // Too fast, probably sensor noise
         } else {
             stage = 1;
             blePrintln("3 Taps. Avg Pace: " + String(avgTapInterval) + "ms. Waiting for pause...");
         }
      }
    } else if (stage == 1) {
      tapTimes[3] = currentTime;
      unsigned long pauseDuration = tapTimes[3] - tapTimes[2];

      // Pause must be at least 2x the normal tap pace, but less than 5x
      if (pauseDuration > (avgTapInterval * 1.5) && pauseDuration < (avgTapInterval * 5.0)) {
          tapCount = 4;
          stage = 2;
          blePrintln("Pause Accepted (" + String(pauseDuration) + "ms). Waiting for final tap...");
      } else {
          blePrintln("Invalid Pause: " + String(pauseDuration) + "ms. Resetting.");
          resetPattern();
          tapTimes[0] = currentTime; // Treat this invalid tap as Tap 1 of a new sequence
          tapCount = 1;
          stage = 0;
      }
    } else if (stage == 2) {
      tapTimes[4] = currentTime;
      unsigned long gapDuration = tapTimes[4] - tapTimes[3];

      // Final tap must follow the original tapping rhythm (within 2.5x avg interval)
      if (gapDuration < (avgTapInterval * 2.5)) {
          blePrintln("Final Tap Accepted! Waiting to verify no extra trailing taps...");
          stage = 3; // Enter the trailing tap verification state!
      } else {
          blePrintln("Final tap too slow. Resetting.");
          resetPattern();
          tapTimes[0] = currentTime;
          tapCount = 1;
          stage = 0;
      }
    } else if (stage == 3) {
      // If we get a tap while in Stage 3, the user is just walking. Cancel the SOS!
      blePrintln("False Alarm! Extra trailing tap detected. Cancelling SOS.");
      resetPattern();
      tapTimes[0] = currentTime;
      tapCount = 1;
      stage = 0;
    }
    lastTapTime = currentTime;
  }

  // --- RELEASE DETECTION ---
  if (fsrValue < (threshold - 200) && (millis() - lastTapTime > 80)) {
    tapDetected = false;
  }

  delay(20);
}