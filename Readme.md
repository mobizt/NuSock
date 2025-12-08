# NuSock

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-Arduino%20%7C%20ESP32%20%7C%20ESP8266%20%7C%20RP2040-orange.svg)
![Version](https://img.shields.io/badge/version-1.0.4-green.svg)

**NuSock** is a lightweight, high-performance WebSocket library designed for embedded systems. It bridges the gap between ease of use and raw performance by offering a **Dual-Mode Architecture** (Generic vs LwIP) and **Secure WebSocket (WSS)** support.

---

## 📑 Table of Contents
- [Features](#-features)
- [Supported Platforms](#-supported-platforms)
- [Installation](#-installation)
- [Configuration Macros](#-configuration-macros)
- [Usage Guide](#-usage-guide)
    - [1. Secure Server (ESP32 Native)](#1-secure-websocket-server-wss-esp32-native)
    - [2. Secure Server (ESP8266 Wrapper)](#2-secure-websocket-server-wss-esp8266-wrapper)
    - [3. Standard Server (WS)](#3-standard-websocket-server-ws)
    - [4. Secure Client (ESP32 Native)](#4-secure-websocket-client-wss-esp32-native)
    - [5. Generic Client (WS/WSS)](#5-generic-websocket-client-wswss)
- [License](#-license)

---

## 🚀 Features

* **🔒 Secure WebSockets (WSS):**
    * **ESP32:** Native, high-performance WSS Server and Client using the internal `esp_tls` stack.
    * **ESP8266 and RPi Pico W:** WSS support via standard `WiFiServerSecure` / `WiFiClientSecure` wrapping.
* **⚡ Dual-Mode Architecture:**
    * **Generic Mode:** Maximum compatibility using standard `WiFiServer` / `WiFiClient` polling.
    * **LwIP Mode:** High-performance, low-overhead mode using native LwIP callbacks (ESP32/ESP8266).
* **📨 Event-Driven:** Non-blocking, callback-based architecture for handling Connect, Disconnect, Text, and Binary events.
* **🔌 Universal Client:** Compatible with any Arduino-supported network interface (`WiFiClient`, `WiFiClientSecure`, `EthernetClient`).

---

## 📦 Supported Platforms

| Platform | Standard Server (WS) | Secure Server (WSS) | Secure Client (WSS) | LwIP Mode (Async) |
| :--- | :---: | :---: | :---: | :---: |
| **ESP32** | ✅ | ✅ | ✅ | ✅ |
| **ESP8266** | ✅ | ✅ | ✅ | ✅ |
| **RP2040 (Pico W)** | ✅ | ✅ | ✅ | ❌ |
| **STM32 / Teensy / Renesas / AVR** | ✅ | ❌ | ✅ | ❌ |

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

### 2. Secure WebSocket Server (WSS) [ESP8266 Wrapper]
ESP8266 uses the Generic Mode to wrap the standard `WiFiServerSecure`.

```cpp
#include <ESP8266WiFi.h>
#include "NuSock.h"

NuSockServer wss;
WiFiServerSecure server(443);

// BearSSL Containers
BearSSL::X509List cert(server_cert);
BearSSL::PrivateKey key(server_key);

void setup() {
    WiFi.begin("SSID", "PASS");
    
    // Configure BearSSL
    server.setRSACert(&cert, &key);
    server.begin();

    // Bind NuSock to the secure server
    wss.onEvent(onEvent);
    wss.begin(&server, 443);
}

void loop() {
    wss.loop();
}
```

### 3. Standard WebSocket Server (WS)
Ideal for local networks (ESP32/ESP8266/Pico W). Use `NUSOCK_SERVER_USE_LWIP` for best performance on ESPs.

```cpp
#define NUSOCK_SERVER_USE_LWIP // Optional: Enable LwIP mode (ESP only)
#include <WiFi.h>
#include "NuSock.h"

NuSockServer ws;

void setup() {
    ws.begin(80); 
    ws.onEvent([](NuClient* c, NuServerEvent e, const uint8_t* p, size_t l){
        if(e == SERVER_EVENT_CLIENT_CONNECTED) {
            Serial.println("New Client!");
        }
    });
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

### 5. Generic WebSocket Client (WS/WSS)
Compatible with `WiFiClient`, `WiFiClientSecure`, or `EthernetClient`. Works on all platforms (ESP8266, RP2040, STM32, etc.).

```cpp
#include <WiFiClientSecure.h>
#include "NuSock.h"

WiFiClientSecure secureClient;
NuSockClient client;

void setup() {
    secureClient.setInsecure(); // Allow self-signed certs
    
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