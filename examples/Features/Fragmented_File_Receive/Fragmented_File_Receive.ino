/**
 * NuSock WebSocket Server (WS) Arduino device Example - File Upload Receiver
 *
 * This sketch demonstrates how to receive a fragmented file upload from a 
 * WebSocket client and save it to an SD card.
 */

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

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
const uint16_t port = 8080; // Changed to 8080 to match Client example

// Define the Chip Select pin for your specific board/shield
const int SD_CS_PIN = 4; // Standard Ethernet/SD Shield

WiFiServer server(port);
NuSockServer ws;

// File handle for the upload
File uploadFile;
// Track which client is currently uploading (simple locking mechanism)
int uploadingClientIndex = -1;

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
        ws.send(client->index, "Ready for upload");
        break;

    case SERVER_EVENT_CLIENT_DISCONNECTED:
        NuSock::printLog("WS  ", "[%d] Client disconnected.\n", client->index);
        // If the uploading client disconnects mid-transfer, close the file
        if (client->index == uploadingClientIndex) {
            if (uploadFile) uploadFile.close();
            uploadingClientIndex = -1;
            NuSock::printLog("FILE", "Upload aborted by disconnect.\n");
        }
        break;

    case SERVER_EVENT_MESSAGE_TEXT:
        NuSock::printLog("WS  ", "[%d] Received Text: %d bytes\n", client->index, len);
        // Echo back
        if (len < 256) {
             char msg[256];
             memcpy(msg, payload, len);
             msg[len] = 0;
             ws.send(client->index, msg);
        }
        break;

    case SERVER_EVENT_MESSAGE_BINARY:
        NuSock::printLog("WS  ", "[%d] Received Binary: %d bytes\n", client->index, len);
        break;

    // Fragmentation support (File Save Logic)

    case SERVER_EVENT_FRAGMENT_START:
    {
        // Check if another client is already uploading
        if (uploadingClientIndex != -1 && uploadingClientIndex != client->index) {
            NuSock::printLog("FILE", "[%d] Upload rejected (Busy)\n", client->index);
            ws.send(client->index, "Error: Server Busy");
            return;
        }

        const char *type = (client->fragmentOpcode == 0x1) ? "TEXT" : "BINARY";
        NuSock::printLog("WS  ", "[%d] Frag Start (%s): %d bytes. Opening file...\n", client->index, type, len);

        // Lock this client
        uploadingClientIndex = client->index;

        // Open file (Overwrite if exists)
        // In a real app, you might parse the filename from a previous text message
        if (SD.exists("uploaded.bin")) SD.remove("uploaded.bin");
        
        uploadFile = SD.open("uploaded.bin", FILE_WRITE);
        if (uploadFile) {
            uploadFile.write(payload, len);
        } else {
            NuSock::printLog("FILE", "Error opening file!\n");
            ws.send(client->index, "Error: IO Failure");
            uploadingClientIndex = -1;
        }
        break;
    }

    case SERVER_EVENT_FRAGMENT_CONT:
    {
        // Only write if this is the correct client and file is open
        if (client->index == uploadingClientIndex && uploadFile) {
            NuSock::printLog("WS  ", "[%d] Frag Cont: %d bytes\n", client->index, len);
            uploadFile.write(payload, len);
        }
        break;
    }

    case SERVER_EVENT_FRAGMENT_FIN:
    {
        if (client->index == uploadingClientIndex && uploadFile) {
             NuSock::printLog("WS  ", "[%d] Frag Fin: %d bytes. Closing file.\n", client->index, len);
             
             // Write final chunk
             uploadFile.write(payload, len);
             
             // Close file
             uploadFile.close();
             
             // Release lock
             uploadingClientIndex = -1;
             
             NuSock::printLog("FILE", "File saved successfully.\n");
             ws.send(client->index, "Upload Complete");
        }
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
    while (!NUSOCK_DEBUG_PORT);

    delay(3000);
    NUSOCK_DEBUG_PORT.println();
    NuSock::printLog("INFO", "NuSock WS Server v%s Booting\n", NUSOCK_VERSION_STR);

    NuSock::printLog("SD  ", "Initializing SD card...");
    if (!SD.begin(SD_CS_PIN)) {
        NuSock::printLog("SD  ", "Initialization failed!");
        while (1);
    }
    NuSock::printLog("SD  ", "Initialization done.\n");

    NuSock::printLog("NET ", "Connecting to WiFi (%s)...\n", ssid);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
    }

    // Waits until we got the IP
    while (WiFi.localIP() == (IPAddress)INADDR_NONE);
    
    NuSock::printLog("NET ", "WiFi Connected (%s)\n", NuSock::ipStr(WiFi.localIP()));
    NuSock::printLog("NET ", "Gateway: %s\n", NuSock::ipStr(WiFi.gatewayIP()));
    NuSock::printLog("WS  ", "Server started on port %d\n", port);
    NuSock::printLog("WS  ", "Ready: ws://%s:%d\n", NuSock::ipStr(WiFi.localIP()), port);

    ws.onEvent(onWebSocketEvent);

    // Start Server
    server.begin();
    ws.begin(&server, port);
}

void loop()
{
    ws.loop();
    
    // Optional status update
    static unsigned long lastTime = 0;
    if (millis() - lastTime > 5000)
    {
        lastTime = millis();
        // If not uploading, broadcast uptime
        if (uploadingClientIndex == -1 && ws.clientCount() > 0)
        {
            String msg = "Uptime: " + String(millis() / 1000) + "s";
            ws.send(msg.c_str());
        }
    }
}