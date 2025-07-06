/*
  ESP32 LoRa + WiFi Server Node + Blynk 

  This sketch runs on an ESP32 acting as the server node in a LoRa + WiFi setup.
  It listens for temperature and humidity data sent from another LoRa node,
  and forwards this data to the Blynk cloud over WiFi.
  It also listens for commands from the Blynk app, such as:
    - turn on
    - turn off
  and sends these commands to the other LoRa node.

  Developer: uugan
  GitHub: https://github.com/uugan/esp32

  Features:
    - Connects to WiFi and Blynk cloud
    - Receives temperature & humidity over LoRa
    - Sends sensor data to Blynk
    - Receives control commands from Blynk
    - Sends commands back to the LoRa node

  Hardware:
    - ESP32 Dev Board
    - LoRa SX1278 module (e.g., Ra-02)

  Notes:
    - LoRa and Blynk credentials must be configured before uploading.
    - LoRa settings (frequency, sync word, etc.) must match client node.
    - Ensure proper GPIO pin assignments and power for LoRa module.
  License: MIT
*/


#define BLYNK_PRINT Serial
#include "secret.h"
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

#include <LoRa.h>
#include <SPI.h>

#define ss 5
#define rst 14
#define dio0 25

//start of config DHT, Blynk, Relay
char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = WIFI_SSID;  // type your wifi name
char pass[] = WIFI_PASSWORD;    // type your wifi password

#define VPIN_BUTTON_1 V2
BlynkTimer timer;
bool toggleState_1 = HIGH;

float temperature = 0;
float humidity = 0;

//Duplex 
byte localAddr = 0xAA;
byte destinationAddr = 0xBB;
byte msgCount = 0;            // count of outgoing messages

BLYNK_CONNECTED() {
  // Request the latest state from the server
  Blynk.syncVirtual(VPIN_BUTTON_1);
}
// When App button is pushed - switch the state
BLYNK_WRITE(VPIN_BUTTON_1) {
  toggleState_1 = param.asInt();
  if (toggleState_1 == 1) {
    sendMessage("relay_on");
  } else {
    sendMessage("relay_off");
  }
}
void setup() {

  Serial.begin(115200);
  // Serial.println("Board Info:");
  // Serial.println(ARDUINO_BOARD);
  // Serial.println(ESP.getChipModel());
  // Serial.println(ESP.getChipRevision());
  // Serial.println(ESP.getEfuseMac(), HEX);  // Unique MAC address

  Blynk.begin(auth, ssid, pass);

  while (!Serial);
  Serial.println("LoRa Receiver");
  LoRa.setPins(ss, rst, dio0);  //setup LoRa transceiver module

  while (!LoRa.begin(433E6))  //433E6 - Asia, 866E6 - Europe, 915E6 - North America
  {
    Serial.println(".");
    delay(500);
  }

  //LoRa.setSpreadingFactor(7);
  //LoRa.setSignalBandwidth(125E3);
  //LoRa.setCodingRate4(5);

  LoRa.setSyncWord(0x12);
  delay(100);
  Serial.println("LoRa Initializing OK!");

  // Call sendSensorData() every 15 minutes
  timer.setInterval(900000L, sendSensorData);

}

void loop() {
  Blynk.run();
  timer.run();
  int packetSize = LoRa.parsePacket(); 
  receiveMessage(packetSize);
}
void sendMessage(String outgoing) {
  LoRa.beginPacket();                   // start packet
  LoRa.write(destinationAddr);          // add destination address
  LoRa.write(localAddr);                // add sender address
  LoRa.write(msgCount);                 // add message ID
  LoRa.write(outgoing.length());        // add payload length
  LoRa.print(outgoing);                 // add payload
  LoRa.endPacket();                     // finish packet and send it
  msgCount++;                           // increment message ID
}

void receiveMessage(int packetSize) {
  if (packetSize == 0) return;  // if there's no packet, return

  // read packet header bytes:
  int recipient = LoRa.read();          // recipient address
  byte sender = LoRa.read();            // sender address
  byte incomingMsgId = LoRa.read();     // incoming msg ID
  byte incomingLength = LoRa.read();    // incoming msg length

  String received = "";
  while (LoRa.available()) {
    received += (char)LoRa.read();  // read byte-by-byte
  }

  if (incomingLength != received.length()) {  
    Serial.println("error: message length does not match length");
    return;
  }

  // if the recipient isn't this device or broadcast,
  if (recipient != localAddr && recipient != destinationAddr) {
    Serial.println("This message is not for me.");
    return;                             // skip rest of function
  }

  
  Serial.println("Received from: 0x" + String(sender, HEX));
  Serial.println("Sent to: 0x" + String(recipient, HEX));
  Serial.println("Message ID: " + String(incomingMsgId));
  Serial.println("Message length: " + String(incomingLength));
  Serial.println("Message: " + received);
  Serial.println("RSSI: " + String(LoRa.packetRssi()));
  Serial.println("Snr: " + String(LoRa.packetSnr()));
  Serial.println();

  int commaIndex = received.indexOf(',');
  if (commaIndex > 0) {
    String tempStr = received.substring(0, commaIndex);
    String humStr = received.substring(commaIndex + 1);
    temperature = tempStr.toFloat();
    humidity = humStr.toFloat();
  } else {
    Serial.println("Invalid format");
  }
}

void sendSensorData() {
  Blynk.virtualWrite(V0, temperature);
  Blynk.virtualWrite(V1, humidity);
  Serial.println("Data sent to Blynk");
}