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
#warning "For ESP32/ESP8266, please check the examples/Server/WS_Server/WS_Server_ESP32_ESP8266/WS_Server_ESP32_ESP8266.ino"
#endif

#include <NuSock.h>

const char *ssid = "SSID";
const char *password = "Password";
const uint16_t port = 80;

// Use EthernetServer for STM32, Teensy and other Arduino boards.

WiFiServer server(port); // External Server required for Generic Mode
NuSockServer ws;

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
        ws.send(client->index, "Welcome!");
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
            ws.send(client->index, (const char *)res);
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
    // The baud rate for UNO WiFi Rev 2 should not exceed 57600
    NUSOCK_DEBUG_PORT.begin(115200);
    while (!NUSOCK_DEBUG_PORT)
        ; // Wait for serial

    delay(3000);

    NUSOCK_DEBUG_PORT.println();

    NuSock::printLog("INFO", "NuSock WS Server v%s Booting\n", NUSOCK_VERSION_STR);

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
    NuSock::printLog("WS  ", "Server started on port %d\n", port);
    NuSock::printLog("WS  ", "Ready: ws://%s\n", NuSock::ipStr(WiFi.localIP()));

    ws.onEvent(onWebSocketEvent);

    // Start Server
    server.begin();          // Start the underlying server
    ws.begin(&server, port); // Generic Mode: Pass server reference and port
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