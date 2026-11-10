"""Exercise the actual local HTTP boundary without third-party Python packages."""

import http.client
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time
from contextlib import contextmanager


def request(port, method, path, body=None, headers=None):
    connection = http.client.HTTPConnection("127.0.0.1", port, timeout=2)
    try:
        connection.request(method, path, body=body, headers=headers or {})
        response = connection.getresponse()
        return response.status, response.read().decode("utf-8")
    finally:
        connection.close()


@contextmanager
def server(directory, demo=False, database="platform.db"):
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        port = sock.getsockname()[1]
    environment = dict(os.environ, PLATFORM_CAREER_DEMO="1" if demo else "0",
                       PLATFORM_DB_PATH=database)
    process = subprocess.Popen(
        [str(Path(sys.argv[1]).resolve()), str(port)], cwd=directory, env=environment,
        stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
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
        yield port
    finally:
        process.terminate()
        try:
            process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
        process.stderr.close()


PREFIX = "/api/modules/career-recommendation"
DEMO = {"X-Career-Demo": "1", "Content-Type": "application/json"}


def platform_smoke(directory):
    with server(directory) as port:
        status, body = request(port, "GET", "/api/modules")
        assert status == 200
        modules = json.loads(body)
        assert len(modules) == 6
        career = next(module for module in modules if module["id"] == "career-recommendation")
        assert {
            ("GET", PREFIX + "/jobs"), ("POST", PREFIX + "/jobs"),
            ("PUT", PREFIX + "/jobs/:id"), ("PUT", PREFIX + "/jobs/:id/status"),
            ("GET", PREFIX + "/strategy"), ("PUT", PREFIX + "/strategy"),
        } <= {(route["method"], route["path"]) for route in career["routes"]}
        status, body = request(port, "GET", "/")
        assert status == 200 and "接口测试台" in body and 'href="/career"' in body
        status, body = request(port, "GET", "/career")
        assert status == 200 and "就业指导中心" in body
        status, _ = request(port, "GET", "/career.js")
        assert status == 200
        status, _ = request(port, "GET", PREFIX + "/jobs", headers=DEMO)
        assert status == 503
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


def career_api(port, method, path, data=None, expected=200):
    body = json.dumps(data) if data is not None else None
    status, response = request(port, method, PREFIX + path, body, DEMO)
    assert status == expected, (method, path, expected, status, response)
    return json.loads(response)


def career_smoke(directory):
    demo_db = Path(directory, "data", "career-demo.db")
    assert not demo_db.exists()
    with server(directory, demo=True, database="data/career-demo.db") as port:
        career_api(port, "GET", "/jobs", expected=503)
    assert not demo_db.exists()

    with server(directory, demo=True) as port:
        assert request(port, "GET", PREFIX + "/jobs")[0] == 503
        assert request(port, "GET", PREFIX + "/jobs",
                       headers=dict(DEMO, Host="example.test"))[0] == 503
        assert not demo_db.exists()
        assert career_api(port, "GET", "/jobs") == []
        career_api(port, "GET", "/strategy", expected=409)
        assert request(port, "POST", PREFIX + "/jobs", "{", DEMO)[0] == 400
        requirement = {"competency_code": "programming", "threshold": 80, "weight": 3}
        job = {
            "title": "C++ 开发实习生", "industry": "信息技术", "job_type": "internship",
            "description": "开发和测试", "requirements": [
                requirement, {"competency_code": "databases", "threshold": 75, "weight": 1},
            ],
        }
        career_api(port, "POST", "/jobs", dict(job, requirements=[]), expected=400)
        career_api(port, "POST", "/jobs",
                   dict(job, requirements=[requirement, requirement]), expected=400)
        career_api(port, "POST", "/jobs",
                   dict(job, requirements=[dict(requirement, threshold=0)]), expected=400)
        assert career_api(port, "GET", "/jobs") == []
        draft = career_api(port, "POST", "/jobs", job, expected=201)
        assert draft == dict(job, id=1, version=1, status="draft")
        job_path = f"/jobs/{draft['id']}"
        status_path = job_path + "/status"
        career_api(port, "PUT", status_path, {"status": "withdrawn"}, expected=409)
        career_api(port, "PUT", "/jobs/0", job, expected=400)
        career_api(port, "PUT", "/jobs/123", job, expected=404)
        published = career_api(port, "PUT", status_path, {"status": "published"})
        assert published == dict(draft, status="published", version=2)
        assert career_api(port, "PUT", status_path, {"status": "published"}) == published
        career_api(port, "PUT", job_path,
                   dict(job, requirements=[dict(requirement, weight=-1)]), expected=400)
        assert career_api(port, "GET", "/jobs") == [published]
        revised = dict(job, title="C++ 工程实习生", requirements=[requirement])
        assert career_api(port, "PUT", job_path, revised) == dict(
            revised, id=draft["id"], version=3, status="published")
        assert career_api(port, "PUT", job_path, revised)["version"] == 3
        assert career_api(port, "PUT", status_path, {"status": "withdrawn"})["status"] == "withdrawn"
        republished = career_api(port, "PUT", status_path, {"status": "published"})
        assert republished == dict(revised, id=draft["id"], version=5, status="published")
        campus = career_api(port, "POST", "/jobs", dict(job, job_type="campus"), expected=201)
        assert campus["status"] == "draft" and campus["job_type"] == "campus"

        strategy = {
            "algorithm": "pass_and_proximity", "min_score": 50,
            "pass_weight": 60, "proximity_weight": 40,
            "allowed_industries": ["A", "B"], "allowed_job_types": ["internship"],
        }
        active = career_api(port, "PUT", "/strategy", strategy)
        assert active == dict(strategy, version=1)
        assert career_api(port, "PUT", "/strategy",
                          dict(strategy, allowed_industries=["B", "A"])) == active
        career_api(port, "PUT", "/strategy",
                   dict(strategy, pass_weight=0, proximity_weight=0), expected=400)
        career_api(port, "PUT", "/strategy", dict(strategy, min_score=101), expected=400)
        assert career_api(port, "GET", "/strategy") == active
        active = career_api(port, "PUT", "/strategy", dict(
            strategy, algorithm="weighted_pass", pass_weight=100, proximity_weight=0))
        assert active["algorithm"] == "weighted_pass" and active["version"] == 2

    with server(directory, demo=True) as port:
        assert career_api(port, "GET", "/jobs") == [campus, republished]
        assert career_api(port, "GET", "/strategy") == active
    assert demo_db.is_file() and not Path(directory, "platform.db").exists()


def main():
    with tempfile.TemporaryDirectory() as directory:
        platform_smoke(directory)
        career_smoke(directory)


if __name__ == "__main__":
    main()
