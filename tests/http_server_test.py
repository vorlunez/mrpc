"""Exercise HTTP over actual TCP, including segmentation and persistent streams."""
import concurrent.futures
import http.client
import json
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import time

root = Path(__file__).resolve().parents[1]
with socket.socket() as reservation:
    reservation.bind(('127.0.0.1', 0))
    port = reservation.getsockname()[1]
log = tempfile.TemporaryFile()
server = subprocess.Popen([str(root / 'build/epoll_server'), str(port), '--http'],
                          stdout=log, stderr=subprocess.PIPE)


def connect():
    return socket.create_connection(('127.0.0.1', port), timeout=5)


class Reader:
    def __init__(self, sock):
        self.sock = sock
        self.data = b''

    def response(self, head=False):
        while b'\r\n\r\n' not in self.data:
            block = self.sock.recv(65536)
            assert block, 'EOF in response headers'
            self.data += block
        header, self.data = self.data.split(b'\r\n\r\n', 1)
        lines = header.decode('ascii').split('\r\n')
        status = int(lines[0].split()[1])
        fields = dict((k.lower(), v.strip()) for k, v in (line.split(':', 1) for line in lines[1:]))
        length = 0 if head else int(fields.get('content-length', 0))
        while len(self.data) < length:
            block = self.sock.recv(65536)
            assert block, 'EOF in response body'
            self.data += block
        body, self.data = self.data[:length], self.data[length:]
        return status, fields, body


def exchange(wire, expected):
    with connect() as sock:
        sock.sendall(wire)
        status, fields, _ = Reader(sock).response()
        assert status == expected, status
        assert fields['connection'] == 'close'
        assert sock.recv(1) == b''


try:
    deadline = time.monotonic() + 5
    while b'listening' not in os.pread(log.fileno(), 4096, 0):
        assert server.poll() is None
        assert time.monotonic() < deadline
        time.sleep(0.01)

    # Use the standard HTTP client as an independent protocol consumer.
    conn = http.client.HTTPConnection('127.0.0.1', port, timeout=5)
    conn.request('GET', '/hello?q=1')
    response = conn.getresponse()
    assert response.status == 200
    assert json.loads(response.read()) == {'method': 'GET', 'target': '/hello?q=1', 'body': ''}
    payload = '你好，mrpc'.encode()
    conn.request('POST', '/echo', body=payload, headers={'Content-Type': 'text/plain'})
    response = conn.getresponse()
    assert json.loads(response.read())['body'] == payload.decode()
    conn.request('HEAD', '/hello')
    response = conn.getresponse()
    assert response.status == 200 and int(response.getheader('Content-Length')) > 0
    assert response.read() == b''
    conn.close()

    # Split both headers and body. No response is allowed before the complete body.
    with connect() as sock:
        sock.sendall(b'POST /split HTTP/1.1\r\nHo')
        time.sleep(0.02)
        sock.sendall(b'st: localhost\r\nContent-Length: 5\r\n\r\nhe')
        sock.settimeout(0.1)
        try:
            sock.recv(1)
            raise AssertionError('response arrived before complete request')
        except socket.timeout:
            pass
        sock.settimeout(5)
        sock.sendall(b'llo')
        assert json.loads(Reader(sock).response()[2])['body'] == 'hello'

    # Chunked body with trailer, followed immediately by another request.
    with connect() as sock:
        sock.sendall(b'POST /chunks HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n'
                     b'2\r\nhi\r\n3\r\n123\r\n0\r\nX-End: yes\r\n\r\n'
                     b'GET /next HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n')
        reader = Reader(sock)
        assert json.loads(reader.response()[2])['body'] == 'hi123'
        assert json.loads(reader.response()[2])['target'] == '/next'
        assert sock.recv(1) == b''

    # Large replies plus pipelining must make progress with a slow reader.
    with connect() as sock:
        body = b'x' * 200000
        sock.sendall(b'POST /large HTTP/1.1\r\nHost: localhost\r\nContent-Length: 200000\r\n\r\n' + body +
                     b'GET /after HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n')
        time.sleep(0.05)
        reader = Reader(sock)
        assert json.loads(reader.response()[2])['body'].encode() == body
        assert json.loads(reader.response()[2])['target'] == '/after'
        assert sock.recv(1) == b''

    exchange(b'GET / HTTP/1.1\r\n\r\n', 400)
    exchange(b'GET / HTTP/1.0\r\nHost: localhost\r\n\r\n', 505)
    exchange(b'POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 1\r\nContent-Length: 2\r\n\r\nxx', 400)
    exchange(b'POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 1048577\r\n\r\n', 413)
    exchange(b'GET / HTTP/1.1\r\nHost: localhost\r\nX: ' + b'x' * 17000 + b'\r\n\r\n', 431)
    exchange(b'POST / HTTP/1.1\r\nHost: localhost\r\nExpect: 100-continue\r\nContent-Length: 5\r\n\r\n', 417)
    with connect() as sock:
        sock.sendall(b'POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 8\r\n\r\nx')
        sock.shutdown(socket.SHUT_WR)
        assert Reader(sock).response()[0] == 400
        assert sock.recv(1) == b''

    def independent_client(index):
        connection = http.client.HTTPConnection('127.0.0.1', port, timeout=5)
        try:
            connection.request('GET', '/' + str(index))
            response = connection.getresponse()
            assert response.status == 200
            assert json.loads(response.read())['target'] == '/' + str(index)
        finally:
            connection.close()

    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
        list(pool.map(independent_client, range(24)))
    server.terminate()
    assert server.wait(timeout=3) == 0
    print('PASS: HTTP GET/POST/HEAD, keep-alive, segmentation, chunked, pipelining, large response, errors, concurrency')
finally:
    if server.poll() is None:
        server.kill()
    server.wait()
    errors = server.stderr.read()
    server.stderr.close()
    log.close()
    if errors:
        print(errors.decode(), end='')
