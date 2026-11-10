#include "store.hpp"

#include <openssl/crypto.h>
#include <algorithm>
#include <limits>
#include <exception>

namespace platform::security {
using namespace data_services;

Json scope_json(const Scope& scope) {
    auto out = Json::object(); out.set("type", Json::text(scope.type));
    if (!scope.org_id.empty()) out.set("org_id", Json::text(scope.org_id));
    if (!scope.class_id.empty()) out.set("class_id", Json::text(scope.class_id));
    if (!scope.student_no.empty()) out.set("student_no", Json::text(scope.student_no));
    return out;
}
bool contains(const Scope& outer, const Scope& inner) {
    if (outer.type == "all") return true;
    if (inner.type == "all" || outer.org_id != inner.org_id || outer.org_id.empty()) return false;
    if (outer.type == "college") return true;
    if (inner.type == "college" || outer.class_id != inner.class_id || outer.class_id.empty()) return false;
    if (outer.type == "class") return true;
    return outer.type == "self" && inner.type == "self" && !outer.student_no.empty() && outer.student_no == inner.student_no;
}
Json principal_json(const Principal& p) {
    auto out = Json::object();
    out.set("user_id", Json::text(p.user_id)); out.set("username", Json::text(p.username));
    out.set("role", Json::text(p.role)); out.set("org_id", Json::text(p.org_id));
    out.set("class_id", Json::text(p.class_id)); out.set("student_no", p.student_no.empty() ? Json{} : Json::text(p.student_no));
    out.set("scope", scope_json(p.scope)); out.set("permissions", strings(p.permissions)); return out;
}
Response api_error(int status, const std::string& code, const std::string& message) {
    auto out = Json::object(); out.set("error", Json::text(code)); out.set("message", Json::text(message));
    auto response = json(status, out);
    if (status == 401) response.headers["WWW-Authenticate"] = "Bearer";
    response.headers["Cache-Control"] = "no-store";
    return response;
}
Security::Security(std::shared_ptr<Store> store) : store_(std::move(store)) {}
Principal Security::authenticate(const Request& request) const {
    std::lock_guard<std::recursive_mutex> lock(store_->mutex);
    const auto auth = request.headers.find("authorization");
    if (auth == request.headers.end() || auth->second.rfind("Bearer usr_", 0) != 0)
        throw ApiError(401, "unauthorized", "login required");
    Statement row(store_->db, "SELECT u.id,u.username,u.role,u.org_id,u.class_id,u.student_no,r.scope_type,r.permissions "
        "FROM sessions s JOIN users u ON u.id=s.user_id JOIN roles r ON r.code=u.role "
        "WHERE s.token_hash=? AND s.expires_at>? AND u.active=1");
    row.bind(1, digest(auth->second.substr(7))).bind(2, store_->now());
    if (!row.step()) throw ApiError(401, "unauthorized", "login invalid or expired");
    Principal p{row.text(0), row.text(1), row.text(2), row.text(3), row.text(4), row.text(5), {}, {}};
    p.scope = {row.text(6), p.org_id, p.class_id, p.student_no};
    p.permissions = string_list(Json::parse(row.text(7)));
    return p;
}
bool Security::permitted(const Principal& p, const std::string& permission) const {
    return std::find(p.permissions.begin(), p.permissions.end(), permission) != p.permissions.end();
}
void Security::register_permission(const std::string& code, const std::string& name) const {
    if (code.empty() || code.size() > 100 || code.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_.-") != std::string::npos ||
        name.empty() || name.size() > 200) throw std::invalid_argument("invalid permission metadata");
    std::lock_guard<std::recursive_mutex> lock(store_->mutex);
    Statement(store_->db, "INSERT INTO permission_catalog VALUES(?,?) ON CONFLICT(code) DO UPDATE SET name=excluded.name")
        .bind(1, code).bind(2, name).step();
}
Principal Security::authorize(const Request& request, const std::string& permission, const Scope& resource) const {
    Principal p;
    try { p = authenticate(request); }
    catch (const ApiError&) {
        std::lock_guard<std::recursive_mutex> lock(store_->mutex);
        AuditEvent event{"authentication.denied", "permission", permission};
        event.outcome = "failure"; event.reason = "unauthorized"; store_->audit("anonymous", event); throw;
    }
    const bool global_admin = permission == "users.manage" || permission == "roles.manage" ||
        permission == "masking.manage" || permission == "audit.read";
    if (!permitted(p, permission) || (global_admin && p.scope.type != "all") ||
        (!resource.type.empty() && !contains(p.scope, resource))) {
        std::lock_guard<std::recursive_mutex> lock(store_->mutex);
        AuditEvent event{"permission.denied", "permission", permission};
        event.outcome = "failure"; event.reason = "forbidden";
        store_->audit(p.user_id, event);
        throw ApiError(403, "forbidden", "permission or data scope denied");
    }
    return p;
}
void Security::audited_write(const Principal& actor, AuditEvent& event, const std::function<void(sqlite3*)>& write) const {
    if (actor.user_id.empty() || event.action.empty() || event.resource_type.empty() || event.resource_id.empty() || event.outcome != "success")
        throw std::invalid_argument("incomplete successful write audit event");
    std::lock_guard<std::recursive_mutex> lock(store_->mutex);
    try {
        Transaction tx(store_->db);
        write(store_->db); store_->audit(actor.user_id, event); tx.commit();
    } catch (...) {
        const auto original = std::current_exception();
        auto failed = event; failed.outcome = "failure"; failed.reason = "transaction_failed";
        try { store_->audit(actor.user_id, failed); }
        catch (...) { throw ApiError(500, "audit_unavailable", "write rolled back; audit storage unavailable"); }
        std::rethrow_exception(original);
    }
}
}  // namespace platform::security

namespace platform::data_services {
using security::ApiError;
using security::AuditEvent;
security::Principal load_principal(Store& store, const std::string& user_id) {
    Statement row(store.db, "SELECT u.id,u.username,u.role,u.org_id,u.class_id,u.student_no,r.scope_type,r.permissions "
        "FROM users u JOIN roles r ON r.code=u.role WHERE u.id=? AND u.active=1"); row.bind(1, user_id);
    if (!row.step()) throw ApiError(403, "forbidden", "account is inactive or unavailable");
    security::Principal p{row.text(0), row.text(1), row.text(2), row.text(3), row.text(4), row.text(5), {}, {}};
    p.scope = {row.text(6), p.org_id, p.class_id, p.student_no}; p.permissions = string_list(Json::parse(row.text(7))); return p;
}
namespace {
std::string optional_text(const Json& value, const std::string& field) {
    return value.has(field) ? text(value, field, 100) : "";
}
Json user_json(Statement& row) {
    auto out = Json::object();
    const char* keys[] = {"user_id", "username", "role", "org_id", "class_id", "student_no"};
    for (int i = 0; i < 6; ++i) out.set(keys[i], Json::text(row.text(i)));
    out.set("active", Json::boolean(row.integer(6) != 0)); return out;
}
void validate_scope_identity(const std::string& type, const std::string& org,
                             const std::string& cls, const std::string& student) {
    if ((type != "all" && org.empty()) || ((type == "class" || type == "self") && cls.empty()) ||
        (type == "self" && student.empty())) throw std::invalid_argument("role requires organization, class or student identity");
}
}  // namespace

void install_identity_routes(App& app, const std::shared_ptr<security::Security>& sec) {
    auto store = sec->store();
    const auto add = [&](const std::string& method, const std::string& path, Handler handler) {
        app.add_module_route("data-services", method, path, guarded(store, std::move(handler)));
    };
    add("POST", "/login", [store](const Request& request) {
        const auto input = body(request);
        const auto username = text(input, "username", 100);
        const auto password = text(input, "password", 1024);
        const auto resource = digest(username);
        Statement attempts(store->db, "SELECT count(*) FROM audit_logs WHERE action='login' AND resource_id=? AND outcome='failure' AND created_at>?");
        attempts.bind(1, resource).bind(2, store->now() - 60); attempts.step();
        if (attempts.integer(0) >= 5) throw ApiError(429, "rate_limited", "retry login after one minute");
        Statement user(store->db, "SELECT id,password_hash,salt,active FROM users WHERE username=?");
        user.bind(1, username); const bool found = user.step();
        // Use the same expensive hash for unknown users to avoid a cheap username probe.
        const auto hash = password_hash(password, found ? user.text(2) : "unknown-user-fixed-salt");
        const auto expected = found ? user.text(1) : std::string(64, '0');
        AuditEvent event{"login", "identity", resource};
        if (!found || user.integer(3) == 0 || CRYPTO_memcmp(hash.data(), expected.data(), hash.size()) != 0) {
            event.outcome = "failure"; event.reason = "invalid_credentials";
            store->audit("anonymous", event);
            throw ApiError(401, "unauthorized", "invalid credentials");
        }
        const auto token = random_token("usr_");
        const auto expires = store->now() + 3600;
        Transaction tx(store->db);
        Statement(store->db, "DELETE FROM sessions WHERE expires_at<=?").bind(1, store->now()).step();
        Statement(store->db, "INSERT INTO sessions VALUES(?,?,?)").bind(1, digest(token)).bind(2, user.text(0)).bind(3, expires).step();
        store->audit(user.text(0), event); tx.commit();
        auto result = Json::object(); result.set("access_token", Json::text(token));
        result.set("token_type", Json::text("Bearer")); result.set("expires_at", Json::number(expires));
        auto response = json(200, result); response.headers["Cache-Control"] = "no-store"; return response;
    });
    add("GET", "/me", [sec](const Request& request) {
        auto response = json(200, security::principal_json(sec->authenticate(request)));
        response.headers["Cache-Control"] = "no-store"; return response;
    });
    add("POST", "/logout", [sec, store](const Request& request) {
        const auto p = sec->authenticate(request);
        AuditEvent event{"logout", "identity", p.user_id};
        sec->audited_write(p, event, [&](sqlite3* db) {
            Statement(db, "DELETE FROM sessions WHERE token_hash=?").bind(1, digest(request.headers.at("authorization").substr(7))).step();
        });
        return json(200, Json::object());
    });
    add("GET", "/users", [sec, store](const Request& request) {
        sec->authorize(request, "users.manage");
        Statement rows(store->db, "SELECT id,username,role,org_id,class_id,student_no,active FROM users ORDER BY username");
        auto result = Json::array(); while (rows.step()) {
            auto item = user_json(rows);
            if (!rows.text(5).empty()) item.set("student_no", Json::text("***"));
            result.push(item);
        } return json(200, result);
    });
    add("POST", "/users", [sec, store](const Request& request) {
        const auto p = sec->authorize(request, "users.manage"); const auto input = body(request);
        const auto username = text(input, "username", 100), password = text(input, "password", 1024);
        if (password.size() < 12) throw std::invalid_argument("password must have at least 12 characters");
        const auto role = text(input, "role", 100);
        const auto org = optional_text(input, "org_id"), cls = optional_text(input, "class_id"), student = optional_text(input, "student_no");
        Statement role_row(store->db, "SELECT scope_type FROM roles WHERE code=?"); role_row.bind(1, role);
        if (!role_row.step()) throw std::invalid_argument("unknown role");
        validate_scope_identity(role_row.text(0), org, cls, student);
        Statement duplicate(store->db, "SELECT 1 FROM users WHERE username=? OR (student_no<>'' AND student_no=?)");
        duplicate.bind(1, username).bind(2, student);
        if (duplicate.step()) throw ApiError(409, "conflict", "username or student identity already exists");
        const auto id = random_token("u_"), salt = random_token("");
        const auto hashed = password_hash(password, salt);
        AuditEvent event{"user.create", "user", id}; event.after = input;
        sec->audited_write(p, event, [&](sqlite3* db) {
            Statement(db, "INSERT INTO users VALUES(?,?,?,?,?,?,?,?,1)").bind(1, id).bind(2, username).bind(3, hashed)
                .bind(4, salt).bind(5, role).bind(6, org).bind(7, cls).bind(8, student).step();
        });
        auto result = Json::object(); result.set("user_id", Json::text(id)); return json(201, result);
    });
    add("PATCH", "/users/:id", [sec, store](const Request& request) {
        const auto p = sec->authorize(request, "users.manage"); const auto input = body(request);
        const auto id = request.path_params.at("id");
        Statement row(store->db, "SELECT id,username,role,org_id,class_id,student_no,active FROM users WHERE id=?"); row.bind(1, id);
        if (!row.step()) throw ApiError(404, "not_found", "user not found");
        const auto before = user_json(row);
        const auto role = input.has("role") ? text(input, "role", 100) : row.text(2);
        const bool active = input.has("active") ? input.get("active").boolean() : row.integer(6) != 0;
        if (id == p.user_id && (!active || role != p.role)) throw ApiError(409, "conflict", "cannot disable or change your own role");
        Statement role_row(store->db, "SELECT scope_type FROM roles WHERE code=?"); role_row.bind(1, role);
        if (!role_row.step()) throw std::invalid_argument("unknown role");
        auto org = input.has("org_id") ? text(input, "org_id", 100) : row.text(3);
        auto cls = input.has("class_id") ? text(input, "class_id", 100) : row.text(4);
        auto student = input.has("student_no") ? text(input, "student_no", 100) : row.text(5);
        validate_scope_identity(role_row.text(0), org, cls, student);
        Statement duplicate(store->db, "SELECT 1 FROM users WHERE student_no<>'' AND student_no=? AND id<>?"); duplicate.bind(1, student).bind(2, id);
        if (duplicate.step()) throw ApiError(409, "conflict", "student identity already exists");
        std::string salt, hash;
        if (input.has("password")) {
            const auto password = text(input, "password", 1024);
            if (password.size() < 12) throw std::invalid_argument("password must have at least 12 characters");
            salt = random_token(""); hash = password_hash(password, salt);
        }
        AuditEvent event{"user.update", "user", id}; event.before = before; event.after = input;
        sec->audited_write(p, event, [&](sqlite3* db) {
            Statement(db, "UPDATE users SET role=?,org_id=?,class_id=?,student_no=?,active=? WHERE id=?")
                .bind(1, role).bind(2, org).bind(3, cls).bind(4, student).bind(5, active ? 1 : 0).bind(6, id).step();
            if (!salt.empty()) Statement(db, "UPDATE users SET password_hash=?,salt=? WHERE id=?").bind(1, hash).bind(2, salt).bind(3, id).step();
            Statement(db, "DELETE FROM sessions WHERE user_id=?").bind(1, id).step();
        });
        return json(200, Json::object());
    });
    add("GET", "/roles", [sec, store](const Request& request) {
        const auto actor = sec->authenticate(request);
        sec->authorize(request, sec->permitted(actor, "roles.manage") ? "roles.manage" : "users.manage");
        auto result = Json::array();
        Statement rows(store->db, "SELECT code,name,scope_type,permissions FROM roles ORDER BY code");
        while (rows.step()) {
            auto item = Json::object(); item.set("code", Json::text(rows.text(0))); item.set("name", Json::text(rows.text(1)));
            item.set("scope_type", Json::text(rows.text(2))); item.set("permissions", Json::parse(rows.text(3))); result.push(item);
        }
        return json(200, result);
    });
    add("PUT", "/roles/:code", [sec, store](const Request& request) {
        const auto p = sec->authorize(request, "roles.manage"); const auto input = body(request);
        const auto code = request.path_params.at("code"), name = text(input, "name", 100), type = text(input, "scope_type", 16);
        if (code == "system_admin") throw ApiError(409, "conflict", "system_admin role is reserved");
        if (code.empty() || code.size() > 100 || code.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") != std::string::npos)
            throw std::invalid_argument("invalid role code");
        if (type != "all" && type != "college" && type != "class" && type != "self") throw std::invalid_argument("invalid scope type");
        const auto codes = string_list(input.get("permissions"));
        for (const auto& permission : codes) {
            Statement known(store->db, "SELECT 1 FROM permission_catalog WHERE code=?"); known.bind(1, permission);
            if (!known.step()) throw std::invalid_argument("unknown permission code");
        }
        Statement users(store->db, "SELECT org_id,class_id,student_no FROM users WHERE role=?"); users.bind(1, code);
        while (users.step()) validate_scope_identity(type, users.text(0), users.text(1), users.text(2));
        Statement old(store->db, "SELECT name,scope_type,permissions FROM roles WHERE code=?"); old.bind(1, code);
        AuditEvent event{"role.update", "role", code}; event.after = input;
        if (old.step()) {
            event.before = Json::object(); event.before.set("name", Json::text(old.text(0)));
            event.before.set("scope_type", Json::text(old.text(1))); event.before.set("permissions", Json::parse(old.text(2)));
        }
        sec->audited_write(p, event, [&](sqlite3* db) {
            Statement(db, "INSERT INTO roles VALUES(?,?,?,?) ON CONFLICT(code) DO UPDATE SET name=excluded.name,scope_type=excluded.scope_type,permissions=excluded.permissions")
                .bind(1, code).bind(2, name).bind(3, type).bind(4, strings(codes).dump()).step();
        });
        return json(200, input);
    });
    add("GET", "/permissions", [sec, store](const Request& request) {
        const auto actor = sec->authenticate(request);
        sec->authorize(request, sec->permitted(actor, "roles.manage") ? "roles.manage" : "users.manage");
        Statement rows(store->db, "SELECT code,name FROM permission_catalog ORDER BY code"); auto out = Json::array();
        while (rows.step()) {
            auto item = Json::object(); item.set("code", Json::text(rows.text(0))); item.set("name", Json::text(rows.text(1))); out.push(item);
        } return json(200, out);
    });
    add("GET", "/audit-logs", [sec, store](const Request& request) {
        sec->authorize(request, "audit.read"); auto result = Json::array(); const auto query = query_parameters(request.query);
        auto sql = std::string("SELECT id,created_at,actor_id,action,resource_type,resource_id,outcome,reason,request_id,before_json,after_json FROM audit_logs WHERE created_at>=? AND created_at<=?");
        std::vector<std::string> filters;
        for (const auto& key : {"actor_id", "action", "resource_type", "outcome"}) if (query.count(key)) { sql += " AND " + std::string(key) + "=?"; filters.push_back(query.at(key)); }
        sql += " ORDER BY id DESC LIMIT ? OFFSET ?";
        Statement rows(store->db, sql);
        rows.bind(1, query_number(query, "from", 0, 0, std::numeric_limits<std::int64_t>::max()))
            .bind(2, query_number(query, "to", std::numeric_limits<std::int64_t>::max(), 0, std::numeric_limits<std::int64_t>::max()));
        int index = 3; for (const auto& value : filters) rows.bind(index++, value);
        rows.bind(index++, query_number(query, "limit", 100, 1, 200)); rows.bind(index, query_number(query, "offset", 0, 0, 100000000));
        while (rows.step()) {
            auto item = Json::object(); item.set("id", Json::number(rows.integer(0))); item.set("created_at", Json::number(rows.integer(1)));
            const char* fields[] = {"actor_id", "action", "resource_type", "resource_id", "outcome", "reason", "request_id"};
            for (int i = 2; i <= 8; ++i) item.set(fields[i - 2], Json::text(rows.text(i)));
            item.set("before", Json::parse(rows.text(9))); item.set("after", Json::parse(rows.text(10))); result.push(item);
        }
        return json(200, result);
    });
}
}  // namespace platform::data_services
