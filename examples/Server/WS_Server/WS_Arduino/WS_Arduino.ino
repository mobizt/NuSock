/**
 * NuSock WebSocket Server (WS) Arduino device Example
 * This sketch demonstrates how to run a WebSocket Server (WS) on port 80
 * using the NuSock library
 * =================================================================================
 * STEP 1: HOW TO TEST
 * =================================================================================
 * Ensure your PC and Arduino are on the same Wi-Fi network.
 *
 * * OPTION A: Python Client (Easiest)
 * ---------------------------------
 * 1. Open 'test_client.py'.
 * 2. Update the 'DEVICE_IP' variable.
 * 3. Run the script. It is pre-configured to ignore SSL warnings.
 *
 * * OPTION B: Web Browser Client (Critical Manual Step)
 * ---------------------------------------------------
 * 1. Open 'test_client.html', enter the IP, and click Connect.
 * * =================================================================================
 */

// For internal debug message printing
#define NUSOCK_DEBUG

#include <Arduino.h>
// For Arduino MKR WiFi 1010, Nano 33 IoT, Arduino MKR VIDOR 4000, Arduino Uno WiFi Rev.2
#include <WiFiNINA.h>

// For Arduino MKR1000 WiFi
// #include <WiFi101.h>

// For UNO R4 WiFi
// #include <WiFiS3.h>

#include <NuSock.h>

const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASS";

// Use EthernetServer for STM32, Teensy and other Arduino boards.

WiFiServer server(80); // External Server required for Generic Mode
NuSockServer ws;

void onWebSocketEvent(NuClient *client, NuServerEvent event, const uint8_t *payload, size_t len)
{
    switch (event)
    {
    case SERVER_EVENT_CONNECT:
        Serial.println("[WS] WebSocket Server Started.");
        break;

    case SERVER_EVENT_CLIENT_HANDSHAKE:
        NuSock::printf("[WS][%d] Client sent handshake.\n", client->index);
        break;

    case SERVER_EVENT_CLIENT_CONNECTED:
        NuSock::printf("[WS][%d] Client handshake successful - WS OPEN!\n", client->index);
        // Optionally send a welcome message
        ws.send(client->index, "Welcome!");
        break;

    case SERVER_EVENT_CLIENT_DISCONNECTED:
        NuSock::printf("[WS][%d] Client disconnected.\n", client->index);
        break;

    case SERVER_EVENT_MESSAGE_TEXT:
    {
        NuSock::printf("[WS][%d] Received Text: ", client->index);
        for (size_t i = 0; i < len; i++)
            Serial.print((char)payload[i]);
        Serial.println();

        char *res = (char *)malloc(len + 1);
        memcpy(res, payload, len);
        res[len] = 0;

        // Echo back
        ws.send(client->index, (const char *)res);
        free(res);
    }
    break;

    case SERVER_EVENT_MESSAGE_BINARY:
        NuSock::printf("[WS][%d] Received Binary: %d bytes\n", client->index, len);
        break;

    case SERVER_EVENT_ERROR:
        NuSock::printf("[WS][%d] Error: %s\n", client->index, payload ? (const char *)payload : "Unknown");
        break;

    default:
        break;
    }
}

void setup()
{
    Serial.begin(115200);
    delay(5000);

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

    ws.onEvent(onWebSocketEvent);

    // Start Server
    server.begin();        // Start the underlying server
    ws.begin(&server, 80); // Generic Mode: Pass server reference and port
}

void loop()
{
    // Drive the server loop
    ws.loop();

    // Example: Broadcast every 5 seconds
    static unsigned long lastTime = 0;
    if (millis() - lastTime > 5000)
    {
        lastTime = millis();
        if (ws.clientCount() > 0)
        {
            String msg = "Uptime: " + String(millis() / 1000) + "s";
            ws.send(msg.c_str());
        }
    }
}