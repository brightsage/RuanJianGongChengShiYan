#include "platform/app.hpp"
#include "platform/json.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

const char* kAlerts = "/api/modules/academic-alerts";

struct Client {
    platform::App& app;
    std::string token;

    platform::Response call(const std::string& method, const std::string& raw_path,
                            const std::string& body = "") const {
        platform::Request request;
        request.method = method;
        const auto mark = raw_path.find('?');
        request.path = raw_path.substr(0, mark);
        request.query = mark == std::string::npos ? std::string{} : raw_path.substr(mark + 1);
        request.body = body;
        if (!token.empty()) request.headers["authorization"] = "Bearer " + token;
        return app.handle(request);
    }

    platform::Json ok(const std::string& method, const std::string& path,
                      const std::string& body = "", int expected = 200) const {
        const auto response = call(method, path, body);
        require(response.status == expected,
                method + " " + path + " expected " + std::to_string(expected) + " got " +
                    std::to_string(response.status) + ": " + response.body);
        return platform::Json::parse(response.body);
    }
};

std::string login(Client& client, const std::string& username, const std::string& password) {
    platform::Json body = platform::Json::object();
    body["username"] = username;
    body["password"] = password;
    return client.ok("POST", "/api/modules/data-services/login", body.dump())
        .at("access_token")
        .as_string();
}

void create_user(Client& admin, const std::string& username, const std::string& role,
                 const std::string& extra) {
    admin.ok("POST", "/api/modules/data-services/users",
             "{\"username\":\"" + username + "\",\"password\":\"test-user-password\",\"role\":\"" +
                 role + "\"" + extra + "}",
             201);
}

void sync_student(Client& admin, const std::string& id, const std::string& name,
                  const std::string& class_name, const std::string& org_id) {
    admin.ok("POST", "/api/modules/student-profile/students",
             "{\"student_id\":\"" + id + "\",\"name\":\"" + name +
                 "\",\"major\":\"计算机科学与技术\",\"class_name\":\"" + class_name +
                 "\",\"grade\":\"2021\",\"org_id\":\"" + org_id + "\"}",
             201);
}

void sync_metric(Client& admin, const std::string& id, const std::string& code,
                 const std::string& term, double value) {
    admin.ok("POST", "/api/modules/student-profile/metrics",
             "{\"student_id\":\"" + id + "\",\"metric_code\":\"" + code + "\",\"term\":\"" +
                 term + "\",\"value\":" + std::to_string(value) + "}",
             201);
}

/// 在预警列表里按学生找一条 open 预警，返回 id（找不到返回 0）。
std::int64_t find_open_alert(const platform::Json& alerts, const std::string& student_id) {
    for (const auto& alert : alerts.at("alerts").as_array()) {
        if (alert.at("student_id").as_string() == student_id &&
            alert.at("status").as_string() == "open")
            return alert.at("id").as_int64();
    }
    return 0;
}

}  // namespace

int main() {
    try {
        platform::AppOptions options;
        options.bootstrap_password = "test-admin-password";
        auto app = platform::make_app(options);

        Client anon{app, ""};
        Client admin{app, ""};

        require(anon.call("GET", std::string(kAlerts) + "/dashboard").status == 401,
                "academic alerts require login");
        admin.token = login(admin, "admin", options.bootstrap_password);

        // ---- 准备学生与指标（模拟 M2 产出写进 sp_student / sp_metric_value）----
        sync_student(admin, "2021001", "张伟", "计科2101", "college-cs");
        sync_student(admin, "2021002", "李娜", "计科2101", "college-cs");

        sync_metric(admin, "2021001", "score_avg", "2024-2025-1", 61.2);
        sync_metric(admin, "2021001", "score_avg", "2025-2026-1", 58.4);
        sync_metric(admin, "2021001", "failed_courses", "2025-2026-1", 3);
        sync_metric(admin, "2021001", "attendance_rate", "2025-2026-1", 72);
        sync_metric(admin, "2021001", "credit_rate", "2025-2026-1", 0.52);

        sync_metric(admin, "2021002", "score_avg", "2025-2026-1", 76.9);
        sync_metric(admin, "2021002", "failed_courses", "2025-2026-1", 1);
        sync_metric(admin, "2021002", "attendance_rate", "2025-2026-1", 93);
        sync_metric(admin, "2021002", "credit_rate", "2025-2026-1", 0.86);

        // ---- UC-AA-01/02 看板 ----
        auto dashboard = admin.ok("GET", std::string(kAlerts) + "/dashboard");
        require(dashboard.at("student_count").as_int64() == 2, "dashboard student count");
        require(std::fabs(dashboard.at("avg_score").as_double() - 67.65) < 1e-6,
                "dashboard avg score from M2 metrics");
        require(dashboard.at("failing_students").as_int64() == 2, "dashboard failing students");
        require(dashboard.at("active_alerts").as_int64() == 0, "dashboard has no alerts yet");
        require(dashboard.at("distribution").at("计科2101").as_int64() == 2,
                "dashboard distribution by class");

        // ---- UC-AA-03 规则 ----
        auto rules = admin.ok("GET", std::string(kAlerts) + "/rules");
        require(rules.at("total").as_int64() >= 4, "seed rules count");
        require(rules.at("rules").at(std::size_t{0}).at("enabled").as_bool(),
                "seed rule enabled");

        require(admin
                    .call("POST", std::string(kAlerts) + "/rules",
                          R"({"name":"非法规则","metric":"gpa","operator":"lt","threshold":3})")
                    .status == 400,
                "invalid metric should be 400");
        const auto created = admin.ok(
            "POST", std::string(kAlerts) + "/rules",
            R"({"name":"极低成绩","metric":"avg_score","operator":"lt","threshold":50,"level":"red","notify":"辅导员"})",
            201);
        require(created.at("created").as_bool(), "create rule flag");
        require(created.at("id").as_string().rfind("R", 0) == 0, "rule id prefix");

        // ---- 规则启停（PATCH）----
        const std::string rule_path =
            std::string(kAlerts) + "/rules/" + created.at("id").as_string();
        const auto patched = admin.ok("PATCH", rule_path, R"({"enabled":false})");
        require(!patched.at("enabled").as_bool(), "alert rule should be disabled");
        require(admin.call("PATCH", rule_path, R"({"level":"fatal"})").status == 400,
                "invalid alert rule level should be rejected");
        require(admin.call("PATCH", std::string(kAlerts) + "/rules/R999", R"({"enabled":false})").status ==
                    404,
                "unknown alert rule should be 404");

        // ---- UC-AA-04 生成（幂等）----
        const auto first = admin.ok("POST", std::string(kAlerts) + "/alerts/generate");
        require(first.at("generated").as_int64() >= 4, "generated alerts from M2 metrics");
        require(first.at("total_alerts").as_int64() >= 4, "total alerts");

        const auto second = admin.ok("POST", std::string(kAlerts) + "/alerts/generate");
        require(second.at("generated").as_int64() == 0, "generate should be idempotent");
        require(second.at("skipped").as_int64() >= 4, "skipped active alerts");

        auto alerts = admin.ok("GET", std::string(kAlerts) + "/alerts?status=open");
        require(alerts.at("total").as_int64() >= 4, "open alerts total");
        const auto& items = alerts.at("alerts").as_array();
        require(items.at(0).at("student_name").as_string().size() > 0,
                "alert should carry student name");
        require(items.at(0).at("class_name").as_string() == "计科2101",
                "alert should carry class name");

        // 三个等级都应该出现：R1 red、R2/R4 orange、R3 yellow
        bool red = false, orange = false, yellow = false;
        for (const auto& alert : items) {
            const auto level = alert.at("level").as_string();
            red = red || level == "red";
            orange = orange || level == "orange";
            yellow = yellow || level == "yellow";
        }
        require(red && orange && yellow, "alerts should cover red/orange/yellow levels");

        // ---- UC-AA-05 认领与处置 ----
        const auto alert_id = find_open_alert(alerts, "2021001");
        require(alert_id > 0, "student 2021001 should have an open alert");
        const std::string alert_path = std::string(kAlerts) + "/alerts/" + std::to_string(alert_id);

        auto claimed = admin.ok("POST", alert_path + "/claim", R"({"assignee":"王老师"})");
        require(claimed.at("status").as_string() == "claimed", "claimed state");
        require(claimed.at("assignee").as_string() == "王老师", "assignee persisted");

        require(admin.call("POST", alert_path + "/claim", R"({"assignee":"李老师"})").status == 409,
                "cross-user claim should be 409");
        require(admin.call("POST", alert_path + "/handle", R"({"action":"close","by":"王老师"})").status ==
                    400,
                "close without measures should be 400");
        require(admin
                    .call("POST", alert_path + "/handle",
                          R"({"action":"not_applicable","by":"王老师"})")
                    .status == 400,
                "not_applicable without reason should be 400");

        const auto closed = admin.ok(
            "POST", alert_path + "/handle",
            R"({"action":"close","measures":"每周两次答疑","note":"已联系家长","by":"王老师"})");
        require(closed.at("status").as_string() == "closed", "closed state");
        require(admin.call("POST", alert_path + "/handle",
                           R"({"action":"follow","by":"王老师"})")
                    .status == 409,
                "closed alert should reject changes");

        const auto history = admin.ok("GET", alert_path + "/interventions");
        require(history.at("records").as_array().size() >= 2, "claim + close records");
        require(history.at("status").as_string() == "closed", "history status");

        // ---- 持久化：重建 app（同一测试库）后数据仍在 ----
        auto app2 = platform::make_app(options);
        Client admin2{app2, ""};
        admin2.token = login(admin2, "admin", options.bootstrap_password);
        const auto history2 = admin2.ok("GET", alert_path + "/interventions");
        require(history2.at("records").as_array().size() >= 2, "interventions persisted in MySQL");
        require(admin2.ok("GET", std::string(kAlerts) + "/alerts?status=closed")
                    .at("total")
                    .as_int64() >= 1,
                "closed alert persisted");

        // ---- UC-AA-07 趋势 ----
        const auto trend = admin.ok("GET", std::string(kAlerts) + "/students/2021001/trend");
        require(trend.at("terms").as_array().size() == 2, "trend terms from M2 metrics");
        require(trend.at("terms").at(std::size_t{1}).at("term").as_string() == "2025-2026-1",
                "trend latest term");
        require(admin.call("GET", std::string(kAlerts) + "/students/9999/trend").status == 404,
                "unknown student trend should be 404");

        // ---- 权限与数据范围 ----
        create_user(admin, "counselor1", "counselor",
                    R"(,"org_id":"college-cs","class_id":"计科2101")");
        create_user(admin, "college1", "college_admin", R"(,"org_id":"college-cs")");
        create_user(admin, "dataadmin1", "data_admin", "");
        create_user(admin, "plainstudent", "student",
                    R"(,"org_id":"college-cs","class_id":"计科2101","student_no":"2021001")");

        Client counselor{app, ""};
        counselor.token = login(counselor, "counselor1", "test-user-password");
        Client college{app, ""};
        college.token = login(college, "college1", "test-user-password");
        Client data_admin{app, ""};
        data_admin.token = login(data_admin, "dataadmin1", "test-user-password");
        Client student{app, ""};
        student.token = login(student, "plainstudent", "test-user-password");

        // 学生没有 alert.read
        require(student.call("GET", std::string(kAlerts) + "/dashboard").status == 403,
                "student must not read academic alerts");
        // 辅导员不能改全局规则
        require(counselor
                    .call("POST", std::string(kAlerts) + "/rules",
                          R"({"name":"越权规则","metric":"avg_score","operator":"lt","threshold":40})")
                    .status == 403,
                "counselor must not create global rules");
        require(counselor.call("PATCH", std::string(kAlerts) + "/rules/R1", R"({"enabled":false})")
                    .status == 403,
                "counselor must not update global rules");

        // 加一个外班学生并生成预警
        sync_student(admin, "2021003", "王强", "计科2102", "college-cs");
        sync_metric(admin, "2021003", "score_avg", "2025-2026-1", 50);
        const auto third = admin.ok("POST", std::string(kAlerts) + "/alerts/generate");
        require(third.at("generated").as_int64() >= 1, "cross-class alert generated");

        // 辅导员只看本班
        const auto counselor_dashboard = counselor.ok("GET", std::string(kAlerts) + "/dashboard");
        require(counselor_dashboard.at("student_count").as_int64() == 2,
                "counselor sees own class only");
        const auto counselor_alerts = counselor.ok("GET", std::string(kAlerts) + "/alerts");
        for (const auto& alert : counselor_alerts.at("alerts").as_array())
            require(alert.at("class_name").as_string() == "计科2101",
                    "counselor must not see other classes");

        // 辅导员不能处置外班预警
        const auto all_alerts = admin.ok("GET", std::string(kAlerts) + "/alerts");
        const auto foreign_alert = find_open_alert(all_alerts, "2021003");
        require(foreign_alert > 0, "foreign alert exists");
        require(counselor
                    .call("POST",
                          std::string(kAlerts) + "/alerts/" + std::to_string(foreign_alert) +
                              "/claim",
                          R"({"assignee":"赵辅导员"})")
                    .status == 403,
                "counselor must not claim other class alerts");
        // 但能认领本班预警
        const auto own_alert = find_open_alert(counselor_alerts, "2021001");
        require(own_alert > 0, "own class alert exists");
        require(counselor
                    .call("POST",
                          std::string(kAlerts) + "/alerts/" + std::to_string(own_alert) + "/claim",
                          R"({"assignee":"赵辅导员"})")
                    .status == 200,
                "counselor can claim own class alerts");

        // 院系管理员看本学院（college-cs 全部 3 人）
        const auto college_dashboard = college.ok("GET", std::string(kAlerts) + "/dashboard");
        require(college_dashboard.at("student_count").as_int64() == 3,
                "college admin sees own college");

        // 数据管理员看全校
        const auto data_dashboard = data_admin.ok("GET", std::string(kAlerts) + "/dashboard");
        require(data_dashboard.at("student_count").as_int64() == 3,
                "data admin sees all students");
    } catch (const std::exception& error) {
        std::cerr << "academic alerts tests failed: " << error.what() << '\n';
        return 1;
    }
    std::cout << "academic alerts tests passed\n";
    return 0;
}
