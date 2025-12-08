/**
 * NuSock Secure WebSocket Client (WSS) Arduino devices Example
 * This sketch demonstrates how to run a Secure WebSocket Client (WSS) on port 443
 * using the NuSock library
 */

// For internal debug message printing
#define NUSOCK_DEBUG

#include <Arduino.h>

#if defined(ESP32)
#include <WiFi.h>
#include <WiFiClientSecure.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#elif defined(ARDUINO_RASPBERRY_PI_PICO_W)
#include <WiFi.h>
#include <WiFiClientSecure.h>
#else

// These WiFi libraries may require the server SSL certificate
// to be uploaded via the Arduino IDE.
// For Arduino MKR WiFi 1010, Nano 33 IoT,
// Arduino MKR VIDOR 4000, Arduino Uno WiFi Rev.2
#include <WiFiNINA.h>

// For Arduino MKR1000 WiFi
// #include <WiFi101.h>

// For UNO R4 WiFi
// #include <WiFiS3.h>

#endif

#include "NuSock.h"

// Network Credentials
const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASS";

#if defined(ESP32)
NuSockClientSecure wss;
#elif defined(ESP8266) || defined(ARDUINO_RASPBERRY_PI_PICO_W)
WiFiClientSecure wifiClient;
NuSockClient wss;
#else
WiFiSSLClient wifiClient;
NuSockClient wss;
#endif

const char *rootCA =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDrjCCApagAwIBAgIU...\n"
    "-----END CERTIFICATE-----\n";

// Event Handler Callback
void onWebSocketEvent(NuClient *client, NuClientEvent event, const uint8_t *payload, size_t len)
{
    switch (event)
    {
    case CLIENT_EVENT_HANDSHAKE:
        Serial.println("[WSS] Handshake completed!");
        break;

    case CLIENT_EVENT_CONNECTED:
        Serial.println("[WSS] Connected to server!");
        // Send a message immediately upon connection
        wss.send("Hello from WS Client");
        break;

    case CLIENT_EVENT_DISCONNECTED:
        Serial.println("[WSS] Disconnected!");
        break;

    case CLIENT_EVENT_MESSAGE_TEXT:
        Serial.print("[WSS] Text: ");
        for (size_t i = 0; i < len; i++)
            Serial.print((char)payload[i]);
        Serial.println();
        break;

    case CLIENT_EVENT_MESSAGE_BINARY:
        NuSock::printf("[WSS] Binary: %d bytes\n", len);
        break;

    case CLIENT_EVENT_ERROR:
        NuSock::printf("[WSS] Error: %s\n", payload ? (const char *)payload : "Unknown");
        break;
    }
}

void setup()
{

    Serial.begin(115200);
    delay(3000); // Wait for serial monitor

    // Connect to WiFi
    Serial.print("Connecting to WiFi");
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(200);
        Serial.print(".");
    }

    Serial.println(" Connected!");
    Serial.print("IP Address: ");
    NuSock::printIP(WiFi.localIP());

// If a CA certificate is needed.
#if defined(ESP32)
    // wss.setCACert(rootCA);
#endif

#if defined(ESP8266) || defined(ARDUINO_RASPBERRY_PI_PICO_W)
    // If no SSL certificate verification is required
    wifiClient.setInsecure();
#endif

    // Register Event Callback
    wss.onEvent(onWebSocketEvent);

    // Configure WebSocket Client
#if defined(ESP32)
    wss.begin("echo.websocket.org", 443, "/");
#else
    wss.begin(&wifiClient, "echo.websocket.org", 443, "/");
#endif

    // Initiate Connection
    Serial.println("Connecting to WebSocket Server...");
    if (wss.connect())
    {
        Serial.println("Connection request sent.");
    }
    else
    {
        Serial.println("Connection failed immediately.");
    }
}

void loop()
{
    // Drive the Network Stack
    // Must be called frequently to process incoming data and events
    wss.loop();

    // Example: Send periodic heartbeat
    static unsigned long lastTime = 0;
    if (millis() - lastTime > 5000)
    {
        lastTime = millis();
        // Only send if connected logic would go here,
        // currently NuSockClient handles state internally and won't send if disconnected
        wss.send("Heartbeat");
    }
}