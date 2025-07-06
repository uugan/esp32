/*
  Project: LoRa Duplex Communication with ESP32 + SX1278
  Description:
    - ESP32 with LoRa SX1278 module
    - Sends temperature and humidity every 5 seconds to another LoRa node
    - Receives control commands such as "relay_on" and "relay_off" from remote node
    - Designed for basic IoT automation over LoRa (non-LoRaWAN)

  Developer: Uugan
  GitHub: https://github.com/uugan/esp32

  Hardware:
    - ESP32 Dev Board
    - SX1278 LoRa Module (e.g., Ra-02)
    - DHT11 sensor (or other temperature/humidity sensor)
    - Relay module for AC load control

  Notes:
    - Make sure both sender and receiver use the same LoRa settings: frequency, sync word, bandwidth, etc.
    - Ensure safe relay handling when working with high-voltage AC devices.
  License: MIT
*/
#include <LoRa.h>
#include <SPI.h>
#include <DHT.h>

#define ss 5
#define rst 14
#define dio0 25

#define DHTPIN 2  //Connect DHT OUT pin to D2 
#define DHTTYPE DHT11
#define RELAYPIN 13  //Connect relay Signal pin to D13

DHT dht(DHTPIN, DHTTYPE);

unsigned long previousMillis = 0;
const unsigned long interval = 5000;  // run every 5000 ms (5 seconds)

//Lora - Duplex connection 
byte localAddr = 0xBB;
byte destinationAddr = 0xAA;
byte msgCount = 0;  // count of outgoing messages


void setup() {
  Serial.begin(115200);
  while (!Serial) ;
  Serial.println("LoRa Sender");
  //DHT11
  dht.begin();
  //Relay
  pinMode(RELAYPIN, OUTPUT);
  digitalWrite(RELAYPIN, LOW);

  //SX1278 set pins
  LoRa.setPins(ss, rst, dio0);

  while (!LoRa.begin(433E6))  //433E6 - Asia, 866E6 - Europe, 915E6 - North America
  {
    Serial.println(".");
    delay(500);
  }
  //LoRa.setSpreadingFactor(7);
  //LoRa.setSignalBandwidth(125E3);
  //LoRa.setCodingRate4(5);
  //Set a moderate TX power level (default is 17)
  LoRa.setTxPower(10);  // 10 dBm ~ lower current draw & less heat
  LoRa.setSyncWord(0x12);
  delay(100);
  Serial.println("LoRa Initializing OK!");
}

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;  // update the last run time
    sendSensor();
  }

  int packetSize = LoRa.parsePacket();
  receiveMessage(packetSize);
}

void sendMessage(String outgoing) {
  LoRa.beginPacket();             // start packet
  LoRa.write(destinationAddr);    // add destination address
  LoRa.write(localAddr);          // add sender address
  LoRa.write(msgCount);           // add message ID
  LoRa.write(outgoing.length());  // add payload length
  LoRa.print(outgoing);           // add payload
  LoRa.endPacket();               // finish packet and send it
  msgCount++;                     // increment message ID
}

void sendSensor() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();  // or dht.readTemperature(true) for Fahrenheit

  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }
  
  Serial.print("Temperature : ");
  Serial.print(t);
  Serial.print("    Humidity : ");
  Serial.println(h);
  String message = String(t) + "," + String(h);
  sendMessage(message);
}

void receiveMessage(int packetSize) {
  if (packetSize == 0) return;  // if there's no packet, return

  // read packet header bytes:
  int recipient = LoRa.read();        // recipient address
  byte sender = LoRa.read();          // sender address
  byte incomingMsgId = LoRa.read();   // incoming msg ID
  byte incomingLength = LoRa.read();  // incoming msg length

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
    return;  // skip rest of function
  }


  Serial.println("Received from: 0x" + String(sender, HEX));
  Serial.println("Sent to: 0x" + String(recipient, HEX));
  Serial.println("Message ID: " + String(incomingMsgId));
  Serial.println("Message length: " + String(incomingLength));
  Serial.println("Message: " + received);
  Serial.println("RSSI: " + String(LoRa.packetRssi()));
  Serial.println("Snr: " + String(LoRa.packetSnr()));
  Serial.println();

  processCommand(received);
}

void processCommand(String cmd) {
  if (cmd.equals("relay_on")) {
    digitalWrite(RELAYPIN, HIGH);
  } else if (cmd.equals("relay_off")) {
    digitalWrite(RELAYPIN, LOW);
  } else {
    Serial.println("Command not found.");
  }
}