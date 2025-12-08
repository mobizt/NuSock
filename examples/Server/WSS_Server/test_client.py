import asyncio
import ssl
import websockets

# REPLACE WITH YOUR ESP32 IP
DEVICE_IP = "192.168.0.x"
URI = f"wss://{DEVICE_IP}:443"

async def test_connection():
    print(f"Attempting to connect to {URI}...")

    # Create a custom SSL context that IGNORES certificate errors
    ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    ssl_context.check_hostname = False
    ssl_context.verify_mode = ssl.CERT_NONE

    try:
        async with websockets.connect(URI, ssl=ssl_context) as ws:
            print("✅ CONNECTED! SSL Handshake successful.")
            
            # Send a message
            msg = "Hello from Python!"
            print(f"📤 Sending: {msg}")
            await ws.send(msg)
            
            # Wait for reply
            reply = await ws.recv()
            print(f"fz Received Reply: {reply}")
            
    except Exception as e:
        print(f"❌ Connection Failed: {e}")

if __name__ == "__main__":
    # You might need to install websockets: pip install websockets
    asyncio.run(test_connection())