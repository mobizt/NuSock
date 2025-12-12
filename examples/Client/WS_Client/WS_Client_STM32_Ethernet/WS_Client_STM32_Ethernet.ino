/**
 * NuSock WebSocket Client (WS) STM32 (e.g. STM32F103C8) and W5500 Ethernet mobule Example
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
HardwareSerial Serial2(PA3 /* RX */, PA2 /* TX */);
#define NUSOCK_DEBUG_PORT Serial2
#define NUSOCK_DEBUG

// The STM32F103C8T6 Blue Pill - W5500 module
// PA4 - CS
// PA5 - SCLK
// PA6 - MISO
// PA7 - MOSI
// PB0 - RST

#include <SPI.h>
#include <Ethernet.h>

#include <NuSock.h>

#define W5500_CS_PIN PA4
#define W5500_RST_PIN PB0

int port = 80;
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xAA};
EthernetClient client;
NuSockClient ws;

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

        // Note: If NUSOCK_RFC_CLOSE_HANDSHAKE is defined, the client has already
        // echoed the Close frame to the server before this event fires.
        break;

    // Standard messages (Unfragmented)
    case CLIENT_EVENT_MESSAGE_TEXT:
        NuSock::printLog("WS  ", "Text: ");
        for (size_t i = 0; i < len; i++)
            NUSOCK_DEBUG_PORT.print((char)payload[i]);
        NUSOCK_DEBUG_PORT.println();
        break;

    case CLIENT_EVENT_MESSAGE_BINARY:
        NuSock::printLog("WS  ", "Binary: %d bytes\n", len);
        break;

        // Fragmented messages (Large Data)
        // Use 'client->fragmentOpcode' to determine the type (0x1=Text, 0x2=Binary)

    case CLIENT_EVENT_FRAGMENT_START:
    {
        // Identify Type
        const char *type = (client->fragmentOpcode == 0x1) ? "TEXT" : "BINARY";
        NuSock::printLog("WS  ", "Frag Start (%s): %d bytes\n", type, len);

        // TODO: Prepare/Clear your buffer based on 'type'
        // if (client->fragmentOpcode == 0x1) { stringBuffer = ""; }
        // else { byteBuffer.clear(); }
        break;
    }

    case CLIENT_EVENT_FRAGMENT_CONT:
    {
        // Type is still available in fragmentOpcode if needed
        NuSock::printLog("WS  ", "Frag Cont: %d bytes\n", len);

        // TODO: Append 'payload' to your buffer
        break;
    }

    case CLIENT_EVENT_FRAGMENT_FIN:
    {
        // This is the final chunk. Identify what we just finished receiving.
        const char *type = (client->fragmentOpcode == 0x1) ? "TEXT" : "BINARY";
        NuSock::printLog("WS  ", "Frag Fin (%s): %d bytes. Full Message Received.\n", type, len);

        // TODO: Append final payload and Process the complete message
        if (client->fragmentOpcode == 0x1)
        {
            // Process Full Text String
        }
        else
        {
            // Process Full Binary Buffer
        }
        break;
    }

    case CLIENT_EVENT_ERROR:
        NuSock::printLog("WS  ", "Error: %s\n", payload ? (const char *)payload : "Unknown");
        break;
    }
}

void setup()
{
    NUSOCK_DEBUG_PORT.begin(115200);
    while (!NUSOCK_DEBUG_PORT)
        ; // Wait for serial

    NUSOCK_DEBUG_PORT.println();

    pinMode(W5500_RST_PIN, OUTPUT);
    digitalWrite(W5500_RST_PIN, HIGH);

    // Reset W5500 module
    NuSock::printLog("INFO", "Resetting W5500 module...\n");
    digitalWrite(W5500_RST_PIN, LOW);
    delay(1);
    digitalWrite(W5500_RST_PIN, HIGH);
    delay(150);

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
    const char *host = "echo.websocket.org";
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