#include "test_support.hpp"
#include "platform/data_services.hpp"
#include <sqlite3.h>
#include <iostream>

int main() {
    std::int64_t now = 1700000000;
    platform::AppOptions options; options.bootstrap_password = "test-admin-password"; options.clock = [&] { return now; };
    auto app = platform::make_app(options);
    Client admin{app, {}}; admin.login("admin", options.bootstrap_password);
    admin.create("data", "data_admin"); const auto client_id = admin.create("client", "third_party");
    Client data{app, {}}, client{app, {}}; data.login("data"); client.login("client");
    const auto service = data.get("POST", "/services", R"({"name":"Profile","provider_id":"profiles","version":"v1","fields":["student_no","phone","score","profile"],"scope":{"type":"all"},"rate_per_minute":10})", 201).get("id").string();
    auto grant = [&](int quota, int rate, const std::string& level = "standard") {
        const auto id = client.get("POST", "/applications", "{\"service_id\":\"" + service +
            R"(","purpose":"integration test","scope":{"type":"college","org_id":"college-a"},"validity_seconds":600,"quota":100,"rate_per_minute":10})", 201).get("id").string();
        data.call("POST", "/applications/" + id + "/decision", "{\"decision\":\"approve\",\"validity_seconds\":600,\"quota\":" +
            std::to_string(quota) + ",\"rate_per_minute\":" + std::to_string(rate) + ",\"mask_level\":\"" + level + "\"}");
        return std::make_pair(id, client.get("POST", "/applications/" + id + "/token").get("access_token").string());
    };
    auto [application, token] = grant(2, 1);
    Client service_client{app, token};
    service_client.call("POST", "/services/" + service + "/invoke", "{}", 503);
    bool observed_scope = false, provider_failure = false;
    app.data_services()->register_provider("profiles", {"student_no", "phone", "score", "profile"}, [&](const platform::data_services::Query& query) {
        if (provider_failure) throw std::runtime_error("private provider error");
        observed_scope = query.scope.type == "college" && query.scope.org_id == "college-a" && query.caller.user_id == client_id;
        return platform::Json::parse(R"([{"org_id":"college-a","class_id":"class-a","student_no":"20260001","phone":"13800000000","score":99,"profile":{"name":"测试学生名称","email":"student@example.test","password_hash":"not-allowed"},"hidden":"not-published"},{"org_id":"college-b","student_no":"outside"},{"student_no":"missing-org"}])");
    });
    service_client.call("POST", "/services/not-this-version/invoke", "{}", 403);
    service_client.call("POST", "/services/" + service + "/invoke", R"({"scope":{"type":"all"}})", 400);
    provider_failure = true;
    service_client.call("POST", "/services/" + service + "/invoke", "{}", 503);
    provider_failure = false;
    auto result = service_client.get("POST", "/services/" + service + "/invoke", "{}");
    expect(observed_scope, "provider must receive authorized scope");
    expect(result.get("records").size() == 1, "must filter records outside scope and without scope metadata");
    expect(result.dump().find("20260001") == std::string::npos && result.dump().find("13800000000") == std::string::npos, "PII must be masked");
    expect(result.dump().find("not-allowed") == std::string::npos && result.dump().find("hidden") == std::string::npos, "nested secrets and unpublished fields must not leak");
    expect(result.get("records").at(0).get("score").string() == "***", "numeric score must be redacted");
    service_client.call("POST", "/services/" + service + "/invoke", "{}", 429);
    now += 61;
    service_client.call("POST", "/services/" + service + "/invoke", "{}");
    now += 61;
    service_client.call("POST", "/services/" + service + "/invoke", "{}", 429);
    auto [strict_id, strict_token] = grant(5, 5, "strict"); (void)strict_id;
    Client strict{app, strict_token};
    auto strict_result = strict.get("POST", "/services/" + service + "/invoke", "{}");
    expect(strict_result.get("records").at(0).get("student_no").string() == "***", "strict mask overrides partial policy");
    admin.call("PUT", "/masking-rules/phone", R"({"algorithm":"hash"})");
    auto [hash_id, hash_token] = grant(5, 5); (void)hash_id;
    Client hashed{app, hash_token};
    auto hashed_result = hashed.get("POST", "/services/" + service + "/invoke", "{}");
    expect(hashed_result.get("records").at(0).get("phone").string().size() == 64, "HMAC hash policy");
    data.call("PUT", "/masking-rules/phone", R"({"algorithm":"redact"})", 403);
    admin.call("PUT", "/masking-rules/phone", R"({"algorithm":"none"})", 400);
    const auto rotated = client.get("POST", "/applications/" + hash_id + "/token").get("access_token").string();
    hashed.call("POST", "/services/" + service + "/invoke", "{}", 401);
    hashed.token = rotated;
    data.call("PATCH", "/services/" + service, R"({"active":false})");
    hashed.call("POST", "/services/" + service + "/invoke", "{}", 403);
    data.call("PATCH", "/services/" + service, R"({"active":true})");
    const auto inspected_id = hash_id;
    const auto used = [&]() {
        const auto applications = client.get("GET", "/applications");
        for (std::size_t i = 0; i < applications.size(); ++i)
            if (applications.at(i).get("id").string() == inspected_id)
                return applications.at(i).get("grant").get("used").integer();
        throw std::runtime_error("grant missing");
    };
    const auto used_before_failure = used();
    // An actual SQLite audit failure must roll back quota accounting and prevent data release.
    platform::Request admin_request{"GET", "/", {{"authorization", "Bearer " + admin.token}}, {}, {}, {}};
    auto principal = app.security()->authenticate(admin_request);
    platform::security::AuditEvent event{"test.setup", "test", "audit-failure"};
    app.security()->audited_write(principal, event, [](sqlite3* db) {
        sqlite3_exec(db, "CREATE TRIGGER fail_invocation_audit BEFORE INSERT ON audit_logs WHEN NEW.action='service.invoke' BEGIN SELECT RAISE(ABORT,'injected failure'); END", nullptr, nullptr, nullptr);
    });
    hashed.call("POST", "/services/" + service + "/invoke", "{}", 500);
    app.security()->audited_write(principal, event, [](sqlite3* db) { sqlite3_exec(db, "DROP TRIGGER fail_invocation_audit", nullptr, nullptr, nullptr); });
    expect(used() == used_before_failure, "failed audit must not consume quota");
    now += 61;
    data.call("PATCH", "/services/" + service, R"({"rate_per_minute":1})");
    strict.call("POST", "/services/" + service + "/invoke", "{}");
    hashed.call("POST", "/services/" + service + "/invoke", "{}", 429);
    data.call("PATCH", "/services/" + service, R"({"rate_per_minute":10})");
    expect(client.get("GET", "/applications").size() == 3, "application history");
    admin.call("PATCH", "/users/" + client_id, R"({"active":false})");
    hashed.call("POST", "/services/" + service + "/invoke", "{}", 403);
    admin.call("GET", "/call-logs");
    platform::Request log_query{"GET", "/api/modules/data-services/call-logs", {{"authorization", "Bearer " + admin.token}}, {}, "status=429&limit=1", {}};
    const auto log_response = app.handle(log_query);
    expect(log_response.status == 200 && platform::Json::parse(log_response.body).size() == 1, "call log filtering/pagination");
    now += 1000;
    strict.call("POST", "/services/" + service + "/invoke", "{}", 401);
    const auto logs = admin.get("GET", "/call-logs");
    expect(logs.size() >= 10, "success and failure calls must be logged");
    std::cout << "provider, scope filtering, masking, limits, revocation and audit failure passed\n";
}
