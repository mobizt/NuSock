/**
 * SPDX-FileCopyrightText: 2025 Suwatchai K. <suwatchai@outlook.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef NUSOCK_H
#define NUSOCK_H

#include <stdarg.h>

class NuSock
{
public:
    /**
     * @brief Helper to print IPAddress to Serial (Useful for AVR)
     */
    static void printIP(IPAddress ip)
    {
        Serial.print(ip[0]);
        Serial.print(".");
        Serial.print(ip[1]);
        Serial.print(".");
        Serial.print(ip[2]);
        Serial.print(".");
        Serial.println(ip[3]);
    }

    /**
     * @brief Helper to provide printf functionality to Serial
     */
    static void printf(const char *format, ...)
    {
        char buf[256]; // Buffer for formatting
        va_list args;
        va_start(args, format);
        vsnprintf(buf, sizeof(buf), format, args);
        va_end(args);
        Serial.print(buf);
    }
};

#include "NuSockServer.h"
#include "NuSockClient.h"

// SSL/TLS Support for ESP32
#if defined(ESP32)
#include "NuSockServerSecure.h"
#include "NuSockClientSecure.h"
#endif

#endif