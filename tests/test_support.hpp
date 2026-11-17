#pragma once
#include "platform/security.hpp"
#include <stdexcept>

inline void expect(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
struct Client {
    platform::App& app;
    std::string token;
    platform::Response call(const std::string& method, const std::string& path,
                            const std::string& body = "", int expected = 200) const {
        platform::Request request{method, "/api/modules/data-services" + path, {}, body, {}, {}};
        if (!token.empty()) request.headers["authorization"] = "Bearer " + token;
        const auto response = app.handle(request);
        expect(response.status == expected, method + " " + path + " expected " + std::to_string(expected) +
            " got " + std::to_string(response.status) + ": " + response.body);
        return response;
    }
    platform::Json get(const std::string& method, const std::string& path,
                       const std::string& body = "", int expected = 200) const {
        return platform::Json::parse(call(method, path, body, expected).body);
    }
    void login(const std::string& username, const std::string& password = "test-user-password") {
        auto input = platform::Json::object(); input.set("username", platform::Json::text(username));
        input.set("password", platform::Json::text(password));
        token = get("POST", "/login", input.dump()).get("access_token").string();
    }
    std::string create(const std::string& username, const std::string& role, const std::string& org = "college-a") {
        auto input = platform::Json::object(); input.set("username", platform::Json::text(username));
        input.set("password", platform::Json::text("test-user-password")); input.set("role", platform::Json::text(role));
        input.set("org_id", platform::Json::text(org));
        return get("POST", "/users", input.dump(), 201).get("user_id").string();
    }
};
