/**
 * SPDX-FileCopyrightText: 2025 Suwatchai K. <suwatchai@outlook.com>
 *
 * SPDX-License-Identifier: MIT
 * NuSockServerSecure - WebSocket Server with SSL/TLS using ESP-IDF esp_tls
 * This implementation uses ESP-IDF's native esp_tls API for proper SSL support.
 * It doesn't rely on WiFiClientSecure which doesn't support server mode properly.
 */

#ifndef NUSOCK_SERVER_SECURE_H
#define NUSOCK_SERVER_SECURE_H

#if defined(ESP32)

#include "NuSockConfig.h"
#include "NuSockUtils.h"
#include "NuSockTypes.h"
#include "vector/dynamic/DynamicVector.h"
#include <WiFi.h>
#include "esp_tls.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

// SSL Client structure
struct NuSSLClient
{
    int sock;
    esp_tls_t *tls;
    NuClient *nuClient;
    uint8_t tmpBuf[1024];
};

typedef void (*NuServerSecureEventCallback)(NuClient *client, NuServerEvent event, const uint8_t *payload, size_t len);

class NuSockServerSecure
{
private:
    NuLock myLock;
    ReadyUtils::DynamicVector<NuSSLClient *> sslClients;
    ReadyUtils::DynamicVector<NuClient *> clients;
    uint16_t _port;
    NuServerSecureEventCallback _onEvent = nullptr;
    bool _running = false;

    // Server socket
    int _serverSock = -1;

    // SSL configuration
    esp_tls_cfg_server_t _tlsCfg;
    const char *_cert = nullptr;
    const char *_key = nullptr;

    void removeClient(NuClient *c, NuSSLClient *sc)
    {
        // Remove from clients vector
        for (size_t i = 0; i < clients.size(); i++)
        {
            if (clients[i] == c)
            {
                clients.erase(i);
                for (size_t j = i; j < clients.size(); j++)
                {
                    clients[j]->index = j;
                }
                break;
            }
        }

        // Remove from SSL clients vector
        for (size_t i = 0; i < sslClients.size(); i++)
        {
            if (sslClients[i] == sc)
            {
                sslClients.erase(i);
                break;
            }
        }

        // Cleanup
        if (c->rxBuffer)
        {
            free(c->rxBuffer);
            c->rxBuffer = nullptr;
        }

        if (sc)
        {
            if (sc->tls)
            {
                esp_tls_server_session_delete(sc->tls);
                sc->tls = nullptr;
            }
            if (sc->sock >= 0)
            {
                close(sc->sock);
                sc->sock = -1;
            }
            delete sc;
        }

        if (c)
            delete c;
    }

    void buildFrame(NuClient *c, uint8_t opcode, const uint8_t *data, size_t len)
    {
        c->appendTx(0x80 | opcode);
        if (len <= 125)
        {
            c->appendTx((uint8_t)len);
        }
        else
        {
            c->appendTx(126);
            c->appendTx(len >> 8);
            c->appendTx(len & 0xFF);
        }
        for (size_t j = 0; j < len; j++)
            c->appendTx(data[j]);
    }

    void processClient(NuClient *c, NuSSLClient *sc)
    {
        if (!c->rxBuffer || !sc->tls)
            return;

        // Read data from SSL connection
        int ret = esp_tls_conn_read(sc->tls, sc->tmpBuf, sizeof(sc->tmpBuf));
        if (ret > 0)
        {
#if defined(NUSOCK_DEBUG)
            NuSock::printLog("DBG ", "Read %d bytes from SSL connection\n");
#endif

            // Copy to RX buffer
            for (int i = 0; i < ret && c->rxLen < MAX_WS_BUFFER; i++)
            {
                c->rxBuffer[c->rxLen++] = sc->tmpBuf[i];
            }
        }
        else if (ret == 0 || ret == ESP_TLS_ERR_SSL_WANT_READ || ret == ESP_TLS_ERR_SSL_WANT_WRITE)
        {
            // No data or non-blocking read
        }
        else
        {
            // Error - connection closed
            if (_onEvent && c->last_event != SERVER_EVENT_CLIENT_DISCONNECTED)
                _onEvent(c, SERVER_EVENT_CLIENT_DISCONNECTED, nullptr, 0);
            c->last_event = SERVER_EVENT_CLIENT_DISCONNECTED;
            removeClient(c, sc); // Ensure removal on error
            return;
        }

        // Process WebSocket handshake
        if (c->state == NuClient::STATE_HANDSHAKE)
        {
            if (c->rxLen > 100)
            {
                if (c->rxLen < MAX_WS_BUFFER)
                    c->rxBuffer[c->rxLen] = 0;
                char *reqBuf = (char *)c->rxBuffer;
                if (strstr(reqBuf, "\r\n\r\n"))
                {
                    char *upgradePtr = strstr(reqBuf, "Upgrade: websocket");
                    if (upgradePtr)
                    {
                        if (_onEvent)
                            _onEvent(c, SERVER_EVENT_CLIENT_HANDSHAKE, nullptr, 0);
                        c->last_event = SERVER_EVENT_CLIENT_HANDSHAKE;

                        char *keyStart = strstr(reqBuf, "Sec-WebSocket-Key: ");
                        if (keyStart)
                        {
                            keyStart += 19;
                            char *keyEnd = strstr(keyStart, "\r\n");
                            if (keyEnd)
                            {
                                char clientKey[64];
                                size_t keyLen = keyEnd - keyStart;
                                if (keyLen > 63)
                                    keyLen = 63;
                                strncpy(clientKey, keyStart, keyLen);
                                clientKey[keyLen] = 0;

                                char acceptKey[64];
                                NuCrypto::getAcceptKey(clientKey, acceptKey, sizeof(acceptKey));

                                char response[256];
                                int respLen = snprintf(response, sizeof(response),
                                                       "HTTP/1.1 101 Switching Protocols\r\n"
                                                       "Upgrade: websocket\r\n"
                                                       "Connection: Upgrade\r\n"
                                                       "Sec-WebSocket-Accept: %s\r\n\r\n",
                                                       acceptKey);

                                esp_tls_conn_write(sc->tls, response, respLen);

                                c->state = NuClient::STATE_CONNECTED;
                                c->rxLen = 0;

                                if (_onEvent)
                                    _onEvent(c, SERVER_EVENT_CLIENT_CONNECTED, nullptr, 0);
                                c->last_event = SERVER_EVENT_CLIENT_CONNECTED;
                            }
                        }
                    }
                    else
                    {
                        if (_onEvent)
                            _onEvent(c, SERVER_EVENT_ERROR, (const uint8_t *)"Invalid Handshake", 17);
                        c->last_event = SERVER_EVENT_ERROR;
                        removeClient(c, sc);
                        return;
                    }
                }
            }
        }
        else
        {
            // Process WebSocket frames
            while (c->rxLen > 0)
            {
                if (c->rxLen < 2)
                    return;
                uint8_t opcode = c->rxBuffer[0] & 0x0F;
                uint8_t lenByte = c->rxBuffer[1] & 0x7F;
                bool isMasked = (c->rxBuffer[1] & 0x80);
                size_t headerSize = 2;
                size_t payloadLen = lenByte;

                if (payloadLen == 126)
                {
                    if (c->rxLen < 4)
                        return;
                    payloadLen = (c->rxBuffer[2] << 8) | c->rxBuffer[3];
                    headerSize += 2;
                }
                if (isMasked)
                    headerSize += 4;
                if (c->rxLen < headerSize + payloadLen)
                    return;

                size_t maskOffset = headerSize - 4;

                if (opcode == 0x8)
                {
                    if (_onEvent && c->last_event != SERVER_EVENT_CLIENT_DISCONNECTED)
                        _onEvent(c, SERVER_EVENT_CLIENT_DISCONNECTED, nullptr, 0);
                    c->last_event = SERVER_EVENT_CLIENT_DISCONNECTED;
                    removeClient(c, sc);
                    return;
                }

                if ((opcode == 0x1 || opcode == 0x2) && isMasked)
                {
                    uint8_t mask[4] = {c->rxBuffer[maskOffset], c->rxBuffer[maskOffset + 1],
                                       c->rxBuffer[maskOffset + 2], c->rxBuffer[maskOffset + 3]};
                    uint8_t *payload = &c->rxBuffer[headerSize];
                    for (size_t i = 0; i < payloadLen; i++)
                        payload[i] ^= mask[i % 4];

                    if (opcode == 0x1)
                    {
                        if (c->id[0] == 0 && payloadLen < sizeof(c->id))
                        {
                            strncpy(c->id, (char *)payload, payloadLen);
                            c->id[payloadLen] = 0;
                        }

                        if (_onEvent)
                            _onEvent(c, SERVER_EVENT_MESSAGE_TEXT, payload, payloadLen);
                        c->last_event = SERVER_EVENT_MESSAGE_TEXT;
                    }
                    else if (opcode == 0x2)
                    {
                        if (_onEvent)
                            _onEvent(c, SERVER_EVENT_MESSAGE_BINARY, payload, payloadLen);
                        c->last_event = SERVER_EVENT_MESSAGE_BINARY;
                    }
                }

                size_t total = headerSize + payloadLen;
                size_t rem = c->rxLen - total;
                if (rem > 0)
                    memmove(c->rxBuffer, &c->rxBuffer[total], rem);
                c->rxLen = rem;
            }
        }

        // Send pending data
        if (c->txBuffer && c->txLen > 0)
        {
            int sent = esp_tls_conn_write(sc->tls, c->txBuffer, c->txLen);
            if (sent > 0)
            {
                if (sent == c->txLen)
                {
                    c->clearTx();
                }
                else
                {
                    // Partial send
                    memmove(c->txBuffer, c->txBuffer + sent, c->txLen - sent);
                    c->txLen -= sent;
                }
            }
        }
    }

public:
    /**
     * @brief Construct a new Nu Sock Server Secure object.
     */
    NuSockServerSecure() : _serverSock(-1)
    {
        memset(&_tlsCfg, 0, sizeof(_tlsCfg));
    }

    /**
     * @brief Destroy the Nu Sock Server Secure object.
     * Stops the server and frees resources.
     */
    ~NuSockServerSecure()
    {
        stop();
    }

    /**
     * @brief Stop the Secure WebSocket Server.
     * Disconnects all clients, releases SSL contexts, closes the listening socket,
     * and fires the DISCONNECTED event.
     */
    void stop()
    {
        if (!_running)
            return;

        myLock.lock();

        // Close all clients
        for (size_t i = 0; i < sslClients.size(); i++)
        {
            NuSSLClient *sc = sslClients[i];
            if (sc->tls)
            {
                esp_tls_server_session_delete(sc->tls);
            }
            if (sc->sock >= 0)
            {
                close(sc->sock);
            }
            delete sc;
        }
        sslClients.clear();

        for (size_t i = 0; i < clients.size(); i++)
        {
            NuClient *c = clients[i];
            if (c->rxBuffer)
                free(c->rxBuffer);
            delete c;
        }
        clients.clear();

        // Close server socket
        if (_serverSock >= 0)
        {
            close(_serverSock);
            _serverSock = -1;
        }

        // Free TLS config
        if (_tlsCfg.servercert_buf)
        {
            _tlsCfg.servercert_buf = nullptr;
        }
        if (_tlsCfg.serverkey_buf)
        {
            _tlsCfg.serverkey_buf = nullptr;
        }

        _running = false;
        if (_onEvent)
            _onEvent(nullptr, SERVER_EVENT_DISCONNECTED, nullptr, 0);

        myLock.unlock();
    }

    /**
     * @brief Start the Secure WebSocket Server.
     * @param port The port to listen on (usually 443 for WSS).
     * @param cert The server certificate in PEM format (null-terminated string).
     * @param key The server private key in PEM format (null-terminated string).
     * @return true if the server started successfully.
     * @return false if the server failed to start (e.g., socket error).
     */
    bool begin(uint16_t port, const char *cert, const char *key)
    {
        if (_running)
            return false;

        _port = port;
        _cert = cert;
        _key = key;

        // Configure TLS
        _tlsCfg.servercert_buf = (const unsigned char *)cert;
        _tlsCfg.servercert_bytes = strlen(cert) + 1;
        _tlsCfg.serverkey_buf = (const unsigned char *)key;
        _tlsCfg.serverkey_bytes = strlen(key) + 1;

        // Create server socket
        _serverSock = socket(AF_INET, SOCK_STREAM, 0);
        if (_serverSock < 0)
        {
#if defined(NUSOCK_DEBUG)
            NuSock::printLog("DBG ", "Failed to create socket\n");
#endif
            return false;
        }

        // Set socket options
        int opt = 1;
        setsockopt(_serverSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        // Set non-blocking
        int flags = fcntl(_serverSock, F_GETFL, 0);
        fcntl(_serverSock, F_SETFL, flags | O_NONBLOCK);

        // Bind
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (bind(_serverSock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        {
#if defined(NUSOCK_DEBUG)
            NuSock::printLog("DBG ", "Failed to bind socket\n");
#endif
            close(_serverSock);
            _serverSock = -1;
            return false;
        }

        // Listen
        if (listen(_serverSock, 5) < 0)
        {
#if defined(NUSOCK_DEBUG)
            NuSock::printLog("DBG ", "Failed to listen on socket\n");
#endif
            close(_serverSock);
            _serverSock = -1;
            return false;
        }

        _running = true;

        if (_onEvent)
            _onEvent(nullptr, SERVER_EVENT_CONNECT, nullptr, 0);

        return true;
    }

    /**
     * @brief Main processing loop.
     * Accepts new connections, performs SSL handshakes, and processes incoming data.
     * MUST be called frequently in the main Arduino loop().
     */
    void loop()
    {
        if (!_running || _serverSock < 0)
            return;

        // Accept new connections
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientSock = accept(_serverSock, (struct sockaddr *)&clientAddr, &clientLen);

        if (clientSock >= 0)
        {
            myLock.lock();

            // Create SSL session
            esp_tls_t *tls = esp_tls_init();
            if (tls)
            {
#if defined(NUSOCK_DEBUG)
                NuSock::printLog("DBG ", "Starting SSL Handshake...\n");
#endif
                int ret = esp_tls_server_session_create(&_tlsCfg, clientSock, tls);

                if (ret == 0)
                {
#if defined(NUSOCK_DEBUG)
                    NuSock::printLog("DBG ", "SSL Handshake Success! Switching to Non-Blocking.\n");
#endif

                    // NOW set to Non-Blocking for normal data usage
                    int flags = fcntl(clientSock, F_GETFL, 0);
                    fcntl(clientSock, F_SETFL, flags | O_NONBLOCK);

                    NuSSLClient *sc = new NuSSLClient();
                    sc->sock = clientSock;
                    sc->tls = tls;

#if defined(NUSOCK_USE_LWIP)
                    NuClient *c = new NuClient(this, nullptr);
#else
                    NuClient *c = new NuClient(this, nullptr, false);
#endif
                    c->isSecure = true;
                    c->index = clients.size();
                    c->state = NuClient::STATE_HANDSHAKE; // Skip SSL_HANDSHAKE, go straight to WS

                    sc->nuClient = c;

                    sslClients.push_back(sc);
                    clients.push_back(c);
                }
                else
                {
#if defined(NUSOCK_DEBUG)
                    NuSock::printLog("DBG ", "SSL Handshake Failed! Error: -0x%x\n", -ret);
#endif
                    esp_tls_server_session_delete(tls);
                    close(clientSock);
                }
            }
            else
            {
#if defined(NUSOCK_DEBUG)
                NuSock::printLog("DBG ", "Failed to init TLS\n");
#endif
                close(clientSock);
            }

            myLock.unlock();
        }

        // Process existing clients
        myLock.lock();
        for (size_t i = 0; i < sslClients.size(); i++)
        {
            NuSSLClient *sc = sslClients[i];
            NuClient *c = sc->nuClient;

            if (!sc->tls)
            {
                removeClient(c, sc);
                i--;
                continue;
            }

            // Standard processing
            processClient(c, sc);
        }
        myLock.unlock();
    }

    /**
     * @brief Register a callback function for server events.
     * @param cb Function pointer to the event handler.
     */
    void onEvent(NuServerSecureEventCallback cb) { _onEvent = cb; }

    /**
     * @brief Broadcast a text message to ALL connected clients.
     * @param msg Null-terminated string to send.
     */
    void send(const char *msg)
    {
        myLock.lock();
        size_t len = strlen(msg);
        for (size_t i = 0; i < clients.size(); i++)
        {
            NuClient *c = clients[i];
            if (c->state == NuClient::STATE_CONNECTED)
                buildFrame(c, 0x1, (const uint8_t *)msg, len);
        }
        myLock.unlock();
    }

    /**
     * @brief Broadcast a binary message to ALL connected clients.
     * @param data Pointer to the data buffer.
     * @param len Length of the data.
     */
    void send(const uint8_t *data, size_t len)
    {
        myLock.lock();
        for (size_t i = 0; i < clients.size(); i++)
        {
            NuClient *c = clients[i];
            if (c->state == NuClient::STATE_CONNECTED)
                buildFrame(c, 0x2, data, len);
        }
        myLock.unlock();
    }

    /**
     * @brief Send a text message to a specific client.
     * @param index The client's internal index.
     * @param msg Null-terminated string to send.
     */
    void send(int index, const char *msg)
    {
        if (index >= clients.size())
            return;
        myLock.lock();
        NuClient *c = clients[index];
        if (c->state == NuClient::STATE_CONNECTED)
            buildFrame(c, 0x1, (const uint8_t *)msg, strlen(msg));
        myLock.unlock();
    }

    /**
     * @brief Send a binary message to a specific client.
     * @param index The client's internal index.
     * @param data Pointer to the data buffer.
     * @param len Length of the data.
     */
    void send(int index, const uint8_t *data, size_t len)
    {
        if (index >= clients.size())
            return;
        myLock.lock();
        NuClient *c = clients[index];
        if (c->state == NuClient::STATE_CONNECTED)
            buildFrame(c, 0x2, data, len);
        myLock.unlock();
    }

    /**
     * @brief Get the number of currently active connections.
     * @return size_t Number of connected clients.
     */
    size_t clientCount()
    {
        myLock.lock();
        size_t n = clients.size();
        myLock.unlock();
        return n;
    }
};

#endif // ESP32
#endif