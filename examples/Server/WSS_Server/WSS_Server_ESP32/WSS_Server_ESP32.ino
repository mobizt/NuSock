/**
 * NuSock Secure WebSocket Server (WSS) ESP32 Example
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

// Define this macro or build flag to use NuSockServerSecure class.
#define NUSOCK_USE_SERVER_SECURE
#include <WiFi.h>

#include "NuSock.h"

// Replace with your contents keygen.py output
const char *server_cert = "-----BEGIN CERTIFICATE-----\n...";
const char *server_key = "-----BEGIN PRIVATE KEY-----\n...";

const char *ssid = "SSID";
const char *password = "Password";
const uint16_t port = 443;

NuSockServerSecure wss;

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

    // Standard messages (Unfragmented)
    case SERVER_EVENT_MESSAGE_TEXT:
    {
        NuSock::printLog("WS  ", "[%d] Received Text: ", client->index);
        for (size_t i = 0; i < len; i++)
            NUSOCK_DEBUG_PORT.print((char)payload[i]);
        NUSOCK_DEBUG_PORT.println();

        // Echo back (Simple echo for unfragmented messages)
        // Note: For large/fragmented messages, you must buffer them yourself before echoing
        char *res = (char *)malloc(len + 1);
        if (res)
        {
            memcpy(res, payload, len);
            res[len] = 0;
            wss.send(client->index, (const char *)res);
            free(res);
        }
    }
    break;

    case SERVER_EVENT_MESSAGE_BINARY:
        NuSock::printLog("WS  ", "[%d] Received Binary: %d bytes\n", client->index, len);
        break;

        // RFC 6455 fragmentation support
        // Handle large messages split into multiple frames

    case SERVER_EVENT_FRAGMENT_START:
    {
        // Identify Type: 0x1 = Text, 0x2 = Binary
        const char *type = (client->fragmentOpcode == 0x1) ? "TEXT" : "BINARY";
        NuSock::printLog("WS  ", "[%d] Frag Start (%s): %d bytes\n", client->index, type, len);

        // TODO: Initialize a buffer for this client (client->index)
        // buffer[client->index] = new Buffer();
        // buffer[client->index].append(payload, len);
        break;
    }

    case SERVER_EVENT_FRAGMENT_CONT:
    {
        NuSock::printLog("WS  ", "[%d] Frag Cont: %d bytes\n", client->index, len);
        // TODO: Append to client's buffer
        // buffer[client->index].append(payload, len);
        break;
    }

    case SERVER_EVENT_FRAGMENT_FIN:
    {
        const char *type = (client->fragmentOpcode == 0x1) ? "TEXT" : "BINARY";
        NuSock::printLog("WS  ", "[%d] Frag Fin (%s): %d bytes. Full Message Received.\n", client->index, type, len);

        // TODO: Finalize buffer and Process complete message
        // buffer[client->index].append(payload, len);
        // processMessage(buffer[client->index]);
        // buffer[client->index].clear();
        break;
    }

    case SERVER_EVENT_ERROR:
        NuSock::printLog("WS  ", "[%d] Error: %s\n", client->index, payload ? (const char *)payload : "Unknown");
        break;

    default:
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

    // Start secure WebSocket server
    wss.begin(port, server_cert, server_key);
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
