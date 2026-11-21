#include "store.hpp"
#include "platform/crypto.hpp"

#include <algorithm>
#include <chrono>
#include <charconv>
#include <iomanip>
#include <sstream>

namespace platform::data_services {
namespace {
Json scrub(const Json& value) {
    if (value.is_array()) {
        auto out = Json::array();
        for (std::size_t i = 0; i < value.as_array().size(); ++i) out.push_back(scrub(value.at(i)));
        return out;
    }
    if (!value.is_object()) return value;
    auto out = Json::object();
    for (const auto& [key, item] : value.as_object()) {
        std::string lower = key;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
        const bool sensitive = lower.find("password") != std::string::npos || lower.find("token") != std::string::npos ||
            lower.find("secret") != std::string::npos || lower == "authorization" || lower == "student_no" ||
            lower == "id_card" || lower == "phone" || lower == "email" || lower == "score" || lower == "name";
        out[key] = sensitive ? Json("[redacted]") : scrub(item);
    }
    return out;
}
}  // namespace

Statement::Statement(Database& db, std::string sql) : db_(&db), sql_(std::move(sql)) {}
Statement::~Statement() = default;
Statement& Statement::bind(int index, const std::string& value) {
    if (index <= 0) throw std::invalid_argument("invalid bind index");
    if (params_.size() < static_cast<std::size_t>(index)) params_.resize(index);
    params_[index - 1] = value;
    return *this;
}
Statement& Statement::bind(int index, std::int64_t value) {
    if (index <= 0) throw std::invalid_argument("invalid bind index");
    if (params_.size() < static_cast<std::size_t>(index)) params_.resize(index);
    params_[index - 1] = value;
    return *this;
}
bool Statement::step() {
    if (!executed_) {
        rows_ = db_->query(sql_, params_);
        executed_ = true;
        row_ = 0;
    }
    if (row_ >= rows_.size()) return false;
    ++row_;
    return true;
}
std::string Statement::text(int index) const {
    if (row_ == 0 || row_ > rows_.size()) return {};
    const auto& value = rows_[row_ - 1].at(static_cast<std::size_t>(index));
    if (const auto* text = std::get_if<std::string>(&value)) return *text;
    return {};
}
std::int64_t Statement::integer(int index) const {
    if (row_ == 0 || row_ > rows_.size()) return 0;
    const auto& value = rows_[row_ - 1].at(static_cast<std::size_t>(index));
    if (const auto* number = std::get_if<std::int64_t>(&value)) return *number;
    if (const auto* number = std::get_if<double>(&value))
        return static_cast<std::int64_t>(*number);
    return 0;
}
void execute(Database& db, const std::string& sql) { db.exec(sql); }
Transaction::Transaction(Database& db) : db_(db) { db_.begin(); }
Transaction::~Transaction() {
    if (!committed_) {
        try { db_.rollback(); } catch (...) {}
    }
}
void Transaction::commit() { db_.commit(); committed_ = true; }

std::string random_token(const std::string& prefix) {
    return prefix + crypto::random_bytes_hex(16);
}
std::string digest(const std::string& text) {
    return crypto::sha256_hex(text);
}
std::string password_hash(const std::string& password, const std::string& salt) {
    return crypto::pbkdf2_hmac_sha256_hex(password, salt, 2000);
}
Json strings(const std::vector<std::string>& values) {
    auto out = Json::array(); for (const auto& value : values) out.push_back(Json(value)); return out;
}
std::vector<std::string> string_list(const Json& value) {
    if (!value.is_array() || value.as_array().size() > 100) throw std::invalid_argument("expected array of at most 100 strings");
    std::vector<std::string> out;
    for (std::size_t i = 0; i < value.as_array().size(); ++i) {
        auto item = value.at(i).as_string();
        if (item.empty() || item.size() > 100 || std::find(out.begin(), out.end(), item) != out.end())
            throw std::invalid_argument("invalid or duplicate array entry");
        out.push_back(std::move(item));
    }
    return out;
}
std::string text(const Json& object, const std::string& key, std::size_t max) {
    auto value = object.at(key).as_string();
    if (value.empty() || value.size() > max || value.find('\0') != std::string::npos)
        throw std::invalid_argument("invalid " + key);
    return value;
}
std::int64_t integer(const Json& object, const std::string& key, std::int64_t min, std::int64_t max) {
    auto value = object.at(key).as_int64();
    if (value < min || value > max) throw std::invalid_argument("invalid " + key);
    return value;
}
Json body(const Request& request) {
    auto value = Json::parse(request.body);
    if (!value.is_object()) throw std::invalid_argument("body must be a JSON object");
    return value;
}
Response json(int status, const Json& value) {
    return {status, "application/json; charset=utf-8", value.dump(), {{"Cache-Control", "no-store"}}};
}
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

Store::Store(const AppOptions& options) : db(options.database_path), clock_(options.clock) {
    if (!clock_) clock_ = [] { return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count(); };

    execute(db, R"SQL(
        CREATE TABLE IF NOT EXISTS roles(
            code VARCHAR(64) PRIMARY KEY,
            name VARCHAR(200) NOT NULL,
            scope_type VARCHAR(16) NOT NULL,
            permissions TEXT NOT NULL
        );
        CREATE TABLE IF NOT EXISTS permission_catalog(
            code VARCHAR(100) PRIMARY KEY,
            name VARCHAR(200) NOT NULL
        );
        CREATE TABLE IF NOT EXISTS users(
            id VARCHAR(64) PRIMARY KEY,
            username VARCHAR(100) UNIQUE NOT NULL,
            password_hash VARCHAR(128) NOT NULL,
            salt VARCHAR(128) NOT NULL,
            role VARCHAR(64) NOT NULL,
            org_id VARCHAR(100) NOT NULL,
            class_id VARCHAR(100) NOT NULL,
            student_no VARCHAR(100) NOT NULL,
            active TINYINT NOT NULL DEFAULT 1,
            CONSTRAINT fk_users_role FOREIGN KEY (role) REFERENCES roles(code)
        );
        CREATE TABLE IF NOT EXISTS sessions(
            token_hash VARCHAR(128) PRIMARY KEY,
            user_id VARCHAR(64) NOT NULL,
            expires_at BIGINT NOT NULL,
            CONSTRAINT fk_sessions_user FOREIGN KEY (user_id) REFERENCES users(id)
        );
        CREATE TABLE IF NOT EXISTS audit_logs(
            id BIGINT AUTO_INCREMENT PRIMARY KEY,
            created_at BIGINT NOT NULL,
            actor_id VARCHAR(64) NOT NULL,
            action VARCHAR(100) NOT NULL,
            resource_type VARCHAR(64) NOT NULL,
            resource_id VARCHAR(100) NOT NULL,
            outcome VARCHAR(16) NOT NULL,
            reason VARCHAR(255) NOT NULL,
            request_id VARCHAR(100) NOT NULL,
            before_json LONGTEXT NOT NULL,
            after_json LONGTEXT NOT NULL,
            INDEX idx_audit_logs_time (created_at)
        );
        CREATE TABLE IF NOT EXISTS services(
            id VARCHAR(64) PRIMARY KEY,
            name VARCHAR(100) NOT NULL,
            provider_id VARCHAR(100) NOT NULL,
            version VARCHAR(32) NOT NULL,
            fields TEXT NOT NULL,
            scope TEXT NOT NULL,
            rate_per_minute INT NOT NULL,
            active TINYINT NOT NULL DEFAULT 1,
            bucket BIGINT NOT NULL DEFAULT 0,
            bucket_count INT NOT NULL DEFAULT 0,
            UNIQUE KEY uq_services_name_version (name, version)
        );
        CREATE TABLE IF NOT EXISTS applications(
            id VARCHAR(64) PRIMARY KEY,
            owner_id VARCHAR(64) NOT NULL,
            service_id VARCHAR(64) NOT NULL,
            purpose TEXT NOT NULL,
            scope TEXT NOT NULL,
            validity_seconds INT NOT NULL,
            quota INT NOT NULL,
            rate_per_minute INT NOT NULL,
            status VARCHAR(16) NOT NULL DEFAULT 'pending',
            reason VARCHAR(255) NOT NULL DEFAULT '',
            created_at BIGINT NOT NULL,
            CONSTRAINT fk_applications_owner FOREIGN KEY (owner_id) REFERENCES users(id),
            CONSTRAINT fk_applications_service FOREIGN KEY (service_id) REFERENCES services(id)
        );
        CREATE TABLE IF NOT EXISTS grants(
            id VARCHAR(64) PRIMARY KEY,
            application_id VARCHAR(64) NOT NULL UNIQUE,
            owner_id VARCHAR(64) NOT NULL,
            service_id VARCHAR(64) NOT NULL,
            scope TEXT NOT NULL,
            expires_at BIGINT NOT NULL,
            quota INT NOT NULL,
            used INT NOT NULL DEFAULT 0,
            rate_per_minute INT NOT NULL,
            mask_level VARCHAR(16) NOT NULL,
            token_hash VARCHAR(128) NOT NULL DEFAULT '',
            revoked TINYINT NOT NULL DEFAULT 0,
            bucket BIGINT NOT NULL DEFAULT 0,
            bucket_count INT NOT NULL DEFAULT 0,
            CONSTRAINT fk_grants_application FOREIGN KEY (application_id) REFERENCES applications(id),
            CONSTRAINT fk_grants_owner FOREIGN KEY (owner_id) REFERENCES users(id),
            CONSTRAINT fk_grants_service FOREIGN KEY (service_id) REFERENCES services(id),
            INDEX idx_grants_token (token_hash)
        );
        CREATE TABLE IF NOT EXISTS masking_rules(
            field VARCHAR(100) PRIMARY KEY,
            algorithm VARCHAR(16) NOT NULL
        );
        CREATE TABLE IF NOT EXISTS settings(
            setting_key VARCHAR(100) PRIMARY KEY,
            value TEXT NOT NULL
        );
        CREATE TABLE IF NOT EXISTS call_logs(
            id BIGINT AUTO_INCREMENT PRIMARY KEY,
            created_at BIGINT NOT NULL,
            service_id VARCHAR(64) NOT NULL,
            grant_id VARCHAR(64) NOT NULL,
            actor_id VARCHAR(64) NOT NULL,
            status INT NOT NULL,
            reason VARCHAR(255) NOT NULL,
            request_id VARCHAR(100) NOT NULL,
            INDEX idx_call_logs_service (service_id, created_at)
        );
    )SQL");

    Transaction tx(db);
    const auto role = [&](const std::string& code, const std::string& name, const std::string& scope,
                          std::vector<std::string> permissions) {
        for (const auto& permission : permissions)
            Statement(db, "INSERT IGNORE INTO permission_catalog VALUES(?,?)")
                .bind(1, permission).bind(2, permission).step();
        Statement(db, "INSERT IGNORE INTO roles VALUES(?,?,?,?)")
            .bind(1, code).bind(2, name).bind(3, scope)
            .bind(4, strings(permissions).dump()).step();
    };
    role("system_admin", "系统管理员", "all", {"users.manage", "roles.manage", "masking.manage",
        "audit.read", "ingestion.read", "ingestion.write", "profile.read", "profile.write",
        "governance.read", "governance.write", "alert.read", "alert.write"});
    role("data_admin", "数据管理员", "all", {"services.publish", "grants.approve", "services.read",
        "ingestion.read", "ingestion.write", "profile.read", "profile.write",
        "governance.read", "governance.write", "alert.read", "alert.write"});
    role("college_admin", "院系管理员", "college", {"grants.apply", "services.read", "profile.read",
        "alert.read", "alert.write"});
    role("third_party", "第三方应用", "college", {"grants.apply", "services.read"});
    role("career_center", "就业指导中心", "all", {"career.jobs.read", "career.jobs.write",
        "career.jobs.publish", "career.strategy.write", "career.outcomes.read"});
    role("student", "学生", "self", {"career.recommendations.read", "profile.read"});
    role("counselor", "辅导员", "class", {"profile.read", "alert.read", "alert.write"});

    // 给已有数据库追加本轮新增权限，保留管理员自定义的其它权限。
    const auto grant_permissions = [&](const std::string& role_code,
                                       std::initializer_list<const char*> additions) {
        Statement row(db, "SELECT permissions FROM roles WHERE code=?");
        row.bind(1, role_code);
        if (!row.step()) return;
        Json current = Json::parse(row.text(0));
        std::vector<std::string> values;
        for (const auto& item : current.as_array()) values.push_back(item.as_string());
        bool changed = false;
        for (const char* permission : additions) {
            if (std::find(values.begin(), values.end(), permission) == values.end()) {
                values.emplace_back(permission);
                Statement(db, "INSERT IGNORE INTO permission_catalog VALUES(?,?)")
                    .bind(1, permission).bind(2, permission).step();
                changed = true;
            }
        }
        if (changed)
            Statement(db, "UPDATE roles SET permissions=? WHERE code=?")
                .bind(1, strings(values).dump()).bind(2, role_code).step();
    };
    grant_permissions("system_admin",
                      {"ingestion.read", "ingestion.write", "profile.read", "profile.write",
                       "governance.read", "governance.write", "alert.read", "alert.write"});
    grant_permissions("data_admin",
                      {"ingestion.read", "ingestion.write", "profile.read", "profile.write",
                       "governance.read", "governance.write", "alert.read", "alert.write"});
    grant_permissions("college_admin", {"profile.read", "alert.read", "alert.write"});
    grant_permissions("student", {"profile.read"});
    grant_permissions("counselor", {"profile.read", "alert.read", "alert.write"});
    for (const auto& field : {"student_no", "id_card", "phone", "name"})
        Statement(db, "INSERT IGNORE INTO masking_rules VALUES(?,'partial')").bind(1, field).step();
    for (const auto& field : {"email", "score", "address", "birth_date", "contact_info"})
        Statement(db, "INSERT IGNORE INTO masking_rules VALUES(?,'redact')").bind(1, field).step();
    Statement(db, "INSERT IGNORE INTO settings (setting_key, value) VALUES('mask_key',?)")
        .bind(1, random_token("")).step();
    Statement count(db, "SELECT count(*) FROM users");
    count.step();
    if (count.integer(0) == 0 && !options.bootstrap_password.empty()) {
        if (options.bootstrap_password.size() < 12)
            throw std::invalid_argument("bootstrap password must have at least 12 characters");
        const auto salt = random_token("");
        Statement(db, "INSERT INTO users VALUES(?,?,?,?,?,'','','',1)")
            .bind(1, random_token("u_")).bind(2, options.bootstrap_username)
            .bind(3, password_hash(options.bootstrap_password, salt)).bind(4, salt)
            .bind(5, "system_admin").step();
    } else if (count.integer(0) == 0 && options.require_bootstrap) {
        throw std::invalid_argument("first start requires PLATFORM_ADMIN_PASSWORD (at least 12 characters)");
    }
    tx.commit();
}
void Store::audit(const std::string& actor, const security::AuditEvent& event) {
    if (actor.empty() || event.action.empty() || event.resource_type.empty() || event.resource_id.empty() ||
        (event.outcome != "success" && event.outcome != "failure")) throw std::invalid_argument("incomplete audit event");
    Statement(db, "INSERT INTO audit_logs(created_at,actor_id,action,resource_type,resource_id,outcome,reason,request_id,before_json,after_json) VALUES(?,?,?,?,?,?,?,?,?,?)")
        .bind(1, now()).bind(2, actor).bind(3, event.action).bind(4, event.resource_type).bind(5, event.resource_id)
        .bind(6, event.outcome).bind(7, event.reason).bind(8, event.request_id.empty() ? random_token("req_") : event.request_id)
        .bind(9, scrub(event.before).dump()).bind(10, scrub(event.after).dump()).step();
}

Handler guarded(const std::shared_ptr<Store>& store, Handler handler) {
    return [store, handler = std::move(handler)](const Request& request) {
        std::lock_guard<std::recursive_mutex> lock(store->mutex);
        const auto record_failure = [&](const std::string& reason) {
            std::string actor = "anonymous";
            const auto header = request.headers.find("authorization");
            if (header != request.headers.end() && header->second.rfind("Bearer usr_", 0) == 0) {
                Statement row(store->db, "SELECT user_id FROM sessions WHERE token_hash=? AND expires_at>?");
                row.bind(1, digest(header->second.substr(7))).bind(2, store->now());
                if (row.step()) actor = row.text(0);
            }
            security::AuditEvent event{"request.rejected", "endpoint", request.method + " " + request.path};
            event.outcome = "failure"; event.reason = reason; store->audit(actor, event);
        };
        try { return handler(request); }
        catch (const security::ApiError& error) {
            record_failure(error.code); return security::api_error(error.status, error.code, error.what());
        } catch (const std::invalid_argument& error) {
            record_failure("invalid_request"); return security::api_error(400, "invalid_request", error.what());
        } catch (const std::exception&) {
            record_failure("internal_error"); return security::api_error(500, "internal_error", "operation could not be completed");
        }
    };
}
std::map<std::string, std::string> query_parameters(const std::string& query) {
    const auto decode = [](const std::string& value) {
        std::string out;
        for (std::size_t i = 0; i < value.size(); ++i) {
            if (value[i] == '%') {
                if (i + 2 >= value.size()) throw std::invalid_argument("invalid query encoding");
                unsigned byte = 0;
                const auto rc = std::from_chars(value.data() + i + 1, value.data() + i + 3, byte, 16);
                if (rc.ec != std::errc{} || rc.ptr != value.data() + i + 3 || byte == 0) throw std::invalid_argument("invalid query encoding");
                out += static_cast<char>(byte); i += 2;
            } else out += value[i] == '+' ? ' ' : value[i];
        }
        return out;
    };
    std::map<std::string, std::string> result;
    for (std::size_t start = 0; start < query.size();) {
        const auto end = query.find('&', start), equals = query.find('=', start);
        const auto stop = end == std::string::npos ? query.size() : end;
        if (equals == std::string::npos || equals >= stop) throw std::invalid_argument("query needs key=value");
        const auto key = decode(query.substr(start, equals - start));
        if (!result.emplace(key, decode(query.substr(equals + 1, stop - equals - 1))).second)
            throw std::invalid_argument("duplicate query parameter");
        start = stop + 1;
    }
    return result;
}
std::int64_t query_number(const std::map<std::string, std::string>& query, const std::string& key,
                          std::int64_t fallback, std::int64_t min, std::int64_t max) {
    const auto it = query.find(key); if (it == query.end()) return fallback;
    std::int64_t value = 0;
    const auto rc = std::from_chars(it->second.data(), it->second.data() + it->second.size(), value);
    if (rc.ec != std::errc{} || rc.ptr != it->second.data() + it->second.size() || value < min || value > max)
        throw std::invalid_argument("invalid query " + key);
    return value;
}
}  // namespace platform::data_services
