/**
 * NuSock WebSocket Client (WS) ESP32/ESP8266 devices Example
 * This sketch demonstrates how to run a WebSocket Client (WS) on port 80
 * using the NuSock library
 *
 * For non-SSL web socket server for highest performance, async operation,
 * using lwIP directly is recommended by defining NUSOCK_CLIENT_USE_LWIP
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
const char *ssid = "SSID";
const char *password = "Password";

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

    NuSock::printLog("INFO", "NuSock WS Client v%s Booting\n", NUSOCK_VERSIOn_STR);

    // Connect to WiFi
    NuSock::printLog("NET ", "Connecting to WiFi (%s)...\n", ssid);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
    }

    NuSock::printLog("NET ", "WiFi Connected (%s)\n", NuSock::ipStr(WiFi.localIP()));
    NuSock::printLog("NET ", "Gateway: %s\n", NuSock::ipStr(WiFi.gatewayIP()));

    // Register Event Callback
    ws.onEvent(onWebSocketEvent);

    // Configure WebSocket Client
    char *host = "echo.websocket.org";
    uint16_t port = 80;
    const char *path = "/";
    NuSock::printLog("WS  ", "Connecting to wss://%s:%d/\n", host, port);

#ifdef NUSOCK_CLIENT_USE_LWIP
    ws.begin(host, port, path);
#else
    ws.begin(&wifiClient, host, port, path);
#endif

    if (ws.connect())
        NuSock::printLog("WS  ", "Connection request sent.\n");
    else
        NuSock::printLog("WS  ", "Connection failed immediately.\n");
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