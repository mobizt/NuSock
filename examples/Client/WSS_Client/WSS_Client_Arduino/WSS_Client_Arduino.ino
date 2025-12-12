/**
 * NuSock Secure WebSocket Client (WSS) Arduino devices Example
 * This sketch demonstrates how to run a Secure WebSocket Client (WSS) on port 443
 * using the NuSock library
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

#if defined(ARDUINO_AVR_UNO_WIFI_REV2) || defined(__AVR_ATmega4809__) || \
    defined(ARDUINO_SAMD_MKRWIFI1010) || defined(ARDUINO_NANO_33_IOT) || \
    defined(ARDUINO_SAMD_MKRVIDOR4000)
#include <WiFiNINA.h>
#elif defined(ARDUINO_SAMD_MKR1000)
#include <WiFi101.h>
#elif defined(ARDUINO_UNOR4_WIFI)
#include <WiFiS3.h>
#elif defined(ARDUINO_PORTENTA_C33)
#include <WiFiC3.h>
#elif defined(ARDUINO_RASPBERRY_PI_PICO_W) || defined(ARDUINO_GIGA) || \
    defined(ARDUINO_OPTA) || defined(ARDUINO_PORTENTA_H7_M7)
#include <WiFi.h>
#else
#warning "For ESP32, please check the examples/Client/WSS_Client/WSS_Client_ESP32/WS_Client_ESP32.ino"
#warning "For ESP8266/RP2040 (Pico W), please check the examples/Client/WSS_Client/WSS_Client_ESP8266_PicoW/WSS_Client_ESP8266_PicoW.ino"
#endif

#include "NuSock.h"

// Network Credentials
const char *ssid = "SSID";
const char *password = "Password";

WiFiSSLClient wifiClient;
NuSockClient wss;

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
    // The baud rate for UNO WiFi Rev 2 should not exceed 57600
    NUSOCK_DEBUG_PORT.begin(115200);
    while (!NUSOCK_DEBUG_PORT)
        ; // Wait for serial

    delay(3000);

    NUSOCK_DEBUG_PORT.println();

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

    // Register Event Callback
    wss.onEvent(onWebSocketEvent);

    // Configure WebSocket Client
    const char *host = "echo.websocket.org";
    uint16_t port = 443;
    const char *path = "/";
    NuSock::printLog("WS  ", "Connecting to wss://%s:%d/\n", host, port);

    wss.begin(&wifiClient, host, port, path);

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