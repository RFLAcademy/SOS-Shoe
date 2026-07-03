#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <Preferences.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

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
//                    FSR SETUP
// =====================================================
#define FSR_PIN 3
int threshold = 2000; 
const unsigned long tapGapMax = 700;
const unsigned long requiredPause = 500;
const unsigned long resetTimeout = 2000;

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
  String url = "[http://demo2.traccar.org:5055/?id=](http://demo2.traccar.org:5055/?id=)" + deviceID + "&lat=" + String(lat, 6) + "&lon=" + String(lng, 6);
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
  String mapsLink = "[https://maps.google.com/?q=](https://maps.google.com/?q=)" + String(latitude, 6) + "," + String(longitude, 6);
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
            if (threshold < 1000) threshold = 1500; 
            
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

  if (currentTime - lastCloudUpload > uploadInterval) {
    if (gps.location.isValid()) {
      uploadToCloud(gps.location.lat(), gps.location.lng(), false);
    }
    lastCloudUpload = currentTime;
  }

  if (fsrValue > threshold && !tapDetected) {
    tapDetected = true;
    if (currentTime - lastTapTime > resetTimeout) resetPattern();

    if (stage == 0) {
      tapCount++;
      if (tapCount == 3) {
        stage = 1;
        blePrintln("3 Taps Completed. Waiting For Pause...");
      }
    } else if (stage == 1) {
      unsigned long pauseTime = currentTime - lastTapTime;
      if (pauseTime > requiredPause) {
        stage = 2;
        tapCount = 1;
        blePrintln("Pause Accepted. Stage 2 Started");
      } else {
        resetPattern();
      }
    } else if (stage == 2) {
      if (currentTime - lastTapTime < tapGapMax) {
        tapCount++;
        if (tapCount == 2) {
          handleSOS();
          resetPattern();
        }
      } else {
        resetPattern();
      }
    }
    lastTapTime = currentTime;
  }

  if (fsrValue < (threshold - 800) && (millis() - lastTapTime > 80)) {
    tapDetected = false;
  }

  delay(20);
}
