/**
 * SPDX-FileCopyrightText: 2025 Suwatchai K. <suwatchai@outlook.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef NUSOCK_CLIENT_SECURE_H
#define NUSOCK_CLIENT_SECURE_H

#if defined(ESP32)

#include "NuSockConfig.h"
#include "NuSockUtils.h"
#include "NuSockTypes.h"
#include <esp_tls.h>
#include <esp_crt_bundle.h> // Required for default public server trust
#include <fcntl.h>

typedef void (*NuClientSecureEventCallback)(NuClient *client, NuClientEvent event, const uint8_t *payload, size_t len);

/**
 * @brief Secure WebSocket Client (WSS) for ESP32 using native esp_tls.
 * This class provides a lightweight, high-performance WSS client implementation
 * that sits directly on top of the ESP-IDF esp_tls stack, avoiding the overhead
 * of the standard WiFiClientSecure.
 */
class NuSockClientSecure
{
private:
    NuLock myLock;
    char _host[128];
    uint16_t _port = 443;
    char _path[128];

    // Custom CA Certificate (PEM format), nullptr means use default bundle
    const char *_ca_cert = nullptr;

    NuClientSecureEventCallback _onEvent = nullptr;

    // Internal State
    esp_tls_t *_tls = nullptr;
    NuClient *_internalClient = nullptr;

    // Helper: Generate random Sec-WebSocket-Key
    void generateRandomKey(char *outBuf)
    {
        uint8_t randomBytes[16];
        for (int i = 0; i < 16; i++)
            randomBytes[i] = random(0, 255);
        NuBase64::encode(randomBytes, 16, outBuf, 32);
    }

    // Helper: Build and append a WebSocket frame to the TX buffer
    void buildFrame(NuClient *c, uint8_t opcode, bool isFin, const uint8_t *data, size_t len)
    {
        uint8_t mask[4];
        for (int i = 0; i < 4; i++)
            mask[i] = random(0, 255);

        // Apply FIN bit based on argument
        uint8_t firstByte = opcode & 0x0F;
        if (isFin)
        {
            firstByte |= 0x80;
        }
        c->appendTx(firstByte);

        if (len <= 125)
        {
            c->appendTx((uint8_t)len | 0x80); // Mask bit set
        }
        else
        {
            c->appendTx(126 | 0x80); // Mask bit set
            c->appendTx(len >> 8);
            c->appendTx(len & 0xFF);
        }
        for (int i = 0; i < 4; i++)
            c->appendTx(mask[i]);
        for (size_t j = 0; j < len; j++)
            c->appendTx(data[j] ^ mask[j % 4]);
    }

    void process_rx_buffer()
    {
        if (!_internalClient)
            return;

        if (_internalClient->state == NuClient::STATE_HANDSHAKE)
        {
            if (_internalClient->rxLen > 0)
            {
                if (_internalClient->rxLen < MAX_WS_BUFFER)
                    _internalClient->rxBuffer[_internalClient->rxLen] = 0;
                else
                    _internalClient->rxBuffer[MAX_WS_BUFFER - 1] = 0;

                if (strstr((char *)_internalClient->rxBuffer, "101 Switching Protocols"))
                {
                    _internalClient->state = NuClient::STATE_CONNECTED;
                    _internalClient->rxLen = 0;
                    if (_onEvent)
                        _onEvent(_internalClient, CLIENT_EVENT_CONNECTED, nullptr, 0);
                }
                else if (_internalClient->rxLen > 1024)
                {
                    stop();
                }
            }
        }
        else
        {
            // Standard WS Frame Parsing
            while (_internalClient->rxLen >= 2)
            {
                uint8_t opcode = _internalClient->rxBuffer[0] & 0x0F;
                bool isFin = (_internalClient->rxBuffer[0] & 0x80);
                uint8_t lenByte = _internalClient->rxBuffer[1] & 0x7F;
                bool isMasked = (_internalClient->rxBuffer[1] & 0x80);

                // Strict checks
#if defined(NUSOCK_FULL_COMPLIANCE) || defined(NUSOCK_RFC_STRICT_MASK_RSV)
                // RSV check
                if ((_internalClient->rxBuffer[0] & 0x70) != 0)
                {
                    stop();
                    return;
                }
                // Strict Masking (Client MUST receive UNMASKED)
                if (isMasked)
                {
                    stop();
                    return;
                }
#endif

                size_t headerSize = 2;
                size_t payloadLen = lenByte;

                if (payloadLen == 126)
                {
                    if (_internalClient->rxLen < 4)
                        return;
                    payloadLen = (_internalClient->rxBuffer[2] << 8) | _internalClient->rxBuffer[3];
                    headerSize += 2;
                }

                if (isMasked)
                    headerSize += 4;

                size_t totalFrameSize = headerSize + payloadLen;
                if (_internalClient->rxLen < totalFrameSize)
                    return;

                // Control frames
                if (opcode >= 0x8)
                {
#if defined(NUSOCK_FULL_COMPLIANCE) || defined(NUSOCK_RFC_FRAGMENTATION) || defined(NUSOCK_RFC_STRICT_MASK_RSV)
                    if (!isFin || payloadLen > 125)
                    {
                        stop();
                        return;
                    }
#endif
                    uint8_t *payload = &_internalClient->rxBuffer[headerSize]; // Unmasked from Server

                    if (opcode == 0x8)
                    {
#if defined(NUSOCK_FULL_COMPLIANCE) || defined(NUSOCK_RFC_CLOSE_HANDSHAKE)
                        if (payloadLen == 1)
                        {
                            stop();
                            return;
                        }

                        // We initiated close
                        if (_internalClient->state == NuClient::STATE_CLOSING)
                        {
                            stop();
                            return;
                        }

                        // Server initiated close
                        // Echo
                        buildFrame(_internalClient, 0x8, true, payload, payloadLen);
                        if (_internalClient->txLen > 0)
                        {
                            esp_tls_conn_write(_tls, _internalClient->txBuffer, _internalClient->txLen);
                            _internalClient->clearTx();
                        }
#endif
                        stop();
                        return;
                    }
                    else if (opcode == 0x9)
                    {
                        buildFrame(_internalClient, 0xA, true, payload, payloadLen);
                    }

                    size_t rem = _internalClient->rxLen - totalFrameSize;
                    if (rem > 0)
                        memmove(_internalClient->rxBuffer, _internalClient->rxBuffer + totalFrameSize, rem);
                    _internalClient->rxLen -= totalFrameSize;
                    continue;
                }

                // Data frames
                uint8_t *payload = &_internalClient->rxBuffer[headerSize];

#if defined(NUSOCK_FULL_COMPLIANCE) || defined(NUSOCK_RFC_UTF8_STRICT)
                // UTF-8 validation
                bool checkUTF8 = (opcode == 0x1);
#if defined(NUSOCK_FULL_COMPLIANCE) || defined(NUSOCK_RFC_FRAGMENTATION)
                if (opcode == 0 && _internalClient->fragmentOpcode == 0x1)
                    checkUTF8 = true;
#endif

                if (checkUTF8)
                {
                    if (!NuUTF8::validate(_internalClient->utf8State, payload, payloadLen))
                    {
#if defined(NUSOCK_DEBUG)
                        NuSock::printLog("DBG ", "Error: Invalid UTF-8 sequence\n");
#endif
                        if (_onEvent)
                            _onEvent(_internalClient, CLIENT_EVENT_ERROR, (uint8_t *)"Invalid UTF-8 sequence", 22);
                        stop(); // Close Code 1007
                        return;
                    }

                    if (isFin)
                    {
                        if (!NuUTF8::isComplete(_internalClient->utf8State))
                        {
#if defined(NUSOCK_DEBUG)
                            NuSock::printLog("DBG ", "Error: Incomplete UTF-8 at FIN\n");
#endif
                            if (_onEvent)
                                _onEvent(_internalClient, CLIENT_EVENT_ERROR, (uint8_t *)"Incomplete UTF-8 at FIN", 23);
                            stop(); // Close Code 1007
                            return;
                        }
                        _internalClient->utf8State = 0; // Reset for next message
                    }
                }
#endif

#if defined(NUSOCK_FULL_COMPLIANCE) || defined(NUSOCK_RFC_FRAGMENTATION)
                if (opcode > 0)
                {
                    if (_internalClient->fragmentOpcode != 0)
                    {
                        stop();
                        return;
                    }

                    if (!isFin)
                    {
                        _internalClient->fragmentOpcode = opcode;
                        if (_onEvent)
                            _onEvent(_internalClient, CLIENT_EVENT_FRAGMENT_START, payload, payloadLen);
                    }
                    else
                    {
                        if (opcode == 0x1 && _onEvent)
                            _onEvent(_internalClient, CLIENT_EVENT_MESSAGE_TEXT, payload, payloadLen);
                        else if (opcode == 0x2 && _onEvent)
                            _onEvent(_internalClient, CLIENT_EVENT_MESSAGE_BINARY, payload, payloadLen);
                    }
                }
                else if (opcode == 0)
                {
                    if (_internalClient->fragmentOpcode == 0)
                    {
                        stop();
                        return;
                    }

                    if (!isFin)
                    {
                        if (_onEvent)
                            _onEvent(_internalClient, CLIENT_EVENT_FRAGMENT_CONT, payload, payloadLen);
                    }
                    else
                    {
                        if (_onEvent)
                            _onEvent(_internalClient, CLIENT_EVENT_FRAGMENT_FIN, payload, payloadLen);
                        _internalClient->fragmentOpcode = 0;
                    }
                }
#else
                if (opcode == 0x1 && _onEvent)
                    _onEvent(_internalClient, CLIENT_EVENT_MESSAGE_TEXT, payload, payloadLen);
                else if (opcode == 0x2 && _onEvent)
                    _onEvent(_internalClient, CLIENT_EVENT_MESSAGE_BINARY, payload, payloadLen);
#endif

                // Consume
                size_t rem = _internalClient->rxLen - totalFrameSize;
                if (rem > 0)
                    memmove(_internalClient->rxBuffer, _internalClient->rxBuffer + totalFrameSize, rem);
                _internalClient->rxLen -= totalFrameSize;
            }
        }
    }

public:
    /**
     * @brief Construct a new Nu Sock Client Secure object.
     */
    NuSockClientSecure() {}

    /**
     * @brief Destroy the Nu Sock Client Secure object.
     * Stops the secure connection and frees TLS/memory resources.
     */
    ~NuSockClientSecure()
    {
        stop();
    }

    /**
     * @brief Initialize the secure client parameters.
     * @param host The hostname of the WebSocket server (e.g., "echo.websocket.org").
     * @param port The port number (default is 443 for WSS).
     * @param path The URL path (endpoint) to connect to (default: "/").
     */
    void begin(const char *host, uint16_t port, const char *path = "/")
    {
        strncpy(_host, host, sizeof(_host) - 1);
        _port = port;
        strncpy(_path, path, sizeof(_path) - 1);
    }

    /**
     * @brief Set a custom Certificate Authority (CA) certificate.
     * If set, this certificate will be used for verification.
     * If NOT set (default), the global ESP32 certificate bundle (Mozilla root certs)
     * will be used, which works for most public websites.
     * * @param cert The CA certificate in PEM format (string).
     */
    void setCACert(const char *cert)
    {
        _ca_cert = cert;
    }

    /**
     * @brief Register a callback function for client events.
     * @param cb Function pointer matching the NuClientSecureEventCallback signature.
     */
    void onEvent(NuClientSecureEventCallback cb) { _onEvent = cb; }

    /**
     * @brief Establish the Secure WebSocket connection (WSS).
     * Initiates the SSL handshake using esp_tls and performs the WebSocket Upgrade.
     * This function blocks during the initial handshake and then switches the
     * socket to non-blocking mode for the main loop.
     * * @return true if the SSL handshake and WebSocket upgrade were successful.
     * @return false if the connection failed.
     */
    bool connect()
    {
        if (_tls)
            return true; // Already connected

        esp_tls_cfg_t cfg = {};

        if (_ca_cert != nullptr)
        {
            // Use Custom CA
            cfg.cacert_buf = (const unsigned char *)_ca_cert;
            cfg.cacert_bytes = strlen(_ca_cert) + 1;
        }
        else
        {
            // Use Built-in Bundle (Fallback for public sites)
            cfg.crt_bundle_attach = esp_crt_bundle_attach;
        }

        cfg.skip_common_name = false;
        cfg.non_block = false;

        // Allocate TLS handle
        _tls = esp_tls_init();
        if (!_tls)
        {
#if defined(NUSOCK_DEBUG)
            NuSock::printLog("DBG ", "TLS Init Failed\n");
#endif
            return false;
        }

        // Synchronous Connect (Using correct API for ESP-IDF v5 / Arduino 3.0)
        String url = "wss://" + String(_host) + ":" + String(_port);
        // Pass -1 as client_fd to indicate a new connection
        int ret = esp_tls_conn_http_new_sync(url.c_str(), &cfg, _tls);

        if (ret != 1)
        { // indicates success
#if defined(NUSOCK_DEBUG)
            NuSock::printLog("DBG ", "Connection Failed. Error: %d\n", ret);
#endif
            char errBuf[64];
            snprintf(errBuf, 64, "Connection Failed. Error: %d\n", ret);
            if (_onEvent)
                _onEvent(_internalClient, CLIENT_EVENT_ERROR, (uint8_t *)errBuf, strlen(errBuf));
            esp_tls_conn_destroy(_tls);
            _tls = nullptr;
            return false;
        }

        // Initialize Internal Client Wrapper
        if (_internalClient)
            delete _internalClient;

// WSS doesn't use LwIP PCB directly in this class, so we pass nullptr
#ifdef NUSOCK_USE_LWIP
        _internalClient = new NuClient((NuSockServer *)nullptr, (struct tcp_pcb *)nullptr);
#else
        _internalClient = new NuClient((NuSockServer *)nullptr, (Client *)nullptr, false);
#endif
        _internalClient->state = NuClient::STATE_HANDSHAKE;

        char keyBuf[32];
        generateRandomKey(keyBuf);
        String req = "GET " + String(_path) + " HTTP/1.1\r\n";
        req += "Host: " + String(_host) + "\r\n";
        req += "Connection: Upgrade\r\n";
        req += "Upgrade: websocket\r\n";
        req += "Sec-WebSocket-Version: 13\r\n";
        req += "Sec-WebSocket-Key: " + String(keyBuf) + "\r\n";
        req += "Origin: https://" + String(_host) + "\r\n";
        req += "User-Agent: NuSock\r\n\r\n";

        // Send Handshake
        size_t written = 0;
        size_t toWrite = req.length();
        while (written < toWrite)
        {
            int ret = esp_tls_conn_write(_tls, req.c_str() + written, toWrite - written);
            if (ret > 0)
            {
                written += ret;
            }
            else if (ret != ESP_TLS_ERR_SSL_WANT_READ && ret != ESP_TLS_ERR_SSL_WANT_WRITE)
            {
#if defined(NUSOCK_DEBUG)
                NuSock::printLog("DBG ", "Failed sending handshake\n");
#endif
                stop();
                return false;
            }
        }

        // Switch to Non-Blocking Mode for loop() processing
        int sockfd;
        esp_tls_get_conn_sockfd(_tls, &sockfd);
        int flags = fcntl(sockfd, F_GETFL, 0);
        fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

        return true;
    }

    /**
     * @brief Check if the client is currently connected.
     * @return true if connected to the server and handshake is complete.
     */
    bool connected()
    {
        return (_tls != nullptr && _internalClient != nullptr && _internalClient->state == NuClient::STATE_CONNECTED);
    }

    /**
     * @brief Main processing loop.
     * Handles SSL data transmission and reception.
     * MUST be called frequently in the main Arduino loop().
     * Checks for incoming data, processes WebSocket frames, and flushes outgoing data.
     */
    void loop()
    {
        if (!_tls || !_internalClient)
            return;

        char buf[512];
        int ret = esp_tls_conn_read(_tls, buf, sizeof(buf));

        if (ret > 0)
        {
            // Data received
            if (_internalClient->rxLen + ret <= MAX_WS_BUFFER)
            {
                memcpy(_internalClient->rxBuffer + _internalClient->rxLen, buf, ret);
                _internalClient->rxLen += ret;
                process_rx_buffer();

                // Safety: If processing caused a disconnect/stop, return immediately
                if (!_internalClient)
                    return;
            }
        }
        else if (ret == 0)
        {
            stop(); // Connection closed cleanly
            return;
        }
        else if (ret != ESP_TLS_ERR_SSL_WANT_READ && ret != ESP_TLS_ERR_SSL_WANT_WRITE)
        {
            stop(); // Error
            return;
        }

        if (_internalClient && _internalClient->txLen > 0)
        {
            int sent = esp_tls_conn_write(_tls, _internalClient->txBuffer, _internalClient->txLen);
            if (sent > 0)
            {
                _internalClient->clearTx(); // Assuming full write for simplicity
            }
            else if (sent != ESP_TLS_ERR_SSL_WANT_READ && sent != ESP_TLS_ERR_SSL_WANT_WRITE)
            {
#if defined(NUSOCK_DEBUG)
                NuSock::printLog("DBG ", "Write Error\n");
#endif
                // Optional: stop() on write error
            }
        }
    }

    /**
     * @brief Send a text message to the server.
     * @param msg Null-terminated string to send.
     */
    void send(const char *msg)
    {
        if (_internalClient && _internalClient->state == NuClient::STATE_CONNECTED)
        {
            buildFrame(_internalClient, 0x1, true, (const uint8_t *)msg, strlen(msg));
        }
    }

    /**
     * @brief Send a binary message to the server.
     * @param data Pointer to the data buffer.
     * @param len Length of the data to send.
     */
    void send(const uint8_t *data, size_t len)
    {
        if (_internalClient && _internalClient->state == NuClient::STATE_CONNECTED)
        {
            buildFrame(_internalClient, 0x2, true, data, len);
        }
    }
    /**
     * @brief Start a fragmented message(FIN = 0).
     * @param payload The first chunk of data.
     * @param len Length of the data chunk.
     * @param isBinary If true,
     * starts a Binary message(Opcode 0x2).If false, starts Text(Opcode 0x1).
     */
    void sendFragmentStart(const uint8_t *payload, size_t len, bool isBinary)
    {
        if (_internalClient && _internalClient->state == NuClient::STATE_CONNECTED)
        {
            // FIN = false, Opcode = 0x1 (Text) or 0x2 (Binary)
            buildFrame(_internalClient, isBinary ? 0x2 : 0x1, false, payload, len);
        }
    }

    /**
     * @brief Send a middle fragment (FIN=0, Opcode=0x0).
     * @param payload The data chunk.
     * @param len Length of the data chunk.
     */
    void sendFragmentCont(const uint8_t *payload, size_t len)
    {
        if (_internalClient && _internalClient->state == NuClient::STATE_CONNECTED)
        {
            // FIN = false, Opcode = 0x0 (Continuation)
            buildFrame(_internalClient, 0x0, false, payload, len);
        }
    }

    /**
     * @brief Finish a fragmented message (FIN=1, Opcode=0x0).
     * @param payload The last data chunk.
     * @param len Length of the data chunk.
     */
    void sendFragmentFin(const uint8_t *payload, size_t len)
    {
        if (_internalClient && _internalClient->state == NuClient::STATE_CONNECTED)
        {
            // FIN = true, Opcode = 0x0 (Continuation)
            buildFrame(_internalClient, 0x0, true, payload, len);
        }
    }

    /**
     * @brief Send a Ping (0x9) control frame to the server.
     */
    void sendPing(const char *msg = "")
    {
        if (_internalClient && _internalClient->state == NuClient::STATE_CONNECTED)
        {
            buildFrame(_internalClient, 0x9, true, (const uint8_t *)msg, strlen(msg));
        }
    }

    /**
     * @brief Initiate a graceful Close Handshake (RFC 6455).
     * @param code Status code (default 1000).
     * @param reason Optional reason string.
     */
    void close(uint16_t code = 1000, const char *reason = "")
    {
        if (_internalClient && _internalClient->state == NuClient::STATE_CONNECTED)
        {
            uint8_t payload[128];
            payload[0] = (uint8_t)((code >> 8) & 0xFF);
            payload[1] = (uint8_t)(code & 0xFF);

            size_t reasonLen = strlen(reason);
            if (reasonLen > 123)
                reasonLen = 123;

            if (reasonLen > 0)
                memcpy(&payload[2], reason, reasonLen);

            buildFrame(_internalClient, 0x8, true, payload, 2 + reasonLen);

            // Flush immediate for close
            if (_internalClient->txLen > 0)
            {
                esp_tls_conn_write(_tls, _internalClient->txBuffer, _internalClient->txLen);
                _internalClient->clearTx();
            }

            _internalClient->state = NuClient::STATE_CLOSING;
        }
    }

    /**
     * @brief Stop the secure client and disconnect.
     * * Gracefully closes the SSL connection, fires the DISCONNECTED event,
     * and frees internal memory buffers.
     */
    void stop()
    {
        if (_internalClient)
        {
            if (_onEvent && _internalClient->state == NuClient::STATE_CONNECTED)
                _onEvent(_internalClient, CLIENT_EVENT_DISCONNECTED, nullptr, 0);

            delete _internalClient;
            _internalClient = nullptr;
        }

        if (_tls)
        {
            // Use destroy instead of delete to match init()
            esp_tls_conn_destroy(_tls);
            _tls = nullptr;
        }
    }

    /**
     * @brief Stop the secure client and disconnect.
     * Gracefully closes the SSL connection, fires the DISCONNECTED event,
     * and frees internal memory buffers.
     */
    void disconnect() { stop(); }
};

#endif // ESP32
#endif // NUSOCK_CLIENT_SECURE_H