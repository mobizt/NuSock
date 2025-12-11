import asyncio
import websockets
import socket

def get_local_ip():
    """Attempts to retrieve the local IP address of the machine."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        # doesn't even have to be reachable
        s.connect(('10.255.255.255', 1))
        IP = s.getsockname()[0]
    except Exception:
        IP = '127.0.0.1'
    finally:
        s.close()
    return IP

async def echo(websocket):
    print(f"Client Connected from {websocket.remote_address}!")
    try:
        async for message in websocket:
            print(f"Received: {message}")
            await websocket.send(f"Echo: {message}")
    except websockets.exceptions.ConnectionClosed:
        print("Client Disconnected")

async def main():
    # Get Local IP
    ip = get_local_ip()
    port = 8080
    
    print("="*40)
    print(f"✅ Server Started!")
    print(f"📡 Local IP:   {ip}")
    print(f"🔌 Port:       {port}")
    print(f"📋 Copy this to your Arduino Sketch:")
    print(f'   wsClient.begin("{ip}", {port}, "/");')
    print("="*40)

    # Listen on all interfaces (0.0.0.0)
    async with websockets.serve(echo, "0.0.0.0", port):
        await asyncio.Future()  # run forever

if __name__ == "__main__":
    asyncio.run(main())