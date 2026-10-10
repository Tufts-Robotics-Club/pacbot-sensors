
import asyncio
import os

from websockets.asyncio.server import serve

HOST = os.getenv("WS_HOST", "127.0.0.1")
PORT = int(os.getenv("WS_PORT", "8765"))

async def handle_client(websocket):
    print(f"client connected: {websocket.remote_address}")

    try:
        async for message in websocket:
            print(f"client connected: {message}")

            # Echo the message back to the client.
            # await websocket.send(f"Server received: {message}")

    except Exception as e:
        print(f"Connection error: {e}")

    finally:
        print("Client disconnected")


async def main():
    async with serve(handle_client, "0.0.0.0", 8765) as server:
        print("Server listening on port 8765")
        await server.serve_forever()


if __name__ == "__main__":
    asyncio.run(main())
