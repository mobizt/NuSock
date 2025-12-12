# NuSockClientSecure Class Reference

**Header:** `NuSockClientSecure.h`

The `NuSockClientSecure` class provides a lightweight, high-performance Secure WebSocket (WSS) client implementation for the **ESP32** platform. It sits directly on top of the ESP-IDF `esp_tls` stack, avoiding the overhead of the standard `WiFiClientSecure`.

## Public Methods

### `NuSockClientSecure()`
Constructs a new Secure Client object.

### `~NuSockClientSecure()`
Destroys the object, stops the secure connection, and frees TLS/memory resources.

### `void begin(const char *host, uint16_t port, const char *path = "/")`
Initializes the secure client parameters.

* **Parameters:**
    * `host`: The hostname of the WebSocket server (e.g., "echo.websocket.org").
    * `port`: The port number (default is 443 for WSS).
    * `path`: The URL path (endpoint) to connect to (default: "/").

### `void setCACert(const char *cert)`
Sets a custom Certificate Authority (CA) certificate.

* **Parameters:**
    * `cert`: The CA certificate in PEM format (string).
* **Note:** If not set, the global ESP32 certificate bundle (Mozilla root certs) is used.

### `void onEvent(NuClientSecureEventCallback cb)`
Registers a callback function for client events.

* **Parameters:**
    * `cb`: Function pointer matching the `NuClientSecureEventCallback` signature.

### `bool connect()`
Establishes the Secure WebSocket connection (WSS). Initiates the SSL handshake and performs the WebSocket Upgrade.

* **Returns:** `true` if the SSL handshake and WebSocket upgrade were successful; `false` otherwise.

### `bool connected()`
Checks if the client is currently connected.

* **Returns:** `true` if connected to the server and the handshake is complete.

### `void loop()`
The main processing loop. Handles SSL data transmission and reception. **MUST** be called frequently in the main Arduino `loop()`.

### `void send(const char *msg)`
Sends a text message to the server.

* **Parameters:**
    * `msg`: Null-terminated string to send.

### `void send(const uint8_t *data, size_t len)`
Sends a binary message to the server.

* **Parameters:**
    * `data`: Pointer to the data buffer.
    * `len`: Length of the data to send.

### `void sendFragmentStart(const uint8_t *payload, size_t len, bool isBinary)`
Starts a fragmented message (Streaming).

* **Parameters:**
    * `payload`: The first chunk of data.
    * `len`: Length of the data chunk.
    * `isBinary`: `true` for Binary opcode (0x2), `false` for Text opcode (0x1).

### `void sendFragmentCont(const uint8_t *payload, size_t len)`
Sends a middle fragment (Continuation).

* **Parameters:**
    * `payload`: The data chunk.
    * `len`: Length of the data chunk.

### `void sendFragmentFin(const uint8_t *payload, size_t len)`
Finishes a fragmented message.

* **Parameters:**
    * `payload`: The last data chunk.
    * `len`: Length of the data chunk.

### `void sendPing(const char *msg = "")`
Sends a Ping (0x9) control frame to the server.

* **Parameters:**
    * `msg`: Optional payload string.

### `void close(uint16_t code = 1000, const char *reason = "")`
Initiates a graceful Close Handshake (RFC 6455).

* **Parameters:**
    * `code`: Status code (default 1000).
    * `reason`: Optional reason string.

### `void stop()`
Stops the secure client and disconnects. Gracefully closes the SSL connection, fires the `DISCONNECTED` event, and frees internal memory buffers.

### `void disconnect()`
Alias for `stop()`.