"""Real HTTP workflow, disk persistence and credential storage checks."""
import http.client
import json
import os
import socket
import sqlite3
import subprocess
import sys
import tempfile
import time


def request(port, method, path, body=None, token=None, expected=200):
    connection = http.client.HTTPConnection("127.0.0.1", port, timeout=3)
    headers = {"Content-Type": "application/json"}
    if token:
        headers["Authorization"] = "Bearer " + token
    try:
        connection.request(method, path, body=None if body is None else json.dumps(body), headers=headers)
        response = connection.getresponse()
        payload = response.read().decode()
        assert response.status == expected, (method, path, response.status, payload)
        return json.loads(payload) if "application/json" in response.getheader("Content-Type", "") else payload
    finally:
        connection.close()


class Server:
    def __init__(self, database):
        self.database = database

    def __enter__(self):
        with socket.socket() as sock:
            sock.bind(("127.0.0.1", 0))
            self.port = sock.getsockname()[1]
        self.process = subprocess.Popen([sys.argv[1], str(self.port)], stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
            env={**os.environ, "PLATFORM_DB_PATH": self.database, "PLATFORM_CAREER_DEMO": "0",
                 "PLATFORM_ADMIN_PASSWORD": "http-admin-password"})
        for _ in range(60):
            try:
                request(self.port, "GET", "/api/health")
                return self
            except (ConnectionRefusedError, ConnectionResetError, TimeoutError):
                if self.process.poll() is not None:
                    raise RuntimeError(self.process.stderr.read().decode())
                time.sleep(0.05)
        self.__exit__(None, None, None)
        raise RuntimeError("server startup timed out")

    def __exit__(self, *_):
        self.process.terminate()
        try:
            self.process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait()

    def call(self, method, path, body=None, token=None, expected=200):
        return request(self.port, method, "/api/modules/data-services" + path, body, token, expected)


def main():
    with tempfile.TemporaryDirectory(prefix="platform-services-") as directory:
        database = os.path.join(directory, "platform.sqlite3")
        with Server(database) as server:
            for path, marker in [("/data-services", "用户管理"), ("/data-services.js", "Authorization"), ("/data-services.css", "hidden")]:
                assert marker in request(server.port, "GET", path)
            server.call("GET", "/users", expected=401)
            admin = server.call("POST", "/login", {"username": "admin", "password": "http-admin-password"})["access_token"]
            for username, role in [("data", "data_admin"), ("client", "third_party")]:
                server.call("POST", "/users", {"username": username, "password": "http-user-password", "role": role, "org_id": "college-a"}, admin, 201)
            data = server.call("POST", "/login", {"username": "data", "password": "http-user-password"})["access_token"]
            client = server.call("POST", "/login", {"username": "client", "password": "http-user-password"})["access_token"]
            service = server.call("POST", "/services", {"name": "Profile", "provider_id": "profiles", "version": "v1", "fields": ["student_no"], "scope": {"type": "all"}, "rate_per_minute": 60}, data, 201)["id"]
            application = server.call("POST", "/applications", {"service_id": service, "purpose": "HTTP test", "scope": {"type": "college", "org_id": "college-a"}, "validity_seconds": 600, "quota": 10, "rate_per_minute": 2}, client, 201)["id"]
            server.call("POST", f"/applications/{application}/decision", {"decision": "approve", "validity_seconds": 600, "quota": 10, "rate_per_minute": 2, "mask_level": "standard"}, data)
            service_token = server.call("POST", f"/applications/{application}/token", token=client)["access_token"]
            result = server.call("POST", f"/services/{service}/invoke", {}, service_token, 503)
            assert result["error"] == "provider_unavailable"
            logs = server.call("GET", "/call-logs?status=503&limit=1", token=admin)
            assert len(logs) == 1 and logs[0]["service_id"] == service
            audit = server.call("GET", "/audit-logs?action=grant.approve", token=admin)
            assert len(audit) == 1
        with sqlite3.connect(database) as db:
            assert db.execute("SELECT count(*) FROM services").fetchone()[0] == 1
            password_hash, salt = db.execute("SELECT password_hash,salt FROM users WHERE username='admin'").fetchone()
            assert password_hash != "http-admin-password" and len(salt) >= 32
            session_hash = db.execute("SELECT token_hash FROM sessions LIMIT 1").fetchone()[0]
            assert session_hash not in [admin, data, client]
            assert db.execute("SELECT token_hash FROM grants").fetchone()[0] != service_token
        if os.name != "nt":
            assert os.stat(database).st_mode & 0o077 == 0, "database must be private to the server account"
        with Server(database) as server:
            assert server.call("GET", "/me", token=admin)["role"] == "system_admin"
            assert server.call("GET", "/applications", token=client)[0]["status"] == "approved"
            server.call("POST", f"/services/{service}/invoke", {}, service_token, 503)
            server.call("POST", f"/applications/{application}/revoke", token=client)
            server.call("POST", f"/services/{service}/invoke", {}, service_token, 401)
            server.call("POST", "/logout", token=client)
            server.call("GET", "/me", token=client, expected=401)


if __name__ == "__main__":
    main()
