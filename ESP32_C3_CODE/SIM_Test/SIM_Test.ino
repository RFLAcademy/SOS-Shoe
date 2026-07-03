#include <HardwareSerial.h>

// Create UART2 object
HardwareSerial sim800(2);

// SIM800 pins
#define SIM800_RX 26   // ESP32 receives from SIM800 TX
#define SIM800_TX 27   // ESP32 transmits to SIM800 RX

void setup() {

  Serial.begin(115200);

  // UART for SIM800
  sim800.begin(9600, SERIAL_8N1, SIM800_RX, SIM800_TX);

  delay(3000);

  Serial.println("SIM800C Ready");

  sim800.println("AT");
}

void loop() {

  // SIM800 -> PC
  while (sim800.available()) {
    Serial.write(sim800.read());
  }

  // PC -> SIM800
  while (Serial.available()) {
    sim800.write(Serial.read());
  }
}