# NuSockClient Class Reference

**Header:** `NuSockClient.h`

The `NuSockClient` class handles standard (non-secure) WebSocket client connections. It features a **Dual-Mode Architecture**:
1.  **LwIP Mode:** Uses native asynchronous callbacks (ESP32/ESP8266) for maximum performance.
2.  **Generic Mode:** Wraps standard Arduino `Client` objects (e.g., `WiFiClient`, `EthernetClient`) for compatibility with Arduino UNO R4, Portenta, Teensy, STM32, and others.

## Public Methods

### `NuSockClient()`
Constructs a new NuSock Client object.

### `~NuSockClient()`
Destroys the object, stops the connection, and frees resources.

### `void begin(const char *host, uint16_t port, const char *path = "/")`
*(LwIP Mode Only)* Initializes client parameters using native LwIP structures. This prepares the client but does not connect immediately.

* **Parameters:**
    * `host` (const char*): The hostname or IP address of the WebSocket server (e.g., "echo.websocket.org").
    * `port` (uint16_t): The port number (usually 80 for WS).
    * `path` (const char*): The URL path/endpoint to connect to. Defaults to "/".

### `void begin(ClientType *client, const char *host, uint16_t port, const char *path = "/")`
*(Generic Mode Only)* Initializes client parameters by wrapping an existing Arduino Client instance.

* **Parameters:**
    * `client` (ClientType*): Pointer to the underlying Arduino Client instance (e.g., `&wifiClient`).
    * `host` (const char*): The hostname or IP address of the WebSocket server.
    * `port` (uint16_t): The port number.
    * `path` (const char*): The URL path. Defaults to "/".

### `bool connect()`
Establishes the WebSocket connection.
* **Generic Mode:** Connects via TCP, sends the HTTP Upgrade headers, and validates the handshake synchronously.
* **LwIP Mode:** Initiates the asynchronous TCP connection and handshake process.

* **Returns:**
    * `true`: If the connection was successful (Generic) or successfully initiated (LwIP).
    * `false`: If the connection failed immediately.

### `bool connected()`
Checks if the client is currently connected to the server and the handshake is complete.

* **Returns:**
    * `true`: If connected and ready.
    * `false`: If disconnected or still handshaking.

### `void stop()`
Stops the client immediately.
* Closes the underlying TCP connection.
* Frees internal memory buffers.
* Fires the `CLIENT_EVENT_DISCONNECTED` event.

### `void disconnect()`
Alias for `stop()`.

### `void loop()`
The main processing loop. **MUST** be called frequently in the main Arduino `loop()`.
* Handles incoming data reception and processing.
* Manages TCP keep-alives and timeouts.

### `void onEvent(NuClientEventCallback cb)`
Registers a user-defined callback function to handle client events.

* **Parameters:**
    * `cb` (NuClientEventCallback): A function pointer matching the signature: `void (*)(NuClient *client, NuClientEvent event, const uint8_t *payload, size_t len)`.

### `void send(const char *msg)`
Sends a text message to the server.

* **Parameters:**
    * `msg` (const char*): A null-terminated C-string containing the message.

### `void send(const uint8_t *data, size_t len)`
Sends a binary message to the server.

* **Parameters:**
    * `data` (const uint8_t*): Pointer to the binary data buffer.
    * `len` (size_t): Size of the data in bytes.

### `void sendFragmentStart(const uint8_t *payload, size_t len, bool isBinary)`
Starts sending a large message (fragmented). This sends the first frame with `FIN=0`.

* **Parameters:**
    * `payload` (const uint8_t*): The data for this chunk.
    * `len` (size_t): Length of this chunk.
    * `isBinary` (bool): 
        * `true`: Sets Opcode to 0x2 (Binary).
        * `false`: Sets Opcode to 0x1 (Text).

### `void sendFragmentCont(const uint8_t *payload, size_t len)`
Sends a continuation chunk for an ongoing fragmented message. This sends a frame with `FIN=0` and `Opcode=0x0`.

* **Parameters:**
    * `payload` (const uint8_t*): The data for this chunk.
    * `len` (size_t): Length of this chunk.

### `void sendFragmentFin(const uint8_t *payload, size_t len)`
Sends the final chunk of a fragmented message. This sends a frame with `FIN=1` and `Opcode=0x0`.

* **Parameters:**
    * `payload` (const uint8_t*): The data for this chunk.
    * `len` (size_t): Length of this chunk.

### `void sendPing(const char *msg = "")`
Sends a Ping control frame (Opcode 0x9) to the server to check connectivity.

* **Parameters:**
    * `msg` (const char*): Optional short text payload (max 125 bytes). Defaults to empty string.

### `void close(uint16_t code = 1000, const char *reason = "")`
Initiates a graceful Close Handshake (RFC 6455). Sends a Close frame and waits for the server's acknowledgement.

* **Parameters:**
    * `code` (uint16_t): The WebSocket status code (e.g., `1000` for Normal Closure). Defaults to `1000`.
    * `reason` (const char*): An optional short string explaining the reason (max 123 bytes). Defaults to empty string.