/**
 * NuSock Secure WebSocket Server (WSS) ESP32 Example
 * This sketch demonstrates how to run a Secure WebSocket Server (WSS) on port 443
 * using the NuSock library with a Self-Signed Certificate.
 * =================================================================================
 * STEP 1: GENERATE CERTIFICATE & KEY
 * =================================================================================
 * You cannot use the dummy certificate in this file. You must generate your own
 * that matches your ESP32's IP address (Common Name/SAN).
 * * 1. Open the provided 'keygen.py' Python script.
 * 2. Edit the 'ESP_IP' variable in the script to match your ESP32's IP address.
 * (It is recommended to set a Static IP on your ESP32 logic or Router).
 * 3. Run the script: `python keygen.py`
 * 4. Copy the output (server_cert and server_key) and paste it into this sketch below.
 * =================================================================================
 * STEP 2: HOW TO TEST
 * =================================================================================
 * Ensure your PC and ESP32 are on the same Wi-Fi network.
 *
 * * OPTION A: Python Client (Easiest)
 * ---------------------------------
 * 1. Open 'test_client.py'.
 * 2. Update the 'ESP_IP' variable.
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

// Define this macro or build flag to use NuSockServerSecure class.
#define NUSOCK_USE_SERVER_SECURE
#include <WiFi.h>
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

NuSockServerSecure wss;

void onWebSocketEvent(NuClient *client, NuServerEvent event, const uint8_t *payload, size_t len)
{
    switch (event)
    {
    case SERVER_EVENT_CONNECT:
        Serial.println("[WSS] WebSocket Server Started.");
        break;

    case SERVER_EVENT_CLIENT_HANDSHAKE:
        Serial.print("[WSS][");
        Serial.print(client->index);
        Serial.println("] Client sent handshake.");
        break;

    case SERVER_EVENT_CLIENT_CONNECTED:
        Serial.print("[WSS][");
        Serial.print(client->index);
        Serial.println("] Client handshake successful - WS OPEN!");
        // Optionally send a welcome message
        wss.send(client->index, "Welcome!");
        break;

    case SERVER_EVENT_CLIENT_DISCONNECTED:
        Serial.print("[WSS][");
        Serial.print(client->index);
        Serial.println("] Client disconnected.");
        break;

    case SERVER_EVENT_MESSAGE_TEXT:
    {
        Serial.print("[WSS][");
        Serial.print(client->index);
        Serial.print("] Received Text: ");
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
        Serial.print("[WSS][");
        Serial.print(client->index);
        Serial.print("] Received Binary: ");
        Serial.print(len);
        Serial.println(" bytes");
        break;

    case SERVER_EVENT_ERROR:
    {
        Serial.print("[WSS][");
        Serial.print(client->index);
        Serial.print("] ");
        char *res = (char *)malloc(len + 1);
        memcpy(res, payload, len);
        res[len] = 0;
        Serial.println(res);
        free(res);
    }
    break;

    default:
        break;
    }
}

void setup()
{
    Serial.begin(115200);
    delay(2000);

    Serial.print("Connecting to WiFi");
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(200);
        Serial.print(".");
    }

    Serial.println(" ✓ Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    wss.onEvent(onWebSocketEvent);

    Serial.println("Starting Secure WebSocket Server...");
    wss.begin(443, server_cert, server_key);
}

void loop()
{
    // Drive the server loop
    wss.loop();

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
}