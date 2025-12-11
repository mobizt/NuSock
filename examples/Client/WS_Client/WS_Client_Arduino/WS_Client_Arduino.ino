/**
 * NuSock WebSocket Client (WS) Arduino devices Example
 * This sketch demonstrates how to run a WebSocket Client (WS) on port 80
 * using the NuSock library
 *
 * Most WebSocket servers reject connections via non-SSL ports (e.g., echo.websocket.org).
 *
 * For this reason, please run the WebSocket server on your PC by running
 * the Python script 'simple_server.py', or by running 'run.bat' or 'run.sh'.
 *
 * To test the Python script WebSocket server,
 * your PC should connect to the same network as your Arduino devices.
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

#warning "For ESP32/ESP8266, please check the examples/Client/WS_Client/WS_Client_ESP32_ESP8266/WS_Client_ESP32_ESP8266.ino"

#endif

// The non-SSL websocket server connection
// might fail in the case of UNO R4 WiFi,
// use WiFiSSLClient and port 443 instead.
#include <NuSock.h>

// Network Credentials
const char *ssid = "SSID";
const char *password = "Password";

// Use EthernetClient for STM32, Teensy and other Arduino boards.
// #include <Ethernet.h>
// EthernetClient ethClient;

WiFiClient wifiClient;
NuSockClient ws;

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
        break;

    case CLIENT_EVENT_MESSAGE_TEXT:
        NuSock::printLog("WS  ", "Text: ");
        for (size_t i = 0; i < len; i++)
            Serial.print((char)payload[i]);
        Serial.println();
        break;

    case CLIENT_EVENT_MESSAGE_BINARY:
        NuSock::printLog("WS  ", "Binary: %d bytes\n", len);
        break;

    case CLIENT_EVENT_ERROR:
        NuSock::printLog("WS  ", "Error: %s\n", payload ? (const char *)payload : "Unknown");
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

    NuSock::printLog("INFO", "NuSock WS Client v%s Booting", NUSOCK_VERSION_STR);

    // Connect to WiFi
    NuSock::printLog("NET ", "Connecting to WiFi (%s)...", ssid);
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
    char *host = "echo.websocket.org";
    uint16_t port = 80;
    const char *path = "/";
    NuSock::printLog("WS  ", "Connecting to ws://%s:%d/\n", host, port);

     ws.begin(&wifiClient, host, port, path);

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