# NuSock

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-Arduino%20%7C%20ESP32%20%7C%20ESP8266%20%7C%20RP2040-orange.svg)
![Version](https://img.shields.io/badge/version-1.0.0-green.svg)

**NuSock** is a lightweight, high-performance WebSocket library designed for embedded systems. It bridges the gap between ease of use and raw performance by offering a **Dual-Mode Server Architecture** and **Secure WebSocket (WSS)** support.

---

## 📑 Table of Contents
- [Features](#-features)
- [Supported Platforms](#-supported-platforms)
- [Installation](#-installation)
- [Configuration Macros](#-configuration-macros)
- [Usage Guide](#-usage-guide)
    - [Secure Server (ESP32)](#1-secure-websocket-server-wss-esp32)
    - [Secure Server (ESP8266)](#2-secure-websocket-server-wss-esp8266)
    - [Standard Server (WS)](#3-standard-websocket-server-ws)
    - [WebSocket Client](#4-websocket-client)
- [API Reference](#-api-reference)
- [License](#-license)

---

## 🚀 Features

* **🔒 Secure WebSockets (WSS):**
    * **ESP32:** Native, high-performance WSS using the internal `esp_tls` stack.
    * **ESP8266 and RPi Pico W:** WSS support via standard `WiFiServerSecure` wrapping.
* **⚡ Dual-Mode Architecture:**
    * **Generic Mode:** Maximum compatibility using standard `WiFiServer` polling.
    * **LwIP Mode:** High-performance, low-overhead mode using native LwIP callbacks (ESP32/ESP8266).
* **📨 Event-Driven:** Non-blocking, callback-based architecture for handling Connect, Disconnect, Text, and Binary events.
* **🔌 Universal Client:** Compatible with any Arduino-supported network interface (`WiFiClient`, `WiFiClientSecure`, `EthernetClient`).

---

## 📦 Supported Platforms

| Platform | Standard Server (WS) | Secure Server (WSS) | Client (WS/WSS) | LwIP Mode |
| :--- | :---: | :---: | :---: | :---: |
| **ESP32** | ✅ | ✅ | ✅ | ✅ |
| **ESP8266** | ✅ | ✅ | ✅ | ✅ |
| **RP2040 (Pico W)** | ✅ | ✅ | ✅ | ❌ |
| **STM32 / SAMD** | ✅ | ❌ | ✅ | ❌ |

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

| Macro | Description | Target |
| :--- | :--- | :--- |
| `NUSOCK_SERVER_USE_LWIP` | Enables LwIP async mode. Reduces RAM/CPU overhead. | ESP32, ESP8266 |
| `NUSOCK_USE_SERVER_SECURE` | Enables `NuSockServerSecure` class (Native SSL). | ESP32 |

---

## 📖 Usage Guide

### 1. Secure WebSocket Server (WSS) [ESP32]
ESP32 uses the native `NuSockServerSecure` class for optimal performance.

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

### 2. Secure WebSocket Server (WSS) [ESP8266]
ESP8266 uses the Generic Mode to wrap the standard `WiFiServerSecure`.

```cpp
#include <ESP8266WiFi.h>
#include "NuSock.h"

// Certificates
const char* server_cert = "-----BEGIN CERTIFICATE-----\n...";
const char* server_key = "-----BEGIN PRIVATE KEY-----\n...";

NuSockServer wss;
WiFiServerSecure server(443);

// BearSSL Containers
BearSSL::X509List cert(server_cert);
BearSSL::PrivateKey key(server_key);

void setup() {
    WiFi.begin("SSID", "PASS");
    // ... wait for connection ...

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
Ideal for local networks (ESP32/ESP8266/Pico W).

```cpp
#define NUSOCK_SERVER_USE_LWIP // Optional: Enable for better performance (ESP only)
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

### 4. WebSocket Client
Connect to WS or WSS servers using standard Arduino clients.

```cpp
#include <WiFiClientSecure.h>
#include "NuSock.h"

WiFiClientSecure secureClient;
NuSockClient client;

void setup() {
    secureClient.setInsecure(); // Allow self-signed certs
    
    // Connect to: wss://[example.com:443/ws](https://example.com:443/ws)
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

Copyright (c) 2025 Suwatchai K. <suwatchai@outlook.com>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software."# NuSock" 
