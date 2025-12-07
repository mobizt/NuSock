/**
 * NuSock WebSocket Client (WS) Arduino devices Example
 * This sketch demonstrates how to run a WebSocket Client (WS) on port 80
 * using the NuSock library
 */

#include <Arduino.h>
// For Arduino MKR WiFi 1010, Nano 33 IoT, Arduino MKR VIDOR 4000, Arduino Uno WiFi Rev.2
#include <WiFiNINA.h>

// For Arduino MKR1000 WiFi
// #include <WiFi101.h>

// For UNO R4 WiFi
// #include <WiFiS3.h>

#include <NuSock.h>

// Network Credentials
const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASS";

WiFiClient wifiClient;
NuSockClient wsClient;

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
        wsClient.send("Hello from WS Client");
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
        Serial.println("[WS] Binary: ");
        Serial.print("bytes: ");
        Serial.println(len);
        break;

    case CLIENT_EVENT_ERROR:
        Serial.print("[WS] Error: ");
        if (payload)
            Serial.println((const char *)payload);
        else
            Serial.println("Unknown");
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

    Serial.println(" ✓ Connected!");
    Serial.print("IP Address: ");
    IPAddress ip = WiFi.localIP();
    Serial.print(ip[0]);
    Serial.print(".");
    Serial.print(ip[1]);
    Serial.print(".");
    Serial.print(ip[2]);
    Serial.print(".");
    Serial.println(ip[3]);

    // Register Event Callback
    wsClient.onEvent(onWebSocketEvent);

    // Configure WebSocket Client
    // Using echo.websocket.org (standard testing server)
    // Note: Use port 80 for WS, 443 for WSS (requires WiFiClientSecure)
    wsClient.begin(&wifiClient, "echo.websocket.org", 80, "/");

    // Initiate Connection
    Serial.println("Connecting to WebSocket Server...");
    if (wsClient.connect())
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
    wsClient.loop();

    // Example: Send periodic heartbeat
    static unsigned long lastTime = 0;
    if (millis() - lastTime > 5000)
    {
        lastTime = millis();
        // Only send if connected logic would go here,
        // currently NuSockClient handles state internally and won't send if disconnected
        wsClient.send("Heartbeat");
    }
}