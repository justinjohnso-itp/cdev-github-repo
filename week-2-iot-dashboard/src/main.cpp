// Based off of TCP WifiClient example: https://tigoe.github.io/html-for-conndev/DeviceDataDashboard/

//
//
/* GENERAL INIT */

#include <Arduino.h>
#include <WiFiNINA.h> // use this for Nano 33 IoT or MKR1010 boards
#include "arduino_secrets.h"
#include "Adafruit_VL53L0X.h" // TOF sensor library
Adafruit_VL53L0X lox = Adafruit_VL53L0X();

//
//
/* WIFI INIT */

WiFiClient client; // Init the Wifi client library

const char server[] = "10.18.159.239"; // target server
const int portNum = 8080;
String deviceName = "justin-nano33iot"; // Arduino device name

const int changeThreshold = 10;
int lastSensorVal = 0;

void setup() {

  Serial.begin(115200);  // Init serial
  while (! Serial) { // Wait until serial port is open
    delay(1);
  }

  //
  // 
  /* WIFI SETUP */

  // Connect to WPA/WPA2 network.
  WiFi.begin(SECRET_SSID, SECRET_PASS);

  // attempt to connect to Wifi network:
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(SECRET_SSID);
    // wait a second for connection:
    delay(1000);
  }
  Serial.print("Connected to to SSID: ");
  Serial.println(SECRET_SSID);
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("Signal Strength (dBm): ");
  Serial.println(WiFi.RSSI());

  // 
  // 
  /* TOF SENSOR SETUP */

  Serial.println("Adafruit VL53L0X test.");
  if (!lox.begin()) {
    Serial.println(F("Failed to boot VL53L0X"));
    while(1);
  }
  
  // power
  Serial.println(F("VL53L0X API Continuous Ranging example\n\n"));

  // start continuous ranging
  lox.startRangeContinuous(50);
}

void loop() {
  //
  //
  /* WIFI LOOP */ 

  if (!client.connected()) { // connect to client
    Serial.println("connecting");
    Serial.println(server);
    Serial.println(portNum);
    client.connect(server, portNum);
    // skip the rest of the loop:
    return;
  }

  //
  //
  /* TOF SENSOR LOOP */

  if (lox.isRangeComplete()) {
    int sensorVal = map(lox.readRange(), 0, 8190, 0, 100);

    // if (abs(sensorVal - lastSensorVal) > changeThreshold) {
      lastSensorVal = sensorVal;

      // send the reading:
      Serial.println(sensorVal);

      String message = "{\"device\": \"DEVICE\", \"sensor\": READING}";
      message.replace("READING", String(sensorVal));
      message.replace("DEVICE", deviceName);
      client.println(message);
    // }

  }

  // check if there is incoming data available to be received
  int messageSize = client.available();
  // if there's a string with length > 0:
  if (messageSize > 0) {
    Serial.println("Received a message:");
    Serial.println(client.readString());
  }
}