/**
 * NuSock Secure WebSocket Server (WSS) ESP8266 Example
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

#include <ESP8266WiFi.h>

#include "NuSock.h"

// REPLACE WITH YOUR CONTENTS FROM keygen.py OUTPUT
const char *server_cert = "-----BEGIN CERTIFICATE-----\n...";
const char *server_key = "-----BEGIN PRIVATE KEY-----\n...";

const char *ssid = "SSID";
const char *password = "Password";
const uint16_t port = 443;

NuSockServer wss;
WiFiServerSecure server(port);

void onWebSocketEvent(NuClient *client, NuServerEvent event, const uint8_t *payload, size_t len)
{
    switch (event)
    {
    case SERVER_EVENT_CONNECT:
        NuSock::printLog("WS  ", "WebSocket Server Started.\n");
        break;

    case SERVER_EVENT_CLIENT_HANDSHAKE:
        NuSock::printLog("WS  ", "[%d] Client sent handshake.\n", client->index);
        break;

    case SERVER_EVENT_CLIENT_CONNECTED:
        NuSock::printLog("WS  ", "[%d] Client handshake successful.\n", client->index);
        // Optionally send a welcome message
        wss.send(client->index, "Welcome!");
        break;

    case SERVER_EVENT_CLIENT_DISCONNECTED:
        NuSock::printLog("WS  ", "[%d] Client disconnected.\n", client->index);
        break;

    case SERVER_EVENT_MESSAGE_TEXT:
    {
        NuSock::printLog("WS  ", "[%d] Received Text: ", client->index);
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
        NuSock::printLog("WS  ", "[%d] Received Binary: %d bytes\n", client->index, len);
        break;

    case SERVER_EVENT_ERROR:
        NuSock::printLog("WS  ", "[%d] Error: %s\n", client->index, payload ? (const char *)payload : "Unknown");
        break;

    default:
        break;
    }
}

BearSSL::X509List cert(server_cert);
BearSSL::PrivateKey key(server_key);

void setup()
{
    Serial.begin(115200);
    while (!Serial)
        ; // Wait for serial

    delay(3000);

    Serial.println();

    NuSock::printLog("INFO", "NuSock WSS Server v%s Booting\n", NUSOCK_VERSION_STR);

    // Connect to WiFi
    NuSock::printLog("NET ", "Connecting to WiFi (%s)...\n", ssid);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
    }

    // Waits until we got the IP
    while (WiFi.localIP() == (IPAddress)INADDR_NONE)
        ;

    NuSock::printLog("NET ", "WiFi Connected (%s)\n", NuSock::ipStr(WiFi.localIP()));
    NuSock::printLog("NET ", "Gateway: %s\n", NuSock::ipStr(WiFi.gatewayIP()));
    NuSock::printLog("WARN", "Using self-signed certificate\n");
    NuSock::printLog("WS  ", "Server started on port %d\n", port);
    NuSock::printLog("WS  ", "Ready: wss://%s\n", NuSock::ipStr(WiFi.localIP()));

    wss.onEvent(onWebSocketEvent);

    server.setRSACert(&cert, &key);

    // Start secure WebSocket server
    server.begin(port);
    wss.begin(&server, port);
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
