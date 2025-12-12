/**
 * SPDX-FileCopyrightText: 2025 Suwatchai K. <suwatchai@outlook.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef NUSOCK_CONFIG_H
#define NUSOCK_CONFIG_H

#include <Arduino.h>

// We will force RP2040 to use Generic Mode (WiFiServer) to avoid 'tcpip_callback' errors.
#if (defined(NUSOCK_SERVER_USE_LWIP) || defined(NUSOCK_CLIENT_USE_LWIP)) && (defined(ESP32) || defined(ESP8266))
#define NUSOCK_USE_LWIP
#endif

#ifdef NUSOCK_USE_LWIP
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <lwip/tcp.h>
extern "C"
{
#include "user_interface.h"
}

#include <schedule.h>

// ESP8266 Polyfill: LwIP runs in NO_SYS mode.
static inline err_t tcpip_callback(void (*f)(void *), void *ctx)
{
    if (schedule_function([f, ctx]()
                          { f(ctx); }))
    {
        return ERR_OK;
    }
    return ERR_MEM;
}

#else // ESP32
#include <WiFi.h>
#include "lwip/tcp.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/tcpip.h"
#include "mbedtls/sha1.h"
#include "mbedtls/base64.h"
#endif
#else
// Generic (SAMD, STM32, RP2040, etc)
#include <Client.h>
#include <Server.h>

#if defined(ESP32)
#include "mbedtls/sha1.h"
#include "mbedtls/base64.h"
#endif

#endif

#define MAX_WS_BUFFER 1024

#endif