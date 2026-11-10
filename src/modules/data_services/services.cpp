#include "store.hpp"
#include <algorithm>

namespace platform::data_services {
using security::ApiError;
using security::AuditEvent;
using security::Scope;
namespace {
void ensure(bool condition, int status, const std::string& code, const std::string& message) {
    if (!condition) throw ApiError(status, code, message);
}
void identifier(const std::string& value) {
    if (value.empty() || value.size() > 100 || value.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-") != std::string::npos)
        throw std::invalid_argument("invalid identifier");
}
Json application_json(Statement& row) {
    auto item = Json::object();
    const char* keys[] = {"id", "owner_id", "service_id", "purpose"};
    for (int i = 0; i < 4; ++i) item.set(keys[i], Json::text(row.text(i)));
    item.set("scope", Json::parse(row.text(4))); item.set("validity_seconds", Json::number(row.integer(5)));
    item.set("quota", Json::number(row.integer(6))); item.set("rate_per_minute", Json::number(row.integer(7)));
    item.set("status", Json::text(row.text(8))); item.set("reason", Json::text(row.text(9)));
    item.set("created_at", Json::number(row.integer(10))); return item;
}
void add_grant(Store& store, Json& item) {
    Statement row(store.db, "SELECT id,scope,expires_at,quota,used,rate_per_minute,mask_level,revoked FROM grants WHERE application_id=?");
    row.bind(1, item.get("id").string());
    if (!row.step()) return;
    auto grant = Json::object(); grant.set("id", Json::text(row.text(0))); grant.set("scope", Json::parse(row.text(1)));
    grant.set("expires_at", Json::number(row.integer(2))); grant.set("quota", Json::number(row.integer(3)));
    grant.set("used", Json::number(row.integer(4))); grant.set("rate_per_minute", Json::number(row.integer(5)));
    grant.set("mask_level", Json::text(row.text(6))); grant.set("revoked", Json::boolean(row.integer(7) != 0));
    item.set("grant", grant);
}
}  // namespace
Json service_json(Statement& row) {
    auto item = Json::object();
    item.set("id", Json::text(row.text(0))); item.set("name", Json::text(row.text(1)));
    item.set("provider_id", Json::text(row.text(2))); item.set("version", Json::text(row.text(3)));
    item.set("fields", Json::parse(row.text(4))); item.set("scope", Json::parse(row.text(5)));
    item.set("rate_per_minute", Json::number(row.integer(6))); item.set("active", Json::boolean(row.integer(7) != 0));
    return item;
}
void install_service_routes(App& app, const std::shared_ptr<security::Security>& sec) {
    auto store = sec->store();
    const auto add = [&](const std::string& method, const std::string& path, Handler handler) {
        app.add_module_route("data-services", method, path, guarded(store, std::move(handler)));
    };
    add("GET", "/services", [sec, store](const Request& request) {
        const auto p = sec->authorize(request, "services.read");
        Statement rows(store->db, "SELECT id,name,provider_id,version,fields,scope,rate_per_minute,active FROM services ORDER BY name,version");
        auto result = Json::array();
        while (rows.step()) {
            const auto scope = parse_scope(Json::parse(rows.text(5)));
            if (security::contains(scope, p.scope) || security::contains(p.scope, scope)) result.push(service_json(rows));
        }
        return json(200, result);
    });
    add("POST", "/services", [sec, store](const Request& request) {
        const auto p = sec->authorize(request, "services.publish"); const auto input = body(request);
        const auto name = text(input, "name", 100), provider = text(input, "provider_id", 100), version = text(input, "version", 32);
        identifier(provider); identifier(version);
        const auto fields = string_list(input.get("fields"));
        if (fields.empty()) throw std::invalid_argument("at least one field must be published");
        for (const auto& field : fields) identifier(field);
        const auto scope = parse_scope(input.get("scope"));
        ensure(security::contains(p.scope, scope), 403, "forbidden", "service scope exceeds publisher scope");
        const auto rate = integer(input, "rate_per_minute", 1, 10000);
        Statement duplicate(store->db, "SELECT 1 FROM services WHERE name=? AND version=?"); duplicate.bind(1, name).bind(2, version);
        ensure(!duplicate.step(), 409, "conflict", "service version already exists");
        const auto id = random_token("s_"); AuditEvent event{"service.publish", "service", id}; event.after = input;
        sec->audited_write(p, event, [&](sqlite3* db) {
            Statement(db, "INSERT INTO services(id,name,provider_id,version,fields,scope,rate_per_minute) VALUES(?,?,?,?,?,?,?)")
                .bind(1, id).bind(2, name).bind(3, provider).bind(4, version).bind(5, strings(fields).dump())
                .bind(6, security::scope_json(scope).dump()).bind(7, rate).step();
        });
        auto result = Json::object(); result.set("id", Json::text(id)); return json(201, result);
    });
    add("PATCH", "/services/:id", [sec, store](const Request& request) {
        const auto p = sec->authorize(request, "services.publish"); const auto input = body(request);
        json_object_object_foreach(input.raw(), key, value) {
            (void)value;
            if (std::string(key) != "active" && std::string(key) != "rate_per_minute")
                throw std::invalid_argument("published configuration is immutable; publish a new version");
        }
        Statement row(store->db, "SELECT id,name,provider_id,version,fields,scope,rate_per_minute,active FROM services WHERE id=?");
        const auto id = request.path_params.at("id"); row.bind(1, id);
        ensure(row.step(), 404, "not_found", "service not found"); const auto before = service_json(row);
        ensure(security::contains(p.scope, parse_scope(before.get("scope"))), 403, "forbidden", "service scope denied");
        const bool active = input.has("active") ? input.get("active").boolean() : row.integer(7) != 0;
        const auto rate = input.has("rate_per_minute") ? integer(input, "rate_per_minute", 1, 10000) : row.integer(6);
        AuditEvent event{"service.update", "service", id}; event.before = before; event.after = input;
        sec->audited_write(p, event, [&](sqlite3* db) {
            Statement(db, "UPDATE services SET active=?,rate_per_minute=? WHERE id=?").bind(1, active ? 1 : 0).bind(2, rate).bind(3, id).step();
        }); return json(200, Json::object());
    });
    add("POST", "/applications", [sec, store](const Request& request) {
        const auto p = sec->authorize(request, "grants.apply"); const auto input = body(request);
        const auto service = text(input, "service_id", 100), purpose = text(input, "purpose", 1000);
        const auto scope = parse_scope(input.get("scope"));
        ensure(security::contains(p.scope, scope), 403, "forbidden", "requested scope exceeds account scope");
        Statement row(store->db, "SELECT scope,active,rate_per_minute FROM services WHERE id=?"); row.bind(1, service);
        ensure(row.step(), 404, "not_found", "service not found");
        ensure(row.integer(1) == 1, 409, "conflict", "service is suspended");
        ensure(security::contains(parse_scope(Json::parse(row.text(0))), scope), 403, "forbidden", "requested scope exceeds service scope");
        const auto validity = integer(input, "validity_seconds", 60, 31536000), quota = integer(input, "quota", 1, 1000000);
        const auto rate = integer(input, "rate_per_minute", 1, row.integer(2));
        const auto id = random_token("a_"); AuditEvent event{"grant.apply", "application", id}; event.after = input;
        sec->audited_write(p, event, [&](sqlite3* db) {
            Statement(db, "INSERT INTO applications(id,owner_id,service_id,purpose,scope,validity_seconds,quota,rate_per_minute,created_at) VALUES(?,?,?,?,?,?,?,?,?)")
                .bind(1, id).bind(2, p.user_id).bind(3, service).bind(4, purpose).bind(5, security::scope_json(scope).dump())
                .bind(6, validity).bind(7, quota).bind(8, rate).bind(9, store->now()).step();
        }); auto out = Json::object(); out.set("id", Json::text(id)); return json(201, out);
    });
    add("GET", "/applications", [sec, store](const Request& request) {
        const auto p = sec->authenticate(request); const bool approver = sec->permitted(p, "grants.approve");
        ensure(approver || sec->permitted(p, "grants.apply"), 403, "forbidden", "application access denied");
        Statement rows(store->db, "SELECT id,owner_id,service_id,purpose,scope,validity_seconds,quota,rate_per_minute,status,reason,created_at FROM applications ORDER BY created_at DESC,id");
        auto result = Json::array();
        while (rows.step()) {
            if (rows.text(1) != p.user_id && !(approver && security::contains(p.scope, parse_scope(Json::parse(rows.text(4)))))) continue;
            auto item = application_json(rows); add_grant(*store, item); result.push(item);
        } return json(200, result);
    });
    add("POST", "/applications/:id/decision", [sec, store](const Request& request) {
        const auto p = sec->authorize(request, "grants.approve"); const auto input = body(request);
        const auto id = request.path_params.at("id"), decision = text(input, "decision", 16);
        if (decision != "approve" && decision != "reject") throw std::invalid_argument("decision must be approve or reject");
        Statement row(store->db, "SELECT id,owner_id,service_id,purpose,scope,validity_seconds,quota,rate_per_minute,status,reason,created_at FROM applications WHERE id=?");
        row.bind(1, id); ensure(row.step(), 404, "not_found", "application not found");
        ensure(row.text(8) == "pending", 409, "conflict", "application already decided");
        ensure(row.text(1) != p.user_id, 403, "forbidden", "cannot approve your own application");
        const auto requested_scope = parse_scope(Json::parse(row.text(4)));
        ensure(security::contains(p.scope, requested_scope), 403, "forbidden", "approval scope denied");
        const auto reason = input.has("reason") ? text(input, "reason", 1000) : "";
        AuditEvent event{"grant." + decision, "application", id}; event.before = application_json(row); event.after = input;
        if (decision == "reject") {
            if (reason.empty()) throw std::invalid_argument("rejection requires reason");
            sec->audited_write(p, event, [&](sqlite3* db) {
                Statement(db, "UPDATE applications SET status='rejected',reason=? WHERE id=?").bind(1, reason).bind(2, id).step();
            }); return json(200, Json::object());
        }
        const auto owner = load_principal(*store, row.text(1));
        const auto scope = input.has("scope") ? parse_scope(input.get("scope")) : requested_scope;
        ensure(security::contains(requested_scope, scope) && security::contains(owner.scope, scope), 403, "forbidden", "approved scope exceeds application or current account scope");
        Statement service(store->db, "SELECT active,scope,rate_per_minute FROM services WHERE id=?"); service.bind(1, row.text(2)); service.step();
        ensure(service.integer(0) == 1, 409, "conflict", "service is suspended");
        ensure(security::contains(parse_scope(Json::parse(service.text(1))), scope), 403, "forbidden", "service scope denied");
        const auto validity = integer(input, "validity_seconds", 60, row.integer(5)), quota = integer(input, "quota", 1, row.integer(6));
        const auto rate = integer(input, "rate_per_minute", 1, std::min(row.integer(7), service.integer(2)));
        const auto mask = text(input, "mask_level", 16);
        if (mask != "standard" && mask != "strict") throw std::invalid_argument("mask_level must be standard or strict");
        const auto grant_id = random_token("g_");
        sec->audited_write(p, event, [&](sqlite3* db) {
            Statement(db, "UPDATE applications SET status='approved',reason=? WHERE id=?").bind(1, reason).bind(2, id).step();
            Statement(db, "INSERT INTO grants(id,application_id,owner_id,service_id,scope,expires_at,quota,rate_per_minute,mask_level) VALUES(?,?,?,?,?,?,?,?,?)")
                .bind(1, grant_id).bind(2, id).bind(3, owner.user_id).bind(4, row.text(2)).bind(5, security::scope_json(scope).dump())
                .bind(6, store->now() + validity).bind(7, quota).bind(8, rate).bind(9, mask).step();
        }); auto out = Json::object(); out.set("grant_id", Json::text(grant_id)); return json(200, out);
    });
    add("POST", "/applications/:id/token", [sec, store](const Request& request) {
        const auto p = sec->authorize(request, "grants.apply"); const auto id = request.path_params.at("id");
        Statement row(store->db, "SELECT g.id,g.owner_id,g.scope,g.expires_at,g.revoked,s.active FROM grants g JOIN services s ON s.id=g.service_id WHERE g.application_id=?");
        row.bind(1, id); ensure(row.step(), 404, "not_found", "approved grant not found");
        ensure(row.text(1) == p.user_id, 403, "forbidden", "only applicant can obtain token");
        ensure(row.integer(3) > store->now() && row.integer(4) == 0 && row.integer(5) == 1, 403, "forbidden", "grant expired, revoked or service suspended");
        ensure(security::contains(p.scope, parse_scope(Json::parse(row.text(2)))), 403, "forbidden", "account scope no longer covers grant");
        const auto token = random_token("svc_"); AuditEvent event{"grant.token.rotate", "grant", row.text(0)};
        sec->audited_write(p, event, [&](sqlite3* db) {
            Statement(db, "UPDATE grants SET token_hash=? WHERE id=?").bind(1, digest(token)).bind(2, row.text(0)).step();
        }); auto out = Json::object(); out.set("access_token", Json::text(token)); out.set("token_type", Json::text("Bearer"));
        out.set("expires_at", Json::number(row.integer(3))); auto response = json(200, out); response.headers["Cache-Control"] = "no-store"; return response;
    });
    add("POST", "/applications/:id/revoke", [sec, store](const Request& request) {
        const auto p = sec->authenticate(request); const auto id = request.path_params.at("id");
        Statement row(store->db, "SELECT id,owner_id,scope FROM grants WHERE application_id=?"); row.bind(1, id);
        ensure(row.step(), 404, "not_found", "grant not found");
        ensure((row.text(1) == p.user_id && sec->permitted(p, "grants.apply")) ||
            (sec->permitted(p, "grants.approve") && security::contains(p.scope, parse_scope(Json::parse(row.text(2))))),
            403, "forbidden", "revocation denied");
        AuditEvent event{"grant.revoke", "grant", row.text(0)};
        sec->audited_write(p, event, [&](sqlite3* db) {
            Statement(db, "UPDATE grants SET revoked=1,token_hash='' WHERE id=?").bind(1, row.text(0)).step();
            Statement(db, "UPDATE applications SET status='revoked' WHERE id=?").bind(1, id).step();
        }); return json(200, Json::object());
    });
}
}  // namespace platform::data_services
