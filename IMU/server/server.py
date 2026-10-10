
import argparse
import json
import os
import queue
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import serial
from serial.tools import list_ports

HOST = os.getenv("IMU_HOST", "127.0.0.1")
PORT = int(os.getenv("IMU_PORT", "8000"))
BAUD = 115200

# USB vendor IDs of the USB-serial chips found on ESP32 dev boards:
# Silicon Labs CP210x, WCH CH340/CH910x, FTDI, Espressif native USB.
ESP_USB_VIDS = {0x10C4, 0x1A86, 0x0403, 0x303A}

INDEX_HTML = os.path.join(os.path.dirname(os.path.abspath(__file__)), "index.html")

clients = set()
clients_lock = threading.Lock()
last_status = "Starting..."


def broadcast(event, data):
    message = sse(event, data)
    with clients_lock:
        for q in clients:
            try:
                q.put_nowait(message)
            except queue.Full:
                pass  # slow browser tab: drop samples rather than fall behind


def sse(event, data):
    return f"event: {event}\ndata: {data}\n\n".encode()


def set_status(text):
    global last_status
    last_status = text
    print(text)
    broadcast("status", json.dumps(text))


def find_port():
    for port in list_ports.comports():
        if port.vid in ESP_USB_VIDS:
            return port.device
    return None


def open_serial(port_name):
    ser = serial.Serial()
    ser.port = port_name
    ser.baudrate = BAUD
    ser.timeout = 1
    # Keep DTR/RTS low so opening the port doesn't hold the ESP32 in reset.
    ser.dtr = False
    ser.rts = False
    ser.open()
    return ser


def read_serial(port_name):
    """Forward lines from the ESP32 to all connected browsers, reconnecting forever."""
    waiting = False
    while True:
        name = port_name or find_port()
        if name is None:
            if not waiting:
                set_status("No ESP32 serial port found. Waiting for one to appear...")
                waiting = True
            time.sleep(1)
            continue
        waiting = False

        try:
            with open_serial(name) as ser:
                set_status(f"Reading from {name}")
                ser.readline()  # discard the first line, we likely joined mid-line
                while True:
                    line = ser.readline().decode("utf-8", errors="replace").strip()
                    if not line:
                        continue
                    if line.startswith("{"):
                        try:
                            json.loads(line)
                        except ValueError:
                            continue  # partial line, e.g. right after connecting
                        broadcast("imu", line)
                    else:
                        # Status messages from the firmware and ESP32 boot output.
                        set_status(f"ESP32: {line.lstrip('# ')}")
        except serial.SerialException as e:
            set_status(f"Serial error on {name}: {e}")
            time.sleep(1)


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/":
            self.send_index()
        elif self.path == "/events":
            self.send_events()
        else:
            self.send_error(404)

    def send_index(self):
        with open(INDEX_HTML, "rb") as f:
            body = f.read()
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def send_events(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.end_headers()

        q = queue.Queue(maxsize=200)
        with clients_lock:
            clients.add(q)
        try:
            self.wfile.write(sse("status", json.dumps(last_status)))
            self.wfile.flush()
            while True:
                try:
                    self.wfile.write(q.get(timeout=5))
                except queue.Empty:
                    self.wfile.write(b": keepalive\n\n")
                self.wfile.flush()
        except OSError:
            pass  # browser tab closed
        finally:
            with clients_lock:
                clients.discard(q)

    def log_message(self, format, *args):
        pass


def main():
    parser = argparse.ArgumentParser(description="Serve ESP32 IMU data on localhost.")
    parser.add_argument("--port", help="serial port of the ESP32, e.g. COM5 (default: auto-detect)")
    args = parser.parse_args()

    threading.Thread(target=read_serial, args=(args.port,), daemon=True).start()

    server = ThreadingHTTPServer((HOST, PORT), Handler)
    server.daemon_threads = True
    print(f"Open http://localhost:{PORT} to view IMU data")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
