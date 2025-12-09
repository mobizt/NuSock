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
const char *ssid = "SSID";
const char *password = "Password";

#if defined(ESP32)

// In ESP32, we use NuSockClientSecure for this reason.

// 1. Performance & Efficiency: It is designed to be a lightweight
// and high-performance WSS (Secure WebSocket) client implementation.

// 2. Direct Native Stack: It sits directly on top of the ESP-IDF esp_tls stack.
// This allows it to avoid the overhead associated with the standard
// Arduino WiFiClientSecure library, providing a more optimized connection.
NuSockClientSecure wss;

#elif defined(ESP8266) || defined(ARDUINO_RASPBERRY_PI_PICO_W)
WiFiClientSecure wifiClient;
NuSockClient wss;
#else
WiFiSSLClient wifiClient;
NuSockClient wss;
#endif

// For easier usage, run "python pem_to_cpp.py"
// and provide root CA certificate to get the text 
// that is easier to use in code.
const char *rootCA = "-----BEGIN CERTIFICATE-----\n...";

// Event Handler Callback
void onWebSocketEvent(NuClient *client, NuClientEvent event, const uint8_t *payload, size_t len)
{
    switch (event)
    {
    case CLIENT_EVENT_HANDSHAKE:
        NuSock::printLog("WS  ", "Handshake completed!\n");
        break;

    case CLIENT_EVENT_CONNECTED:
        NuSock::printLog("WS  ", "Connected to server!\n");
        // Send a message immediately upon connection
        wss.send("Hello from WS Client");
        break;

    case CLIENT_EVENT_DISCONNECTED:
        NuSock::printLog("WS  ", "Disconnected!\n");
        break;

    case CLIENT_EVENT_MESSAGE_TEXT:
        NuSock::printLog("WS  ", "Text: ");
        for (size_t i = 0; i < len; i++)
            Serial.print((char)payload[i]);
        Serial.println();
        break;

    case CLIENT_EVENT_MESSAGE_BINARY:
        NuSock::printLog("WS  ", "Binary: %d bytes\n", len);
        break;

    case CLIENT_EVENT_ERROR:
        NuSock::printLog("WS  ", "Error: %s\n", payload ? (const char *)payload : "Unknown");
        break;
    }
}

void setup()
{

    Serial.begin(115200);
    while (!Serial)
        ; // Wait for serial

    delay(3000);

    Serial.println();

    NuSock::printLog("INFO", "NuSock WSS Client v%s Booting\n", NUSOCK_VERSION_STR);

    // Connect to WiFi
    NuSock::printLog("NET ", "Connecting to WiFi (%s)...\n", ssid);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
    }

    NuSock::printLog("NET ", "WiFi Connected (%s)\n", NuSock::ipStr(WiFi.localIP()));
    NuSock::printLog("NET ", "Gateway: %s\n", NuSock::ipStr(WiFi.gatewayIP()));

// If a CA certificate is needed.
#if defined(ESP32)
    // wss.setCACert(rootCA);
#endif

#if defined(ESP8266) || defined(ARDUINO_RASPBERRY_PI_PICO_W)
    // If no SSL certificate verification is required
    wifiClient.setInsecure();
    NuSock::printLog("WARN", "Skipping SSL Verification (Insecure Mode)\n");
#endif

    // Register Event Callback
    wss.onEvent(onWebSocketEvent);

    // Configure WebSocket Client
    char *host = "echo.websocket.org";
    uint16_t port = 443;
    const char *path = "/";
    NuSock::printLog("WS  ", "Connecting to wss://%s:%d/\n", host, port);

#if defined(ESP32)
    wss.begin(host, port, path);
#else
    wss.begin(&wifiClient, host, port, path);
#endif

    if (wss.connect())
        NuSock::printLog("WS  ", "Connection request sent.\n");
    else
        NuSock::printLog("WS  ", "Connection failed immediately.\n");
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