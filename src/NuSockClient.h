/**
 * SPDX-FileCopyrightText: 2025 Suwatchai K. <suwatchai@outlook.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef NUSOCK_CLIENT_H
#define NUSOCK_CLIENT_H

#include "NuSockConfig.h"
#include "NuSockUtils.h"
#include "NuSockTypes.h"
#include "vector/dynamic/DynamicVector.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

// Event Callback Typedef
typedef void (*NuClientEventCallback)(NuClient *client, NuClientEvent event, const uint8_t *payload, size_t len);

#ifndef NUSOCK_USE_LWIP

static bool headerEquals(const char *line, const char *header, const char *value)
{
    const char *colon = strchr(line, ':');
    if (!colon)
        return false;

    size_t headerLen = colon - line;
    if (strlen(header) != headerLen)
        return false;

    for (size_t i = 0; i < headerLen; i++)
    {
        if (tolower((unsigned char)line[i]) != tolower((unsigned char)header[i]))
            return false;
    }

    const char *valStart = colon + 1;
    while (*valStart == ' ')
        valStart++;

    const char *p = valStart;
    size_t needleLen = strlen(value);

    if (needleLen == 0)
        return true;

    while (*p)
    {
        if (tolower((unsigned char)*p) == tolower((unsigned char)value[0]))
        {
            bool match = true;
            for (size_t i = 1; i < needleLen; i++)
            {
                if (p[i] == 0 || tolower((unsigned char)p[i]) != tolower((unsigned char)value[i]))
                {
                    match = false;
                    break;
                }
            }
            if (match)
                return true;
        }
        p++;
    }
    return false;
}
#endif

class NuSockClient
{
private:
    NuLock myLock;
    char _host[128];
    uint16_t _port = 0;
    char _path[128];

    NuClientEventCallback _onEvent = nullptr;

#ifdef NUSOCK_USE_LWIP
    struct tcp_pcb *client_pcb;
    NuClient *_internalClient = nullptr;
#else
    void *_genericClientRef = nullptr;
    Client *(*_connectFunc)(void *, const char *, uint16_t) = nullptr;
    NuClient *_internalClient = nullptr;
#endif

    void generateRandomKey(char *outBuf)
    {
        uint8_t randomBytes[16];
        for (int i = 0; i < 16; i++)
            randomBytes[i] = random(0, 255);
        NuBase64::encode(randomBytes, 16, outBuf, 32);
    }

    void buildFrame(NuClient *c, uint8_t opcode, const uint8_t *data, size_t len)
    {
        uint8_t mask[4];
        for (int i = 0; i < 4; i++)
            mask[i] = random(0, 255);

        c->appendTx(0x80 | opcode);

        if (len <= 125)
        {
            c->appendTx((uint8_t)len | 0x80);
        }
        else
        {
            c->appendTx(126 | 0x80);
            c->appendTx(len >> 8);
            c->appendTx(len & 0xFF);
        }

        for (int i = 0; i < 4; i++)
            c->appendTx(mask[i]);
        for (size_t j = 0; j < len; j++)
            c->appendTx(data[j] ^ mask[j % 4]);
    }

#ifndef NUSOCK_USE_LWIP

    int readLine(Client *client, char *buffer, size_t maxLen, unsigned long timeout = 5000)
    {
        size_t pos = 0;
        unsigned long start = millis();

        while (millis() - start < timeout)
        {
            if (client->available())
            {
                int c = client->read();
                if (c == -1)
                    continue;

                char ch = (char)c;
                if (ch == '\n')
                {
                    buffer[pos] = 0;
                    if (pos > 0 && buffer[pos - 1] == '\r')
                    {
                        buffer[pos - 1] = 0;
                        return pos - 1;
                    }
                    return pos;
                }

                if (pos < maxLen - 1)
                {
                    buffer[pos++] = ch;
                }
            }
            else if (!client->connected())
            {
                break;
            }
            else
            {
                delay(1);
            }
        }
        buffer[pos] = 0;
        return (pos == 0) ? -1 : pos;
    }

    void generic_process()
    {
        if (!_internalClient || !_internalClient->client || !_internalClient->client->connected())
        {
            if (_internalClient && _internalClient->state == NuClient::STATE_CONNECTED)
            {
                // Connection lost unexpectedly
                if (_onEvent)
                    _onEvent(_internalClient, CLIENT_EVENT_DISCONNECTED, nullptr, 0);
                stop(); // Cleanup
            }
            return;
        }

        if (_internalClient->state == NuClient::STATE_HANDSHAKE)
        {
            if (_internalClient->client->available())
            {
                char lineBuf[256];

                int len = readLine(_internalClient->client, lineBuf, sizeof(lineBuf));
                if (len < 0)
                    return;

                int statusCode = 0;
                if (strncmp(lineBuf, "HTTP/", 5) == 0)
                {
                    char *space1 = strchr(lineBuf, ' ');
                    if (space1)
                    {
                        statusCode = atoi(space1 + 1);
                    }
                }

                if (statusCode != 101)
                {
                    if (_onEvent)
                    {
#if defined(NUSOCK_DEBUG)
                        Serial.println("[WS Debug] Error: Bad Status");
#endif
                        char errBuf[128];
                        snprintf(errBuf, sizeof(errBuf), "Bad Status: %s", lineBuf);
                        _onEvent(_internalClient, CLIENT_EVENT_ERROR, (const uint8_t *)errBuf, strlen(errBuf));
                    }
                    stop();
                    return;
                }

                bool isWebsocket = false;
                bool isUpgrade = false;

                while (true)
                {
                    len = readLine(_internalClient->client, lineBuf, sizeof(lineBuf));
                    if (len == 0)
                        break; // Empty line ends headers
                    if (len < 0)
                        break;

                    if (headerEquals(lineBuf, "Upgrade", "websocket"))
                        isUpgrade = true;
                    if (headerEquals(lineBuf, "Connection", "upgrade"))
                        isWebsocket = true;
                }

                if (isUpgrade)
                {
                    if (_onEvent)
                        _onEvent(_internalClient, CLIENT_EVENT_HANDSHAKE, nullptr, 0);
                    _internalClient->state = NuClient::STATE_CONNECTED;
                    if (_onEvent)
                        _onEvent(_internalClient, CLIENT_EVENT_CONNECTED, nullptr, 0);
                }
                else
                {
#if defined(NUSOCK_DEBUG)
                    Serial.println("[WS Debug] Error: Missing Headers");
#endif
                    if (_onEvent)
                        _onEvent(_internalClient, CLIENT_EVENT_ERROR, (const uint8_t *)"Missing Headers", 15);
                    stop();
                }
            }
        }
        else
        {
            while (_internalClient->client->available())
            {
                int byte = _internalClient->client->read();
                if (byte == -1)
                    break;
                if (_internalClient->rxLen < MAX_WS_BUFFER)
                {
                    _internalClient->rxBuffer[_internalClient->rxLen++] = (uint8_t)byte;
                }
            }

            while (_internalClient->rxLen >= 2)
            {
                uint8_t opcode = _internalClient->rxBuffer[0] & 0x0F;
                uint8_t lenByte = _internalClient->rxBuffer[1] & 0x7F;
                size_t headerSize = 2;
                size_t payloadLen = lenByte;

                if (payloadLen == 126)
                {
                    if (_internalClient->rxLen < 4)
                        return;
                    payloadLen = (_internalClient->rxBuffer[2] << 8) | _internalClient->rxBuffer[3];
                    headerSize += 2;
                }

                if (_internalClient->rxLen < headerSize + payloadLen)
                    return;

                uint8_t *payload = &_internalClient->rxBuffer[headerSize];

                if (opcode == 0x1)
                {
                    if (_onEvent)
                        _onEvent(_internalClient, CLIENT_EVENT_MESSAGE_TEXT, payload, payloadLen);
                }
                else if (opcode == 0x2)
                {
                    if (_onEvent)
                        _onEvent(_internalClient, CLIENT_EVENT_MESSAGE_BINARY, payload, payloadLen);
                }
                else if (opcode == 0x8)
                {
                    stop(); // Server sent close
                    return;
                }

                size_t total = headerSize + payloadLen;
                size_t rem = _internalClient->rxLen - total;
                if (rem > 0)
                    memmove(_internalClient->rxBuffer, &_internalClient->rxBuffer[total], rem);
                _internalClient->rxLen = rem;
            }
        }

        if (_internalClient->txLen > 0 && _internalClient->client && _internalClient->client->connected())
        {
            _internalClient->client->write(_internalClient->txBuffer, _internalClient->txLen);
            _internalClient->clearTx();
        }
    }
#endif

public:
public:
    /**
     * @brief Construct a new Nu Sock Client object.
     */
    NuSockClient() {}

    /**
     * @brief Destroy the Nu Sock Client object.
     * * Stops the connection and frees resources.
     */
    ~NuSockClient()
    {
        stop();
        // Free strings if allocated (though currently arrays)
    }

#ifndef NUSOCK_USE_LWIP
    /**
     * @brief Initialize the client parameters (Generic Mode).
     * * This prepares the client for connection but does not connect immediately.
     * Call connect() after this.
     * * @tparam ClientType The type of the underlying client (e.g., WiFiClient).
     * @param client Pointer to the underlying Arduino Client instance.
     * @param host The hostname or IP address of the WebSocket server.
     * @param port The port number of the WebSocket server.
     * @param path The URL path (endpoint) to connect to (default: "/").
     */
    template <typename ClientType>
    void begin(ClientType *client, const char *host, uint16_t port, const char *path = "/")
    {
        _genericClientRef = client;

        strncpy(_host, host, sizeof(_host) - 1);
        _host[sizeof(_host) - 1] = 0;
        _port = port;
        strncpy(_path, path, sizeof(_path) - 1);
        _path[sizeof(_path) - 1] = 0;

        _connectFunc = [](void *c, const char *h, uint16_t p) -> Client *
        {
            ClientType *cli = (ClientType *)c;
            if (cli->connect(h, p))
            {
                return cli;
            }
            return nullptr;
        };
    }

    /**
     * @brief Establish the WebSocket connection (Generic Mode).
     * * Connects via TCP, sends the HTTP Upgrade headers, and validates the handshake.
     * * @return true if the connection and handshake were successful.
     * @return false if the connection failed.
     */
    bool connect()
    {
        if (!_connectFunc || !_genericClientRef)
            return false;

        Client *c = _connectFunc(_genericClientRef, _host, _port);

        if (c && c->connected())
        {
            if (_internalClient)
                stop(); // Cleanup previous if any

            _internalClient = new NuClient((NuSockServer *)nullptr, c, false /* false for pointer to external client */);
            strncpy(_internalClient->id, "SERVER", sizeof(_internalClient->id));
            _internalClient->state = NuClient::STATE_HANDSHAKE;

            char keyBuf[32];
            generateRandomKey(keyBuf);

            c->print("GET ");
            c->print(_path);
            c->print(" HTTP/1.1\r\n");
            c->print("Host: ");
            c->print(_host);
            c->print("\r\n");
            c->print("Connection: Upgrade\r\n");
            c->print("Upgrade: websocket\r\n");
            c->print("Sec-WebSocket-Version: 13\r\n");
            c->print("Sec-WebSocket-Key: ");
            c->print(keyBuf);
            c->print("\r\n");
            // Add standard headers for compatibility
            c->print("User-Agent: NuSock\r\n");
            c->print("\r\n");

            return true;
        }
        return false;
    }
#endif

    /**
     * @brief Stop the client and disconnect.
     * * Gracefully closes the underlying TCP connection, fires the DISCONNECTED event,
     * and frees internal memory buffers.
     */
    void stop()
    {
        if (_internalClient)
        {
            // Fire disconnected event if we were previously connected
            if (_onEvent && _internalClient->state == NuClient::STATE_CONNECTED)
            {
                _onEvent(_internalClient, CLIENT_EVENT_DISCONNECTED, nullptr, 0);
            }

// Close the underlying client
#ifndef NUSOCK_USE_LWIP
            if (_internalClient->client)
            {
                _internalClient->client->stop();
            }
#endif

            // NuClient destructor handles rxBuffer/txBuffer cleanup
            if (_internalClient)
                delete _internalClient;
            _internalClient = nullptr;
        }
    }

    /**
     * @brief Main processing loop.
     * * MUST be called frequently in the main Arduino loop().
     * Handles incoming data processing, keep-alives, and event triggering.
     */
    void loop()
    {
#ifndef NUSOCK_USE_LWIP
        if (_internalClient)
        {
            generic_process();
        }
#endif
    }

    /**
     * @brief Register a callback function for client events.
     * * @param cb Function pointer matching the NuClientEventCallback signature.
     */
    void onEvent(NuClientEventCallback cb) { _onEvent = cb; }

    /**
     * @brief Send a text message to the server.
     * * @param msg Null-terminated string to send.
     */
    void send(const char *msg)
    {
        if (_internalClient && _internalClient->state == NuClient::STATE_CONNECTED)
        {
            buildFrame(_internalClient, 0x1, (const uint8_t *)msg, strlen(msg));
        }
    }

    /**
     * @brief Send a binary message to the server.
     * * @param data Pointer to the data buffer.
     * @param len Length of the data to send.
     */
    void send(const uint8_t *data, size_t len)
    {
        if (_internalClient && _internalClient->state == NuClient::STATE_CONNECTED)
        {
            buildFrame(_internalClient, 0x2, data, len);
        }
    }
};

#endif