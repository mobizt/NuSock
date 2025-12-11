/**
 * NuSock WebSocket Client (WS) Teensy 4.0 device Example
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
// paulstoffregen/Ethernet
#include <Ethernet.h>

#include <NuSock.h>

int port = 80;
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xAA};

// W5500 - Teensy 4.0 Connection:
// MOSI -> 11, MISO -> 12, SCK -> 13, CS -> 10, RST -> 9
uint8_t csPin = 10;
uint8_t rstPin = 9;

EthernetClient client;
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
    Serial.begin(115200);
    while (!Serial)
        ; // Wait for serial

    Serial.println();

    NuSock::printLog("INFO", "NuSock WS Client v%s Booting\n", NUSOCK_VERSION_STR);

    pinMode(rstPin, OUTPUT);
    digitalWrite(rstPin, HIGH);

    // Reset W5500 module
    NuSock::printLog("INFO", "Resetting W5500 module...\n");
    digitalWrite(rstPin, LOW);
    delay(1);
    digitalWrite(rstPin, HIGH);
    delay(150);

    NuSock::printLog("NET", "Connecting to Ethernet...\n");

    if (Ethernet.begin(mac) == 0)
    {
        NuSock::printLog("NET", "Error: DHCP Failed.\n");
        while (1)
            ;
    }

    NuSock::printLog("NET ", "Ethernet Connected (%s)\n", NuSock::ipStr(Ethernet.localIP()));
    NuSock::printLog("NET ", "Gateway: %s\n", NuSock::ipStr(Ethernet.gatewayIP()));

    // Register Event Callback
    ws.onEvent(onWebSocketEvent);

    // Configure WebSocket Client
    char *host = "echo.websocket.org";
    uint16_t port = 80;
    const char *path = "/";

    NuSock::printLog("WS  ", "Connecting to ws://%s:%d/\n", host, port);

     ws.begin(&client, host, port, path);

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