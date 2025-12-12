/**
 * NuSock WebSocket Client (WS) Arduino devices Example - Fragmented File Send
 *
 * This sketch demonstrates how to send a file from an SD card as a fragmented
 * WebSocket message using the NuSock library.
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
#warning "For ESP32/ESP8266, please check the examples/Client/WS_Client/WS_Client_ESP32_ESP8266/WS_Client_ESP32_ESP8266.ino"
#endif

#include <NuSock.h>

const char *ssid = "SSID";
const char *password = "Password";
const char *host = "192.168.1.10"; // Change to your PC's IP running the Python server
uint16_t port = 8080;              // Common dev port (echo.websocket.org usually rejects non-SSL on 80)

// SD Card Chip Select Pin (Adjust for your board)
const int SD_CS_PIN = 4;

WiFiClient wifiClient;
NuSockClient ws;
bool fileSent = false; // Flag to ensure we send only once

// Create a Dummy File for Testing
void createDummyFile(const char *filename)
{
    if (SD.exists(filename))
        return;

    NuSock::printLog("FILE", "Creating dummy file: %s\n", filename);
    File f = SD.open(filename, FILE_WRITE);
    if (f)
    {
        for (int i = 0; i < 50; i++)
        {
            f.println("This is a line of text in the dummy file. We repeat it to create size.");
        }
        f.close();
    }
}

// Send file fragmented
void sendFileFragmented(const char *filename)
{
    NuSock::printLog("FILE", "Opening %s to send...\n", filename);
    File file = SD.open(filename, FILE_READ);
    if (!file)
    {
        NuSock::printLog("FILE", "Failed to open file!\n");
        return;
    }

    size_t fileSize = file.size();
    size_t totalSent = 0;
    uint8_t buffer[128]; // Small chunk size to force fragmentation on small files

    // Read first chunk
    size_t len = file.read(buffer, sizeof(buffer));

    if (len == fileSize)
    {
        // File is smaller than one chunk: Send as normal message
        ws.send(buffer, len);
    }
    else
    {
        // Start Fragmentation (Binary Mode = true)
        // We use the new API method: sendFragmentStart(payload, len, isBinary)
        ws.sendFragmentStart(buffer, len, true);
        totalSent += len;

        // Loop for middle chunks
        while (file.available())
        {
            len = file.read(buffer, sizeof(buffer));
            size_t remaining = file.available();

            if (remaining > 0)
            {
                // Middle Fragment
                ws.sendFragmentCont(buffer, len);
            }
            else
            {
                // Final Fragment
                ws.sendFragmentFin(buffer, len);
            }
            totalSent += len;

            // Optional: Small delay to not overwhelm slow serial/receivers
            delay(10);
        }
    }

    file.close();
    NuSock::printLog("FILE", "Sent %d bytes fragmented.\n", totalSent);
}

void onWebSocketEvent(NuClient *client, NuClientEvent event, const uint8_t *payload, size_t len)
{
    switch (event)
    {
    case CLIENT_EVENT_HANDSHAKE:
        NuSock::printLog("WS  ", "Handshake completed!\n");
        break;

    case CLIENT_EVENT_CONNECTED:
        NuSock::printLog("WS  ", "Connected to server!\n");
        break;

    case CLIENT_EVENT_DISCONNECTED:
        NuSock::printLog("WS  ", "Disconnected!\n");
        break;

    case CLIENT_EVENT_MESSAGE_TEXT:
        NuSock::printLog("WS  ", "Text Received: %d bytes\n", len);
        break;

    case CLIENT_EVENT_MESSAGE_BINARY:
        NuSock::printLog("WS  ", "Binary Received: %d bytes\n", len);
        break;

    // Receiving fragmentation support
    case CLIENT_EVENT_FRAGMENT_START:
        NuSock::printLog("WS  ", "Frag Start: %d bytes\n", len);
        break;

    case CLIENT_EVENT_FRAGMENT_CONT:
        NuSock::printLog("WS  ", "Frag Cont: %d bytes\n", len);
        break;

    case CLIENT_EVENT_FRAGMENT_FIN:
        NuSock::printLog("WS  ", "Frag Fin: %d bytes. Full Message Received.\n", len);
        break;

    case CLIENT_EVENT_ERROR:
        NuSock::printLog("WS  ", "Error: %s\n", payload ? (const char *)payload : "Unknown");
        break;
    }
}

void setup()
{
    NUSOCK_DEBUG_PORT.begin(115200);
    while (!NUSOCK_DEBUG_PORT)
        ;

    delay(3000);
    NUSOCK_DEBUG_PORT.println();
    NuSock::printLog("INFO", "NuSock WS Client v%s Booting", NUSOCK_VERSION_STR);

    // SD card init
    NuSock::printLog("SD  ", "Initializing SD card...");
    if (!SD.begin(SD_CS_PIN))
    {
        NuSock::printLog("SD  ", "Initialization failed!");
        while (1)
            ;
    }
    NuSock::printLog("SD  ", "Initialization done.");

    // Ensure we have a file to send
    createDummyFile("test.txt");

    NuSock::printLog("NET ", "Connecting to WiFi (%s)...\n", ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
    }
    Serial.println();
    NuSock::printLog("NET ", "WiFi Connected (%s)\n", NuSock::ipStr(WiFi.localIP()));

    ws.onEvent(onWebSocketEvent);
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
    ws.loop();

    // Check if connected and file hasn't been sent yet
    if (ws.connected() && !fileSent)
    {

        // Wait a small moment after connection before sending heavy data
        delay(1000);

        NuSock::printLog("APP ", "Starting File Upload...\n");
        sendFileFragmented("test.txt");

        fileSent = true; // Prevent looping
    }
}