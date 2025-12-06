/**
 * SPDX-FileCopyrightText: 2025 Suwatchai K. <suwatchai@outlook.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef NUSOCK_SERVER_H
#define NUSOCK_SERVER_H

#include "NuSockConfig.h"
#include "NuSockUtils.h"
#include "NuSockTypes.h"
#include "vector/dynamic/DynamicVector.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

// Callback Signature (uses client->index inside NuClient)
typedef void (*NuServerEventCallback)(NuClient *client, NuServerEvent event, const uint8_t *payload, size_t len);

class NuSockServer
{
private:
    NuLock myLock;
    ReadyUtils::DynamicVector<NuClient *> clients;
    uint16_t _port;
    NuServerEventCallback _onEvent = nullptr;
    bool _running = false;

#ifdef NUSOCK_USE_LWIP
    struct tcp_pcb *server_pcb = nullptr;
#else
    // GENERIC MODE VARIABLES
    void *_genericServerRef = nullptr;
    Client *(*_acceptFunc)(void *) = nullptr;
#endif

    void removeClient(NuClient *c)
    {
        for (size_t i = clients.size() - 1; i >= 0; i--)
        {
            if (clients[i] == c)
            {
                clients.erase(i);
                // Update index for remaining clients
                for (size_t j = i; j < clients.size(); j++)
                {
                    clients[j]->index = j;
                }
                break;
            }
        }

        if (c->rxBuffer)
        {
            free(c->rxBuffer);
            c->rxBuffer = nullptr;
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

#ifdef NUSOCK_USE_LWIP

    static void static_close_client(void *arg)
    {
        NuClient *c = (NuClient *)arg;
        if (!c)
            return;
        NuSockServer *s = (NuSockServer *)c->server;
        s->myLock.lock();

        if (s->_onEvent && c->last_event != SERVER_EVENT_CLIENT_DISCONNECTED)
            s->_onEvent(c, SERVER_EVENT_CLIENT_DISCONNECTED, nullptr, 0);
        c->last_event = SERVER_EVENT_CLIENT_DISCONNECTED;

        if (c->pcb)
        {
            tcp_arg(c->pcb, NULL);
            tcp_close(c->pcb);
            c->pcb = NULL;
        }
        s->removeClient(c);
        s->myLock.unlock();
    }

    static void static_flush_client(void *arg)
    {
        NuClient *c = (NuClient *)arg;
        if (!c || !c->pcb)
            return;
        NuSockServer *s = (NuSockServer *)c->server;
        s->myLock.lock();
        while (c->txLen > 0)
        {
            size_t available = tcp_sndbuf(c->pcb);
            size_t mss = tcp_mss(c->pcb);
            size_t send_len = c->txLen;
            if (send_len > available)
                send_len = available;
            if (send_len > mss)
                send_len = mss;
            if (send_len == 0)
                break;
            err_t err = tcp_write(c->pcb, &c->txBuffer[0], send_len, TCP_WRITE_FLAG_COPY);
            if (err == ERR_OK)
            {
                size_t remaining = c->txLen - send_len;
                if (remaining == 0)
                    c->clearTx();
                else
                {
                    memmove(c->txBuffer, c->txBuffer + send_len, remaining);
                    c->txLen = remaining;
                }
            }
            else
            {
#if defined(NUSOCK_DEBUG)
                Serial.println("[WS Debug] Error: Write Error");
#endif
                if (s->_onEvent)
                    s->_onEvent(c, SERVER_EVENT_ERROR, (const uint8_t *)"Write Error", 11);
                c->last_event = SERVER_EVENT_ERROR;
                break;
            }
        }
        tcp_output(c->pcb);
        s->myLock.unlock();
    }

    void lwip_process(NuClient *c)
    {
        if (!c->rxBuffer)
            return;

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
            else if (payloadLen == 127)
            {
#if defined(NUSOCK_DEBUG)
                Serial.println("[WS Debug] Error: Frame Too Large");
#endif
                if (_onEvent)
                    _onEvent(c, SERVER_EVENT_ERROR, (const uint8_t *)"Frame Too Large", 15);
                c->last_event = SERVER_EVENT_ERROR;
                tcpip_callback(static_close_client, c);
                return;
            }
            if (isMasked)
                headerSize += 4;
            if (c->rxLen < headerSize + payloadLen)
                return;
            size_t maskOffset = headerSize - 4;
            if (opcode == 0x8)
            {
                tcpip_callback(static_close_client, c);
                return;
            }

            if ((opcode == 0x1 || opcode == 0x2) && isMasked)
            {
                uint8_t mask[4] = {c->rxBuffer[maskOffset], c->rxBuffer[maskOffset + 1], c->rxBuffer[maskOffset + 2], c->rxBuffer[maskOffset + 3]};
                uint8_t *payload = &c->rxBuffer[headerSize];
                for (size_t i = 0; i < payloadLen; i++)
                    payload[i] ^= mask[i % 4];

                if (opcode == 0x1)
                {
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

    static err_t cb_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
    {
        NuClient *c = (NuClient *)arg;
        if (!p)
        {
            tcpip_callback(static_close_client, c);
            return ERR_OK;
        }

        tcp_recved(pcb, p->tot_len);

        if (!c->rxBuffer)
        {
            pbuf_free(p);
            tcpip_callback(static_close_client, c);
            return ERR_MEM;
        }

        struct pbuf *ptr = p;
        while (ptr)
        {
            if (c->rxLen + ptr->len <= MAX_WS_BUFFER)
            {
                memcpy(c->rxBuffer + c->rxLen, ptr->payload, ptr->len);
                c->rxLen += ptr->len;
            }
            ptr = ptr->next;
        }
        pbuf_free(p);

        NuSockServer *s = (NuSockServer *)c->server;

        if (c->state == NuClient::STATE_HANDSHAKE)
        {
            if (c->rxLen > 100)
            {
                if (c->rxLen < MAX_WS_BUFFER)
                    c->rxBuffer[c->rxLen] = 0;
                if (strstr((char *)c->rxBuffer, "\r\n\r\n"))
                {
                    char *reqBuf = (char *)c->rxBuffer;
                    char *upgradeHeader = strstr(reqBuf, "Upgrade: websocket");
                    if (upgradeHeader)
                    {
                        if (s->_onEvent)
                            s->_onEvent(c, SERVER_EVENT_CLIENT_HANDSHAKE, nullptr, 0);
                        c->last_event = SERVER_EVENT_CLIENT_HANDSHAKE;

                        char *keyHeader = strstr(reqBuf, "Sec-WebSocket-Key: ");
                        if (keyHeader)
                        {
                            keyHeader += 19;
                            char *keyEnd = strstr(keyHeader, "\r\n");
                            if (keyEnd)
                            {
                                char clientKey[64];
                                size_t keyLen = keyEnd - keyHeader;
                                if (keyLen > 63)
                                    keyLen = 63;
                                strncpy(clientKey, keyHeader, keyLen);
                                clientKey[keyLen] = 0;

                                char acceptKey[64];
                                NuCrypto::getAcceptKey(clientKey, acceptKey, sizeof(acceptKey));

                                char respHead[] = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ";
                                tcp_write(pcb, respHead, strlen(respHead), TCP_WRITE_FLAG_COPY | TCP_WRITE_FLAG_MORE);
                                tcp_write(pcb, acceptKey, strlen(acceptKey), TCP_WRITE_FLAG_COPY | TCP_WRITE_FLAG_MORE);
                                tcp_write(pcb, "\r\n\r\n", 4, TCP_WRITE_FLAG_COPY);

                                tcp_output(pcb);
                                c->state = NuClient::STATE_CONNECTED;
                                c->rxLen = 0;

                                if (s->_onEvent)
                                    s->_onEvent(c, SERVER_EVENT_CLIENT_CONNECTED, nullptr, 0);
                                c->last_event = SERVER_EVENT_CLIENT_CONNECTED;
                            }
                        }
                    }
                    else
                    {
#if defined(NUSOCK_DEBUG)
                        Serial.println("[WS Debug] Error: Invalid Handshake");
#endif
                        if (s->_onEvent)
                            s->_onEvent(c, SERVER_EVENT_ERROR, (const uint8_t *)"Invalid Handshake", 17);
                        c->last_event = SERVER_EVENT_ERROR;
                    }
                }
            }
        }
        else
        {
            s->lwip_process(c);
        }
        return ERR_OK;
    }
    static err_t cb_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
    {
        NuSockServer *s = (NuSockServer *)arg;
        s->myLock.lock();
        NuClient *c = new NuClient(s, newpcb);
        c->index = s->clients.size();
        s->clients.push_back(c);
        tcp_arg(newpcb, c);
        tcp_recv(newpcb, cb_recv);
        tcp_sent(newpcb, [](void *arg, struct tcp_pcb *pcb, u16_t len) -> err_t
                 { tcpip_callback(static_flush_client, arg); return ERR_OK; });
        ip_set_option(newpcb, SOF_KEEPALIVE);

        s->myLock.unlock();
        return ERR_OK;
    }
    static void static_begin(void *arg)
    {
        NuSockServer *s = (NuSockServer *)arg;
        s->server_pcb = tcp_new();
        if (s->server_pcb)
        {
            tcp_bind(s->server_pcb, IP_ADDR_ANY, s->_port);
            s->server_pcb = tcp_listen(s->server_pcb);
            tcp_arg(s->server_pcb, s);
            tcp_accept(s->server_pcb, cb_accept);

            if (s->_onEvent)
                s->_onEvent(nullptr, SERVER_EVENT_CONNECT, nullptr, 0);
        }
    }
    static void static_stop(void *arg)
    {
        NuSockServer *s = (NuSockServer *)arg;
        if (s->server_pcb)
        {
            tcp_close(s->server_pcb);
            s->server_pcb = nullptr;
        }
    }
#endif

#ifndef NUSOCK_USE_LWIP

    void generic_process(NuClient *c)
    {
        if (!c->rxBuffer)
            return;

        while (c->client && c->client->connected() && c->client->available())
        {
            int byte = c->client->read();
            if (byte == -1)
                break;
            if (c->rxLen < MAX_WS_BUFFER)
                c->rxBuffer[c->rxLen++] = (uint8_t)byte;
        }

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

                                c->client->print("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ");
                                c->client->print(acceptKey);
                                c->client->print("\r\n\r\n");

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
#if defined(NUSOCK_DEBUG)
                        Serial.println("[WS Debug] Error: Invalid Handshake");
#endif
                        if (_onEvent)
                            _onEvent(c, SERVER_EVENT_ERROR, (const uint8_t *)"Invalid Handshake", 17);
                        c->last_event = SERVER_EVENT_ERROR;
                    }
                }
            }
        }
        else
        {
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
                    c->client->stop();
                    c->last_event = SERVER_EVENT_CLIENT_DISCONNECTED;
                    return;
                }

                if ((opcode == 0x1 || opcode == 0x2) && isMasked)
                {
                    uint8_t mask[4] = {c->rxBuffer[maskOffset], c->rxBuffer[maskOffset + 1], c->rxBuffer[maskOffset + 2], c->rxBuffer[maskOffset + 3]};
                    uint8_t *payload = &c->rxBuffer[headerSize];
                    for (size_t i = 0; i < payloadLen; i++)
                        payload[i] ^= mask[i % 4];

                    if (opcode == 0x1)
                    {
                        if (c->id[0] == 0)
                        {
                            char saved = payload[payloadLen];
                            payload[payloadLen] = 0;
                            strncpy(c->id, (char *)payload, sizeof(c->id) - 1);
                            c->id[sizeof(c->id) - 1] = 0;
                            payload[payloadLen] = saved;

                            if (_onEvent)
                                _onEvent(c, SERVER_EVENT_MESSAGE_TEXT, payload, payloadLen);
                        }
                        else if (_onEvent)
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

        if (c->txBuffer && c->txLen > 0 && c->client && c->client->connected())
        {
            c->client->write(c->txBuffer, c->txLen);
            c->clearTx();
        }
    }
#endif

public:
    /**
     * @brief Construct a new Nu Sock Server object.
     */
    NuSockServer() :
#if defined(NUSOCK_USE_LWIP)
                     server_pcb(NULL)
#else
                     _genericServerRef(NULL)
#endif
    {
    }

    /**
     * @brief Destroy the Nu Sock Server object.
     * * Stops the server and disconnects all clients.
     */
    ~NuSockServer()
    {
        stop();
    }

    /**
     * @brief Stop the server.
     * * Disconnects all connected clients, frees their resources, stops the listener,
     * and fires the SERVER_EVENT_DISCONNECTED event.
     */
    void stop()
    {
        if (!_running)
            return;

        myLock.lock();
        for (size_t i = 0; i < clients.size(); i++)
        {
            NuClient *c = clients[i];
#ifdef NUSOCK_USE_LWIP
            if (c->pcb)
            {
                tcp_arg(c->pcb, NULL);
                tcp_close(c->pcb);
                c->pcb = NULL;
            }
#else
            if (c->client)
            {
                c->client->stop();
            }
#endif

            if (c->rxBuffer)
            {
                free(c->rxBuffer);
                c->rxBuffer = nullptr;
            }

            if (c)
                delete c;
        }
        clients.clear();

#ifdef NUSOCK_USE_LWIP
#if defined(ESP8266) || defined(ARDUINO_ARCH_RP2040)
        static_stop(this);
#else
        tcpip_callback(static_stop, this);
#endif
#else
        _acceptFunc = nullptr;
#endif

        _running = false;
        if (_onEvent)
            _onEvent(nullptr, SERVER_EVENT_DISCONNECTED, nullptr, 0);
        myLock.unlock();
    }

#ifdef NUSOCK_USE_LWIP
    /**
     * @brief Start the WebSocket Server (LwIP Mode).
     * * @param port The port to listen on (e.g., 80 or 8080).
     */
    void begin(uint16_t port)
    {
        if (_running)
            return;
        _port = port;
#if defined(ESP8266) || defined(ARDUINO_ARCH_RP2040)
        static_begin(this);
#else
        tcpip_callback(static_begin, this);
#endif
        _running = true;
    }
#else
    /**
     * @brief Start the WebSocket Server (Generic Mode).
     * * Wraps a standard Arduino Server object (e.g., WiFiServer).
     * * @tparam ServerType The class type of the underlying server.
     * @param server Pointer to the underlying Arduino Server instance.
     * @param port The port the server is listening on.
     */
    template <typename ServerType>
    void begin(ServerType *server, uint16_t port)
    {
        if (_running)
            return;
        _port = port;
        _genericServerRef = server;

        _acceptFunc = [](void *s) -> Client *
        {
            ServerType *srv = (ServerType *)s;
#if defined(ARDUINO_ARCH_RP2040) || defined(ESP32)
            auto c = srv->accept();
#else
            auto c = srv->available();
#endif
            if (c)
            {
                return (Client *)new decltype(c)(c);
            }
            return (Client *)nullptr;
        };

        server->begin();
        _running = true;

        if (_onEvent)
            _onEvent(nullptr, SERVER_EVENT_CONNECT, nullptr, 0);
    }
#endif

    /**
     * @brief Main processing loop.
     * * MUST be called frequently in the main Arduino loop() when using Generic Mode.
     * Accepts new clients and processes data for existing clients.
     */
    void loop()
    {
#ifndef NUSOCK_USE_LWIP
        if (!_genericServerRef || !_acceptFunc)
            return;

        Client *tempC = _acceptFunc(_genericServerRef);

        if (tempC)
        {
            myLock.lock();
            NuClient *c = new NuClient(this, tempC, true /* true for self dynamic allocated client */);
            if (c->rxBuffer)
            {
                c->index = clients.size();
                clients.push_back(c);
            }
            else
            {
#if defined(NUSOCK_DEBUG)
                Serial.println("[WS Debug] Error: Alloc Failed");
#endif
                if (_onEvent)
                    _onEvent(c, SERVER_EVENT_ERROR, (const uint8_t *)"Alloc Failed", 12);
                delete c;
            }
            myLock.unlock();
        }

        myLock.lock();
        for (size_t i = 0; i < clients.size(); i++)
        {
            NuClient *c = clients[i];
            if (!c->client || !c->client->connected())
            {
                if (_onEvent && c->last_event != SERVER_EVENT_CLIENT_DISCONNECTED)
                    _onEvent(c, SERVER_EVENT_CLIENT_DISCONNECTED, nullptr, 0);
                c->last_event = SERVER_EVENT_CLIENT_DISCONNECTED;
                removeClient(c);
                i--;
                continue;
            }
            generic_process(c);
        }
        myLock.unlock();
#endif
    }

    /**
     * @brief Register a callback function for server events.
     * * @param cb Function pointer matching the NuServerEventCallback signature.
     */
    void onEvent(NuServerEventCallback cb) { _onEvent = cb; }

    /**
     * @brief Broadcast a text message to ALL connected clients.
     * * @param msg Null-terminated string to broadcast.
     */
    void send(const char *msg)
    {
        myLock.lock();
        size_t len = strlen(msg);
        for (size_t i = 0; i < clients.size(); i++)
        {
            NuClient *c = clients[i];
            if (c->state != NuClient::STATE_CONNECTED)
                continue;
            buildFrame(c, 0x1, (const uint8_t *)msg, len);
#ifdef NUSOCK_USE_LWIP
            tcpip_callback(static_flush_client, c);
#endif
        }
        myLock.unlock();
    }

    /**
     * @brief Broadcast a binary message to ALL connected clients.
     * * @param data Pointer to the data buffer.
     * @param len Length of the data to broadcast.
     */
    void send(const uint8_t *data, size_t len)
    {
        myLock.lock();
        for (size_t i = 0; i < clients.size(); i++)
        {
            NuClient *c = clients[i];
            if (c->state != NuClient::STATE_CONNECTED)
                continue;
            buildFrame(c, 0x2, data, len);
#ifdef NUSOCK_USE_LWIP
            tcpip_callback(static_flush_client, c);
#endif
        }
        myLock.unlock();
    }

    /**
     * @brief Send a text message to a specific client by internal index.
     * * @param index The index of the client in the internal list.
     * @param msg Null-terminated string to send.
     */
    void send(int index, const char *msg)
    {
        if (index >= (int)clients.size())
            return;

        myLock.lock();
        size_t len = strlen(msg);

        NuClient *c = clients[index];
        if (c->state == NuClient::STATE_CONNECTED)
        {
            buildFrame(c, 0x1, (const uint8_t *)msg, len);
#ifdef NUSOCK_USE_LWIP
            tcpip_callback(static_flush_client, c);
#endif
        }
        myLock.unlock();
    }

    /**
     * @brief Send a binary message to a specific client by internal index.
     * * @param index The index of the client in the internal list.
     * @param data Pointer to the data buffer.
     * @param len Length of the data to send.
     */
    void send(int index, const uint8_t *data, size_t len)
    {
        if (index >= (int)clients.size())
            return;

        myLock.lock();
        NuClient *c = clients[index];
        if (c->state == NuClient::STATE_CONNECTED)
        {
            buildFrame(c, 0x2, data, len);
#ifdef NUSOCK_USE_LWIP
            tcpip_callback(static_flush_client, c);
#endif
        }
        myLock.unlock();
    }

    /**
     * @brief Send a text message to a specific client by Client ID.
     * * The ID is usually assigned by the user logic or extracted from the handshake.
     * * @param targetId The ID string to match.
     * @param msg Null-terminated string to send.
     */
    void send(const char *targetId, const char *msg)
    {
        myLock.lock();
        size_t len = strlen(msg);
        for (size_t i = 0; i < clients.size(); i++)
        {
            NuClient *c = clients[i];
            if (c->state == NuClient::STATE_CONNECTED && strcmp(c->id, targetId) == 0)
            {
                buildFrame(c, 0x1, (const uint8_t *)msg, len);
#ifdef NUSOCK_USE_LWIP
                tcpip_callback(static_flush_client, c);
#endif
            }
        }
        myLock.unlock();
    }

    /**
     * @brief Send a binary message to a specific client by Client ID.
     * * @param targetId The ID string to match.
     * @param data Pointer to the data buffer.
     * @param len Length of the data to send.
     */
    void send(const char *targetId, const uint8_t *data, size_t len)
    {
        myLock.lock();
        for (size_t i = 0; i < clients.size(); i++)
        {
            NuClient *c = clients[i];
            if (c->state == NuClient::STATE_CONNECTED && strcmp(c->id, targetId) == 0)
            {
                buildFrame(c, 0x2, data, len);
#ifdef NUSOCK_USE_LWIP
                tcpip_callback(static_flush_client, c);
#endif
            }
        }
        myLock.unlock();
    }

    /**
     * @brief Get the number of currently connected clients.
     * * @return size_t Number of active connections.
     */
    size_t clientCount()
    {
        myLock.lock();
        size_t n = clients.size();
        myLock.unlock();
        return n;
    }
};

#endif