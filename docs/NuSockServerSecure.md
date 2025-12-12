# NuSockServerSecure Class Reference

**Header:** `NuSockServerSecure.h`

The `NuSockServerSecure` class provides a Secure WebSocket Server (WSS) implementation for the **ESP32** platform. It utilizes the native ESP-IDF `esp_tls` API (via `esp_tls.h`) to handle SSL/TLS handshakes and encryption directly, offering better performance and stability than wrapping standard Arduino Client objects.

## Public Methods

### `NuSockServerSecure()`
Constructs a new Secure WebSocket Server object.

### `~NuSockServerSecure()`
Destroys the object, stops the server, disconnects all clients, and frees all allocated SSL/memory resources.

### `bool begin(uint16_t port, const char *cert, const char *key)`
Starts the Secure WebSocket Server on the specified port using the provided certificate and private key.

* **Parameters:**
    * `port` (uint16_t): The TCP port to listen on. Standard WSS uses port **443**.
    * `cert` (const char*): The server certificate in PEM format (null-terminated string).
    * `key` (const char*): The server private key in PEM format (null-terminated string).
* **Returns:** * `true`: If the server socket was created and bound successfully.
    * `false`: If the server failed to start (e.g., port in use or memory error).

### `void stop()`
Stops the server immediately.
* Disconnects all active clients.
* Releases all SSL contexts (`esp_tls` handles).
* Closes the listening server socket.
* Fires the `SERVER_EVENT_DISCONNECTED` event.

### `void loop()`
The main processing loop. **MUST** be called frequently in the main Arduino `loop()`.
* Accepts new incoming TCP connections.
* Performs non-blocking SSL handshakes for new clients.
* decrypts incoming WSS data and dispatches events.
* Encrypts and sends outgoing WSS data.

### `void onEvent(NuServerSecureEventCallback cb)`
Registers a user-defined callback function to handle server events (connect, disconnect, message, error).

* **Parameters:**
    * `cb` (NuServerSecureEventCallback): A function pointer matching the signature: `void (*)(NuClient *client, NuServerEvent event, const uint8_t *payload, size_t len)`.

### `size_t clientCount()`
Gets the number of currently active, connected clients.

* **Returns:** * `size_t`: The number of clients.

### `void send(const char *msg)`
Broadcasts a text message to **ALL** currently connected clients.

* **Parameters:**
    * `msg` (const char*): A null-terminated C-string containing the text message.

### `void send(const uint8_t *data, size_t len)`
Broadcasts a binary message to **ALL** currently connected clients.

* **Parameters:**
    * `data` (const uint8_t*): Pointer to the binary data buffer.
    * `len` (size_t): Size of the data in bytes.

### `void send(int index, const char *msg)`
Sends a text message to a specific client identified by their index.

* **Parameters:**
    * `index` (int): The client's internal index (accessible via `client->index` in callbacks).
    * `msg` (const char*): A null-terminated C-string containing the text message.

### `void send(int index, const uint8_t *data, size_t len)`
Sends a binary message to a specific client identified by their index.

* **Parameters:**
    * `index` (int): The client's internal index.
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
Broadcasts a Ping control frame (Opcode 0x9) to **ALL** connected clients to check connectivity or keep sessions alive.

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