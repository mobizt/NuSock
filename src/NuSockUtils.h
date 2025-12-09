/**
 * SPDX-FileCopyrightText: 2025 Suwatchai K. <suwatchai@outlook.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef NUSOCK_UTILS_H
#define NUSOCK_UTILS_H

#include "NuSockConfig.h"

class NuLock
{
private:
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
    SemaphoreHandle_t _mutex;
#endif

public:
    NuLock()
    {
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
        _mutex = xSemaphoreCreateRecursiveMutex();
#endif
    }
    void lock()
    {
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
        xSemaphoreTakeRecursive(_mutex, portMAX_DELAY);
#else
        // Do not disable interrupts on AVR/SAMD/Renesas.
        // It blocks UART communication with WiFi modules (NINA/S3).
#endif
    }
    void unlock()
    {
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
        xSemaphoreGiveRecursive(_mutex);
#else
        // Interrupts remain enabled.
#endif
    }
};

class NuBase64
{
public:
    static size_t encode(const uint8_t *data, size_t length, char *outBuffer, size_t outMaxLen)
    {
        const char *table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        size_t outLen = 0;
        int val = 0, valb = -6;

        for (size_t i = 0; i < length; i++)
        {
            val = (val << 8) + data[i];
            valb += 8;
            while (valb >= 0)
            {
                if (outLen < outMaxLen - 1)
                    outBuffer[outLen++] = table[(val >> valb) & 0x3F];
                valb -= 6;
            }
        }
        if (valb > -6)
        {
            if (outLen < outMaxLen - 1)
                outBuffer[outLen++] = table[((val << 8) >> (valb + 8)) & 0x3F];
        }
        while (outLen % 4)
        {
            if (outLen < outMaxLen - 1)
                outBuffer[outLen++] = '=';
        }
        outBuffer[outLen] = 0;
        return outLen;
    }
};

class NuSHA1
{
private:
    uint32_t state[5];
    uint32_t count[2];
    uint8_t buffer[64];

    void transform(const uint8_t data[64])
    {
        uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];
        uint32_t w[80];

        for (int i = 0; i < 16; i++)
        {
            w[i] = ((uint32_t)data[i * 4] << 24) |
                   ((uint32_t)data[i * 4 + 1] << 16) |
                   ((uint32_t)data[i * 4 + 2] << 8) |
                   ((uint32_t)data[i * 4 + 3]);
        }
        for (int i = 16; i < 80; i++)
        {
            w[i] = (w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16]);
            w[i] = (w[i] << 1) | (w[i] >> 31);
        }

        for (int i = 0; i < 80; i++)
        {
            uint32_t f, k;
            if (i < 20)
            {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            }
            else if (i < 40)
            {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            }
            else if (i < 60)
            {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            }
            else
            {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }
            uint32_t temp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
            e = d;
            d = c;
            c = (b << 30) | (b >> 2);
            b = a;
            a = temp;
        }
        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
    }

public:
    void init()
    {
        state[0] = 0x67452301;
        state[1] = 0xEFCDAB89;
        state[2] = 0x98BADCFE;
        state[3] = 0x10325476;
        state[4] = 0xC3D2E1F0;
        count[0] = count[1] = 0;
    }
    void update(const uint8_t *data, size_t len)
    {
        uint32_t i, j;
        j = (count[0] >> 3) & 63;
        if ((count[0] += (uint32_t)len << 3) < ((uint32_t)len << 3))
            count[1]++;
        count[1] += ((uint32_t)len >> 29);
        if ((j + len) > 63)
        {
            memcpy(&buffer[j], data, (i = 64 - j));
            transform(buffer);
            for (; i + 63 < len; i += 64)
                transform(&data[i]);
            j = 0;
        }
        else
            i = 0;
        memcpy(&buffer[j], &data[i], len - i);
    }
    void final(uint8_t digest[20])
    {
        uint32_t i;
        uint8_t finalcount[8];
        for (i = 0; i < 8; i++)
            finalcount[i] = (unsigned char)((count[(i >= 4 ? 0 : 1)] >> ((3 - (i & 3)) * 8)) & 255);
        update((uint8_t *)"\200", 1);
        while ((count[0] & 504) != 448)
            update((uint8_t *)"\0", 1);
        update(finalcount, 8);
        for (i = 0; i < 20; i++)
            digest[i] = (unsigned char)((state[i >> 2] >> ((3 - (i & 3)) * 8)) & 255);
    }
};

class NuCrypto
{
public:
    static void getAcceptKey(const char *clientKey, char *outBuffer, size_t outMaxLen)
    {
        char concat[128];
        snprintf(concat, sizeof(concat), "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", clientKey);

#if defined(ESP32)
        uint8_t hash[20];
        mbedtls_sha1((const unsigned char *)concat, strlen(concat), hash);
        size_t dlen = 0;
        mbedtls_base64_encode((unsigned char *)outBuffer, outMaxLen, &dlen, hash, 20);
        outBuffer[dlen] = 0;
#else
        uint8_t hash[20];
        NuSHA1 sha;
        sha.init();
        sha.update((const uint8_t *)concat, strlen(concat));
        sha.final(hash);
        NuBase64::encode(hash, 20, outBuffer, outMaxLen);
#endif
    }
};

#endif