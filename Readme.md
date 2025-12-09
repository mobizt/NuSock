# NuSock

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-ESP32%20%7C%20ESP8266%20%7C%20RP2040%20%7C%20SAMD%20%7C%20Renesas-orange.svg)
![Version](https://img.shields.io/badge/version-1.0.5-green.svg)

**NuSock** is a lightweight, high-performance WebSocket library designed for embedded systems. It bridges the gap between ease of use and raw performance by offering a **Dual-Mode Architecture** (Generic vs LwIP) and **Secure WebSocket (WSS)** support.

It features a **Zero-Interrupt Architecture** for Generic mode, ensuring stability on UART-based WiFi modules like the **Arduino UNO R4 WiFi** and **Nano 33 IoT**.

---

## 📑 Table of Contents
- [Features](#-features)
- [Supported Platforms](#-supported-platforms)
- [Installation](#-installation)
- [Configuration Macros](#-configuration-macros)
- [Usage Guide](#-usage-guide)
    - [1. Secure Server (ESP32 Native)](#1-secure-websocket-server-wss-esp32-native)
    - [2. Secure Server (ESP8266 / Pico W - Wrapper)](#2-secure-websocket-server-wss-esp8266--pico-w---wrapper)
    - [3. Standard Server (ESP32/ESP8266 - LwIP Mode)](#3-standard-server-esp32esp8266---lwip-mode)
    - [4. Standard Server (Uno R4 / MKR / Nano - Generic Mode)](#4-standard-server-uno-r4--mkr--nano---generic-mode)
    - [5. Secure Client (ESP32 Native)](#5-secure-websocket-client-wss-esp32-native)
    - [6. Generic Client (WS/WSS)](#6-generic-websocket-client-wswss)
- [License](#-license)

---

## 🚀 Features

* **🔒 Secure WebSockets (WSS):**
    * **ESP32:** Native, high-performance WSS Server and Client using the internal `esp_tls` stack.
    * **ESP8266 / Pico W:** WSS Server/Client support via `WiFiServerSecure` / `WiFiClientSecure` wrapping.
* **⚡ Dual-Mode Architecture:**
    * **Generic Mode:** Maximum compatibility using standard `WiFiServer` / `WiFiClient` wrapping. Supports **accept()** logic for robust connection handling on newer boards.
    * **LwIP Mode:** High-performance, low-overhead mode using native LwIP callbacks (ESP32/ESP8266).
* **🛡️ Robust Stability:**
    * **Zero-Interrupt Locking:** Prevents UART deadlocks on Arduino Uno R4 WiFi and Nano 33 IoT.
    * **Smart Duplicate Detection:** Automatically handles and cleans up duplicate socket handles returned by underlying WiFi libraries.
* **📨 Event-Driven:** Non-blocking, callback-based architecture for handling Connect, Disconnect, Text, and Binary events.

---

## 📦 Supported Platforms

| Platform | WS Server | WSS Server | WS Client | WSS Client | Mode |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **ESP32** | ✅ | ✅ | ✅ | ✅ | LwIP / Generic |
| **ESP8266** | ✅ | ✅ | ✅ | ✅ | LwIP / Generic |
| **RP2040 (Pico W)** | ✅ | ✅ | ✅ | ✅ | Generic |
| **Arduino UNO R4 WiFi** | ✅ | ❌ | ✅ | ✅ | Generic |
| **SAMD (MKR / Nano 33)**| ✅ | ❌ | ✅ | ✅ | Generic |

### 📝 Platform Notes
* **ESP8266 / Pico W:** 
  * WSS Server requires passing a `WiFiServerSecure` instance to `begin()`.
* **Arduino R4 / SAMD:** 
  * WS Server requires passing a `WiFiServer` instance.
  * WSS Client requires Root CA certificates to be uploaded via the Arduino IDE (Tools > WiFi101 / WiFiNINA Firmware Updater).

---

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

Define these macros **before** including `NuSock.h` to enable high-performance modes.

| Macro | Description | Target |
| :--- | :--- | :--- |
| `NUSOCK_SERVER_USE_LWIP` | Enables LwIP async mode for **Server**. Reduces RAM/CPU overhead. | ESP32, ESP8266 |
| `NUSOCK_CLIENT_USE_LWIP` | Enables LwIP async mode for **Client** (Plain WS). | ESP32, ESP8266 |
| `NUSOCK_USE_SERVER_SECURE` | Enables `NuSockServerSecure` class (Native SSL). | ESP32 |

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
    // ... wait for connection ...
    
    wss.onEvent(onEvent); // Define callback
    wss.begin(443, server_cert, server_key);
}

void loop() {
    wss.loop();
}
```

### 2. Secure WebSocket Server (WSS) [ESP8266 / Pico W - Wrapper]
ESP8266 and RPi Pico W use the Generic Mode to wrap their native `WiFiServerSecure`.

```cpp
#include <WiFi.h> // or ESP8266WiFi.h
#include "NuSock.h"

NuSockServer wss;
WiFiServerSecure server(443);

// Certificates (BearSSL for ESP8266 / Certs for Pico W)
const char* cert = "...";
const char* key = "...";

void setup() {
    WiFi.begin("SSID", "PASS");
    
    // Configure SSL Server
    server.setServerCert(cert); // API varies by platform/core
    server.setServerKey(key);
    server.begin();

    // Bind NuSock to the secure server
    wss.onEvent(onEvent);
    wss.begin(&server, 443);
}

void loop() {
    wss.loop();
}
```

### 3. Standard Server (ESP32/ESP8266 - LwIP Mode)
Ideal for high-performance non-SSL applications.

```cpp
#define NUSOCK_SERVER_USE_LWIP 
#include <WiFi.h> // or ESP8266WiFi.h
#include "NuSock.h"

NuSockServer ws;

void setup() {
    WiFi.begin("SSID", "PASS");
    // ... wait ...
    
    ws.onEvent(onEvent);
    ws.begin(80); // Internal LwIP server created automatically
}

void loop() {
    ws.loop();
}
```

### 4. Standard Server (Uno R4 / MKR / Nano - Generic Mode)
Compatible with **Arduino Uno R4 WiFi**, **Nano 33 IoT**, **MKR 1010**, etc. NuSock wraps the existing `WiFiServer` object.

```cpp
#include <WiFiS3.h> // Or WiFiNINA.h
#include "NuSock.h"

// 1. Create the standard WiFiServer
WiFiServer server(80);
NuSockServer ws;

void setup() {
    WiFi.begin("SSID", "PASS");
    // ... wait ...

    // 2. Start the underlying server
    server.begin();

    // 3. Bind NuSock to it
    ws.onEvent(onEvent);
    ws.begin(&server, 80);
}

void loop() {
    ws.loop();
}
```

### 5. Secure WebSocket Client (WSS) [ESP32 Native]
ESP32 can use `NuSockClientSecure` which uses the native `esp_tls` stack for high performance.

```cpp
#include <WiFi.h>
#include "NuSock.h"

NuSockClientSecure client;

void setup() {
    WiFi.begin("SSID", "PASS");
    // ... wait ...

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

### 6. Generic WebSocket Client (WS/WSS)
Compatible with `WiFiClient`, `WiFiClientSecure`, or `EthernetClient`. Works on all platforms (Uno R4, MKR, ESP8266, etc.).

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

## 📄 License

**MIT License**

Copyright (c) 2025 Mobizt (Suwatchai K.)

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.