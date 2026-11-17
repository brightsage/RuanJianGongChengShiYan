#include "test_support.hpp"
#include <iostream>

int main() {
    platform::AppOptions options; options.bootstrap_password = "test-admin-password";
    auto app = platform::make_app(options);
    Client admin{app, {}}; admin.login("admin", options.bootstrap_password);
    admin.create("data", "data_admin"); admin.create("client", "third_party");
    admin.create("other", "third_party", "college-b");
    Client data{app, {}}, client{app, {}}, other{app, {}};
    data.login("data"); client.login("client"); other.login("other");
    const std::string service_input = R"({"name":"画像查询","provider_id":"student-profile","version":"v1","fields":["student_no","score"],"scope":{"type":"all"},"rate_per_minute":10})";
    const auto service = data.get("POST", "/services", service_input, 201).at("id").as_string();
    client.call("POST", "/services", service_input, 403);
    data.call("POST", "/services", service_input, 409);
    data.call("PATCH", "/services/" + service, R"({"fields":["phone"]})", 400);
    const auto apply = [&](const std::string& org) {
        return "{\"service_id\":\"" + service + R"(","purpose":"test integration","scope":{"type":"college","org_id":")" + org +
            R"("},"validity_seconds":3600,"quota":20,"rate_per_minute":5})";
    };
    client.call("POST", "/applications", apply("college-b"), 403);
    const auto application = client.get("POST", "/applications", apply("college-a"), 201).at("id").as_string();
    expect(other.get("GET", "/applications").as_array().size() == 0, "other client must not see applications");
    client.call("POST", "/applications/" + application + "/token", "", 404);
    const std::string approval = R"({"decision":"approve","validity_seconds":600,"quota":10,"rate_per_minute":2,"mask_level":"standard"})";
    client.call("POST", "/applications/" + application + "/decision", approval, 403);
    data.call("POST", "/applications/" + application + "/decision",
              R"({"decision":"approve","validity_seconds":600,"quota":100,"rate_per_minute":2,"mask_level":"standard"})", 400);
    data.call("POST", "/applications/" + application + "/decision", approval);
    data.call("POST", "/applications/" + application + "/decision", approval, 409);
    other.call("POST", "/applications/" + application + "/token", "", 403);
    const auto token = client.get("POST", "/applications/" + application + "/token").at("access_token").as_string();
    expect(token.rfind("svc_", 0) == 0, "separate service token namespace");
    const auto rotated = client.get("POST", "/applications/" + application + "/token").at("access_token").as_string();
    expect(token != rotated, "rotation must issue fresh token");
    const auto logs = admin.get("GET", "/audit-logs").dump();
    expect(logs.find(token) == std::string::npos && logs.find(rotated) == std::string::npos, "token must not appear in audit");
    client.call("POST", "/applications/" + application + "/revoke");
    client.call("POST", "/applications/" + application + "/token", "", 403);
    const auto rejected = client.get("POST", "/applications", apply("college-a"), 201).at("id").as_string();
    data.call("POST", "/applications/" + rejected + "/decision", R"({"decision":"reject","reason":"用途不符合"})");
    expect(client.get("GET", "/applications").as_array().size() == 2, "approved/rejected history must persist");
    data.call("PATCH", "/services/" + service, R"({"active":false})");
    client.call("POST", "/applications", apply("college-a"), 409);
    std::cout << "publication, scope, approval, ownership, token rotation and revocation passed\n";
}
