#include "modules/student_profile/sqlite_repository.hpp"
#include "test_support.hpp"

using platform::Json;

namespace {
Json career(const Client& user, const char* method, const std::string& suffix,
            const std::string& body = {}, int expected = 200) {
    platform::Request request{method, "/api/modules/career-recommendation" + suffix,
                              {{"authorization", "Bearer " + user.token}}, body, {}, {}};
    const auto response = user.app.handle(request);
    expect(response.status == expected, suffix + " HTTP " + std::to_string(response.status) + " " + response.body);
    return Json::parse(response.body);
}
}  // namespace

int main() {
    platform::AppOptions options;
    options.bootstrap_password = "test-admin-password";
    auto app = platform::make_app(options);
    Client admin{app, {}};
    admin.login("admin", options.bootstrap_password);
    admin.create("center", "career_center");
    for (int i = 1; i <= 2; ++i) {
        const auto suffix = std::to_string(i);
        admin.call("POST", "/users", Json(Json::Object{
            {"username", "student" + suffix}, {"password", "test-user-password"},
            {"role", "student"}, {"org_id", "college-a"}, {"class_id", "class-a"},
            {"student_no", "100" + suffix},
        }).dump(), 201);
    }
    Client center{app, {}}, student{app, {}}, other{app, {}};
    center.login("center");
    student.login("student1");
    other.login("student2");
    const auto job = career(center, "POST", "/jobs",
        R"({"title":"测试实习","industry":"信息技术","job_type":"internship","description":"测试画像对接","requirements":[{"competency_code":"programming","threshold":80,"weight":3},{"competency_code":"databases","threshold":80,"weight":1}]})", 201);
    const auto job_path = "/jobs/" + std::to_string(job.at("id").as_int64());
    career(center, "PUT", job_path + "/status", R"({"status":"published"})");
    career(center, "PUT", "/strategy",
        R"({"algorithm":"pass_and_proximity","min_score":0,"pass_weight":60,"proximity_weight":40,"allowed_industries":[],"allowed_job_types":[]})");
    career(center, "POST", "/recommendations/query", "{}", 403);
    career(student, "POST", "/recommendations/query", "{}", 409);
    app.security()->with_connection([](sqlite3* connection) {
        platform::Database db(*connection);
        platform::student_profile::SqliteProfileRepository profiles(db);
        profiles.upsert_student({"1001", "样例学生", "", "", 0, ""});
        profiles.upsert_metric({0, "1001", "programming", "2026-2027-1", 0.8, "ratio_1", "", ""});
        profiles.upsert_metric({0, "1001", "databases", "2026-2027-1", 40, "%", "", ""});
        profiles.upsert_metric({0, "1001", "ignored", "2026-2027-1", 90, "unknown", "", ""});
        profiles.upsert_metric({0, "1001", "programming", "2025-2026-2", 60, "score_100", "", ""});
        db.exec("UPDATE sp_metric_value SET calculated_at='2026-10-01 10:00:00' WHERE term='2025-2026-2'");
        db.exec("UPDATE sp_metric_value SET calculated_at='2026-11-01 10:00:00' WHERE term='2026-2027-1'");
    });
    const auto recs = career(student, "POST", "/recommendations/query", "{}");
    const auto& first = recs.at("items").at(std::size_t{0});
    expect(recs.at("term").as_string() == "2026-2027-1", "latest term");
    expect(first.at("score").as_double() == 80, "normalized 80/40 profile");
    const auto id = first.at("id").as_int64();
    const auto rec_path = "/recommendations/" + std::to_string(id);
    career(other, "GET", rec_path + "/reason", "", 404);
    career(other, "PUT", rec_path + "/feedback", R"({"choice":"interested","reason":"测试"})", 404);
    const auto old_reason = career(student, "GET", rec_path + "/reason");
    const auto past = career(student, "POST", "/recommendations/query", R"({"term":"2025-2026-2"})");
    expect(past.at("items").at(std::size_t{0}).at("score").as_double() == 22.5, "explicit term");
    app.security()->with_connection([](sqlite3* connection) {
        platform::Database db(*connection);
        platform::student_profile::SqliteProfileRepository profiles(db);
        profiles.upsert_metric({0, "1001", "databases", "2026-2027-1", 80, "%", "", ""});
    });
    const auto result = career(student, "POST", "/recommendations/query", "{}");
    const auto& changed = result.at("items").at(std::size_t{0});
    expect(changed.at("id").as_int64() != id && changed.at("score").as_double() == 100, "new profile version");
    expect(career(student, "GET", rec_path + "/reason").dump() == old_reason.dump(), "original reason retained");
}
