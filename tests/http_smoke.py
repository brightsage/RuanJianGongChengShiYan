"""Exercise the actual local HTTP boundary without third-party Python packages."""

import http.client
import json
import os
from pathlib import Path
import socket
import sqlite3
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
                       PLATFORM_DB_PATH=database, PLATFORM_ADMIN_PASSWORD="http-admin-password")
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
        assert status == 401
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


def recommendation_smoke(directory):
    def student(port, method, path, data=None, person="demo-1", expected=200):
        headers = dict(DEMO, **{"X-Career-Demo-Student": person})
        status, body = request(port, method, PREFIX + "/recommendations" + path,
                               json.dumps(data) if data is not None else None, headers)
        assert status == expected, (method, path, status, body)
        return json.loads(body)

    with server(directory, demo=True) as port:
        assert career_api(port, "GET", "/mode") == {"demo": True}
        student(port, "POST", "/query", {}, expected=409)
        job = {
            "title": "后端实习", "industry": "信息技术", "job_type": "internship",
            "description": "样例", "requirements": [
                {"competency_code": "programming", "threshold": 80, "weight": 3},
                {"competency_code": "databases", "threshold": 80, "weight": 1},
            ],
        }

        def create(title, published=True, **overrides):
            saved = career_api(port, "POST", "/jobs", dict(job, title=title, **overrides), 201)
            if published:
                career_api(port, "PUT", f"/jobs/{saved['id']}/status", {"status": "published"})
            return saved["id"]

        job_id = create("后端实习")
        tied_id = create("数据实习")
        create("校招", job_type="campus")
        create("其他行业", industry="制造")
        create("草稿", published=False)
        career_api(port, "PUT", "/strategy", {
            "algorithm": "pass_and_proximity", "min_score": 70, "pass_weight": 60,
            "proximity_weight": 40, "allowed_industries": ["信息技术"],
            "allowed_job_types": ["internship"],
        })
        rows = student(port, "POST", "/query", {})["items"]
        assert [row["job_id"] for row in rows] == [job_id, tied_id]
        assert [row["score"] for row in rows] == [80, 80]
        assert student(port, "POST", "/query", {"min_score": 80})["items"] == rows
        assert student(port, "POST", "/query", {"min_score": 80.01})["items"] == []
        assert student(port, "POST", "/query", {"industry": "制造"})["items"] == []
        assert student(port, "POST", "/query", {"term": "missing"}, expected=409)
        assert student(port, "POST", "/query", {"student_no": "demo-2"}, expected=400)
        rec = f"/{rows[0]['id']}"
        reason = student(port, "GET", rec + "/reason")
        assert reason["score"] == 80 and reason["criteria"][1]["gap"] == 40
        assert reason["criteria"][1]["status"] == "below_threshold"
        other = student(port, "POST", "/query", {}, person="demo-2")["items"][0]
        other_reason = student(port, "GET", f"/{other['id']}/reason", person="demo-2")
        assert other["score"] == 75 and other_reason["criteria"][1]["status"] == "missing"
        assert other_reason["criteria"][1]["student_score"] is None
        student(port, "GET", rec + "/reason", person="demo-2", expected=404)
        initial = {"choice": "interested", "reason": "方向符合"}
        student(port, "PUT", rec + "/feedback", initial, person="demo-2", expected=404)
        student(port, "PUT", rec + "/feedback", dict(initial, reason=" "), expected=400)
        feedback = student(port, "PUT", rec + "/feedback", initial)
        assert student(port, "PUT", rec + "/feedback", initial) == feedback
        feedback = student(port, "PUT", rec + "/feedback",
                           {"choice": "not_interested", "reason": "已确定其他方向"})
        assert student(port, "POST", "/query", {})["items"][0]["feedback"] == feedback
        career_api(port, "PUT", f"/jobs/{job_id}/status", {"status": "withdrawn"})
        assert [row["job_id"] for row in student(port, "POST", "/query", {})["items"]] == [tied_id]
        assert student(port, "GET", rec + "/reason") == dict(reason, feedback=feedback)

    with server(directory, demo=True) as port:
        assert student(port, "GET", rec + "/reason") == dict(reason, feedback=feedback)


def authenticated_career_smoke(directory):
    def call(port, method, path, data=None, token=None, expected=200):
        headers = {"Content-Type": "application/json"}
        if token:
            headers["Authorization"] = "Bearer " + token
        payload = json.dumps(data) if data is not None else None
        status, response = request(port, method, path, payload, headers)
        assert status == expected, (method, path, status, response)
        return json.loads(response)

    m6 = "/api/modules/data-services"
    database = Path(directory, "platform.db")
    with server(directory) as port:
        admin = call(port, "POST", m6 + "/login",
                     {"username": "admin", "password": "http-admin-password"})["access_token"]
        credentials = {"username": "career", "password": "http-user-password"}
        call(port, "POST", m6 + "/users", dict(credentials, role="career_center"), admin, 201)
        career = call(port, "POST", m6 + "/login", credentials)["access_token"]
        call(port, "GET", PREFIX + "/jobs", token=admin, expected=403)
        job = {
            "title": "正式岗位", "industry": "信息技术", "job_type": "campus",
            "description": "审计测试", "requirements": [
                {"competency_code": "programming", "threshold": 80, "weight": 3},
            ],
        }
        created = call(port, "POST", PREFIX + "/jobs", job, career, 201)
        job_path = PREFIX + f"/jobs/{created['id']}"
        published = call(port, "PUT", job_path + "/status", {"status": "published"}, career)
        assert published["status"] == "published"
        call(port, "PUT", job_path, dict(job, requirements=[]), career, 400)
        logs = call(port, "GET", m6 + "/audit-logs?action=career.job.update", token=admin)
        assert len(logs) == 1 and logs[0]["outcome"] == "failure"
        logs = call(port, "GET", m6 + "/audit-logs?action=career.job.publish", token=admin)
        assert len(logs) == 1 and logs[0]["after"] == {"status": "published"}
        with sqlite3.connect(database) as db:
            db.execute("CREATE TRIGGER test_audit_failure BEFORE INSERT ON audit_logs "
                       "WHEN NEW.action = 'career.job.update' "
                       "BEGIN SELECT RAISE(ABORT, 'test failure'); END")
        call(port, "PUT", job_path, dict(job, title="应回滚"), career, 500)
        assert call(port, "GET", PREFIX + "/jobs", token=career) == [published]
        with sqlite3.connect(database) as db:
            db.execute("DROP TRIGGER test_audit_failure")
        updated = call(port, "PUT", job_path, dict(job, title="新岗位"), career)
        logs = call(port, "GET", m6 + "/audit-logs?action=career.job.update", token=admin)
        assert logs[0]["before"] == {"title": published["title"]}
        assert logs[0]["after"] == {"title": updated["title"]}


def main():
    with tempfile.TemporaryDirectory() as directory:
        career_smoke(directory)
        platform_smoke(directory)
        authenticated_career_smoke(directory)
    with tempfile.TemporaryDirectory() as directory:
        recommendation_smoke(directory)


if __name__ == "__main__":
    main()
