"""向已经启动的 platform_server 写入一组演示数据。

用法：
    py -3 scripts/seed_demo.py 8090
"""

import http.client
import json
import sys


def request(port, method, path, body=None):
    headers = {}
    if body is not None:
        body = json.dumps(body, ensure_ascii=False).encode("utf-8")
        headers["Content-Type"] = "application/json; charset=utf-8"
    connection = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
    try:
        connection.request(method, path, body=body, headers=headers)
        response = connection.getresponse()
        text = response.read().decode("utf-8")
        if response.status >= 400:
            raise RuntimeError(f"{method} {path} -> {response.status}: {text}")
        return json.loads(text) if text else None
    finally:
        connection.close()


def ensure_source(port):
    source = request(
        port,
        "POST",
        "/api/modules/data-ingestion/sources",
        {
            "name": "教务成绩样例",
            "kind": "academic",
            "access_mode": "file",
            "owner": "教务处",
            "description": "演示数据：每学期成绩单",
        },
    )
    return source["id"]


def find_source(port):
    try:
        return ensure_source(port)
    except RuntimeError:
        for source in request(port, "GET", "/api/modules/data-ingestion/sources"):
            if source["name"] == "教务成绩样例":
                return source["id"]
        raise


def ensure_task(port, source_id):
    tasks = request(port, "GET", "/api/modules/data-ingestion/tasks")
    for task in tasks:
        if task["data_source_id"] == source_id and task["name"] == "成绩导入":
            return task["id"]
    task = request(
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
    return task["id"]


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8090
    source_id = find_source(port)
    request(
        port,
        "PUT",
        f"/api/modules/data-ingestion/sources/{source_id}/mappings",
        [
            {"source_field": "xh", "standard_field": "student_id", "transform_expr": "trim"},
            {"source_field": "cj", "standard_field": "score", "transform_expr": "number"},
        ],
    )
    task_id = ensure_task(port, source_id)
    request(
        port,
        "POST",
        f"/api/modules/data-ingestion/tasks/{task_id}/run",
        {
            "records": [
                {"xh": "2023001", "cj": "95"},
                {"xh": "2023002", "cj": "82"},
                {"xh": "", "cj": "50"},
            ],
            "idempotency_key": "seed-run-v1",
        },
    )

    for student_id, name, score, hours in (
        ("2023001", "张三", 95, 40),
        ("2023002", "李四", 82, 60),
    ):
        request(
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
        for code, value, unit, source in (
            ("comprehensive_score", score, "分", "academic"),
            ("practice_hours", hours, "小时", "practice"),
        ):
            request(
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

    request(
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
    request(
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
    request(
        port,
        "POST",
        "/api/modules/student-profile/recalculate/batch",
        {"term": "2026-fall"},
    )

    base = f"http://127.0.0.1:{port}/api/modules/student-profile/students"
    print("演示数据已写入。")
    print(f"  画像：{base}/2023001?term=2026-fall")
    print(f"  报告：{base}/2023001/report?term=2026-fall")
    print(f"  对比：{base}/2023001/compare?peer=2023002&term=2026-fall")


if __name__ == "__main__":
    main()
