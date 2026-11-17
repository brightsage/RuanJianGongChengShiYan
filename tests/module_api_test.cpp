#include "platform/app.hpp"
#include "platform/json.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>

namespace {

void require(bool condition, const std::string& message = "assertion failed") {
    if (!condition) throw std::runtime_error(message);
}

std::string make_temp_db_path() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return (std::filesystem::temp_directory_path() /
            ("student_platform_api_test_" + std::to_string(stamp) + ".db"))
        .string();
}

void set_database_path(const std::string& path) {
#ifdef _WIN32
    _putenv_s("PLATFORM_DB_PATH", path.c_str());
#else
    setenv("PLATFORM_DB_PATH", path.c_str(), 1);
#endif
}

void remove_database_files(const std::string& path) {
    std::error_code error;
    std::filesystem::remove(path, error);
    std::filesystem::remove(path + "-wal", error);
    std::filesystem::remove(path + "-shm", error);
}

platform::Response call(platform::App& app, const std::string& method,
                        const std::string& path, const std::string& body = {},
                        const std::string& query = {}) {
    platform::Request request;
    request.method = method;
    request.path = path;
    request.body = body;
    request.query = query;
    if (!body.empty()) request.headers["content-type"] = "application/json";
    return app.handle(request);
}

platform::Json body_of(const platform::Response& response) {
    return platform::Json::parse(response.body);
}

std::int64_t find_module_route_count(const platform::Json& catalog, const std::string& id) {
    for (const auto& module : catalog.as_array()) {
        if (module.at("id").as_string() == id) {
            return static_cast<std::int64_t>(module.at("routes").as_array().size());
        }
    }
    return 0;
}

}  // namespace

int main() {
    const std::string path = make_temp_db_path();
    set_database_path(path);
    try {
        {
            auto app = platform::make_app();

            auto response = call(app, "GET", "/api/modules");
            require(response.status == 200, "module catalog status");
            const auto catalog = body_of(response);
            require(find_module_route_count(catalog, "data-ingestion") > 0,
                    "data-ingestion routes should be registered");
            require(find_module_route_count(catalog, "student-profile") > 0,
                    "student-profile routes should be registered");

            response = call(app, "POST", "/api/modules/data-ingestion/sources",
                            R"({"name":"教务成绩样例","kind":"academic","access_mode":"file",
                                "owner":"教务处","description":"接口测试"})");
            require(response.status == 201, "create data source status");
            const auto source = body_of(response);
            const auto source_id = source.at("id").as_int64();
            require(source.at("name").as_string() == "教务成绩样例", "data source name");

            const std::string source_path =
                "/api/modules/data-ingestion/sources/" + std::to_string(source_id);
            response = call(app, "PUT", source_path + "/mappings",
                            R"([{"source_field":"xh","standard_field":"student_id",
                                 "transform_expr":"trim"},
                                {"source_field":"cj","standard_field":"score",
                                 "transform_expr":"number"}])");
            require(response.status == 200, "replace mappings status");
            require(body_of(response).as_array().size() == 2, "mapping count");

            response = call(app, "POST", "/api/modules/data-ingestion/tasks",
                            R"({"data_source_id":)" + std::to_string(source_id) +
                                R"(,"name":"成绩导入","import_mode":"manual",
                                "validation_rules":{"required":["student_id","score"],
                                "ranges":{"score":{"min":0,"max":100}}}})");
            require(response.status == 201, "create import task status");
            const auto task = body_of(response);
            const auto task_id = task.at("id").as_int64();

            response = call(app, "POST",
                            "/api/modules/data-ingestion/tasks/" + std::to_string(task_id) +
                                "/run",
                            R"({"records":[{"xh":" 2023001 ","cj":"92"},
                                           {"xh":"","cj":"50"}]})");
            require(response.status == 200, "execute import status");
            auto execution = body_of(response);
            require(execution.at("run").at("status").as_string() == "partial",
                    "import run should be partial");
            require(execution.at("run").at("total_count").as_int64() == 2, "import total");
            require(execution.at("run").at("success_count").as_int64() == 1, "import success");
            require(execution.at("run").at("failure_count").as_int64() == 1, "import failure");
            require(execution.at("failures").as_array().size() == 1, "failure details");
            const auto run_id = execution.at("run").at("id").as_int64();

            response = call(app, "GET",
                            "/api/modules/data-ingestion/runs/" + std::to_string(run_id) +
                                "/records",
                            {}, "failed=1");
            require(response.status == 200, "failed records status");
            require(body_of(response).as_array().size() == 1, "failed records count");

            response = call(app, "GET",
                            "/api/modules/data-ingestion/tasks/" + std::to_string(task_id));
            require(response.status == 200, "task detail status");
            require(body_of(response).at("status").as_string() == "partial",
                    "task status should follow run");

            response = call(app, "POST", "/api/modules/data-ingestion/push",
                            R"({"source_id":)" + std::to_string(source_id) +
                                R"(,"records":[{"xh":"2023002","cj":"88"}]})");
            require(response.status == 200, "push records status");
            require(body_of(response).at("run").at("status").as_string() == "success",
                    "push run status");

            response = call(app, "POST",
                            "/api/modules/data-ingestion/runs/" + std::to_string(run_id) +
                                "/retry",
                            R"({"records":[{"xh":"2023003","cj":"88"}],
                                "idempotency_key":"retry-1"})");
            require(response.status == 200, "retry import status");
            const auto retry = body_of(response);
            require(retry.at("run").at("status").as_string() == "success",
                    "retry run should succeed");
            require(retry.at("run").at("total_count").as_int64() == 1, "retry total");
            require(retry.at("run").at("success_count").as_int64() == 1, "retry success");
            const auto retry_id = retry.at("run").at("id").as_int64();

            response = call(app, "POST",
                            "/api/modules/data-ingestion/runs/" + std::to_string(run_id) +
                                "/retry",
                            R"({"records":[{"xh":"2023003","cj":"88"}],
                                "idempotency_key":"retry-1"})");
            require(response.status == 200, "retry idempotent status");
            require(body_of(response).at("run").at("id").as_int64() == retry_id,
                    "retry should reuse run");

            const std::string run_body =
                R"({"records":[{"xh":"2023004","cj":"90"}],
                    "idempotency_key":"run-1"})";
            response = call(app, "POST",
                            "/api/modules/data-ingestion/tasks/" + std::to_string(task_id) +
                                "/run",
                            run_body);
            require(response.status == 200, "idempotent run status");
            const auto first_run_id = body_of(response).at("run").at("id").as_int64();
            response = call(app, "POST",
                            "/api/modules/data-ingestion/tasks/" + std::to_string(task_id) +
                                "/run",
                            run_body);
            require(response.status == 200, "idempotent run second status");
            require(body_of(response).at("run").at("id").as_int64() == first_run_id,
                    "idempotent run should reuse run");

            response = call(app, "POST", "/api/modules/student-profile/students",
                            R"({"student_id":"2023001","name":"张三","major":"软件工程",
                                "class_name":"软件2301","grade":2023})");
            require(response.status == 201, "sync student 1");
            response = call(app, "POST", "/api/modules/student-profile/students",
                            R"({"student_id":"2023002","name":"李四","major":"软件工程",
                                "class_name":"软件2301","grade":2023})");
            require(response.status == 201, "sync student 2");

            response = call(app, "POST", "/api/modules/student-profile/metrics",
                            R"({"student_id":"2023001","metric_code":"comprehensive_score",
                                "term":"2026-秋","value":95,"unit":"分","source":"academic"})");
            require(response.status == 201, "sync metric 1");
            response = call(app, "POST", "/api/modules/student-profile/metrics",
                            R"({"student_id":"2023001","metric_code":"practice_hours",
                                "term":"2026-秋","value":40,"unit":"小时","source":"practice"})");
            require(response.status == 201, "sync metric 2");
            response = call(app, "POST", "/api/modules/student-profile/metrics",
                            R"({"student_id":"2023002","metric_code":"comprehensive_score",
                                "term":"2026-秋","value":82,"unit":"分","source":"academic"})");
            require(response.status == 201, "sync metric 3");
            response = call(app, "POST", "/api/modules/student-profile/metrics",
                            R"({"student_id":"2023002","metric_code":"practice_hours",
                                "term":"2026-秋","value":60,"unit":"小时","source":"practice"})");
            require(response.status == 201, "sync metric 4");

            response = call(app, "POST", "/api/modules/student-profile/tags",
                            R"({"code":"score_excellent","name":"成绩优秀","dimension":"学业",
                                "description":"综合成绩不低于 90",
                                "rule":{"metric":"comprehensive_score","operator":">=","value":90}})");
            require(response.status == 201, "upsert tag 1");
            response = call(app, "POST", "/api/modules/student-profile/tags",
                            R"({"code":"practice_active","name":"实践积极","dimension":"实践",
                                "description":"实践时长不少于 50 小时",
                                "rule":{"metric":"practice_hours","operator":">=","value":50}})");
            require(response.status == 201, "upsert tag 2");

            response = call(app, "POST", "/api/modules/student-profile/recalculate/batch",
                            R"({"term":"2026-秋"})");
            require(response.status == 200, "batch recalculate status");
            const auto batch = body_of(response);
            require(batch.at("count").as_int64() == 2, "batch recalculate count");
            require(batch.at("students").as_array().size() == 2, "batch recalculate students");

            response = call(app, "POST", "/api/modules/student-profile/recalculate",
                            R"({"student_id":"2023001","term":"2026-秋"})");
            require(response.status == 200, "recalculate status");
            auto profile = body_of(response);
            require(profile.at("metrics").as_array().size() == 2, "profile metric count");
            require(profile.at("tags").as_array().size() == 1, "profile tag count");
            require(profile.at("tags").at(0).at("code").as_string() == "score_excellent",
                    "profile tag code");

            response = call(app, "GET", "/api/modules/student-profile/students/2023001",
                            {}, "term=2026-秋");
            require(response.status == 200, "get profile status");
            profile = body_of(response);
            require(profile.at("student").at("name").as_string() == "张三", "profile student");
            require(profile.at("snapshot").is_object(), "profile snapshot");

            response = call(app, "GET",
                            "/api/modules/student-profile/students/2023001/tags/"
                            "score_excellent/evidence");
            require(response.status == 200, "tag evidence status");
            const auto evidence = body_of(response);
            require(evidence.at("evidence").at("metric").as_string() == "comprehensive_score",
                    "tag evidence metric");

            response = call(app, "POST", "/api/modules/student-profile/recalculate",
                            R"({"student_id":"2023002","term":"2026-秋"})");
            require(response.status == 200, "recalculate peer status");

            response = call(app, "GET", "/api/modules/student-profile/students/2023001/compare",
                            {}, "peer=2023002&term=2026-秋");
            require(response.status == 200, "compare status");
            const auto comparison = body_of(response);
            bool found_score = false;
            for (const auto& metric : comparison.at("metrics").as_array()) {
                if (metric.at("metric_code").as_string() == "comprehensive_score") {
                    found_score = true;
                    require(metric.at("delta").as_double() == 13.0, "score delta");
                }
            }
            require(found_score, "compare should include comprehensive_score");

            response = call(app, "GET", "/api/modules/student-profile/students/2023001/report",
                            {}, "term=2026-秋");
            require(response.status == 200, "report status");
            require(response.content_type.find("text/html") != std::string::npos,
                    "report content type");
            require(response.body.find("张三") != std::string::npos, "report body");

            response = call(app, "POST", "/api/modules/student-profile/metrics",
                            R"({"student_id":"2023001","metric_code":"comprehensive_score",
                                "term":"2027-春","value":70,"unit":"分","source":"academic"})");
            require(response.status == 201, "sync spring metric");
            response = call(app, "POST", "/api/modules/student-profile/recalculate",
                            R"({"student_id":"2023001","term":"2027-春"})");
            require(response.status == 200, "recalculate spring status");

            response = call(app, "GET", "/api/modules/student-profile/students/2023001",
                            {}, "term=2026-秋");
            require(body_of(response).at("tags").as_array().size() == 1,
                    "old term tags should remain");
            response = call(app, "GET", "/api/modules/student-profile/students/2023001",
                            {}, "term=2027-春");
            require(body_of(response).at("tags").as_array().empty(),
                    "spring term should have no tag");
            response = call(app, "GET",
                            "/api/modules/student-profile/students/2023001/tags/"
                            "score_excellent/evidence",
                            {}, "term=2027-春");
            require(response.status == 404, "spring tag evidence should be missing");

            response = call(app, "GET", "/api/modules/student-profile/students/9999");
            require(response.status == 404, "unknown student should be 404");

            response = call(app, "POST", "/api/modules/data-ingestion/sources",
                            R"({"name":"教务成绩样例","kind":"academic","access_mode":"file",
                                "owner":"教务处"})");
            require(response.status == 409, "duplicate data source should be 409");
        }
    } catch (const std::exception& error) {
        remove_database_files(path);
        std::cerr << "module api tests failed: " << error.what() << '\n';
        return 1;
    }
    remove_database_files(path);
    std::cout << "module api tests passed\n";
    return 0;
}
