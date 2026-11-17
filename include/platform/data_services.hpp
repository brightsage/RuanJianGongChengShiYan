#pragma once
#include "platform/security.hpp"
#include <map>

namespace platform::data_services {
struct Query {
    security::Principal caller;
    security::Scope scope;
    std::vector<std::string> fields;
    Json parameters;
};
// Providers are read-only, return an array of records, and include scope identity fields.
using Provider = std::function<Json(const Query&)>;
class Gateway {
public:
    explicit Gateway(std::shared_ptr<security::Security> security) : security_(std::move(security)) {}
    void register_provider(const std::string& id, std::vector<std::string> fields, Provider provider);
    Json providers() const;
    Response invoke(const Request& request);
    std::shared_ptr<security::Security> security() const { return security_; }
private:
    struct Entry { std::vector<std::string> fields; Provider provider; };
    std::shared_ptr<security::Security> security_;
    std::map<std::string, Entry> providers_;
};
}  // namespace platform::data_services
