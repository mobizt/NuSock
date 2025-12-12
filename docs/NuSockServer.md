# NuSockServer Class Reference

**Header:** `NuSockServer.h`

The `NuSockServer` class handles standard (non-secure) WebSocket server operations. It implements a **Dual-Mode Architecture** to support a wide range of hardware:
1.  **LwIP Mode:** Uses native asynchronous callbacks (ESP32/ESP8266 only) for high performance and low memory overhead.
2.  **Generic Mode:** Wraps standard Arduino `Server` objects (e.g., `WiFiServer`, `EthernetServer`) for compatibility with Arduino UNO R4, Portenta, Teensy, STM32, and others.

## Public Methods

### `NuSockServer()`
Constructs a new Server object.

### `~NuSockServer()`
Destroys the object, stops the server, disconnects all clients, and frees resources.

### `void begin(uint16_t port)`
*(LwIP Mode Only)* Starts the internal LwIP server on the specified port.

* **Parameters:**
    * `port` (uint16_t): The TCP port to listen on (e.g., 80 or 8080).

### `void begin(ServerType *server, uint16_t port)`
*(Generic Mode Only)* Starts the server wrapping an existing Arduino Server instance.

* **Parameters:**
    * `server` (ServerType*): Pointer to the underlying Arduino Server instance (e.g., `&server` where `server` is a `WiFiServer` object).
    * `port` (uint16_t): The port the server is listening on. This is used for internal reference.

### `void stop()`
Stops the server immediately.
* Disconnects all connected clients.
* Frees all internal buffers.
* Stops the underlying listener (if LwIP) or stops polling (if Generic).
* Fires the `SERVER_EVENT_DISCONNECTED` event.

### `void loop()`
The main processing loop. **MUST** be called frequently in the main Arduino `loop()`.
* **Generic Mode:** Checks for new clients via `accept()`/`available()` and handles data IO.
* **LwIP Mode:** Handles internal polling/timeout tasks (though most work is done in async callbacks).

### `void onEvent(NuServerEventCallback cb)`
Registers a user-defined callback function to handle server events.

* **Parameters:**
    * `cb` (NuServerEventCallback): A function pointer matching the signature: `void (*)(NuClient *client, NuServerEvent event, const uint8_t *payload, size_t len)`.

### `size_t clientCount()`
Gets the number of currently connected clients.

* **Returns:** * `size_t`: The number of active connections.

### `void send(const char *msg)`
Broadcasts a text message to **ALL** currently connected clients.

* **Parameters:**
    * `msg` (const char*): A null-terminated C-string containing the message.

### `void send(const uint8_t *data, size_t len)`
Broadcasts a binary message to **ALL** currently connected clients.

* **Parameters:**
    * `data` (const uint8_t*): Pointer to the binary data buffer.
    * `len` (size_t): Size of the data in bytes.

### `void send(int index, const char *msg)`
Sends a text message to a specific client identified by their internal index.

* **Parameters:**
    * `index` (int): The client's internal index (accessible via `client->index`).
    * `msg` (const char*): A null-terminated C-string containing the message.

### `void send(int index, const uint8_t *data, size_t len)`
Sends a binary message to a specific client identified by their internal index.

* **Parameters:**
    * `index` (int): The client's internal index.
    * `data` (const uint8_t*): Pointer to the binary data buffer.
    * `len` (size_t): Size of the data in bytes.

### `void send(const char *targetId, const char *msg)`
Sends a text message to a specific client identified by their Client ID.
*Note: Client IDs must be manually assigned or parsed from headers if implemented.*

* **Parameters:**
    * `targetId` (const char*): The ID string to match.
    * `msg` (const char*): A null-terminated C-string containing the message.

### `void send(const char *targetId, const uint8_t *data, size_t len)`
Sends a binary message to a specific client identified by their Client ID.

* **Parameters:**
    * `targetId` (const char*): The ID string to match.
    * `data` (const uint8_t*): Pointer to the binary data buffer.
    * `len` (size_t): Size of the data in bytes.

### `void sendFragmentStart(int index, const uint8_t *payload, size_t len, bool isBinary)`
Starts sending a large message (fragmented) to a specific client. This sends the first frame with `FIN=0`.

* **Parameters:**
    * `index` (int): The client's internal index.
    * `payload` (const uint8_t*): The data for this chunk.
    * `len` (size_t): Length of this chunk.
    * `isBinary` (bool): 
        * `true`: Sets Opcode to 0x2 (Binary).
        * `false`: Sets Opcode to 0x1 (Text).

### `void sendFragmentCont(int index, const uint8_t *payload, size_t len)`
Sends a continuation chunk for an ongoing fragmented message. This sends a frame with `FIN=0` and `Opcode=0x0`.

* **Parameters:**
    * `index` (int): The client's internal index.
    * `payload` (const uint8_t*): The data for this chunk.
    * `len` (size_t): Length of this chunk.

### `void sendFragmentFin(int index, const uint8_t *payload, size_t len)`
Sends the final chunk of a fragmented message. This sends a frame with `FIN=1` and `Opcode=0x0`.

* **Parameters:**
    * `index` (int): The client's internal index.
    * `payload` (const uint8_t*): The data for this chunk.
    * `len` (size_t): Length of this chunk.

### `void sendPing(const char *msg = "")`
Broadcasts a Ping control frame (Opcode 0x9) to **ALL** connected clients.

* **Parameters:**
    * `msg` (const char*): Optional short text payload (max 125 bytes). Defaults to empty string.

### `void sendPing(int index, const char *msg = "")`
Sends a Ping control frame (Opcode 0x9) to a specific client.

* **Parameters:**
    * `index` (int): The client's internal index.
    * `msg` (const char*): Optional short text payload (max 125 bytes). Defaults to empty string.

### `void close(int index, uint16_t code = 1000, const char *reason = "")`
Initiates a graceful Close Handshake (RFC 6455) with a specific client.

* **Parameters:**
    * `index` (int): The client's internal index.
    * `code` (uint16_t): The WebSocket status code (e.g., `1000` for Normal Closure, `1001` for Going Away). Defaults to `1000`.
    * `reason` (const char*): An optional short string explaining the reason for closing (max 123 bytes). Defaults to empty string.