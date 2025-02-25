// Based off of TCP WifiClient example: https://tigoe.github.io/html-for-conndev/DeviceDataDashboard/
// Used this example to get current time: https://nerdhut.de/2021/12/15/arduino-esp32-esp8266-ntp/

//
//
/* GENERAL INIT */

#include <Arduino.h>
#include <WiFiNINA.h> // use this for Nano 33 IoT or MKR1010 boards
#include <WiFiUdp.h> // UDP library for NTP
#include <NTPClient.h> // NTP library

#include "arduino_secrets.h"
#include <time.h>

#include <Wire.h> // I2C library
#include <VL53L0X.h> // TOF sensor library
VL53L0X sensor;

//
//
/* WIFI INIT */

WiFiClient client; // Init the Wifi client library

const char server[] = "10.18.159.239"; // target server
const int portNum = 8080;
String deviceName = "justin-nano33iot"; // Arduino device name

const int changeThreshold = 2; // change threshold for sensor readings
const int maxDistance = 250; // max distance in mm
int lastSensorVal = 0;

//
//
/* NTP INIT */

const long utcOffsetUSEast = -18000; // Offset from UTC in seconds (-18000 seconds = -5h) -- UTC-5 (Eastern Standard Time)
 
// Define NTP Client to get time
WiFiUDP udpSocket;
NTPClient ntpClient(udpSocket, "pool.ntp.org", utcOffsetUSEast);

void setup() {

  Serial.begin(115200);  // Init serial
  Wire.begin(); // Init I2C
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

  ntpClient.begin(); // Start NTP

  // 
  // 
  /* TOF SENSOR SETUP */
  
  sensor.setTimeout(500);
  if (!sensor.init())
  {
    Serial.println("Failed to detect and initialize sensor!");
    while (1) {}
  }
  
  sensor.startContinuous(50); // 50ms polling interval
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

  int sensorVal = sensor.readRangeContinuousMillimeters();
  int sensorValSmoothed = lastSensorVal * 0.9 + sensorVal * 0.1;

  if (sensorVal < maxDistance && abs(sensorVal - lastSensorVal) > changeThreshold) {
    lastSensorVal = sensorVal;

    // send the reading:
    Serial.println(sensorValSmoothed);

    // get current time
    // TODO: Use WIFININA .getTime() instead: https://docs.arduino.cc/libraries/wifinina/#%60WiFi.getTime()%60
    ntpClient.update();
    time_t epochTime = ntpClient.getEpochTime();
    struct tm *ptm = gmtime((time_t *)&epochTime);
    char dateString[11];
    // format the date and time:
    sprintf(dateString, "%04d-%02d-%02d", ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday);
    String timestamp = ntpClient.getFormattedTime();

    String message = "{\"device\": \"DEVICE\", \"time\": \"DATETTIME\", \"sensor\": READING}";
    message.replace("READING", String(sensorVal));
    message.replace("DEVICE", deviceName);
    message.replace("DATE", String(dateString));
    message.replace("TIME", timestamp);
    client.println(message);
  }

  // check if there is incoming data available to be received
  int messageSize = client.available();
  // if there's a string with length > 0:
  if (messageSize > 0) {
    Serial.println("Received a message:");
    Serial.println(client.readString());
  }
}