# NuSock

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-ESP32%20%7C%20ESP8266%20%7C%20RP2040%20%7C%20SAMD%20%7C%20Renesas%20%7C%20Portenta%20%7C%20Giga%20%7C%20Opta%20%7C%20AVR-orange.svg)
![Version](https://img.shields.io/badge/version-2.0.0-green.svg)

**NuSock** is a lightweight, high-performance WebSocket library designed for embedded systems. It bridges the gap between ease of use and raw performance by offering a **Dual-Mode Architecture** (Generic vs LwIP) and **Secure WebSocket (WSS)** support.

It features a **Zero-Interrupt Architecture** for Generic mode, ensuring stability on UART-based WiFi modules like the **Arduino UNO R4 WiFi**, **Nano 33 IoT**, **Portenta C33**, and **Uno WiFi Rev2**.

---

## 📑 Table of Contents
- [Features](#-features)
- [Supported Platforms](#-supported-platforms)
- [Installation](#-installation)
- [Configuration Macros](#-configuration-macros)
- [RFC 6455 Compliance](#-rfc-6455-compliance-macros)
- [Usage Guide](#-usage-guide)
    - [1. Secure Server (ESP32 Native)](#1-secure-websocket-server-wss-esp32-native)
    - [2. Standard Server (LwIP Mode)](#2-standard-server-esp32esp8266---lwip-mode)
    - [3. Standard Server (Generic Mode)](#3-standard-server-uno-r4--portenta--giga--mkr---generic-mode)
    - [4. Secure Client (ESP32 Native)](#4-secure-websocket-client-wss-esp32-native)
    - [5. Generic Client (WS/WSS)](#5-generic-websocket-client-wswss)
- [Advanced Features](#-advanced-features)
    - [Sending Fragmented Data](#sending-fragmented-data-streaming)
    - [Graceful Disconnect](#graceful-disconnect-close-handshake)
- [License](#-license)

---

## 🚀 Features

* **🔒 Secure WebSockets (WSS):**
    * **ESP32:** Native, high-performance WSS Server and Client using the internal `esp_tls` stack.
    * **ESP8266 / Pico W:** WSS Server/Client support via `WiFiServerSecure` / `WiFiClientSecure` wrapping.
* **⚡ Dual-Mode Architecture:**
    * **Generic Mode:** Maximum compatibility using standard `WiFiServer` / `WiFiClient` wrapping. Supports **accept()** logic for robust connection handling on newer boards.
    * **LwIP Mode:** High-performance, low-overhead mode using native LwIP callbacks (ESP32/ESP8266).
* **📜 RFC 6455 Compliance:**
    * **Fragmentation:** Supports sending and receiving large messages via streaming fragments.
    * **Close Handshake:** Supports graceful shutdowns with status codes and reasoning.
    * **Strict Mode:** Optional strict UTF-8 validation and Masking enforcement for enterprise environments.
* **🛡️ Robust Stability:**
    * **Zero-Interrupt Locking:** Prevents UART deadlocks on Arduino Uno R4 WiFi, Nano 33 IoT, Portenta C33, and Uno WiFi Rev2.
    * **Smart Duplicate Detection:** Automatically handles and cleans up duplicate socket handles returned by underlying WiFi libraries.
* **📨 Event-Driven:** Non-blocking, callback-based architecture for handling Connect, Disconnect, Text, Binary, and Fragment events.

---

## 📦 Supported Platforms

| Platform | WS Server | WSS Server | WS Client | WSS Client | Mode |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **ESP32** | ✅ | ✅ | ✅ | ✅ | LwIP / Generic |
| **ESP8266** | ✅ | ✅ | ✅ | ✅ | LwIP / Generic |
| **RP2040 (Pico W)** | ✅ | ✅ | ✅ | ✅ | Generic |
| **Arduino UNO R4 WiFi** | ✅ | ❌ | ✅ | ✅ | Generic |
| **Portenta / Giga / Opta** | ✅ | ❌ | ✅ | ✅ | Generic |
| **Arduino UNO WiFi Rev2** | ✅ | ❌ | ✅ | ✅ | Generic |
| **SAMD (MKR / Nano 33)**| ✅ | ❌ | ✅ | ✅ | Generic |
| **Teensy 3.2 - 4.1** | ✅ | ❌ | ✅ | ✅ | Generic |
| **STM32**| ✅ | ❌ | ✅ | ✅ | Generic |
| **AVR**| ✅ | ❌ | ✅ | ✅ | Generic |

---

### 📝 Platform Notes
* **ESP8266 / Pico W:** 
  * WSS Server requires passing a `WiFiServerSecure` instance to `begin()`.
* **Arduino Pro (Portenta/Giga/Opta):**
  * **Portenta C33:** Uses `WiFiC3.h`.
  * **Portenta H7 / Giga R1 / Opta:** Use the mbed-native `WiFi.h`.
* **Arduino R4 / SAMD / AVR:** 
  * WS Server requires passing a `WiFiServer` instance.
  * WSS Client requires Root CA certificates to be uploaded via the Firmware Updater tool where applicable.
* **STM32 / AVR (Ethernet):** * Requires passing `EthernetClient` or `EthernetServer` instances.


## 📦 Installation

### Option 1: Arduino IDE
1.  Navigate to **Sketch** → **Include Library** → **Manage Libraries...**
2.  Search for `NuSock`.
3.  Click **Install**.

### Option 2: PlatformIO
Add the following to your `platformio.ini`:

```ini
lib_deps =
    suwatchai/NuSock
```

---

## ⚙️ Configuration Macros

Define these macros **before** including `NuSock.h` to enable specific modes.

| Macro | Description | Target |
| :--- | :--- | :--- |
| `NUSOCK_SERVER_USE_LWIP` | Enables LwIP async mode for **Server**. Reduces RAM/CPU overhead. | ESP32, ESP8266 |
| `NUSOCK_CLIENT_USE_LWIP` | Enables LwIP async mode for **Client** (Plain WS). | ESP32, ESP8266 |
| `NUSOCK_USE_SERVER_SECURE` | Enables `NuSockServerSecure` class (Native SSL). | ESP32 |

### 📜 RFC 6455 Compliance Macros
Use these to enable strict protocol features.

| Macro | Description |
| :--- | :--- |
| `NUSOCK_FULL_COMPLIANCE` | **Enables All** features below. |
| `NUSOCK_RFC_FRAGMENTATION` | Enables processing of fragmented messages (Streaming). |
| `NUSOCK_RFC_CLOSE_HANDSHAKE` | Enables strict Close Handshake (Echoing & State Machine). |
| `NUSOCK_RFC_STRICT_MASK_RSV` | Enforces strict Masking (Client->Server) and RSV bit checks. |
| `NUSOCK_RFC_UTF8_STRICT` | Enforces strict UTF-8 validation on Text Frames. |

---

## 📖 Usage Guide

### 1. Secure WebSocket Server (WSS) [ESP32 Native]
ESP32 uses the native `NuSockServerSecure` class for optimal performance without relying on `WiFiClientSecure`.

```cpp
#define NUSOCK_USE_SERVER_SECURE
#include <WiFi.h>
#include "NuSock.h"

// Certificates (Generate using keygen.py)
const char* server_cert = "-----BEGIN CERTIFICATE-----\n...";
const char* server_key = "-----BEGIN PRIVATE KEY-----\n...";

NuSockServerSecure wss;

void setup() {
    WiFi.begin("SSID", "PASS");
    // wait for connection
    
    wss.onEvent(onEvent); // Define callback
    wss.begin(443, server_cert, server_key);
}

void loop() {
    wss.loop();
}
```

### 2. Standard Server (ESP32/ESP8266 - LwIP Mode)
Ideal for high-performance non-SSL applications.

```cpp
#define NUSOCK_SERVER_USE_LWIP 
#include <WiFi.h> // or ESP8266WiFi.h
#include "NuSock.h"

NuSockServer ws;

void setup() {
    WiFi.begin("SSID", "PASS");
    // wait
    
    ws.onEvent(onEvent);
    ws.begin(80); // Internal LwIP server created automatically
}

void loop() {
    ws.loop();
}
```

### 3. Standard Server (Uno R4 / Portenta / Giga / MKR - Generic Mode)
Compatible with **Arduino Uno R4 WiFi**, **Portenta C33/H7**, **Giga R1**, **Nano 33 IoT**, **MKR 1010**, **Uno WiFi Rev2**, etc. NuSock wraps the existing `WiFiServer` object.

```cpp
// Select the correct library for your board:
#if defined(ARDUINO_UNOR4_WIFI)
  #include <WiFiS3.h>
#elif defined(ARDUINO_PORTENTA_C33)
  #include <WiFiC3.h>
#elif defined(ARDUINO_PORTENTA_H7_M7) || defined(ARDUINO_GIGA)
  #include <WiFi.h>
#else
  #include <WiFiNINA.h> // Nano 33 IoT, MKR 1010, Vidor 4000, Uno WiFi Rev2
#endif

#include "NuSock.h"

// 1. Create the standard WiFiServer
WiFiServer server(80);
NuSockServer ws;

void setup() {
    WiFi.begin("SSID", "PASS");
    // wait

    // Start the underlying server
    server.begin();

    // Bind NuSock to it
    ws.onEvent(onEvent);
    ws.begin(&server, 80);
}

void loop() {
    ws.loop();
}
```

### 4. Secure WebSocket Client (WSS) [ESP32 Native]
ESP32 can use `NuSockClientSecure` which uses the native `esp_tls` stack for high performance.

```cpp
#include <WiFi.h>
#include "NuSock.h"

NuSockClientSecure client;

void setup() {
    WiFi.begin("SSID", "PASS");
    // wait

    // Set custom CA if needed, otherwise uses built-in bundle
    // client.setCACert(root_ca); 

    client.onEvent(onEvent);
    client.begin("echo.websocket.org", 443, "/");
    client.connect();
}

void loop() {
    client.loop();
}
```

### 5. Generic WebSocket Client (WS/WSS)
Compatible with `WiFiClient`, `WiFiClientSecure`, or `EthernetClient`. Works on all platforms (Uno R4, Portenta, Giga, MKR, ESP8266, Rev2, etc.).

```cpp
#include <WiFiClientSecure.h>
#include "NuSock.h"

WiFiClientSecure secureClient;
NuSockClient client;

void setup() {
    secureClient.setInsecure(); // Allow self-signed certs (if needed)
    
    client.begin(&secureClient, "example.com", 443, "/ws");
    client.connect();
}

void loop() {
    client.loop();
}
```

---

## 🛠 Advanced Features

### Sending Fragmented Data (Streaming)
You can send large files or data streams by breaking them into chunks.

**Server Example:**
```cpp
// 1. Start the stream (isBinary = true)
ws.sendFragmentStart(clientIndex, buffer, len, true);

// 2. Send middle chunks (Loop)
ws.sendFragmentCont(clientIndex, buffer, len);

// 3. Finish the stream
ws.sendFragmentFin(clientIndex, buffer, len);
```

* **See Example:** [`examples/Features/Fragmented_File_Send`](/examples/Features/Fragmented_File_Send)

**Client Example:**

* **See Example:** [`examples/Features/Fragmented_File_Receive`](/examples/Features/Fragmented_File_Receive)

### Graceful Disconnect (Close Handshake)
Initiate a clean disconnect compliant with RFC 6455 by sending a status code and reason.

```cpp
// Server: Close specific client with code 1001 (Going Away)
ws.close(clientIndex, 1001, "Server Shutting Down");

// Client: Close connection with code 1000 (Normal)
client.close(1000, "Job Done");
```

---

## 📄 License

**MIT License**

Copyright (c) 2025 Mobizt (Suwatchai K.)

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.