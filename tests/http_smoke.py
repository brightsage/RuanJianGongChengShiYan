"""Exercise the actual local HTTP boundary without third-party Python packages."""

import http.client
import json
import socket
import subprocess
import sys
import time


def request(port, method, path, body=None, headers=None):
    connection = http.client.HTTPConnection("127.0.0.1", port, timeout=2)
    try:
        connection.request(method, path, body=body, headers=headers or {})
        response = connection.getresponse()
        return response.status, response.read().decode("utf-8")
    finally:
        connection.close()


def main():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        port = sock.getsockname()[1]
    process = subprocess.Popen(
        [sys.argv[1], str(port)], stdout=subprocess.DEVNULL, stderr=subprocess.PIPE
    )
    try:
        for _ in range(40):
            try:
                status, body = request(port, "GET", "/api/health")
                break
            except (ConnectionRefusedError, ConnectionResetError, TimeoutError):
                if process.poll() is not None:
                    raise RuntimeError(process.stderr.read().decode())
                time.sleep(0.05)
        else:
            raise RuntimeError("server did not start")
        assert status == 200 and json.loads(body) == {"status": "ok"}
        status, body = request(port, "GET", "/api/modules")
        assert status == 200
        modules = json.loads(body)
        assert len(modules) == 6 and all(module["routes"] == [] for module in modules)
        status, body = request(port, "GET", "/")
        assert status == 200 and "接口测试台" in body
        status, body = request(port, "GET", "/api/modules/student-profile/123")
        assert status == 404
        status, body = request(port, "POST", "/api/health", body="hello")
        assert status == 404
        with socket.create_connection(("127.0.0.1", port), timeout=2) as sock:
            sock.sendall(
                b"POST /api/modules HTTP/1.1\r\nHost: localhost\r\n"
                b"Content-Length: 1048577\r\n\r\n"
            )
            assert b"HTTP/1.1 413" in sock.recv(512)
    finally:
        process.terminate()
        try:
            process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()


if __name__ == "__main__":
    main()
