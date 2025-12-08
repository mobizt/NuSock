/**
 * NuSock WebSocket Client (WS) ESP32/ESP8266 devices Example
 * This sketch demonstrates how to run a WebSocket Client (WS) on port 80
 * using the NuSock library
 *
 * For non-SSL WebSocket clients, using LwIP directly is recommended
 * (NUSOCK_CLIENT_USE_LWIP).
 *
 * Most WebSocket servers reject connections via non-SSL ports (e.g., echo.websocket.org).
 *
 * For this reason, please run the WebSocket server on your PC by running
 * the Python script 'simple_server.py', or by running 'run.bat' or 'run.sh'.
 *
 * To test the Python script WebSocket server,
 * your PC should connect to the same network as your ESP32/ESP8266 devices.
 */

// For internal debug message printing
#define NUSOCK_DEBUG

// For ESP32 and ESP8266 only.
#define NUSOCK_CLIENT_USE_LWIP

#include <Arduino.h>

#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

#include "NuSock.h"

// Network Credentials
const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASS";

#ifdef NUSOCK_CLIENT_USE_LWIP
NuSockClient ws; // LwIP manages the connection internally
#else
WiFiClient wifiClient;
NuSockClient ws;
#endif

// Event Handler Callback
void onWebSocketEvent(NuClient *client, NuClientEvent event, const uint8_t *payload, size_t len)
{
    switch (event)
    {
    case CLIENT_EVENT_HANDSHAKE:
        Serial.println("[WS] Handshake completed!");
        break;

    case CLIENT_EVENT_CONNECTED:
        Serial.println("[WS] Connected to server!");
        // Send a message immediately upon connection
        ws.send("Hello from WS Client");
        break;

    case CLIENT_EVENT_DISCONNECTED:
        Serial.println("[WS] Disconnected!");
        break;

    case CLIENT_EVENT_MESSAGE_TEXT:
        Serial.print("[WS] Text: ");
        for (size_t i = 0; i < len; i++)
            Serial.print((char)payload[i]);
        Serial.println();
        break;

    case CLIENT_EVENT_MESSAGE_BINARY:
        NuSock::printf("[WS] Binary: %d bytes\n", len);
        break;

    case CLIENT_EVENT_ERROR:
        NuSock::printf("[WS] Error: %s\n", payload ? (const char *)payload : "Unknown");
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

    // Register Event Callback
    ws.onEvent(onWebSocketEvent);

    // Configure WebSocket Client
#ifdef NUSOCK_CLIENT_USE_LWIP
    ws.begin("echo.websocket.org", 80, "/");
#else
    ws.begin(&wifiClient, "echo.websocket.org", 80, "/");
#endif

    // Initiate Connection
    Serial.println("Connecting to WebSocket Server...");
    if (ws.connect())
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
    ws.loop();

    // Example: Send periodic heartbeat
    static unsigned long lastTime = 0;
    if (millis() - lastTime > 5000)
    {
        lastTime = millis();
        // Only send if connected logic would go here,
        // currently NuSockClient handles state internally and won't send if disconnected
        ws.send("Heartbeat");
    }
}