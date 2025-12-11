/**
 * NuSock WebSocket Client (WS) Mega 2560 board with Ethernet shield Example
 * This sketch demonstrates how to run a WebSocket Client (WS) on port 80
 * using the NuSock library
 *
 * Most websocket servers reject the connection via non-SSL port e.g echo.websocket.org.
 *
 * For this reason, please runnning websocket server on your PC by running
 * the python script simple_server.py or running run.bat or run.sh.
 *
 * To test python script websocket server,
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


EthernetServer server(80);
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
    Serial.begin(115200);
    while (!Serial)
        ; // Wait for serial

    Serial.println();

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
    NuSock::printLog("WS  ", "Server started on port %d\n", port);
    NuSock::printLog("WS  ", "Ready: ws://%s\n", NuSock::ipStr(Ethernet.localIP()));

    ws.onEvent(onWebSocketEvent);

    // Start Server
    server.begin();        // Start the underlying server
    ws.begin(&server, 80); // Generic Mode: Pass server reference and port
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