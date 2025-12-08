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

// Callback signature for secure client events
typedef void (*NuClientSecureEventCallback)(NuClient *client, NuClientEvent event, const uint8_t *payload, size_t len);

/**
 * @brief Secure WebSocket Client (WSS) for ESP32 using native esp_tls.
 * * This class provides a lightweight, high-performance WSS client implementation
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

    // Helper: Process incoming data from the SSL buffer
    void process_rx_buffer()
    {
        if (!_internalClient)
            return;

        if (_internalClient->state == NuClient::STATE_HANDSHAKE)
        {
            if (_internalClient->rxLen > 0)
            {
                // Ensure null termination for string search safety
                if (_internalClient->rxLen < MAX_WS_BUFFER)
                    _internalClient->rxBuffer[_internalClient->rxLen] = 0;
                else
                    _internalClient->rxBuffer[MAX_WS_BUFFER - 1] = 0;

#if defined(NUSOCK_DEBUG)
// Serial.println((char*)_internalClient->rxBuffer);
#endif

                // Check for successful upgrade
                if (strstr((char *)_internalClient->rxBuffer, "101 Switching Protocols"))
                {
                    _internalClient->state = NuClient::STATE_CONNECTED;
                    _internalClient->rxLen = 0;
                    if (_onEvent)
                        _onEvent(_internalClient, CLIENT_EVENT_CONNECTED, nullptr, 0);
                }
                else if (_internalClient->rxLen > 1024)
                {
                    // Buffer full without valid handshake -> Fail
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
                uint8_t lenByte = _internalClient->rxBuffer[1] & 0x7F;
                size_t headerSize = 2;
                size_t payloadLen = lenByte;

                // Handle extended payload lengths
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

                // Dispatch Events
                if (opcode == 0x1 && _onEvent)
                    _onEvent(_internalClient, CLIENT_EVENT_MESSAGE_TEXT, payload, payloadLen);
                else if (opcode == 0x2 && _onEvent)
                    _onEvent(_internalClient, CLIENT_EVENT_MESSAGE_BINARY, payload, payloadLen);
                else if (opcode == 0x8)
                {
                    stop();
                    return;
                }

                // Shift processed frame out of buffer
                size_t total = headerSize + payloadLen;
                size_t rem = _internalClient->rxLen - total;
                if (rem > 0)
                    memmove(_internalClient->rxBuffer, &_internalClient->rxBuffer[total], rem);
                _internalClient->rxLen = rem;
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
     * * If set, this certificate will be used for verification.
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
     * * Initiates the SSL handshake using esp_tls and performs the WebSocket Upgrade.
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
            Serial.println("[WSS] TLS Init Failed");
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
            Serial.printf("[WSS] Connection Failed. Error: %d\n", ret);
#endif
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
                Serial.println("[WSS] Failed sending handshake");
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
     * * Handles SSL data transmission and reception.
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
                Serial.println("[WSS] Write Error");
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
            buildFrame(_internalClient, 0x1, (const uint8_t *)msg, strlen(msg));
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
            buildFrame(_internalClient, 0x2, data, len);
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
};

#endif // ESP32
#endif // NUSOCK_CLIENT_SECURE_H