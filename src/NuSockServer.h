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
    void *_genericServerRef = nullptr;
    NuClient *(*_acceptFunc)(void *, NuSockServer *) = nullptr;
#endif

    void removeClient(NuClient *c)
    {
        for (size_t i = clients.size() - 1; i >= 0; i--)
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
        if (c->rxBuffer)
        {
            free(c->rxBuffer);
            c->rxBuffer = nullptr;
        }
        if (c)
        {
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdelete-non-virtual-dtor"
#endif
            delete c;
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
        }
    }

    void buildFrame(NuClient *c, uint8_t opcode, bool isFin, const uint8_t *data, size_t len)
    {
        uint8_t firstByte = opcode & 0x0F;
        if (isFin)
        {
            firstByte |= 0x80;
        }
        c->appendTx(firstByte);

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

        // Process Loop: Standard consuming loop (parse -> dispatch -> consume)
        while (c->rxLen > 0)
        {
            if (c->rxLen < 2)
                return; // Wait for header

            uint8_t opcode = c->rxBuffer[0] & 0x0F;
            bool isFin = (c->rxBuffer[0] & 0x80);
            uint8_t lenByte = c->rxBuffer[1] & 0x7F;
            bool isMasked = (c->rxBuffer[1] & 0x80);

            // Strict masking & RSV checks
#if defined(NUSOCK_FULL_COMPLIANCE) || defined(NUSOCK_RFC_STRICT_MASK_RSV)
            // RSV bit check (must be 0)
            if ((c->rxBuffer[0] & 0x70) != 0)
            {
                if (_onEvent)
                    _onEvent(c, SERVER_EVENT_ERROR, (const uint8_t *)"RSV Error", 9);
                c->last_event = SERVER_EVENT_ERROR;
                tcpip_callback(static_close_client, c);
                return;
            }
            // Strict Masking check (server must receive masked)
            if (!isMasked)
            {
                if (_onEvent)
                    _onEvent(c, SERVER_EVENT_ERROR, (const uint8_t *)"Mask Error", 10);
                c->last_event = SERVER_EVENT_ERROR;
                tcpip_callback(static_close_client, c);
                return;
            }
            // Control frame validation (basic)
            if (opcode >= 0x8)
            {
                // Control frames cannot be fragmented (FIN must be 1)
                // Control frames cannot have payload > 125
                if (!isFin || lenByte > 125)
                {
                    if (_onEvent)
                        _onEvent(c, SERVER_EVENT_ERROR, (const uint8_t *)"Control Err", 11);
                    tcpip_callback(static_close_client, c);
                    return;
                }
            }
#endif

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
                // (Optional 64-bit support or Error)
                if (_onEvent)
                    _onEvent(c, SERVER_EVENT_ERROR, (const uint8_t *)"Frame Too Large", 15);
                tcpip_callback(static_close_client, c);
                return;
            }

            if (isMasked)
                headerSize += 4;

            size_t totalFrameSize = headerSize + payloadLen;
            if (c->rxLen < totalFrameSize)
                return; // Wait for full payload

            size_t maskOffset = headerSize - 4;

            // Control frame handling (OpCode >= 0x8)
            if (opcode >= 0x8)
            {
                // Unmask control frame in-place
                if (isMasked)
                {
                    uint8_t mask[4] = {c->rxBuffer[maskOffset], c->rxBuffer[maskOffset + 1],
                                       c->rxBuffer[maskOffset + 2], c->rxBuffer[maskOffset + 3]};
                    // Use a temp pointer to avoid affecting the buffer logic below
                    uint8_t *ctrlPayload = &c->rxBuffer[headerSize];
                    for (size_t i = 0; i < payloadLen; i++)
                        ctrlPayload[i] ^= mask[i % 4];
                }
                uint8_t *ctrlPayload = &c->rxBuffer[headerSize];

                // Close handshake (Opcode 0x8)
                if (opcode == 0x8)
                {
#if defined(NUSOCK_FULL_COMPLIANCE) || defined(NUSOCK_RFC_CLOSE_HANDSHAKE)
                    // Protocol Error: Payload length 1 is illegal (must be 0 or >= 2)
                    if (payloadLen == 1)
                    {
                        tcpip_callback(static_close_client, c);
                        return;
                    }

                    // We initiated the close (State is CLOSING).
                    // This frame is the Client's acknowledgement.
                    if (c->state == NuClient::STATE_CLOSING)
                    {
                        // Handshake complete. Close TCP connection.
                        tcpip_callback(static_close_client, c);
                        return;
                    }

                    // Client initiated the close (State is CONNECTED).
                    // We must Echo the payload back and then close.
                    buildFrame(c, 0x8, true, ctrlPayload, payloadLen);
                    tcpip_callback(static_flush_client, c);

                    // Fire Event
                    if (_onEvent && c->last_event != SERVER_EVENT_CLIENT_DISCONNECTED)
                        _onEvent(c, SERVER_EVENT_CLIENT_DISCONNECTED, ctrlPayload, payloadLen);

                    c->last_event = SERVER_EVENT_CLIENT_DISCONNECTED;
                    tcpip_callback(static_close_client, c);
                    return;
#else
                    // Legacy/Simple behavior
                    tcpip_callback(static_close_client, c);
                    return;
#endif
                }
                else if (opcode == 0x9)
                {
                    // Ping -> Pong
                    buildFrame(c, 0xA, true, ctrlPayload, payloadLen);
                    tcpip_callback(static_flush_client, c);
                }

                // Strip Control frame from buffer and continue
                // We use memmove to shift the rest of the buffer over this control frame
                size_t rem = c->rxLen - totalFrameSize;
                if (rem > 0)
                    memmove(c->rxBuffer, c->rxBuffer + totalFrameSize, rem);
                c->rxLen -= totalFrameSize;
                continue;
            }

            // Fragmentation state validation
#if defined(NUSOCK_FULL_COMPLIANCE) || defined(NUSOCK_RFC_FRAGMENTATION)
            if (opcode > 0) // New data frame
            {
                if (c->fragmentOpcode != 0)
                {
                    // Error: nested message
                    if (_onEvent)
                        _onEvent(c, SERVER_EVENT_ERROR, (const uint8_t *)"Frag Error", 10);
                    tcpip_callback(static_close_client, c);
                    return;
                }
                if (!isFin)
                    c->fragmentOpcode = opcode; // Mark start
            }
            else if (opcode == 0) // Continuation
            {
                if (c->fragmentOpcode == 0)
                {
                    // Error: Orphan continuation
                    if (_onEvent)
                        _onEvent(c, SERVER_EVENT_ERROR, (const uint8_t *)"Frag Error", 10);
                    tcpip_callback(static_close_client, c);
                    return;
                }
                if (isFin)
                    c->fragmentOpcode = 0; // Mark end
            }
#endif
            // Unmask Payload
            if (isMasked)
            {
                uint8_t mask[4] = {c->rxBuffer[maskOffset], c->rxBuffer[maskOffset + 1],
                                   c->rxBuffer[maskOffset + 2], c->rxBuffer[maskOffset + 3]};
                uint8_t *payload = &c->rxBuffer[headerSize];
                for (size_t i = 0; i < payloadLen; i++)
                    payload[i] ^= mask[i % 4];
            }

            uint8_t *payload = &c->rxBuffer[headerSize];

            // Streaming event dispatch
#if defined(NUSOCK_FULL_COMPLIANCE) || defined(NUSOCK_RFC_FRAGMENTATION)
            if (opcode > 0)
            {
                if (!isFin)
                {
                    if (_onEvent)
                        _onEvent(c, SERVER_EVENT_FRAGMENT_START, payload, payloadLen);
                }
                else
                {
                    // Normal complete message
                    if (opcode == 0x1 && _onEvent)
                        _onEvent(c, SERVER_EVENT_MESSAGE_TEXT, payload, payloadLen);
                    else if (opcode == 0x2 && _onEvent)
                        _onEvent(c, SERVER_EVENT_MESSAGE_BINARY, payload, payloadLen);
                }
            }
            else if (opcode == 0)
            {
                if (!isFin)
                {
                    if (_onEvent)
                        _onEvent(c, SERVER_EVENT_FRAGMENT_CONT, payload, payloadLen);
                }
                else
                {
                    if (_onEvent)
                        _onEvent(c, SERVER_EVENT_FRAGMENT_FIN, payload, payloadLen);
                }
            }
#else
            // Legacy event dispatch (Ignore OpCode 0 logic)
            if (opcode == 0x1 && _onEvent)
                _onEvent(c, SERVER_EVENT_MESSAGE_TEXT, payload, payloadLen);
            else if (opcode == 0x2 && _onEvent)
                _onEvent(c, SERVER_EVENT_MESSAGE_BINARY, payload, payloadLen);
#endif
            // Consume frame
            size_t rem = c->rxLen - totalFrameSize;
            if (rem > 0)
                memmove(c->rxBuffer, c->rxBuffer + totalFrameSize, rem);
            c->rxLen -= totalFrameSize;
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
            if (c->rxLen > 0)
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
                bool isFin = (c->rxBuffer[0] & 0x80);
                uint8_t lenByte = c->rxBuffer[1] & 0x7F;
                bool isMasked = (c->rxBuffer[1] & 0x80);

#if defined(NUSOCK_FULL_COMPLIANCE) || defined(NUSOCK_RFC_STRICT_MASK_RSV)
                // RSV bit check
                if ((c->rxBuffer[0] & 0x70) != 0)
                {
                    if (_onEvent)
                        _onEvent(c, SERVER_EVENT_ERROR, (const uint8_t *)"RSV Error", 9);
                    c->last_event = SERVER_EVENT_ERROR;
                    c->client->stop();
                    return;
                }
                // Strict masking check
                if (!isMasked)
                {
                    if (_onEvent)
                        _onEvent(c, SERVER_EVENT_ERROR, (const uint8_t *)"Mask Error", 10);
                    c->last_event = SERVER_EVENT_ERROR;
                    c->client->stop();
                    return;
                }
#endif

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
                    // Frame too large / 64-bit not supported in basic implementation
                    c->client->stop();
                    return;
                }

                if (isMasked)
                    headerSize += 4;

                size_t totalFrameSize = headerSize + payloadLen;
                if (c->rxLen < totalFrameSize)
                    return;

                size_t maskOffset = headerSize - 4;

                // Control frame handling
                if (opcode >= 0x8)
                {
#if defined(NUSOCK_FULL_COMPLIANCE) || defined(NUSOCK_RFC_FRAGMENTATION) || defined(NUSOCK_RFC_STRICT_MASK_RSV)
                    // Control frames cannot be fragmented or > 125 bytes
                    if (!isFin || payloadLen > 125)
                    {
                        c->client->stop();
                        return;
                    }
#endif

                    // Unmask control frame (in place)
                    if (isMasked)
                    {
                        uint8_t mask[4] = {c->rxBuffer[maskOffset], c->rxBuffer[maskOffset + 1], c->rxBuffer[maskOffset + 2], c->rxBuffer[maskOffset + 3]};
                        uint8_t *ctrlPayload = &c->rxBuffer[headerSize];
                        for (size_t i = 0; i < payloadLen; i++)
                            ctrlPayload[i] ^= mask[i % 4];
                    }
                    uint8_t *ctrlPayload = &c->rxBuffer[headerSize];

                    // Close Handshake Handling
                    if (opcode == 0x8)
                    {
#if defined(NUSOCK_FULL_COMPLIANCE) || defined(NUSOCK_RFC_CLOSE_HANDSHAKE)
                        // Protocol Error
                        if (payloadLen == 1)
                        {
                            c->client->stop();
                            return;
                        }

                        // We initiated close (State is CLOSING)
                        if (c->state == NuClient::STATE_CLOSING)
                        {
                            c->client->stop();
                            removeClient(c);
                            return;
                        }

                        // Client initiated close (State is CONNECTED)
                        // Echo close frame
                        buildFrame(c, 0x8, true, ctrlPayload, payloadLen);
                        if (c->txBuffer && c->txLen > 0)
                        {
                            c->client->write(c->txBuffer, c->txLen);
                            c->clearTx();
                        }

                        if (_onEvent && c->last_event != SERVER_EVENT_CLIENT_DISCONNECTED)
                            _onEvent(c, SERVER_EVENT_CLIENT_DISCONNECTED, ctrlPayload, payloadLen);
                        c->last_event = SERVER_EVENT_CLIENT_DISCONNECTED;

                        c->client->stop();
                        return;
#else
                        if (_onEvent && c->last_event != SERVER_EVENT_CLIENT_DISCONNECTED)
                            _onEvent(c, SERVER_EVENT_CLIENT_DISCONNECTED, nullptr, 0);
                        c->client->stop();
                        c->last_event = SERVER_EVENT_CLIENT_DISCONNECTED;
                        return;
#endif
                    }
                    else if (opcode == 0x9)
                    {
                        // Ping -> Pong
                        buildFrame(c, 0xA, true, ctrlPayload, payloadLen);
                    }

                    // Strip control frame
                    size_t rem = c->rxLen - totalFrameSize;
                    if (rem > 0)
                        memmove(c->rxBuffer, c->rxBuffer + totalFrameSize, rem);
                    c->rxLen -= totalFrameSize;
                    continue;
                }

                // Data frame handling
#if defined(NUSOCK_FULL_COMPLIANCE) || defined(NUSOCK_RFC_FRAGMENTATION)
                // Streaming fragmentation logic
                if (opcode > 0) // New data frame
                {
                    if (c->fragmentOpcode != 0)
                    {
                        c->client->stop();
                        return; // Error: Nested fragmentation
                    }

                    // Unmask payload
                    if (isMasked)
                    {
                        uint8_t mask[4] = {c->rxBuffer[maskOffset], c->rxBuffer[maskOffset + 1], c->rxBuffer[maskOffset + 2], c->rxBuffer[maskOffset + 3]};
                        uint8_t *payload = &c->rxBuffer[headerSize];
                        for (size_t i = 0; i < payloadLen; i++)
                            payload[i] ^= mask[i % 4];
                    }
                    uint8_t *payload = &c->rxBuffer[headerSize];

                    // Strict UTF-8 validation
#if defined(NUSOCK_FULL_COMPLIANCE) || defined(NUSOCK_RFC_UTF8_STRICT)
                    if (opcode == 0x1)
                    {
                        if (!NuUTF8::validate(c->utf8State, payload, payloadLen))
                        {
                            if (_onEvent)
                                _onEvent(c, SERVER_EVENT_ERROR, (const uint8_t *)"Invalid UTF-8", 13);
                            // RFC 6455 1007: Invalid frame payload data
                            c->client->stop();
                            return;
                        }
                    }
#endif

                    if (!isFin)
                    {
                        // Start of fragmentation
                        c->fragmentOpcode = opcode;
                        if (_onEvent)
                            _onEvent(c, SERVER_EVENT_FRAGMENT_START, payload, payloadLen);
                    }
                    else
                    {
#if defined(NUSOCK_FULL_COMPLIANCE) || defined(NUSOCK_RFC_UTF8_STRICT)
                        // If FIN check validation state is accept
                        if (opcode == 0x1 && c->utf8State != NuUTF8::UTF8_ACCEPT)
                        {
                            if (_onEvent)
                                _onEvent(c, SERVER_EVENT_ERROR, (const uint8_t *)"Truncated UTF-8", 15);
                            c->client->stop();
                            return;
                        }
                        c->utf8State = 0; // Reset for next message
#endif

                        // Normal complete message
                        if (opcode == 0x1 && _onEvent)
                            _onEvent(c, SERVER_EVENT_MESSAGE_TEXT, payload, payloadLen);
                        else if (opcode == 0x2 && _onEvent)
                            _onEvent(c, SERVER_EVENT_MESSAGE_BINARY, payload, payloadLen);
                    }
                }
                else if (opcode == 0) // Continuation frame
                {
                    if (c->fragmentOpcode == 0)
                    {
                        c->client->stop();
                        return; // Error: Orphan continuation
                    }

                    // Unmask payload
                    if (isMasked)
                    {
                        uint8_t mask[4] = {c->rxBuffer[maskOffset], c->rxBuffer[maskOffset + 1], c->rxBuffer[maskOffset + 2], c->rxBuffer[maskOffset + 3]};
                        uint8_t *payload = &c->rxBuffer[headerSize];
                        for (size_t i = 0; i < payloadLen; i++)
                            payload[i] ^= mask[i % 4];
                    }
                    uint8_t *payload = &c->rxBuffer[headerSize];

#if defined(NUSOCK_FULL_COMPLIANCE) || defined(NUSOCK_RFC_UTF8_STRICT)
                    if (c->fragmentOpcode == 0x1)
                    {
                        if (!NuUTF8::validate(c->utf8State, payload, payloadLen))
                        {
                            if (_onEvent)
                                _onEvent(c, SERVER_EVENT_ERROR, (const uint8_t *)"Invalid UTF-8", 13);
                            c->client->stop();
                            return;
                        }
                    }
#endif

                    if (!isFin)
                    {
                        // Middle fragment
                        if (_onEvent)
                            _onEvent(c, SERVER_EVENT_FRAGMENT_CONT, payload, payloadLen);
                    }
                    else
                    {
#if defined(NUSOCK_FULL_COMPLIANCE) || defined(NUSOCK_RFC_UTF8_STRICT)
                        if (c->fragmentOpcode == 0x1 && c->utf8State != NuUTF8::UTF8_ACCEPT)
                        {
                            if (_onEvent)
                                _onEvent(c, SERVER_EVENT_ERROR, (const uint8_t *)"Truncated UTF-8", 15);
                            c->client->stop();
                            return;
                        }
                        c->utf8State = 0;
#endif

                        // End of fragmentation
                        if (_onEvent)
                            _onEvent(c, SERVER_EVENT_FRAGMENT_FIN, payload, payloadLen);
                        c->fragmentOpcode = 0; // Reset state
                    }
                }
#else
                // Legacy behavior
                if ((opcode == 0x1 || opcode == 0x2) && isMasked)
                {
                    uint8_t mask[4] = {c->rxBuffer[maskOffset], c->rxBuffer[maskOffset + 1], c->rxBuffer[maskOffset + 2], c->rxBuffer[maskOffset + 3]};
                    uint8_t *payload = &c->rxBuffer[headerSize];
                    for (size_t i = 0; i < payloadLen; i++)
                        payload[i] ^= mask[i % 4];

#if defined(NUSOCK_FULL_COMPLIANCE) || defined(NUSOCK_RFC_UTF8_STRICT)
                    if (opcode == 0x1)
                    {
                        uint32_t tempState = 0;
                        if (!NuUTF8::validate(tempState, payload, payloadLen) || tempState != NuUTF8::UTF8_ACCEPT)
                        {
                            if (_onEvent)
                                _onEvent(c, SERVER_EVENT_ERROR, (const uint8_t *)"Invalid UTF-8", 13);
                            c->client->stop();
                            return;
                        }
                    }
#endif

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
#endif

                // Consume frame (remove from buffer)
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
     * @brief Construct a new NuSock Server object.
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
     * @brief Destroy the NuSock Server object.
     * Stops the server and disconnects all clients.
     */
    ~NuSockServer() { stop(); }

    /**
     * @brief Stop the server.
     * Disconnects all connected clients, frees their resources, stops the listener,
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
                c->client->stop();
#endif
            if (c->rxBuffer)
                free(c->rxBuffer);
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
     * Wraps a standard Arduino Server object (e.g., WiFiServer).
     * @tparam ServerType The class type of the underlying server.
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

        _acceptFunc = [](void *s, NuSockServer *ns) -> NuClient *
        {
            ServerType *srv = (ServerType *)s;

// Use accept() for UNO R4 (S3), NINA, ESP, RP2040.
// Exclude SAMD MKR1000 specifically (WiFi101 doesn't support accept).
#if (defined(ARDUINO_ARCH_SAMD) && !defined(ARDUINO_SAMD_MKR1000)) ||     \
    defined(ARDUINO_AVR_UNO_WIFI_REV2) || defined(__AVR_ATmega4809__) ||  \
    defined(ARDUINO_SAMD_MKRWIFI1010) || defined(ARDUINO_NANO_33_IOT) ||  \
    defined(ARDUINO_ARCH_RP2040) || defined(ESP32) || defined(ESP8266) || \
    defined(ARDUINO_UNOR4_WIFI)
            auto c = srv->accept();
#else
            auto c = srv->available();
#endif

            if (c)
            {
                // Copy the client object to heap to persist it.
                Client *clientWrapper = new decltype(c)(c);
                NuClient *nc = new NuClient(ns, clientWrapper, true);
                nc->remoteIP = c.remoteIP();
                nc->remotePort = c.remotePort();
                return nc;
            }
            return nullptr;
        };

        // Double begin check
        // Only call begin() if server is not already running.
        // MKR1000 (WiFi101) does not support operator bool(), so we skip check there.
#if defined(ARDUINO_AVR_UNO_WIFI_REV2) || defined(__AVR_ATmega4809__) || \
    defined(ARDUINO_SAMD_MKRWIFI1010) || defined(ARDUINO_NANO_33_IOT) || \
    defined(ARDUINO_UNOR4_WIFI)
        if (!(*server))
            server->begin();
#else
        server->begin();
#endif

        _running = true;
        if (_onEvent)
            _onEvent(nullptr, SERVER_EVENT_CONNECT, nullptr, 0);
    }
#endif

    /**
     * @brief Main processing loop.
     */
    void loop()
    {
#ifndef NUSOCK_USE_LWIP
        if (!_genericServerRef || !_acceptFunc)
            return;

        NuClient *newClient = _acceptFunc(_genericServerRef, this);

        if (newClient)
        {
            if (!newClient->client || !newClient->client->connected())
            {
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdelete-non-virtual-dtor"
#endif
                delete newClient;
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
            }
            else
            {
                // Duplicate check
                bool duplicate = false;
                myLock.lock();
                for (size_t i = 0; i < clients.size(); i++)
                {
                    if (clients[i]->client && clients[i]->client->connected())
                    {
                        if (clients[i]->remoteIP == newClient->remoteIP &&
                            clients[i]->remotePort == newClient->remotePort)
                        {
                            duplicate = true;
                            break;
                        }
                    }
                }

                if (duplicate)
                {

                    // Safe duplicate cleanup
                    // Ethernet (Teensy/Mega/STM32) and WiFiS3 (R4) clients must be deleted to avoid leaks.
                    // WiFi101 (MKR1000) and WiFiNINA clients must not be deleted to avoid closing the socket.

                    Client *rawWrapper = newClient->client;

                    // Detach from NuClient to prevent stop() call in ~NuClient destructor
                    newClient->client = nullptr;

                    // Delete NuClient container
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdelete-non-virtual-dtor"
#endif
                    delete newClient;
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

// Delete the wrapper for Safe Platforms (Ethernet/S3)
#if defined(ARDUINO_UNOR4_WIFI) || defined(TEENSYDUINO) || defined(ARDUINO_ARCH_STM32) || defined(ARDUINO_ARCH_AVR) || defined(ESP32) || defined(ESP8266)
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdelete-non-virtual-dtor"
#endif
                    if (rawWrapper)
                        delete rawWrapper;
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#else
                    (void)rawWrapper; // Keep it alive for NINA/101
#endif
                }
                else
                {
                    if (newClient->rxBuffer)
                    {
                        newClient->index = clients.size();
                        clients.push_back(newClient);
                    }
                    else
                    {
                        delete newClient;
                    }
                }
                myLock.unlock();
            }
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
     * @param cb Function pointer matching the NuServerEventCallback signature.
     */
    void onEvent(NuServerEventCallback cb) { _onEvent = cb; }

    /**
     * @brief Broadcast a text message to ALL connected clients.
     * @param msg Null-terminated string to broadcast.
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
            buildFrame(c, 0x1, true, (const uint8_t *)msg, len);
#ifdef NUSOCK_USE_LWIP
            tcpip_callback(static_flush_client, c);
#endif
        }
        myLock.unlock();
    }

    /**
     * @brief Broadcast a binary message to ALL connected clients.
     * @param data Pointer to the data buffer.
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
            buildFrame(c, 0x2, true, data, len);
#ifdef NUSOCK_USE_LWIP
            tcpip_callback(static_flush_client, c);
#endif
        }
        myLock.unlock();
    }

    /**
     * @brief Send a text message to a specific client by internal index.
     * @param index The index of the client in the internal list.
     * @param msg Null-terminated string to send.
     */
    void send(int index, const char *msg)
    {
        if (index >= (int)clients.size())
            return;
        myLock.lock();
        NuClient *c = clients[index];
        if (c->state == NuClient::STATE_CONNECTED)
        {
            buildFrame(c, 0x1, true, (const uint8_t *)msg, strlen(msg));
#ifdef NUSOCK_USE_LWIP
            tcpip_callback(static_flush_client, c);
#endif
        }
        myLock.unlock();
    }

    /**
     * @brief Send a binary message to a specific client by internal index.
     * @param index The index of the client in the internal list.
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
            buildFrame(c, 0x2, true, data, len);
#ifdef NUSOCK_USE_LWIP
            tcpip_callback(static_flush_client, c);
#endif
        }
        myLock.unlock();
    }

    /**
     * @brief Send a text message to a specific client by Client ID.
     * The ID is usually assigned by the user logic or extracted from the handshake.
     * @param targetId The ID string to match.
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
                buildFrame(c, 0x1, true, (const uint8_t *)msg, len);
#ifdef NUSOCK_USE_LWIP
                tcpip_callback(static_flush_client, c);
#endif
            }
        }
        myLock.unlock();
    }

    /**
     * @brief Send a binary message to a specific client by Client ID.
     * @param targetId The ID string to match.
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
                buildFrame(c, 0x2, true, data, len);
#ifdef NUSOCK_USE_LWIP
                tcpip_callback(static_flush_client, c);
#endif
            }
        }
        myLock.unlock();
    }

    /**
     * @brief Start a fragmented message (FIN=0).
     * @param index The client index.
     * @param payload The first chunk of data.
     * @param len Length of the data chunk.
     * @param isBinary If true, starts a Binary message (Opcode 0x2). If false, starts Text (Opcode 0x1).
     */
    void sendFragmentStart(int index, const uint8_t *payload, size_t len, bool isBinary)
    {
        if (index >= (int)clients.size())
            return;
        myLock.lock();
        NuClient *c = clients[index];
        if (c->state == NuClient::STATE_CONNECTED)
        {
            // FIN = false, Opcode = 0x1 (Text) or 0x2 (Binary)
            buildFrame(c, isBinary ? 0x2 : 0x1, false, payload, len);
#ifdef NUSOCK_USE_LWIP
            tcpip_callback(static_flush_client, c);
#endif
        }
        myLock.unlock();
    }

    /**
     * @brief Send a middle fragment (FIN=0, Opcode=0x0).
     * @param index The client index.
     * @param payload The data chunk.
     * @param len Length of the data chunk.
     */
    void sendFragmentCont(int index, const uint8_t *payload, size_t len)
    {
        if (index >= (int)clients.size())
            return;
        myLock.lock();
        NuClient *c = clients[index];
        if (c->state == NuClient::STATE_CONNECTED)
        {
            // FIN = false, Opcode = 0x0 (Continuation)
            buildFrame(c, 0x0, false, payload, len);
#ifdef NUSOCK_USE_LWIP
            tcpip_callback(static_flush_client, c);
#endif
        }
        myLock.unlock();
    }

    /**
     * @brief Finish a fragmented message (FIN=1, Opcode=0x0).
     * @param index The client index.
     * @param payload The last data chunk.
     * @param len Length of the data chunk.
     */
    void sendFragmentFin(int index, const uint8_t *payload, size_t len)
    {
        if (index >= (int)clients.size())
            return;
        myLock.lock();
        NuClient *c = clients[index];
        if (c->state == NuClient::STATE_CONNECTED)
        {
            // FIN = true, Opcode = 0x0 (Continuation)
            buildFrame(c, 0x0, true, payload, len);
#ifdef NUSOCK_USE_LWIP
            tcpip_callback(static_flush_client, c);
#endif
        }
        myLock.unlock();
    }

    /**
     * @brief Broadcast a Ping (0x9) to ALL connected clients.
     * @param msg Optional payload string.
     */
    void sendPing(const char *msg = "")
    {
        myLock.lock();
        size_t len = strlen(msg);
        for (size_t i = 0; i < clients.size(); i++)
        {
            NuClient *c = clients[i];
            if (c->state != NuClient::STATE_CONNECTED)
                continue;

            buildFrame(c, 0x9, true, (const uint8_t *)msg, len);
#ifdef NUSOCK_USE_LWIP
            tcpip_callback(static_flush_client, c);
#endif
        }
        myLock.unlock();
    }

    /**
     * @brief Send a Ping (0x9) to a specific client by index.
     * @param index Client index.
     * @param msg Optional payload string.
     */
    void sendPing(int index, const char *msg = "")
    {
        if (index >= (int)clients.size())
            return;
        myLock.lock();
        NuClient *c = clients[index];
        if (c->state == NuClient::STATE_CONNECTED)
        {
            buildFrame(c, 0x9, true, (const uint8_t *)msg, strlen(msg));
#ifdef NUSOCK_USE_LWIP
            tcpip_callback(static_flush_client, c);
#endif
        }
        myLock.unlock();
    }

    /**
     * @brief Initiate a graceful Close Handshake (RFC 6455).
     * Sends a Close frame with a status code and reason, then waits for the client to reply.
     * @param index Client index.
     * @param code Status code (e.g., 1000 for Normal, 1001 for Going Away).
     * @param reason Optional short string reason (max 123 bytes).
     */
    void close(int index, uint16_t code = 1000, const char *reason = "")
    {
        if (index >= (int)clients.size())
            return;

        myLock.lock();
        NuClient *c = clients[index];

        // Only initiate if currently connected
        if (c->state == NuClient::STATE_CONNECTED)
        {
            uint8_t payload[128];
            payload[0] = (uint8_t)((code >> 8) & 0xFF);
            payload[1] = (uint8_t)(code & 0xFF);

            size_t reasonLen = strlen(reason);
            if (reasonLen > 123)
                reasonLen = 123; // Limit to maintain max control frame size (125)

            if (reasonLen > 0)
            {
                memcpy(&payload[2], reason, reasonLen);
            }

            // Send Close frame
            buildFrame(c, 0x8, true, payload, 2 + reasonLen);

#ifdef NUSOCK_USE_LWIP
            tcpip_callback(static_flush_client, c);
#endif

            // Update state to wait for Echo
            c->state = NuClient::STATE_CLOSING;
        }
        myLock.unlock();
    }

    /**
     * @brief Get the number of currently connected clients.
     * @return size_t Number of active connections.
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