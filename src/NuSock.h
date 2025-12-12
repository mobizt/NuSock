/**
 * SPDX-FileCopyrightText: 2025 Suwatchai K. <suwatchai@outlook.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef NUSOCK_H
#define NUSOCK_H
#include <Arduino.h>
#include <stdarg.h>

#define NUSOCK_VERSION_MAJOR 2
#define NUSOCK_VERSION_MINOR 0
#define NUSOCK_VERSION_PATCH 0
#define NUSOCK_VERSION_STR "2.0.0"

#ifndef NUSOCK_DEBUG_PORT
#define NUSOCK_DEBUG_PORT Serial
#endif

class NuSock
{
public:
    /**
     * @brief Helper to print IPAddress to NUSOCK_DEBUG_PORT (Useful for AVR)
     */
    static const char *ipStr(IPAddress ip)
    {
        static char buf[16];
        sprintf(buf, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
        return buf;
    }

    /**
     * @brief Helper to provide printf functionality to NUSOCK_DEBUG_PORT
     */
    static void printf(const char *format, ...)
    {
        char buf[256]; // Buffer for formatting
        va_list args;
        va_start(args, format);
        vsnprintf(buf, sizeof(buf), format, args);
        va_end(args);
        NUSOCK_DEBUG_PORT.print(buf);
    }

    static void printLog(const char *tag, const char *format, ...)
    {
        unsigned long now = millis();
        unsigned long hours = now / 3600000;
        unsigned long mins = (now % 3600000) / 60000;
        unsigned long secs = (now % 60000) / 1000;

        // Print Timestamp [HH:MM:SS]
        char timeBuf[20];
        sprintf(timeBuf, "[%02lu:%02lu:%02lu]", hours, mins, secs);
        NUSOCK_DEBUG_PORT.print(timeBuf);

        // Print Tag [TAG ]
        NUSOCK_DEBUG_PORT.print(" [");
        NUSOCK_DEBUG_PORT.print(tag);
        NUSOCK_DEBUG_PORT.print("] ");

        // Print Message
        char msg[128];
        va_list args;
        va_start(args, format);
        vsnprintf(msg, sizeof(msg), format, args);
        va_end(args);

        NUSOCK_DEBUG_PORT.print(msg);
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