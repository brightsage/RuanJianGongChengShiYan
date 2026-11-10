#pragma once

#include "platform/security.hpp"
#include <sqlite3.h>
#include <mutex>

namespace platform::data_services {
class Statement {
public:
    Statement(sqlite3* db, const std::string& sql);
    ~Statement();
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    Statement& bind(int index, const std::string& value);
    Statement& bind(int index, std::int64_t value);
    bool step();
    std::string text(int index) const;
    std::int64_t integer(int index) const;
private:
    sqlite3_stmt* statement_ = nullptr;
};
void execute(sqlite3* db, const std::string& sql);
class Transaction {
public:
    explicit Transaction(sqlite3* db);
    ~Transaction();
    void commit();
private:
    sqlite3* db_;
    bool committed_ = false;
};
class Store {
public:
    explicit Store(const AppOptions& options);
    ~Store();
    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;
    std::int64_t now() const { return clock_(); }
    void audit(const std::string& actor, const security::AuditEvent& event);
    sqlite3* db = nullptr;
    mutable std::recursive_mutex mutex;
private:
    std::function<std::int64_t()> clock_;
};
std::string random_token(const std::string& prefix);
std::string digest(const std::string& text);
std::string password_hash(const std::string& password, const std::string& salt);
std::string text(const Json& object, const std::string& key, std::size_t max = 256);
std::int64_t integer(const Json& object, const std::string& key, std::int64_t min, std::int64_t max);
Json body(const Request& request);
Response json(int status, const Json& value);
security::Scope parse_scope(const Json& value);
Json strings(const std::vector<std::string>& values);
std::vector<std::string> string_list(const Json& value);
void install_identity_routes(App& app, const std::shared_ptr<security::Security>& security);
}  // namespace platform::data_services
