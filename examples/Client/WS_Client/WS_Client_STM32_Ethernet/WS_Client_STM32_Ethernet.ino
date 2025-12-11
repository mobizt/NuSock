/**
 * NuSock WebSocket Client (WS) STM32 and W5500 Ethernet board Example
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

#include <Ethernet.h>
// #include <LwIP.h>
// #include <STM32Ethernet.h>

#include <NuSock.h>

// CS Pin depends on your wiring. Often PA4 or PB6 on STM32.
// Example: CS on PA4
#define W5500_CS_PIN PA4

int port = 80;
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xAA};

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

    // SPI initialization might be needed depending on your core variant
    // SPI.setMOSI(PA7); SPI.setMISO(PA6); SPI.setSCLK(PA5);
    // SPI.begin();

    Ethernet.init(W5500_CS_PIN);

    NuSock::printLog("INFO", "NuSock WS Client v%s Booting\n", NUSOCK_VERSION_STR);

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