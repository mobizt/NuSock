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
 * 
 * =================================================================================
 * Configurable Macros or build flags to enable RFC 6455 web socket features.
 * =================================================================================
 *
 * NUSOCK_RFC_STRICT_MASK_RSV   To enable strict Masking & RSV bit enforcement
 * NUSOCK_RFC_CLOSE_HANDSHAKE   To enable strict Close Handshake (Echo & Validate)
 * NUSOCK_RFC_FRAGMENTATION     To enable message fragmentation support
 * NUSOCK_RFC_UTF8_STRICT       To enable strict UTF-8 validation for Text Frames
 * NUSOCK_FULL_COMPLIANCE       To enable all RFC compliance features above
 *
 */

#include <Arduino.h>

// Enable all RFC compliance features
#define NUSOCK_FULL_COMPLIANCE

// For internal debug message printing
#define NUSOCK_DEBUG_PORT Serial
#define NUSOCK_DEBUG

// For ESP32 and ESP8266 only.
#define NUSOCK_CLIENT_USE_LWIP

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
        ws.send("Hello from WS Client");
        break;

    case CLIENT_EVENT_DISCONNECTED:
        NuSock::printLog("WS  ", "Disconnected!\n");

        // Note: If NUSOCK_RFC_CLOSE_HANDSHAKE is defined, the client has already
        // echoed the Close frame to the server before this event fires.
        break;

    // Standard messages (Unfragmented)
    case CLIENT_EVENT_MESSAGE_TEXT:
        NuSock::printLog("WS  ", "Text: ");
        for (size_t i = 0; i < len; i++) NUSOCK_DEBUG_PORT.print((char)payload[i]);
        NUSOCK_DEBUG_PORT.println();
        break;

    case CLIENT_EVENT_MESSAGE_BINARY:
        NuSock::printLog("WS  ", "Binary: %d bytes\n", len);
        break;

    // Fragmented messages (Large Data)
    // Use 'client->fragmentOpcode' to determine the type (0x1=Text, 0x2=Binary)
    
    case CLIENT_EVENT_FRAGMENT_START:
    {
        // Identify Type
        const char *type = (client->fragmentOpcode == 0x1) ? "TEXT" : "BINARY";
        NuSock::printLog("WS  ", "Frag Start (%s): %d bytes\n", type, len);

        // TODO: Prepare/Clear your buffer based on 'type'
        // if (client->fragmentOpcode == 0x1) { stringBuffer = ""; }
        // else { byteBuffer.clear(); }
        break;
    }

    case CLIENT_EVENT_FRAGMENT_CONT:
    {
        // Type is still available in fragmentOpcode if needed
        NuSock::printLog("WS  ", "Frag Cont: %d bytes\n", len);
        
        // TODO: Append 'payload' to your buffer
        break;
    }

    case CLIENT_EVENT_FRAGMENT_FIN:
    {
        // This is the final chunk. Identify what we just finished receiving.
        const char *type = (client->fragmentOpcode == 0x1) ? "TEXT" : "BINARY";
        NuSock::printLog("WS  ", "Frag Fin (%s): %d bytes. Full Message Received.\n", type, len);
        
        // TODO: Append final payload and Process the complete message
        if (client->fragmentOpcode == 0x1) {
            // Process Full Text String
        } else {
            // Process Full Binary Buffer
        }
        break;
    }

    case CLIENT_EVENT_ERROR:
        NuSock::printLog("WS  ", "Error: %s\n", payload ? (const char *)payload : "Unknown");
        break;
    }
}

void setup()
{

    NUSOCK_DEBUG_PORT.begin(115200);
    while (!NUSOCK_DEBUG_PORT)
        ; // Wait for serial

    delay(3000);

    NUSOCK_DEBUG_PORT.println();

    NuSock::printLog("INFO", "NuSock WS Client v%s Booting\n", NUSOCK_VERSION_STR);

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
    const char *host = "echo.websocket.org";
    uint16_t port = 80;
    const char *path = "/";
    NuSock::printLog("WS  ", "Connecting to ws://%s:%d/\n", host, port);

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