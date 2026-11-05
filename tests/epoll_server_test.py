"""Exercise the actual TCP event loop, including slow readers and half-close."""
import concurrent.futures
import os
import socket
import struct
import subprocess
import threading
import time
import tempfile
from pathlib import Path


def receive(sock, expected):
    result = bytearray()
    while len(result) < len(expected):
        block = sock.recv(65536)
        assert block, "connection closed before all bytes were echoed"
        result.extend(block)
    assert result == expected, "echo bytes differ"


with socket.socket() as reservation:
    reservation.bind(("127.0.0.1", 0))
    port = reservation.getsockname()[1]

binary = Path(__file__).resolve().parents[1] / "build" / "epoll_server"
# Receive diagnostics can exceed pipe capacity during the large-payload test.
server_output = tempfile.TemporaryFile()
process = subprocess.Popen([str(binary), str(port)], stdout=server_output,
                           stderr=subprocess.PIPE, text=True)


def connect():
    return socket.create_connection(("127.0.0.1", port), timeout=10)


def round_trip(index):
    with connect() as sock:
        chunks = [b"hello", bytes([index, 0, 255]), os.urandom(20000)]
        for chunk in chunks:
            sock.sendall(chunk)
            receive(sock, chunk)
        sock.shutdown(socket.SHUT_WR)
        assert sock.recv(1) == b"", "half-closed connection was not drained"


try:
    deadline = time.monotonic() + 5
    while b"listening" not in os.pread(server_output.fileno(), 4096, 0):
        assert process.poll() is None, "server failed to start"
        assert time.monotonic() < deadline, "server startup timed out"
        time.sleep(0.01)
    with concurrent.futures.ThreadPoolExecutor(max_workers=12) as pool:
        list(pool.map(round_trip, range(24)))

    # Delay reads until the server's send buffer fills. A separate sender avoids
    # deadlocking the test itself when TCP applies backpressure in both directions.
    payload = os.urandom(8 * 1024 * 1024)
    with connect() as slow:
        errors = []

        def send_large():
            try:
                slow.sendall(payload)
                slow.shutdown(socket.SHUT_WR)
            except Exception as error:
                errors.append(error)

        sender = threading.Thread(target=send_large)
        sender.start()
        try:
            time.sleep(0.2)
            round_trip(42)  # A slow reader must not block unrelated connections.
            receive(slow, payload)
            assert slow.recv(1) == b""
        finally:
            sender.join(timeout=12)
        assert not sender.is_alive(), "sender did not finish"
        assert not errors, errors

    # A reset must not terminate the process (SIGPIPE) or break later clients.
    with connect() as reset:
        reset.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
        reset.sendall(b"reset peer" * 1000)
    round_trip(43)
    process.terminate()
    assert process.wait(timeout=3) == 0
    print("PASS: concurrent clients, fragmented/binary data, slow reader, half-close, reset, shutdown")
finally:
    if process.poll() is None:
        process.kill()
    process.wait()
    error_output = process.stderr.read()
    server_output.close()
    process.stderr.close()
    if error_output:
        print(error_output, end="")
