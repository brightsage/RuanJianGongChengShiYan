#include "platform/security.hpp"
#include <sqlite3.h>
#include <iostream>

using namespace platform;
void check(bool condition, const std::string& message) { if (!condition) throw std::runtime_error(message); }
Response call(const App& app, const std::string& method, const std::string& suffix,
              const std::string& input = "", const std::string& token = "") {
    Request request{method, "/api/modules/data-services" + suffix, {}, input, {}, {}};
    if (!token.empty()) request.headers["authorization"] = "Bearer " + token;
    return app.handle(request);
}
std::string login(const App& app, const std::string& username, const std::string& password) {
    auto input = Json::object(); input["username"] = Json(username); input["password"] = Json(password);
    const auto response = call(app, "POST", "/login", input.dump());
    check(response.status == 200, "login failed: " + response.body);
    return Json::parse(response.body).at("access_token").as_string();
}
int main() {
    std::int64_t now = 1700000000;
    AppOptions options; options.bootstrap_password = "test-admin-password"; options.clock = [&] { return now; };
    auto app = make_app(options);
    check(call(app, "GET", "/users").status == 401, "anonymous must be rejected");
    check(call(app, "POST", "/login", "{broken").status == 400, "malformed JSON");
    check(call(app, "POST", "/login", R"({"username":"admin","password":"wrong"})").status == 401, "wrong password");
    const auto admin = login(app, "admin", options.bootstrap_password);
    auto create = [&](const std::string& name, const std::string& role, const std::string& attrs = "") {
        const auto response = call(app, "POST", "/users", "{\"username\":\"" + name +
            "\",\"password\":\"test-user-password\",\"role\":\"" + role + "\"" + attrs + "}", admin);
        check(response.status == 201, "create user: " + response.body);
        return Json::parse(response.body).at("user_id").as_string();
    };
    const auto student_id = create("student", "student", R"(,"org_id":"college-a","class_id":"class-a","student_no":"20260001")");
    const auto career_id = create("career", "career_center"); (void)career_id;
    const auto student = login(app, "student", "test-user-password");
    auto me = Json::parse(call(app, "GET", "/me", "", student).body);
    check(me.at("student_no").as_string() == "20260001", "student_no must come from authenticated account");
    check(call(app, "GET", "/users", "", student).status == 403, "role boundary");
    Request request{"GET", "/", {{"authorization", "Bearer " + student}}, {}, {}, {}};
    const auto principal = app.security()->authorize(request, "career.recommendations.read", {"self", "college-a", "class-a", "20260001"});
    bool denied = false;
    try { app.security()->authorize(request, "career.recommendations.read", {"self", "college-a", "class-a", "20269999"}); }
    catch (const security::ApiError& e) { denied = e.status == 403; }
    check(denied, "must reject another student's scope");
    security::AuditEvent event{"test.write", "test", "record"};
    app.security()->audited_write(principal, event, [&](sqlite3* db) {
        check(sqlite3_exec(db, "CREATE TABLE test_business(value TEXT)", nullptr, nullptr, nullptr) == SQLITE_OK, "create business table");
    });
    auto invalid = event; invalid.action.clear();
    bool rolled_back = false;
    try { app.security()->audited_write(principal, invalid, [&](sqlite3* db) {
        sqlite3_exec(db, "INSERT INTO test_business VALUES('must rollback')", nullptr, nullptr, nullptr);
    }); } catch (const std::invalid_argument&) { rolled_back = true; }
    check(rolled_back, "audit failure must abort");
    app.security()->audited_write(principal, event, [&](sqlite3* db) {
        sqlite3_stmt* row = nullptr; sqlite3_prepare_v2(db, "SELECT count(*) FROM test_business", -1, &row, nullptr);
        sqlite3_step(row); const int count = sqlite3_column_int(row, 0); sqlite3_finalize(row);
        check(count == 0, "business mutation must roll back with audit");
    });
    bool business_failure = false;
    try { app.security()->audited_write(principal, event, [&](sqlite3* db) {
        sqlite3_exec(db, "INSERT INTO test_business VALUES('temporary')", nullptr, nullptr, nullptr);
        throw std::runtime_error("business failure");
    }); } catch (const std::runtime_error&) { business_failure = true; }
    check(business_failure, "business failure must propagate");
    const auto logs = call(app, "GET", "/audit-logs", "", admin);
    check(logs.status == 200 && Json::parse(logs.body).is_array(), "audit log must be readable");
    check(logs.body.find("transaction_failed") != std::string::npos, "business failure must have an audit event");
    check(logs.body.find("test-user-password") == std::string::npos, "password must not appear in audit");
    check(logs.body.find("20260001") == std::string::npos, "student number must be redacted in audit");
    check(call(app, "PATCH", "/users/" + student_id, R"({"active":false})", admin).status == 200, "disable user");
    check(call(app, "GET", "/me", "", student).status == 401, "disable must revoke session");
    const auto career = login(app, "career", "test-user-password");
    Request career_request{"GET", "/", {{"authorization", "Bearer " + career}}, {}, {}, {}};
    check(app.security()->authorize(career_request, "career.strategy.write").role == "career_center", "global strategy permission");
    check(call(app, "PUT", "/roles/career_center", R"({"name":"就业指导中心","scope_type":"all","permissions":["career.jobs.read"]})", admin).status == 200, "role update");
    denied = false;
    try { app.security()->authorize(career_request, "career.strategy.write"); }
    catch (const security::ApiError& e) { denied = e.status == 403; }
    check(denied, "role change must take effect on an existing session");
    app.security()->register_permission("custom.records.read", "自定义模块读取");
    check(call(app, "PUT", "/roles/custom_reader", R"({"name":"扩展角色","scope_type":"college","permissions":["custom.records.read"]})", admin).status == 200, "other modules can register permissions");
    check(call(app, "PUT", "/roles/custom_reader", R"({"name":"扩展角色","scope_type":"college","permissions":["unknown.permission"]})", admin).status == 400, "reject unknown permission codes");
    check(call(app, "PUT", "/roles/college_operator", R"({"name":"院级账号","scope_type":"college","permissions":["users.manage"]})", admin).status == 200, "create scoped role");
    check(call(app, "POST", "/users", R"({"username":"limited","password":"test-user-password","role":"college_operator","org_id":"college-a"})", admin).status == 201, "create scoped user");
    const auto limited = login(app, "limited", "test-user-password");
    check(call(app, "GET", "/users", "", limited).status == 403, "scoped role must not administer all accounts");
    check(call(app, "PUT", "/roles/system_admin", R"({"name":"x","scope_type":"all","permissions":[]})", admin).status == 409, "protect reserved administrator role");
    check(call(app, "POST", "/logout", "", career).status == 200, "logout");
    check(call(app, "GET", "/me", "", career).status == 401, "logout revokes");
    now += 3601;
    check(call(app, "GET", "/me", "", admin).status == 401, "expired token");
    std::cout << "identity, permissions, scopes, expiry and audit rollback passed\n";
}
