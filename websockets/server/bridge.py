import asyncio
import os

from server.messages import RegistrationRequestMessage, RegistrationResponseMessage

from websockets.asyncio.server import serve
from websockets.exceptions import ConnectionClosed

HOST = os.getenv("WS_HOST", "127.0.0.1")
PORT = int(os.getenv("WS_PORT", "8765"))

VALID_ROLES = ["bot", "cv"]

CLIENT_IDS = {"bot": "1", "cv": "2"}

# Connected clients as a dictionary of websockets
connected_clients = {}


async def handle_connection(websocket):

    # Wait for 5 seconds to receive registration message
    raw_message = await asyncio.wait_for(
        websocket.recv(),
        timeout=5,
    )

    client_id = None

    try:
        # Parse registration message
        registration = RegistrationRequestMessage.from_json(raw_message)

        # Capture client ID and role
        client_id = registration.client_id
        role = registration.role

        # Check if this client is already connected
        if client_id in connected_clients:
            await websocket.close(
                code=1008,
                reason="Client ID already connected",
            )
            return

        # Validate role and expected client ID
        if role not in VALID_ROLES or CLIENT_IDS.get(role) != client_id:
            await websocket.close(
                code=1008,
                reason="Invalid role or client ID",
            )
            return

        # Register the client
        connected_clients[client_id] = websocket
        print(f"Client {client_id} ({role}) connected")

        # Inform client of successful registration
        response = RegistrationResponseMessage(
            success=True,
            message="Registration successful",
        )

        await websocket.send(response.to_json())

        # Keep handling messages until disconnection
        async for raw in websocket:
            await process_message(client_id, raw)

    except asyncio.TimeoutError:
        await websocket.close(
            code=1008,
            reason="Registration timed out",
        )

    except (ValueError, TypeError):
        await websocket.close(
            code=1008,
            reason="Registration failed",
        )

    except ConnectionClosed:
        pass

    finally:
        # Only remove the connection owned by this handler
        if client_id is not None and connected_clients.get(client_id) is websocket:
            del connected_clients[client_id]
            print(f"Client {client_id} disconnected")


async def process_message(client_id, raw):
    # TODO: Parse and route messages.
    print(f"Message from {client_id}: {raw}")


async def main():
    async with serve(handle_connection, "0.0.0.0", 8765) as server:
        print("Server listening on port 8765")
        await server.serve_forever()


if __name__ == "__main__":
    asyncio.run(main())
