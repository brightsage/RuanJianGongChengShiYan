"""Exercise the actual local HTTP boundary without third-party Python packages."""

import http.client
import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time


def request(port, method, path, body=None, headers=None):
    request_headers = dict(headers or {})
    if body is not None and not isinstance(body, (bytes, bytearray)):
        if isinstance(body, (dict, list)):
            body = json.dumps(body, ensure_ascii=False).encode("utf-8")
            request_headers.setdefault("Content-Type", "application/json; charset=utf-8")
        else:
            body = body.encode("utf-8")
    connection = http.client.HTTPConnection("127.0.0.1", port, timeout=3)
    try:
        connection.request(method, path, body=body, headers=request_headers)
        response = connection.getresponse()
        return response.status, response.read().decode("utf-8")
    finally:
        connection.close()


def check(status, expected, body, label):
    assert status == expected, f"{label}: expected {expected}, got {status}: {body}"


def main():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        port = sock.getsockname()[1]

    workdir = tempfile.mkdtemp(prefix="student_platform_smoke_")
    env = os.environ.copy()
    env["PLATFORM_DB_PATH"] = os.path.join(workdir, "platform.db")
    process = subprocess.Popen(
        [sys.argv[1], str(port)],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        env=env,
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
        check(status, 200, body, "module catalog")
        modules = json.loads(body)
        by_id = {module["id"]: module for module in modules}
        assert len(modules) == 6
        assert by_id["data-ingestion"]["routes"]
        assert by_id["student-profile"]["routes"]
        assert all(
            not module["routes"]
            for module_id, module in by_id.items()
            if module_id not in ("data-ingestion", "student-profile")
        )

        status, body = request(port, "GET", "/")
        assert status == 200 and "接口测试台" in body
        status, body = request(port, "GET", "/api/modules/student-profile/unknown")
        check(status, 404, body, "unknown profile")
        status, body = request(port, "POST", "/api/health", body="hello")
        check(status, 404, body, "wrong method")

        status, body = request(
            port,
            "POST",
            "/api/modules/data-ingestion/sources",
            {
                "name": "教务成绩样例",
                "kind": "academic",
                "access_mode": "file",
                "owner": "教务处",
                "description": "HTTP 冒烟测试",
            },
        )
        check(status, 201, body, "create data source")
        source_id = json.loads(body)["id"]

        status, body = request(
            port,
            "PUT",
            f"/api/modules/data-ingestion/sources/{source_id}/mappings",
            [
                {"source_field": "xh", "standard_field": "student_id", "transform_expr": "trim"},
                {"source_field": "cj", "standard_field": "score", "transform_expr": "number"},
            ],
        )
        check(status, 200, body, "replace mappings")
        assert len(json.loads(body)) == 2

        status, body = request(
            port,
            "POST",
            "/api/modules/data-ingestion/tasks",
            {
                "data_source_id": source_id,
                "name": "成绩导入",
                "import_mode": "manual",
                "validation_rules": {
                    "required": ["student_id", "score"],
                    "ranges": {"score": {"min": 0, "max": 100}},
                },
            },
        )
        check(status, 201, body, "create task")
        task_id = json.loads(body)["id"]

        run_body = {
            "records": [
                {"xh": " 2023001 ", "cj": "92"},
                {"xh": "", "cj": "50"},
            ],
            "idempotency_key": "smoke-run-1",
        }
        status, body = request(
            port,
            "POST",
            f"/api/modules/data-ingestion/tasks/{task_id}/run",
            run_body,
        )
        check(status, 200, body, "execute task")
        execution = json.loads(body)
        assert execution["run"]["status"] == "partial"
        assert execution["run"]["total_count"] == 2
        assert execution["run"]["success_count"] == 1
        assert execution["run"]["failure_count"] == 1
        assert len(execution["failures"]) == 1
        run_id = execution["run"]["id"]

        status, body = request(
            port,
            "POST",
            f"/api/modules/data-ingestion/tasks/{task_id}/run",
            run_body,
        )
        check(status, 200, body, "idempotent run")
        assert json.loads(body)["run"]["id"] == run_id

        status, body = request(
            port,
            "GET",
            f"/api/modules/data-ingestion/runs/{run_id}/records?failed=1",
        )
        check(status, 200, body, "failed records")
        assert len(json.loads(body)) == 1

        status, body = request(
            port,
            "POST",
            f"/api/modules/data-ingestion/runs/{run_id}/retry",
            {
                "records": [{"xh": "2023003", "cj": "88"}],
                "idempotency_key": "smoke-retry-1",
            },
        )
        check(status, 200, body, "retry failed records")
        retry = json.loads(body)
        assert retry["run"]["status"] == "success"
        assert retry["run"]["total_count"] == 1
        retry_id = retry["run"]["id"]
        status, body = request(
            port,
            "POST",
            f"/api/modules/data-ingestion/runs/{run_id}/retry",
            {
                "records": [{"xh": "2023003", "cj": "88"}],
                "idempotency_key": "smoke-retry-1",
            },
        )
        check(status, 200, body, "idempotent retry")
        assert json.loads(body)["run"]["id"] == retry_id

        status, body = request(
            port,
            "POST",
            "/api/modules/data-ingestion/push",
            {"source_id": source_id, "records": [{"xh": "2023002", "cj": "88"}]},
        )
        check(status, 200, body, "push records")
        assert json.loads(body)["run"]["status"] == "success"

        for student_id, name, score, hours in (
            ("2023001", "张三", 95, 40),
            ("2023002", "李四", 82, 60),
        ):
            status, body = request(
                port,
                "POST",
                "/api/modules/student-profile/students",
                {
                    "student_id": student_id,
                    "name": name,
                    "major": "软件工程",
                    "class_name": "软件2301",
                    "grade": 2023,
                },
            )
            check(status, 201, body, f"sync student {student_id}")
            for code, value, unit, source in (
                ("comprehensive_score", score, "分", "academic"),
                ("practice_hours", hours, "小时", "practice"),
            ):
                status, body = request(
                    port,
                    "POST",
                    "/api/modules/student-profile/metrics",
                    {
                        "student_id": student_id,
                        "metric_code": code,
                        "term": "2026-fall",
                        "value": value,
                        "unit": unit,
                        "source": source,
                    },
                )
                check(status, 201, body, f"sync metric {student_id}/{code}")

        status, body = request(
            port,
            "POST",
            "/api/modules/student-profile/tags",
            {
                "code": "score_excellent",
                "name": "成绩优秀",
                "dimension": "学业",
                "description": "综合成绩不低于 90",
                "rule": {"metric": "comprehensive_score", "operator": ">=", "value": 90},
            },
        )
        check(status, 201, body, "upsert tag score")
        status, body = request(
            port,
            "POST",
            "/api/modules/student-profile/tags",
            {
                "code": "practice_active",
                "name": "实践积极",
                "dimension": "实践",
                "description": "实践时长不少于 50 小时",
                "rule": {"metric": "practice_hours", "operator": ">=", "value": 50},
            },
        )
        check(status, 201, body, "upsert tag practice")

        status, body = request(
            port,
            "POST",
            "/api/modules/student-profile/recalculate/batch",
            {"term": "2026-fall"},
        )
        check(status, 200, body, "batch recalculate")
        assert json.loads(body)["count"] == 2

        status, body = request(
            port,
            "GET",
            "/api/modules/student-profile/students/2023001?term=2026-fall",
        )
        check(status, 200, body, "get profile")
        profile = json.loads(body)
        assert profile["student"]["name"] == "张三"
        assert len(profile["metrics"]) == 2
        assert len(profile["tags"]) == 1
        assert profile["tags"][0]["code"] == "score_excellent"
        assert profile["snapshot"] is not None

        status, body = request(
            port,
            "GET",
            "/api/modules/student-profile/students/2023001/tags/score_excellent/evidence",
        )
        check(status, 200, body, "tag evidence")
        assert json.loads(body)["evidence"]["metric"] == "comprehensive_score"

        status, body = request(
            port,
            "GET",
            "/api/modules/student-profile/students/2023001/compare?peer=2023002&term=2026-fall",
        )
        check(status, 200, body, "compare profiles")
        comparison = json.loads(body)
        score = next(
            item
            for item in comparison["metrics"]
            if item["metric_code"] == "comprehensive_score"
        )
        assert score["delta"] == 13

        status, body = request(
            port,
            "GET",
            "/api/modules/student-profile/students/2023001/report?term=2026-fall",
        )
        check(status, 200, body, "profile report")
        assert "张三" in body and "成绩优秀" in body

        status, body = request(
            port,
            "POST",
            "/api/modules/data-ingestion/sources",
            {
                "name": "教务成绩样例",
                "kind": "academic",
                "access_mode": "file",
                "owner": "教务处",
            },
        )
        check(status, 409, body, "duplicate data source")

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
        shutil.rmtree(workdir, ignore_errors=True)


if __name__ == "__main__":
    main()
