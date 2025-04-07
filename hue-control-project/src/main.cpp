#include <Arduino.h>
#include <WiFiNINA.h>  // for Nano 33 IoT, MKR1010 - change to WiFi101.h for MKR1000 or WiFis3.h for Uno v.4
#include <ArduinoHttpClient.h>
#include "arduino_secrets.h"

// Hue control settings - modify these values as needed
const int lightNumber = 1;  // The ID of the light you want to control

// Pin definitions
const int toggleSwitchPin = 2;  // Digital pin for the toggle switch
const int brightnessPotPin = A0;  // Analog pin for brightness potentiometer
const int colorTempPotPin = A1;  // Analog pin for color temperature potentiometer

// Variables to track state
bool isLightOn = false;
int currentBrightness = 0;  // Range: 0-254
int currentColorTemp = 0;   // Range: 153-500 (mired scale)
int lastBrightnessValue = -1;
int lastColorTempValue = -1;
int lastSwitchState = -1;

// Control parameters
const int minBrightness = 1;      // Minimum brightness value (0-254)
const int maxBrightness = 254;    // Maximum brightness value (0-254)
const int minColorTemp = 153;     // Minimum color temp in mired (6500K)
const int maxColorTemp = 500;     // Maximum color temp in mired (2000K)
const int debounceDelay = 50;     // Debounce delay in ms
const int updateThreshold = 5;    // Threshold for sending updates
const int mainLoopDelay = 100;    // Main loop delay in ms

// Make a WiFi client and HttpClient instance
WiFiClient wifi;
HttpClient httpClient = HttpClient(wifi, SECRET_HUE_IP);

// Forward declarations
bool connectToWiFi();
bool sendHueCommand(const String& command, const String& value);
void updateLight();
void printNetworkStatus();

void setup() {
  // Initialize serial and wait for port to open
  Serial.begin(9600);
  while (!Serial) delay(3000);  // Wait for serial port to connect

  Serial.println(F("Philips Hue Light Controller"));
  Serial.println(F("---------------------------"));
  
  // Setup pins
  pinMode(toggleSwitchPin, INPUT_PULLUP);
  pinMode(brightnessPotPin, INPUT);
  pinMode(colorTempPotPin, INPUT);

  // Connect to WiFi
  if (!connectToWiFi()) {
    Serial.println(F("Failed to connect to WiFi. Check credentials in arduino_secrets.h"));
    while (1) {
      // Flash the LED to indicate WiFi connection failure
      digitalWrite(LED_BUILTIN, HIGH);
      delay(300);
      digitalWrite(LED_BUILTIN, LOW);
      delay(300);
    }
  }
  
  Serial.println(F("Ready to control Hue light"));
}

void loop() {
  // Check WiFi connection and reconnect if necessary
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("WiFi connection lost. Attempting to reconnect..."));
    if (!connectToWiFi()) {
      delay(5000);  // Wait before trying again
      return;
    }
  }
  
  // Read toggle switch (inverted because of INPUT_PULLUP)
  int switchState = !digitalRead(toggleSwitchPin);
  
  // Read potentiometers
  int brightnessValue = analogRead(brightnessPotPin);
  int colorTempValue = analogRead(colorTempPotPin);
  
  // Map potentiometer values to Hue ranges
  int mappedBrightness = map(brightnessValue, 0, 1023, minBrightness, maxBrightness);
  int mappedColorTemp = map(colorTempValue, 0, 1023, minColorTemp, maxColorTemp); // 153 (6500K) to 500 (2000K)
  
  // Check if switch state changed
  if (switchState != lastSwitchState) {
    isLightOn = switchState;
    if (sendHueCommand("on", isLightOn ? "true" : "false")) {
      Serial.println(F("Light power state changed"));
    }
    lastSwitchState = switchState;
    delay(debounceDelay); // Debounce
  }
  
  // Only send brightness updates if the light is on and the value changed significantly
  if (isLightOn && abs(mappedBrightness - lastBrightnessValue) > updateThreshold) {
    currentBrightness = mappedBrightness;
    if (sendHueCommand("bri", String(currentBrightness))) {
      Serial.print(F("Brightness updated to: "));
      Serial.println(currentBrightness);
    }
    lastBrightnessValue = currentBrightness;
    delay(debounceDelay); // Rate limiting
  }
  
  // Only send color temperature updates if the light is on and the value changed significantly
  if (isLightOn && abs(mappedColorTemp - lastColorTempValue) > updateThreshold) {
    currentColorTemp = mappedColorTemp;
    if (sendHueCommand("ct", String(currentColorTemp))) {
      Serial.print(F("Color temperature updated to: "));
      Serial.println(currentColorTemp);
    }
    lastColorTempValue = currentColorTemp;
    delay(debounceDelay); // Rate limiting
  }
  
  // Add a small delay to avoid overwhelming the bridge
  delay(mainLoopDelay);
}

/**
 * Connect to the WiFi network using credentials in arduino_secrets.h
 * 
 * @return true if connection successful, false otherwise
 */
bool connectToWiFi() {
  Serial.print(F("Connecting to WiFi network: "));
  Serial.println(SECRET_SSID);
  
  // Set LED pin as output to indicate connection status
  pinMode(LED_BUILTIN, OUTPUT);
  
  // Begin WiFi connection with credentials from arduino_secrets.h
  WiFi.begin(SECRET_SSID, SECRET_PASS);
  
  // Wait for connection with timeout
  int connectionAttempts = 0;
  const int maxAttempts = 20; // ~10 seconds timeout
  
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_BUILTIN, connectionAttempts % 2); // Blink LED
    delay(500);
    Serial.print(".");
    connectionAttempts++;
    
    if (connectionAttempts > maxAttempts) {
      Serial.println(F("\nWiFi connection timeout"));
      return false;
    }
  }
  
  // Connection successful
  digitalWrite(LED_BUILTIN, HIGH); // Keep LED on when connected
  Serial.println();
  printNetworkStatus();
  return true;
}

/**
 * Send a command to the Philips Hue light
 * 
 * @param command The command parameter (e.g., "on", "bri", "ct")
 * @param value The value to set for the command 
 * @return true if command was sent successfully, false otherwise
 */
bool sendHueCommand(const String& command, const String& value) {
  // Make a String for the HTTP request path
  String request = "/api/";
  request += SECRET_HUE_KEY;
  request += "/lights/";
  request += String(lightNumber);
  request += "/state";

  // Content type for the request
  String contentType = "application/json";

  // Create the JSON command
  String hueCmd = "{\"" + command + "\":" + value + "}";
  
  // Print what we're sending (for debugging)
  Serial.print(F("Sending command: "));
  Serial.println(hueCmd);
  
  // Make the PUT request to the hub
  httpClient.beginRequest();
  int putResult = httpClient.put(request, contentType, hueCmd);
  
  if (putResult != 0) {
    Serial.print(F("Failed to send command, error: "));
    Serial.println(putResult);
    return false;
  }
  
  httpClient.endRequest();
  
  // Read the status code and body of the response
  int statusCode = httpClient.responseStatusCode();
  String response = httpClient.responseBody();
  
  Serial.print(F("Status code: "));
  Serial.println(statusCode);
  Serial.print(F("Response: "));
  Serial.println(response);
  
  // Check if the request was successful
  return (statusCode >= 200 && statusCode < 300);
}

/**
 * Update all light properties at once (on/off, brightness, color temperature)
 * More efficient than sending multiple separate commands
 */
void updateLight() {
  if (!isLightOn) {
    sendHueCommand("on", "false");
    return;
  }
  
  // Format: {"on":true,"bri":brightness,"ct":colortemp}
  String request = "/api/";
  request += SECRET_HUE_KEY;
  request += "/lights/";
  request += String(lightNumber);
  request += "/state";
  
  String contentType = "application/json";
  
  // Build the JSON string without using + operator with F() macro
  String hueCmd = "{\"on\":true,\"bri\":";
  hueCmd += String(currentBrightness);
  hueCmd += ",\"ct\":";
  hueCmd += String(currentColorTemp);
  hueCmd += "}";
  
  httpClient.beginRequest();
  httpClient.put(request, contentType, hueCmd);
  httpClient.endRequest();
  
  int statusCode = httpClient.responseStatusCode();
  String response = httpClient.responseBody();
  
  Serial.print(F("Status code: "));
  Serial.println(statusCode);
  Serial.print(F("Response: "));
  Serial.println(response);
}

/**
 * Print information about the current WiFi network connection
 */
void printNetworkStatus() {
  Serial.println(F("WiFi connection established"));
  Serial.print(F("SSID: "));
  Serial.println(WiFi.SSID());
  Serial.print(F("Signal strength (RSSI): "));
  Serial.print(WiFi.RSSI());
  Serial.println(F(" dBm"));
  Serial.print(F("IP address: "));
  Serial.println(WiFi.localIP());
  
  // WiFiNINA might not have macAddress() method directly on WiFi object
  byte mac[6];
  WiFi.macAddress(mac);
  
  Serial.print(F("MAC address: "));
  for (int i = 0; i < 6; i++) {
    if (mac[i] < 0x10) {
      Serial.print("0");
    }
    Serial.print(mac[i], HEX);
    if (i < 5) Serial.print(":");
  }
  Serial.println();
}