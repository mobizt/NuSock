/**
 * SPDX-FileCopyrightText: 2025 Suwatchai K. <suwatchai@outlook.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef NUSOCK_H
#define NUSOCK_H

#include "NuSockServer.h"
#include "NuSockClient.h"

// SSL/TLS Support for ESP32
#if defined(ESP32)
#include "NuSockServerSecure.h"
#endif

#endif