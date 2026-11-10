#pragma once

#include "platform/app.hpp"
#include "platform/json.hpp"
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

struct sqlite3;
namespace platform::data_services { class Store; }

namespace platform::security {
struct ApiError : std::runtime_error {
    ApiError(int status, std::string code, std::string message)
        : std::runtime_error(std::move(message)), status(status), code(std::move(code)) {}
    int status;
    std::string code;
};
struct Scope {
    std::string type = "all";
    std::string org_id, class_id, student_no;
};
struct Principal {
    std::string user_id, username, role, org_id, class_id, student_no;
    Scope scope;
    std::vector<std::string> permissions;
};
struct AuditEvent {
    AuditEvent(std::string action, std::string resource_type, std::string resource_id)
        : action(std::move(action)), resource_type(std::move(resource_type)), resource_id(std::move(resource_id)) {}
    std::string action, resource_type, resource_id;
    std::string outcome = "success";
    std::string reason, request_id;
    Json before, after;
};
bool contains(const Scope& outer, const Scope& inner);
Json scope_json(const Scope& scope);
Json principal_json(const Principal& principal);

class Security {
public:
    explicit Security(std::shared_ptr<data_services::Store> store);
    Principal authenticate(const Request& request) const;
    Principal authorize(const Request& request, const std::string& permission,
                        const Scope& resource = Scope{"", {}, {}, {}}) const;
    bool permitted(const Principal& principal, const std::string& permission) const;
    void register_permission(const std::string& code, const std::string& name) const;
    // Business SQL and the audit event use ONE SQLite transaction. Exceptions roll both back.
    void audited_write(const Principal& actor, AuditEvent& event,
                       const std::function<void(sqlite3*)>& write) const;
    std::shared_ptr<data_services::Store> store() const { return store_; }
private:
    std::shared_ptr<data_services::Store> store_;
};
Response api_error(int status, const std::string& code, const std::string& message);
}  // namespace platform::security
