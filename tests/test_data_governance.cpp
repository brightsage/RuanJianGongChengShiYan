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

const char* kRules = "/api/modules/data-governance/quality-rules";
const char* kMaster = "/api/modules/data-governance/student-master/";
const char* kMerge = "/api/modules/data-governance/student-master/merge";
const char* kRecalc = "/api/modules/data-governance/recalculate";
const char* kReport = "/api/modules/data-governance/quality-report";

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

bool has_metric(const platform::Json& metrics, const std::string& code, double expected) {
    for (const auto& metric : metrics.as_array()) {
        if (metric.at("metric_code").as_string() != code) continue;
        return std::fabs(metric.at("value").as_double() - expected) < 1e-9;
    }
    return false;
}

}  // namespace

int main() {
    try {
        platform::AppOptions options;
        options.bootstrap_password = "test-admin-password";
        auto app = platform::make_app(options);

        Client anon{app, ""};
        Client admin{app, ""};

        require(anon.call("GET", kRules).status == 401, "governance requires login");
        admin.token = login(admin, "admin", options.bootstrap_password);

        // ---- 质量规则：种子数据 + 真实落库 ----
        auto rules = admin.ok("GET", kRules);
        require(rules.is_array(), "quality rules should be an array");
        require(rules.as_array().size() >= 3, "quality rules seed data");
        require(rules.at(std::size_t{0}).at("rule_name").as_string() == "学号非空校验",
                "quality rule name");

        const auto created = admin.ok(
            "POST", kRules,
            R"({"rule_name":"成绩非空校验","rule_type":"not_null","target_field":"score","level":"error"})",
            201);
        require(created.at("rule_id").as_int64() > 0, "created rule id");
        require(created.at("rule_name").as_string() == "成绩非空校验", "created rule name");
        require(created.at("enabled").as_bool(), "created rule enabled");
        const auto rule_id = created.at("rule_id").as_int64();

        require(admin
                    .call("POST", kRules,
                          R"({"rule_name":"成绩非空校验","rule_type":"not_null","target_field":"score"})")
                    .status == 409,
                "duplicate rule name should conflict");
        require(admin.call("POST", kRules, R"({"rule_type":"not_null"})").status == 400,
                "missing rule_name should be rejected");
        require(admin
                    .call("POST", kRules,
                          R"({"rule_name":"等级非法","rule_type":"range","target_field":"score","level":"fatal"})")
                    .status == 400,
                "invalid level should be rejected");

        rules = admin.ok("GET", kRules);
        bool found = false;
        for (const auto& rule : rules.as_array()) {
            if (rule.at("rule_id").as_int64() == rule_id) found = true;
        }
        require(found, "created rule must be persisted");

        // ---- 规则启停（PATCH）----
        const std::string rule_path = std::string(kRules) + "/" + std::to_string(rule_id);
        const auto disabled = admin.ok("PATCH", rule_path, R"({"enabled":false})");
        require(!disabled.at("enabled").as_bool(), "quality rule should be disabled");
        require(admin.call("PATCH", rule_path, R"({"level":"fatal"})").status == 400,
                "invalid rule level should be rejected");
        require(admin.call("PATCH", std::string(kRules) + "/999999", R"({"enabled":false})").status ==
                    404,
                "unknown quality rule should be 404");

        // ---- 学生主数据列表 ----
        const auto masters = admin.ok("GET", "/api/modules/data-governance/student-master");
        require(masters.is_array() && masters.as_array().size() >= 3, "student master list");
        bool listed = false;
        for (const auto& row : masters.as_array()) {
            if (row.at("student_id").as_string() == "1001") listed = true;
        }
        require(listed, "student master list should contain seeded 1001");

        // ---- 学生主数据：查询 + 归并 ----
        const auto master = admin.ok("GET", std::string(kMaster) + "1001");
        require(master.at("student_no").as_string() == "2023001", "student master number");
        require(master.at("name").as_string() == "张三", "student master name");

        const auto missing = admin.call("GET", std::string(kMaster) + "9999");
        require(missing.status == 404, "unknown student master should be 404");
        require(platform::Json::parse(missing.body).at("error").is_string(),
                "error response should contain error");

        const auto merged = admin.ok(
            "POST", kMerge,
            R"({"student_id":"2001","student_no":"2024001","name":"赵六","major":"软件工程","class_name":"软工2401","org_id":"college-cs","source":"academic-office"})");
        require(merged.at("name").as_string() == "赵六", "merged master name");
        require(merged.at("synced_to_profile").as_bool(), "merged master should sync to M3");

        // 低质量来源只带空值，不应覆盖已有主数据。
        const auto merged_again = admin.ok("POST", kMerge, R"({"student_id":"2001","major":"","name":""})");
        require(merged_again.at("name").as_string() == "赵六", "empty value must not overwrite name");
        require(merged_again.at("major").as_string() == "软件工程",
                "empty value must not overwrite major");

        const auto profile_2001 = admin.ok(
            "GET", "/api/modules/student-profile/students/2001");
        require(profile_2001.at("student").at("name").as_string() == "赵六",
                "M2 master must be visible in M3 profile");

        // ---- 通过 M1 写入原始记录，供 M2 计算 ----
        const auto source = admin.ok(
            "POST", "/api/modules/data-ingestion/sources",
            R"({"name":"教务系统-治理测试","kind":"academic","access_mode":"push","owner":"governance"})",
            201);
        const auto source_id = source.at("id").as_int64();

        // 源字段用 xh/cj，M2 需要按 M1 的字段映射还原成 student_id/score。
        admin.ok(
            "PUT", "/api/modules/data-ingestion/sources/" + std::to_string(source_id) + "/mappings",
            R"([{"source_field":"xh","standard_field":"student_id","transform_expr":"trim"},
                {"source_field":"cj","standard_field":"score","transform_expr":"number"}])");

        const auto task = admin.ok(
            "POST", "/api/modules/data-ingestion/tasks",
            "{\"data_source_id\":" + std::to_string(source_id) +
                ",\"name\":\"成绩导入-治理测试\",\"import_mode\":\"manual\"}",
            201);
        const auto task_id = task.at("id").as_int64();

        const auto run = admin.ok(
            "POST", "/api/modules/data-ingestion/tasks/" + std::to_string(task_id) + "/run",
            R"({"records":[
                {"xh":" 2023001 ","cj":"88","attendance":95,"credit":3},
                {"xh":"2023001","cj":"92","attendance":100,"credit":2}
            ]})");
        require(run.at("run").at("success_count").as_int64() == 2, "raw records imported");

        // ---- 指标重算：真实计算并写入 M3 ----
        const auto recalc = admin.ok(
            "POST", kRecalc, R"({"term":"2025-2026-1","student_ids":["1001"]})");
        require(recalc.at("task_id").as_string().rfind("recalc_", 0) == 0, "recalculate task id");
        require(recalc.at("status").as_string() == "finished", "recalculate finished");
        require(recalc.at("term").as_string() == "2025-2026-1", "recalculate term");
        require(recalc.at("student_count").as_int64() == 1, "recalculate student count");
        require(recalc.at("metric_count").as_int64() >= 4, "recalculate metric count");

        const auto metrics = recalc.at("results").as_array().at(0).at("metrics");
        require(std::fabs(metrics.at("score_avg").as_double() - 90.0) < 1e-9,
                "average score must be computed");
        require(std::fabs(metrics.at("score_pass_rate").as_double() - 100.0) < 1e-9,
                "pass rate must be computed");
        require(std::fabs(metrics.at("attendance_rate").as_double() - 97.5) < 1e-9,
                "attendance rate must be computed");
        require(std::fabs(metrics.at("credit_total").as_double() - 5.0) < 1e-9,
                "credit total must be computed");

        const auto profile_1001 = admin.ok(
            "GET", "/api/modules/student-profile/students/1001?term=2025-2026-1");
        require(has_metric(profile_1001.at("metrics"), "score_avg", 90.0),
                "recalculated metric must land in sp_metric_value");

        // 空 body 走默认口径 + 全部主数据学生。
        const auto recalc_all = admin.ok("POST", kRecalc, "");
        require(recalc_all.at("student_count").as_int64() >= 1, "default recalculate students");
        require(recalc_all.at("metric_count").as_int64() >= 1, "default recalculate metrics");

        require(admin.call("POST", kRecalc, R"({"student_ids":["nobody"]})").status == 404,
                "unknown student should be 404");

        // ---- 质量报告：真实统计 ----
        const auto report = admin.ok("GET", kReport);
        require(report.at("total_records").as_int64() == 2, "quality report total");
        require(report.at("cleaned_records").as_int64() == 2, "quality report cleaned");
        require(report.at("dirty_records").as_int64() == 0, "quality report dirty");
        require(std::fabs(report.at("quality_score").as_double() - 100.0) < 1e-9,
                "quality score");
        require(report.at("main_data_coverage").as_double() > 0.0, "main data coverage");
        require(report.at("master_students").as_int64() >= 4, "master student count");

        // ---- 权限：学生角色不得访问治理接口 ----
        const auto user = admin.ok(
            "POST", "/api/modules/data-services/users",
            R"({"username":"govstudent","password":"test-user-password","role":"student","org_id":"college-cs","class_id":"软工2301","student_no":"2023001"})",
            201);
        (void)user;
        Client student{app, ""};
        student.token = login(student, "govstudent", "test-user-password");
        require(student.call("GET", kRules).status == 403, "student must not read governance");
        require(student.call("POST", kRecalc, "{}").status == 403,
                "student must not recalculate governance");
        require(student.call("PATCH", rule_path, R"({"enabled":true})").status == 403,
                "student must not update governance rules");
    } catch (const std::exception& error) {
        std::cerr << "data governance tests failed: " << error.what() << '\n';
        return 1;
    }
    std::cout << "data governance tests passed\n";
    return 0;
}
