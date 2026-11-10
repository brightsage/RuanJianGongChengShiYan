#include "store.hpp"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace platform::data_services {
namespace {
void check(int rc, sqlite3* db) {
    if (rc != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
}
std::string hex(const unsigned char* bytes, std::size_t length) {
    std::ostringstream out;
    for (std::size_t i = 0; i < length; ++i) out << std::hex << std::setw(2) << std::setfill('0') << unsigned(bytes[i]);
    return out.str();
}
Json scrub(const Json& value) {
    if (value.is(json_type_array)) {
        auto out = Json::array();
        for (std::size_t i = 0; i < value.size(); ++i) out.push(scrub(value.at(i)));
        return out;
    }
    if (!value.is(json_type_object)) return value;
    auto out = Json::object();
    json_object_object_foreach(value.raw(), key, item) {
        std::string lower = key;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
        const bool sensitive = lower.find("password") != std::string::npos || lower.find("token") != std::string::npos ||
            lower.find("secret") != std::string::npos || lower == "authorization" || lower == "student_no" ||
            lower == "id_card" || lower == "phone" || lower == "email" || lower == "score" || lower == "name";
        out.set(key, sensitive ? Json::text("[redacted]") : scrub(Json(json_object_get(item))));
    }
    return out;
}
}  // namespace

Statement::Statement(sqlite3* db, const std::string& sql) { check(sqlite3_prepare_v2(db, sql.c_str(), -1, &statement_, nullptr), db); }
Statement::~Statement() { sqlite3_finalize(statement_); }
Statement& Statement::bind(int index, const std::string& value) {
    check(sqlite3_bind_text(statement_, index, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT), sqlite3_db_handle(statement_));
    return *this;
}
Statement& Statement::bind(int index, std::int64_t value) {
    check(sqlite3_bind_int64(statement_, index, value), sqlite3_db_handle(statement_));
    return *this;
}
bool Statement::step() {
    const int rc = sqlite3_step(statement_);
    if (rc == SQLITE_ROW) return true;
    if (rc != SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(sqlite3_db_handle(statement_)));
    return false;
}
std::string Statement::text(int index) const {
    const auto* value = sqlite3_column_text(statement_, index);
    return value ? reinterpret_cast<const char*>(value) : "";
}
std::int64_t Statement::integer(int index) const { return sqlite3_column_int64(statement_, index); }
void execute(sqlite3* db, const std::string& sql) {
    char* error = nullptr;
    const int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &error);
    const std::string message = error ? error : "SQL failed";
    sqlite3_free(error);
    if (rc != SQLITE_OK) throw std::runtime_error(message);
}
Transaction::Transaction(sqlite3* db) : db_(db) { execute(db_, "BEGIN IMMEDIATE"); }
Transaction::~Transaction() { if (!committed_) sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr); }
void Transaction::commit() { execute(db_, "COMMIT"); committed_ = true; }

std::string random_token(const std::string& prefix) {
    unsigned char bytes[32];
    if (RAND_bytes(bytes, sizeof(bytes)) != 1) throw std::runtime_error("random generator failed");
    return prefix + hex(bytes, sizeof(bytes));
}
std::string digest(const std::string& text) {
    unsigned char bytes[EVP_MAX_MD_SIZE];
    unsigned int count = 0;
    if (EVP_Digest(text.data(), text.size(), bytes, &count, EVP_sha256(), nullptr) != 1)
        throw std::runtime_error("digest failed");
    return hex(bytes, count);
}
std::string password_hash(const std::string& password, const std::string& salt) {
    unsigned char bytes[32];
    if (PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()),
        reinterpret_cast<const unsigned char*>(salt.data()), static_cast<int>(salt.size()),
        210000, EVP_sha256(), sizeof(bytes), bytes) != 1) throw std::runtime_error("password hashing failed");
    return hex(bytes, sizeof(bytes));
}
Json strings(const std::vector<std::string>& values) {
    auto out = Json::array(); for (const auto& value : values) out.push(Json::text(value)); return out;
}
std::vector<std::string> string_list(const Json& value) {
    if (!value.is(json_type_array) || value.size() > 100) throw std::invalid_argument("expected array of at most 100 strings");
    std::vector<std::string> out;
    for (std::size_t i = 0; i < value.size(); ++i) {
        auto item = value.at(i).string();
        if (item.empty() || item.size() > 100 || std::find(out.begin(), out.end(), item) != out.end())
            throw std::invalid_argument("invalid or duplicate array entry");
        out.push_back(std::move(item));
    }
    return out;
}
std::string text(const Json& object, const std::string& key, std::size_t max) {
    auto value = object.get(key).string();
    if (value.empty() || value.size() > max || value.find('\0') != std::string::npos)
        throw std::invalid_argument("invalid " + key);
    return value;
}
std::int64_t integer(const Json& object, const std::string& key, std::int64_t min, std::int64_t max) {
    auto value = object.get(key).integer();
    if (value < min || value > max) throw std::invalid_argument("invalid " + key);
    return value;
}
Json body(const Request& request) {
    auto value = Json::parse(request.body);
    if (!value.is(json_type_object)) throw std::invalid_argument("body must be a JSON object");
    return value;
}
Response json(int status, const Json& value) { return {status, "application/json; charset=utf-8", value.dump()}; }
security::Scope parse_scope(const Json& value) {
    security::Scope scope;
    scope.type = text(value, "type", 16);
    if (scope.type != "all" && scope.type != "college" && scope.type != "class" && scope.type != "self")
        throw std::invalid_argument("invalid scope type");
    if (scope.type != "all") scope.org_id = text(value, "org_id", 100);
    if (scope.type == "class" || scope.type == "self") scope.class_id = text(value, "class_id", 100);
    if (scope.type == "self") scope.student_no = text(value, "student_no", 100);
    return scope;
}

Store::Store(const AppOptions& options) : clock_(options.clock) {
    if (!clock_) clock_ = [] { return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count(); };
    check(sqlite3_open(options.database_path.c_str(), &db), db);
    try {
        sqlite3_busy_timeout(db, 3000);
        execute(db, "PRAGMA foreign_keys=ON; PRAGMA journal_mode=WAL;");
        execute(db, R"SQL(
            CREATE TABLE IF NOT EXISTS roles(code TEXT PRIMARY KEY, name TEXT NOT NULL,
                scope_type TEXT NOT NULL, permissions TEXT NOT NULL);
            CREATE TABLE IF NOT EXISTS users(id TEXT PRIMARY KEY, username TEXT UNIQUE NOT NULL,
                password_hash TEXT NOT NULL, salt TEXT NOT NULL, role TEXT NOT NULL REFERENCES roles(code),
                org_id TEXT NOT NULL, class_id TEXT NOT NULL, student_no TEXT NOT NULL, active INTEGER NOT NULL DEFAULT 1);
            CREATE TABLE IF NOT EXISTS sessions(token_hash TEXT PRIMARY KEY, user_id TEXT NOT NULL REFERENCES users(id), expires_at INTEGER NOT NULL);
            CREATE TABLE IF NOT EXISTS audit_logs(id INTEGER PRIMARY KEY, created_at INTEGER NOT NULL,
                actor_id TEXT NOT NULL, action TEXT NOT NULL, resource_type TEXT NOT NULL, resource_id TEXT NOT NULL,
                outcome TEXT NOT NULL, reason TEXT NOT NULL, request_id TEXT NOT NULL, before_json TEXT NOT NULL, after_json TEXT NOT NULL);
            CREATE TABLE IF NOT EXISTS services(id TEXT PRIMARY KEY, name TEXT NOT NULL, provider_id TEXT NOT NULL,
                version TEXT NOT NULL, fields TEXT NOT NULL, scope TEXT NOT NULL, rate_per_minute INTEGER NOT NULL,
                active INTEGER NOT NULL DEFAULT 1, bucket INTEGER NOT NULL DEFAULT 0, bucket_count INTEGER NOT NULL DEFAULT 0,
                UNIQUE(name,version));
            CREATE TABLE IF NOT EXISTS applications(id TEXT PRIMARY KEY, owner_id TEXT NOT NULL REFERENCES users(id),
                service_id TEXT NOT NULL REFERENCES services(id), purpose TEXT NOT NULL, scope TEXT NOT NULL,
                validity_seconds INTEGER NOT NULL, quota INTEGER NOT NULL, rate_per_minute INTEGER NOT NULL,
                status TEXT NOT NULL DEFAULT 'pending', reason TEXT NOT NULL DEFAULT '', created_at INTEGER NOT NULL);
            CREATE TABLE IF NOT EXISTS grants(id TEXT PRIMARY KEY, application_id TEXT UNIQUE NOT NULL REFERENCES applications(id),
                owner_id TEXT NOT NULL REFERENCES users(id), service_id TEXT NOT NULL REFERENCES services(id), scope TEXT NOT NULL,
                expires_at INTEGER NOT NULL, quota INTEGER NOT NULL, used INTEGER NOT NULL DEFAULT 0,
                rate_per_minute INTEGER NOT NULL, mask_level TEXT NOT NULL, token_hash TEXT NOT NULL DEFAULT '',
                revoked INTEGER NOT NULL DEFAULT 0, bucket INTEGER NOT NULL DEFAULT 0, bucket_count INTEGER NOT NULL DEFAULT 0);
            CREATE INDEX IF NOT EXISTS grants_token ON grants(token_hash);
        )SQL");
        Transaction tx(db);
        const auto role = [&](const std::string& code, const std::string& name, const std::string& scope,
                              std::vector<std::string> permissions) {
            Statement(db, "INSERT OR IGNORE INTO roles VALUES(?,?,?,?)").bind(1, code).bind(2, name).bind(3, scope)
                .bind(4, strings(permissions).dump()).step();
        };
        role("system_admin", "系统管理员", "all", {"users.manage", "roles.manage", "masking.manage", "audit.read"});
        role("data_admin", "数据管理员", "all", {"services.publish", "grants.approve", "services.read"});
        role("college_admin", "院系管理员", "college", {"grants.apply", "services.read"});
        role("third_party", "第三方应用", "college", {"grants.apply", "services.read"});
        role("career_center", "就业指导中心", "all", {"career.jobs.read", "career.jobs.write", "career.jobs.publish", "career.strategy.write"});
        role("student", "学生", "self", {"career.recommendations.read"});
        role("counselor", "辅导员", "class", {});
        Statement count(db, "SELECT count(*) FROM users"); count.step();
        if (count.integer(0) == 0 && !options.bootstrap_password.empty()) {
            if (options.bootstrap_password.size() < 12) throw std::invalid_argument("bootstrap password must have at least 12 characters");
            auto salt = random_token("");
            Statement(db, "INSERT INTO users VALUES(?,?,?,?,?,'','','',1)")
                .bind(1, random_token("u_")).bind(2, options.bootstrap_username)
                .bind(3, password_hash(options.bootstrap_password, salt)).bind(4, salt).bind(5, "system_admin").step();
        } else if (count.integer(0) == 0 && options.require_bootstrap) {
            throw std::invalid_argument("first start requires PLATFORM_ADMIN_PASSWORD (at least 12 characters)");
        }
        tx.commit();
    } catch (...) { sqlite3_close(db); db = nullptr; throw; }
}
Store::~Store() { sqlite3_close(db); }
void Store::audit(const std::string& actor, const security::AuditEvent& event) {
    if (actor.empty() || event.action.empty() || event.resource_type.empty() || event.resource_id.empty() ||
        (event.outcome != "success" && event.outcome != "failure")) throw std::invalid_argument("incomplete audit event");
    Statement(db, "INSERT INTO audit_logs(created_at,actor_id,action,resource_type,resource_id,outcome,reason,request_id,before_json,after_json) VALUES(?,?,?,?,?,?,?,?,?,?)")
        .bind(1, now()).bind(2, actor).bind(3, event.action).bind(4, event.resource_type).bind(5, event.resource_id)
        .bind(6, event.outcome).bind(7, event.reason).bind(8, event.request_id.empty() ? random_token("req_") : event.request_id)
        .bind(9, scrub(event.before).dump()).bind(10, scrub(event.after).dump()).step();
}
}  // namespace platform::data_services
