#!/usr/bin/env python3
"""端到端演示：M1 数据接入 → M2 数据治理 → M3 学生画像 → M4 学业预警 → M5 职业推荐 → M6 数据服务。

用法（先启动 platform_server，建议演示模式）：

    $env:PLATFORM_CAREER_DEMO = "1"
    $env:PLATFORM_ADMIN_PASSWORD = "AdminPassword123!"
    .\\build\\develop-check\\platform_server.exe 8090
    py -3 scripts/e2e_demo.py 8090

可选参数：

    --password      管理员密码（默认读 PLATFORM_ADMIN_PASSWORD，没有则 AdminPassword123!）
    --term          指标学期（默认 2026-fall）
    --skip-career   跳过 M5（服务端没开 PLATFORM_CAREER_DEMO 时用）
    --skip-m6       跳过 M6 数据服务链路

脚本是可重跑的：M1 用固定 idempotency_key，M2/M4 的生成逻辑本身幂等，
M6 会复用已发布的同名服务版本，数据不会无限重复。
失败时会打印失败的步骤、请求方法和路径、HTTP 状态与响应体，退出码为 1。
"""

import argparse
import http.client
import json
import os
import sys
import time

MODULES = "/api/modules"
M1 = MODULES + "/data-ingestion"
M2 = MODULES + "/data-governance"
M3 = MODULES + "/student-profile"
M4 = MODULES + "/academic-alerts"
M5 = MODULES + "/career-recommendation"
M6 = MODULES + "/data-services"

SOURCE_NAME = "端到端演示数据源"
TASK_NAME = "端到端成绩导入"
IMPORT_KEY = "e2e-run-v1"

# xh/cj 是源字段，靠 M1 的字段映射转成 student_id/score；
# attendance/credit 本身就是标准字段，M2 直接聚合。
RECORDS = [
    {"xh": "2023001", "cj": "95", "attendance": 95, "credit": 9},
    {"xh": "2023001", "cj": "92", "attendance": 95, "credit": 9},
    {"xh": "2023002", "cj": "82", "attendance": 90, "credit": 12},
    {"xh": "2023003", "cj": "50", "attendance": 60, "credit": 2},
    {"xh": "2023003", "cj": "55", "attendance": 60, "credit": 2},
]

SERVICE_NAME = "e2e_student_profile"
SERVICE_VERSION = "v1"
# 申请人需要 grants.apply（数据管理员没有该权限），批准人需要 grants.approve。
APPLICANT = "e2e_college"
APPROVER = "e2e_approver"
APPLICANT_ORG = "college-cs"
USER_PASSWORD = "e2e-user-password"

DEMO_HEADERS = {"X-Career-Demo": "1"}


class HttpFailure(RuntimeError):
    def __init__(self, method, path, status, body):
        self.method = method
        self.path = path
        self.status = status
        self.body = body
        super().__init__(f"{method} {path} -> {status}: {body[:200]}")


class Client:
    def __init__(self, port):
        self.port = port
        self.token = None

    def call(self, method, path, body=None, expected=(200,), headers=None):
        payload = None
        request_headers = {"Accept": "application/json"}
        if headers:
            request_headers.update(headers)
        if self.token:
            request_headers["Authorization"] = "Bearer " + self.token
        if body is not None:
            payload = json.dumps(body, ensure_ascii=False).encode("utf-8")
            request_headers["Content-Type"] = "application/json; charset=utf-8"

        connection = http.client.HTTPConnection("127.0.0.1", self.port, timeout=10)
        try:
            connection.request(method, path, body=payload, headers=request_headers)
            response = connection.getresponse()
            text = response.read().decode("utf-8", "replace")
            status = response.status
        finally:
            connection.close()

        if status not in expected:
            raise HttpFailure(method, path, status, text)
        if not text:
            return None
        try:
            return json.loads(text)
        except json.JSONDecodeError:
            return text

    def login(self, username, password):
        data = self.call("POST", M6 + "/login", {"username": username, "password": password})
        self.token = data["access_token"]
        return self.token


class Runner:
    def __init__(self):
        self.steps = []

    def add(self, title, handler):
        self.steps.append((title, handler))

    def run(self):
        total = len(self.steps)
        started_all = time.time()
        for index, (title, handler) in enumerate(self.steps, 1):
            started = time.time()
            print(f"[{index}/{total}] {title} ... ", end="", flush=True)
            try:
                detail = handler()
            except HttpFailure as exc:
                print("FAILED")
                print(f"    请求: {exc.method} {exc.path}")
                print(f"    响应: HTTP {exc.status} {exc.body[:800]}")
                print(f"    失败步骤: 第 {index} 步 / 共 {total} 步 —— {title}")
                return 1
            except Exception as exc:  # noqa: BLE001 - 演示脚本要打印一切失败原因
                print("FAILED")
                print(f"    异常: {exc}")
                print(f"    失败步骤: 第 {index} 步 / 共 {total} 步 —— {title}")
                return 1
            elapsed = time.time() - started
            suffix = f"  {detail}" if detail else ""
            print(f"ok ({elapsed:.2f}s){suffix}")
        print(f"\n全部通过，总耗时 {time.time() - started_all:.2f}s。")
        return 0


def expect(condition, message):
    if not condition:
        raise RuntimeError(message)


def fmt(value):
    return json.dumps(value, ensure_ascii=False)


def main():
    parser = argparse.ArgumentParser(description="高校学生成长平台端到端演示")
    parser.add_argument("port", nargs="?", type=int, default=8090, help="服务端口，默认 8090")
    parser.add_argument("--password", default=os.environ.get("PLATFORM_ADMIN_PASSWORD", "AdminPassword123!"),
                        help="管理员密码，默认读 PLATFORM_ADMIN_PASSWORD")
    parser.add_argument("--term", default="2026-fall", help="指标/画像学期，默认 2026-fall")
    parser.add_argument("--skip-career", action="store_true", help="跳过 M5 职业推荐")
    parser.add_argument("--skip-m6", action="store_true", help="跳过 M6 数据服务链路")
    args = parser.parse_args()

    port = args.port
    term = args.term
    admin = Client(port)
    context = {"port": port, "term": term, "admin": admin}
    runner = Runner()

    # ---------------------------------------------------------------- 0. 健康检查
    def health():
        modules = admin.call("GET", MODULES)
        expect(isinstance(modules, list) and len(modules) == 6,
               f"模块数量应为 6，实际为 {len(modules) if isinstance(modules, list) else modules}")
        status = admin.call("GET", "/api/health")
        expect(status and status.get("status") == "ok", f"健康检查失败：{fmt(status)}")
        return f"6 个模块，health=ok"

    runner.add("检查服务与模块目录", health)

    # ---------------------------------------------------------------- M6 登录
    def login():
        admin.login("admin", args.password)
        me = admin.call("GET", M6 + "/me")
        expect(me.get("username") == "admin", f"登录身份异常：{fmt(me)}")
        return f"admin / {me.get('role')}"

    runner.add("登录管理员（M6 身份）", login)

    # ---------------------------------------------------------------- M1
    def ensure_source():
        for source in admin.call("GET", M1 + "/sources"):
            if source["name"] == SOURCE_NAME:
                context["source_id"] = source["id"]
                return f"复用数据源 id={source['id']}"
        source = admin.call("POST", M1 + "/sources", {
            "name": SOURCE_NAME,
            "kind": "academic",
            "access_mode": "file",
            "owner": "教务处",
            "description": "端到端演示：成绩、出勤、学分",
        }, expected=(201,))
        context["source_id"] = source["id"]
        return f"新建数据源 id={source['id']}"

    runner.add("M1 登记数据源", ensure_source)

    def set_mappings():
        admin.call("PUT", f"{M1}/sources/{context['source_id']}/mappings", [
            {"source_field": "xh", "standard_field": "student_id", "transform_expr": "trim"},
            {"source_field": "cj", "standard_field": "score", "transform_expr": "number"},
        ])
        return "xh→student_id，cj→score"

    runner.add("M1 配置字段映射", set_mappings)

    def ensure_task():
        for task in admin.call("GET", M1 + "/tasks"):
            if task["data_source_id"] == context["source_id"] and task["name"] == TASK_NAME:
                context["task_id"] = task["id"]
                return f"复用导入任务 id={task['id']}"
        task = admin.call("POST", M1 + "/tasks", {
            "data_source_id": context["source_id"],
            "name": TASK_NAME,
            "import_mode": "manual",
            "validation_rules": {
                "required": ["student_id", "score"],
                "ranges": {"score": {"min": 0, "max": 100}},
            },
        }, expected=(201,))
        context["task_id"] = task["id"]
        return f"新建导入任务 id={task['id']}"

    runner.add("M1 配置导入任务", ensure_task)

    def import_records():
        result = admin.call("POST", f"{M1}/tasks/{context['task_id']}/run", {
            "records": RECORDS,
            "idempotency_key": IMPORT_KEY,
        })
        run = result["run"]
        expect(run["success_count"] == len(RECORDS),
               f"导入成功行数应为 {len(RECORDS)}，实际 {run['success_count']}：{fmt(result)}")
        context["run_id"] = run["id"]
        return f"run {run['id']}，成功 {run['success_count']} 行"

    runner.add("M1 导入原始记录（幂等）", import_records)

    # ---------------------------------------------------------------- M2
    def merge_master():
        merged = admin.call("POST", M2 + "/student-master/merge", {
            "student_id": "1003",
            "student_no": "2023003",
            "name": "王五",
            "major": "软件工程",
            "class_name": "软工2301",
            "org_id": "college-cs",
            "source": "e2e-academic",
        })
        expect(merged.get("synced_to_profile") is True,
               f"主数据没有同步到 M3：{fmt(merged)}")
        return "1003/王五 归并完成并同步"

    runner.add("M2 归并学生主数据", merge_master)

    def recalculate():
        result = admin.call("POST", M2 + "/recalculate", {
            "term": term,
            "student_ids": ["1001", "1002", "1003"],
        })
        metrics = {row["student_id"]: row["metrics"] for row in result["results"]}
        low = metrics.get("1003", {})
        expect(abs(low.get("score_avg", 0) - 52.5) < 1e-6,
               f"1003 的平均成绩应为 52.5，实际 {low.get('score_avg')}")
        expect(low.get("failed_courses", 0) >= 2,
               f"1003 的不及格门数应 ≥ 2，实际 {low.get('failed_courses')}")
        expect(abs(low.get("attendance_rate", 0) - 60) < 1e-6,
               f"1003 的出勤率应为 60，实际 {low.get('attendance_rate')}")
        context["task_id_m2"] = result["task_id"]
        return f"任务 {result['task_id']}，{result['metric_count']} 个指标"

    runner.add("M2 按指标口径重算", recalculate)

    # ---------------------------------------------------------------- M3
    def upsert_tag():
        admin.call("POST", M3 + "/tags", {
            "code": "e2e_score_good",
            "name": "成绩良好",
            "dimension": "学业",
            "description": "平均成绩不低于 85",
            "rule": {"metric": "score_avg", "operator": ">=", "value": 85},
        }, expected=(201,))
        return "e2e_score_good（score_avg ≥ 85）"

    runner.add("M3 维护画像标签", upsert_tag)

    def batch_profile():
        result = admin.call("POST", M3 + "/recalculate/batch", {"term": term})
        expect(result["count"] >= 3, f"批量重算学生数应 ≥ 3，实际 {result['count']}")
        return f"学期 {term}，{result['count']} 名学生"

    runner.add("M3 批量生成画像/标签/快照", batch_profile)

    def read_profile():
        profile = admin.call("GET", f"{M3}/students/1001?term={term}")
        metrics = profile.get("metrics") or []
        tags = [tag.get("code") for tag in (profile.get("tags") or [])]
        expect(metrics, "1001 的画像没有指标")
        expect("e2e_score_good" in tags, f"1001 应有 e2e_score_good 标签，实际 {tags}")
        expect(profile.get("snapshot"), "1001 的画像快照缺失")
        return f"指标 {len(metrics)} 项，标签 {tags}"

    runner.add("M3 查询画像（指标/标签/快照）", read_profile)

    # ---------------------------------------------------------------- M4
    def generate_alerts():
        result = admin.call("POST", M4 + "/alerts/generate")
        expect(result["total_alerts"] > 0, f"没有生成任何预警：{fmt(result)}")
        context["generated"] = result["generated"]
        return f"新生成 {result['generated']}，跳过 {result['skipped']}，共 {result['total_alerts']} 条"

    runner.add("M4 按规则生成预警（幂等）", generate_alerts)

    def handle_alert():
        alerts = admin.call("GET", M4 + "/alerts")["alerts"]
        own = [alert for alert in alerts if alert["student_id"] == "1003"]
        expect(own, "1003 没有任何预警")

        opened = next((alert for alert in own if alert["status"] == "open"), None)
        if opened is None:
            # 重跑时预警可能已在上次运行里被关闭，验证历史仍然可追溯即可。
            closed = next((alert for alert in own if alert["status"] in ("closed", "not_applicable")), None)
            expect(closed is not None, f"1003 的预警状态异常：{fmt(own)}")
            history = admin.call("GET", f"{M4}/alerts/{closed['id']}/interventions")
            expect(len(history["records"]) >= 2, "已关闭预警的干预记录不足 2 条")
            return f"复用已处置预警 {closed['id']}（干预记录 {len(history['records'])} 条）"

        alert_id = opened["id"]
        claimed = admin.call("POST", f"{M4}/alerts/{alert_id}/claim", {"assignee": "王老师"})
        expect(claimed["status"] == "claimed", f"认领失败：{fmt(claimed)}")
        closed = admin.call("POST", f"{M4}/alerts/{alert_id}/handle", {
            "action": "close",
            "measures": "每周两次答疑，安排补考",
            "note": "端到端演示",
            "by": "王老师",
        })
        expect(closed["status"] == "closed", f"关闭失败：{fmt(closed)}")
        history = admin.call("GET", f"{M4}/alerts/{alert_id}/interventions")
        expect(len(history["records"]) >= 2, "认领+关闭应留下至少 2 条干预记录")
        return f"预警 {alert_id} 认领并关闭，干预记录 {len(history['records'])} 条"

    runner.add("M4 认领并处置预警", handle_alert)

    def dashboard():
        data = admin.call("GET", M4 + "/dashboard")
        expect(data["student_count"] >= 3, f"看板学生数异常：{fmt(data)}")
        return f"学生 {data['student_count']}，活跃预警 {data['active_alerts']}，超时 {data['overdue_alerts']}"

    runner.add("M4 学业看板", dashboard)

    # ---------------------------------------------------------------- M5
    if not args.skip_career:
        def career_flow():
            mode = admin.call("GET", M5 + "/mode")
            expect(mode.get("demo") is True,
                   "服务端没有开启职业推荐演示模式，请用 PLATFORM_CAREER_DEMO=1 启动，或加 --skip-career")

            previous = None
            try:
                previous = admin.call("GET", M5 + "/strategy", expected=(200, 409))
            except HttpFailure:
                previous = None
            if isinstance(previous, dict) and previous.get("error"):
                previous = None

            job = admin.call("POST", M5 + "/jobs", {
                "title": "端到端演示岗位",
                "industry": "信息技术",
                "job_type": "internship",
                "description": "端到端演示用岗位",
                "requirements": [{"competency_code": "programming", "threshold": 80, "weight": 3}],
            }, headers=DEMO_HEADERS, expected=(201,))
            admin.call("PUT", f"{M5}/jobs/{job['id']}/status", {"status": "published"},
                       headers=DEMO_HEADERS)
            admin.call("PUT", M5 + "/strategy", {
                "algorithm": "pass_and_proximity",
                "min_score": 70,
                "pass_weight": 60,
                "proximity_weight": 40,
                "allowed_industries": ["信息技术"],
                "allowed_job_types": ["internship"],
            }, headers=DEMO_HEADERS)

            student_headers = dict(DEMO_HEADERS, **{"X-Career-Demo-Student": "demo-1"})
            try:
                items = admin.call("POST", M5 + "/recommendations/query", {},
                                   headers=student_headers)["items"]
                expect(items, "演示学生没有任何推荐结果")
                item = items[0]
                reason = admin.call("GET", f"{M5}/recommendations/{item['id']}/reason",
                                    headers=student_headers)
                expect("score" in reason, f"推荐理由缺少 score：{fmt(reason)}")
                feedback = admin.call("PUT", f"{M5}/recommendations/{item['id']}/feedback", {
                    "choice": "interested",
                    "reason": "端到端演示：方向匹配",
                }, headers=student_headers)
                expect(feedback.get("choice") == "interested", f"反馈保存失败：{fmt(feedback)}")
                return f"推荐 {len(items)} 条，理由 score={reason['score']}，反馈已保存"
            finally:
                # 恢复原有策略，并撤下演示岗位，避免污染演示数据。
                if isinstance(previous, dict) and previous.get("algorithm"):
                    admin.call("PUT", M5 + "/strategy", {
                        key: previous[key]
                        for key in ("algorithm", "min_score", "pass_weight", "proximity_weight",
                                    "allowed_industries", "allowed_job_types")
                        if key in previous
                    }, headers=DEMO_HEADERS)
                admin.call("PUT", f"{M5}/jobs/{job['id']}/status", {"status": "withdrawn"},
                           headers=DEMO_HEADERS)

        runner.add("M5 推荐岗位 + 理由 + 反馈", career_flow)

    # ---------------------------------------------------------------- M6
    if not args.skip_m6:
        def ensure_users():
            existing = {user["username"] for user in admin.call("GET", M6 + "/users")}
            accounts = [
                (APPLICANT, {"role": "college_admin", "org_id": APPLICANT_ORG}),
                (APPROVER, {"role": "data_admin"}),
            ]
            created = []
            for username, extra in accounts:
                if username in existing:
                    continue
                payload = {"username": username, "password": USER_PASSWORD}
                payload.update(extra)
                admin.call("POST", M6 + "/users", payload, expected=(201,))
                created.append(username)
            return f"新用户 {created}" if created else "复用已有演示账号"

        runner.add("M6 准备数据管理员账号", ensure_users)

        def ensure_service():
            applicant = Client(port)
            applicant.login(APPLICANT, USER_PASSWORD)
            context["applicant"] = applicant
            context["approver"] = Client(port)
            context["approver"].login(APPROVER, USER_PASSWORD)

            for service in applicant.call("GET", M6 + "/services"):
                if service["name"] == SERVICE_NAME and service["version"] == SERVICE_VERSION:
                    context["service_id"] = service["id"]
                    return f"复用服务 {service['id']}"
            service = applicant.call("POST", M6 + "/services", {
                "name": SERVICE_NAME,
                "provider_id": "student_profile",
                "version": SERVICE_VERSION,
                "fields": ["student_id", "name", "major", "class_name", "org_id"],
                "scope": {"type": "all"},
                "rate_per_minute": 60,
            }, expected=(201,))
            context["service_id"] = service["id"]
            return f"发布服务 {service['id']}（provider=student_profile）"

        runner.add("M6 发布数据服务", ensure_service)

        def apply_and_approve():
            applicant = context["applicant"]
            application = applicant.call("POST", M6 + "/applications", {
                "service_id": context["service_id"],
                "purpose": "端到端演示：读取学生画像基础数据",
                "scope": {"type": "college", "org_id": APPLICANT_ORG},
                "validity_seconds": 3600,
                "quota": 100,
                "rate_per_minute": 30,
            }, expected=(201,))
            context["application_id"] = application["id"]
            grant = context["approver"].call(
                "POST", f"{M6}/applications/{application['id']}/decision",
                {"decision": "approve", "mask_level": "standard",
                 "validity_seconds": 3600, "quota": 100, "rate_per_minute": 30})
            context["grant_id"] = grant["grant_id"]
            return f"申请 {application['id']} 已批准，grant={grant['grant_id']}"

        runner.add("M6 申请授权并批准", apply_and_approve)

        def issue_and_invoke():
            applicant = context["applicant"]
            token = applicant.call(
                "POST", f"{M6}/applications/{context['application_id']}/token")["access_token"]
            expect(token.startswith("svc_"), f"服务令牌格式异常：{token[:20]}")

            service_client = Client(port)
            service_client.token = token
            result = service_client.call(
                "POST", f"{M6}/services/{context['service_id']}/invoke",
                {"parameters": {"student_id": "1001"}})
            records = result.get("records") or []
            expect(len(records) == 1, f"应返回 1 条学生记录，实际 {len(records)}：{fmt(result)[:400]}")
            expect(records[0].get("student_id") == "1001",
                   f"返回的学生不是 1001：{fmt(records[0])[:300]}")

            applicant.call("POST", f"{M6}/applications/{context['application_id']}/revoke")
            revoked = service_client.call(
                "POST", f"{M6}/services/{context['service_id']}/invoke",
                {"parameters": {"student_id": "1001"}}, expected=(401,))
            expect(isinstance(revoked, dict) and revoked.get("error"), "撤销后调用应被拒绝")
            return f"调用成功（记录 {len(records)} 条），撤销后返回 401"

        runner.add("M6 签发令牌、调用服务、撤销授权", issue_and_invoke)

    code = runner.run()
    if code == 0:
        base = f"http://127.0.0.1:{port}"
        print("\n演示入口：")
        print(f"  工作台: {base}/workspace")
        print(f"  画像:   {base}/api/modules/student-profile/students/1001?term={term}")
        print(f"  预警:   {base}/api/modules/academic-alerts/alerts?status=open")
        print(f"  课程数据: 学期 {term}，学生 1001/1002/1003")
    return code


if __name__ == "__main__":
    sys.exit(main())
