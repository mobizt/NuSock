/**
 * NuSock Secure WebSocket Server (WSS) ESP32/ESP8266/RPi Pico W Example
 * This sketch demonstrates how to run a Secure WebSocket Server (WSS) on port 443
 * using the NuSock library with a Self-Signed Certificate.
 * =================================================================================
 * STEP 1: GENERATE CERTIFICATE & KEY
 * =================================================================================
 * You cannot use the dummy certificate in this file. You must generate your own
 * that matches your Arduino device's IP address (Common Name/SAN).
 * * 1. Open the provided 'keygen.py' Python script.
 * 2. Edit the 'DEVICE_IP' variable in the script to match your Arduino device's IP address.
 * (It is recommended to set a Static IP on your Arduino device logic or Router).
 * 3. Run the script: `python keygen.py`
 * 4. Copy the output (server_cert and server_key) and paste it into this sketch below.
 * =================================================================================
 * STEP 2: HOW TO TEST
 * =================================================================================
 * Ensure your PC and Arduino device are on the same Wi-Fi network.
 *
 * * OPTION A: Python Client (Easiest)
 * ---------------------------------
 * 1. Open 'test_client.py'.
 * 2. Update the 'DEVICE_IP' variable.
 * 3. Run the script. It is pre-configured to ignore SSL warnings.
 *
 * * OPTION B: Web Browser Client (Critical Manual Step)
 * ---------------------------------------------------
 * Browsers block self-signed WSS connections by default. You must manually "accept"
 * the certificate before the JavaScript client can connect.
 * * 1. Open your Web Browser (Chrome/Edge/etc).
 * 2. Type the address: https://<DEVICE_IP>:443
 * (Example: https://192.168.1.100:443)
 * 3. You will see a "Your connection is not private" or "Not Secure" warning.
 * 4. Click "Advanced" and then "Proceed to <IP> (unsafe)".
 * 5. Once the browser accepts it (you might see a blank page), close that tab.
 * 6. Now open 'test_client.html', enter the IP, and click Connect.
 * * =================================================================================
 */

// For internal debug message printing
#define NUSOCK_DEBUG

#if defined(ESP32)
// Define this macro or build flag to use NuSockServerSecure class.
#define NUSOCK_USE_SERVER_SECURE
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ARDUINO_RASPBERRY_PI_PICO_W)
#include <WiFi.h>
#endif

#include "NuSock.h"

// REPLACE WITH YOUR CONTENTS FROM keygen.py OUTPUT
const char *server_cert =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDrjCCApagAwIBAgIUJ+...\n"
    "...\n"
    "YOUR_BASE64_CERT_DATA_HERE\n"
    "...\n"
    "-----END CERTIFICATE-----\n";

const char *server_key =
    "-----BEGIN PRIVATE KEY-----\n"
    "MIIEvgIBADANBgkqhkiG9w0B...\n"
    "...\n"
    "YOUR_BASE64_KEY_DATA_HERE\n"
    "...\n"
    "-----END PRIVATE KEY-----\n";

const char *ssid = "YourSSID";
const char *password = "YourPassword";

#if defined(ESP32)
NuSockServerSecure wss;
#elif defined(ESP8266) || defined(ARDUINO_RASPBERRY_PI_PICO_W)
NuSockServer wss;
WiFiServerSecure server(443);
#endif

void onWebSocketEvent(NuClient *client, NuServerEvent event, const uint8_t *payload, size_t len)
{
    switch (event)
    {
    case SERVER_EVENT_CONNECT:
        Serial.println("[WSS] WebSocket Server Started.");
        break;

    case SERVER_EVENT_CLIENT_HANDSHAKE:
        NuSock::printf("[WSS][%d] Client sent handshake.\n", client->index);
        break;

    case SERVER_EVENT_CLIENT_CONNECTED:
        NuSock::printf("[WSS][%d] Client handshake successful - WS OPEN!\n", client->index);
        // Optionally send a welcome message
        wss.send(client->index, "Welcome!");
        break;

    case SERVER_EVENT_CLIENT_DISCONNECTED:
        NuSock::printf("[WSS][%d] Client disconnected.\n", client->index);
        break;

    case SERVER_EVENT_MESSAGE_TEXT:
    {
        NuSock::printf("[WSS][%d] Received Text: ", client->index);
        for (size_t i = 0; i < len; i++)
            Serial.print((char)payload[i]);
        Serial.println();

        char *res = (char *)malloc(len + 1);
        memcpy(res, payload, len);
        res[len] = 0;

        // Echo back
        wss.send(client->index, (const char *)res);
        free(res);
    }
    break;

    case SERVER_EVENT_MESSAGE_BINARY:
        NuSock::printf("[WSS][%d] Received Binary: %d bytes\n", client->index, len);
        break;

    case SERVER_EVENT_ERROR:
        NuSock::printf("[WSS][%d] Error: %s\n", client->index, payload ? (const char *)payload : "Unknown");
        break;

    default:
        break;
    }
}

#if defined(ESP8266) || defined(ARDUINO_RASPBERRY_PI_PICO_W)
BearSSL::X509List cert(server_cert);
BearSSL::PrivateKey key(server_key);
#endif

void setup()
{
    Serial.begin(115200);
    delay(1000);

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

    wss.onEvent(onWebSocketEvent);

#if defined(ESP8266) || defined(ARDUINO_RASPBERRY_PI_PICO_W)
    server.setRSACert(&cert, &key);
#endif

    Serial.println("Starting Secure WebSocket Server...");

// Start secure WebSocket server
#if defined(ESP32)
    wss.begin(443, server_cert, server_key);
#elif defined(ESP8266) || defined(ARDUINO_RASPBERRY_PI_PICO_W)
    server.begin(443);
    wss.begin(&server, 443);
#endif
}

void loop()
{
    wss.loop(); // Process SSL clients

    // Example: Broadcast every 5 seconds
    static unsigned long lastTime = 0;
    if (millis() - lastTime > 5000)
    {
        lastTime = millis();
        if (wss.clientCount() > 0)
        {
            String msg = "Uptime: " + String(millis() / 1000) + "s";
            wss.send(msg.c_str());
        }
    }

    delay(10);
}
