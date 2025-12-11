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

// For Arduino MKR WiFi 1010, Nano 33 IoT, Arduino MKR VIDOR 4000, Arduino UNO WiFi Rev.2
#if defined(ARDUINO_AVR_UNO_WIFI_REV2) || defined(__AVR_ATmega4809__) || \
    defined(ARDUINO_SAMD_MKRWIFI1010) || defined(ARDUINO_NANO_33_IOT) || \
    defined(ARDUINO_SAMD_MKRVIDOR4000)

#include <WiFiNINA.h>

// For Atduino MKR 1000 WIFI
#elif defined(ARDUINO_SAMD_MKR1000)

#include <WiFi101.h>

// For Atduino UNO R4 WiFi
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
        ws.send(client->index, (const char *)res);
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

void setup()
{
    // The baud rate for UNO WiFi Rev 2 should not exceed 57600
    Serial.begin(115200);
    while (!Serial)
        ; // Wait for serial

    delay(3000);

    Serial.println();

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