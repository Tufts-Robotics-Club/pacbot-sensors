# one terminal for everything: shows the robots imu data, anything else it sends us,
# and whatever you type gets sent to the robot
# usage: .venv/bin/python console.py   (stop server.py first since they use the same port)

import asyncio
import json
import sys
import threading

from websockets.asyncio.server import serve

PORT = 8765

robot = None        # the robots connection once it connects
latest_imu = None   # newest imu reading
imu_count = 0       # how many imu messages came in this second


async def handle_robot(ws):
    global robot, latest_imu, imu_count
    robot = ws
    print(f"robot connected from {ws.remote_address[0]}")
    try:
        async for msg in ws:
            if msg.startswith("{"):
                latest_imu = msg
                imu_count += 1
            else:
                print(f"robot: {msg}")
    except Exception as e:
        print(f"connection error: {e}")
    finally:
        if robot is ws:
            robot = None
        print("robot disconnected, waiting for it to come back...")


# once a second instead of 50 times a second so you can still read it and type
async def show_imu():
    global imu_count
    while True:
        await asyncio.sleep(1)
        if robot is None or latest_imu is None:
            continue
        d = json.loads(latest_imu)
        a, g = d["a"], d["g"]
        print(f"imu {imu_count}/s   accel {a[0]:7.1f} {a[1]:7.1f} {a[2]:7.1f} mg   "
              f"gyro {g[0]:6.2f} {g[1]:6.2f} {g[2]:6.2f} deg/s")
        imu_count = 0


async def send_typed_lines():
    # reading the keyboard blocks so it gets its own thread that hands lines to us
    loop = asyncio.get_running_loop()
    lines = asyncio.Queue()

    def read_keyboard():
        for line in sys.stdin:
            loop.call_soon_threadsafe(lines.put_nowait, line.strip())

    threading.Thread(target=read_keyboard, daemon=True).start()

    while True:
        line = await lines.get()
        if not line:
            continue
        if robot is None:
            print("no robot connected yet")
            continue
        await robot.send(line)
        print(f"sent: {line}")


async def main():
    async with serve(handle_robot, "0.0.0.0", PORT):
        print(f"waiting for the robot on port {PORT}. type a message and hit enter to send it")
        await asyncio.gather(show_imu(), send_typed_lines())


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
