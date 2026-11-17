#include "store.hpp"

#include <openssl/hmac.h>
#include <algorithm>
#include <cctype>
#include <limits>

namespace platform::data_services {
using security::ApiError;
using security::AuditEvent;
namespace {
void ensure(bool condition, int status, const std::string& code, const std::string& message) {
    if (!condition) throw ApiError(status, code, message);
}
std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return std::tolower(c); }); return value;
}
std::string partial(const Json& value) {
    if (!value.is_string()) return "***";
    const auto text = value.as_string(); std::vector<std::size_t> starts;
    for (std::size_t i = 0; i < text.size(); ++i) if ((static_cast<unsigned char>(text[i]) & 0xc0) != 0x80) starts.push_back(i);
    if (starts.size() < 4) return "***";
    return text.substr(0, starts[1]) + "***" + text.substr(starts.back());
}
std::string keyed_hash(const std::string& key, const std::string& input) {
    unsigned char output[EVP_MAX_MD_SIZE]; unsigned length = 0;
    if (!HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
              reinterpret_cast<const unsigned char*>(input.data()), input.size(), output, &length))
        throw std::runtime_error("mask hash failed");
    const char* hex = "0123456789abcdef"; std::string result;
    for (unsigned i = 0; i < length; ++i) { result += hex[output[i] >> 4]; result += hex[output[i] & 15]; }
    return result;
}
Json mask(const Json& input, const std::map<std::string, std::string>& rules, const std::string& level, const std::string& key) {
    if (input.is_array()) {
        auto out = Json::array(); for (std::size_t i = 0; i < input.as_array().size(); ++i) out.push_back(mask(input.at(i), rules, level, key)); return out;
    }
    if (!input.is_object()) return input;
    auto out = Json::object();
    for (const auto& [name, item] : input.as_object()) {
        const auto field = lower(name); const auto it = rules.find(field);
        const bool secret = field.find("password") != std::string::npos || field.find("token") != std::string::npos ||
            field.find("secret") != std::string::npos || field == "authorization" || field == "salt";
        if (secret || (it != rules.end() && (level == "strict" || it->second == "redact"))) out[name] = Json("***");
        else if (it != rules.end() && it->second == "partial") out[name] = Json(partial(item));
        else if (it != rules.end() && it->second == "hash") out[name] = Json(keyed_hash(key, item.dump()));
        else out[name] = mask(item, rules, level, key);
    }
    return out;
}
bool record_in_scope(const Json& row, const security::Scope& scope) {
    if (scope.type == "all") return true;
    const auto matches = [&](const std::string& field, const std::string& expected) {
        return !expected.empty() && row.contains(field) && row.at(field).is_string() && row.at(field).as_string() == expected;
    };
    if (!matches("org_id", scope.org_id)) return false;
    if (scope.type == "college") return true;
    if (!matches("class_id", scope.class_id)) return false;
    if (scope.type == "class") return true;
    return scope.type == "self" && matches("student_no", scope.student_no);
}
void call_log(Store& store, const std::string& service, const std::string& grant, const std::string& actor,
              int status, const std::string& reason, const std::string& request_id) {
    Statement(store.db, "INSERT INTO call_logs(created_at,service_id,grant_id,actor_id,status,reason,request_id) VALUES(?,?,?,?,?,?,?)")
        .bind(1, store.now()).bind(2, service).bind(3, grant).bind(4, actor).bind(5, status).bind(6, reason).bind(7, request_id).step();
    AuditEvent event{"service.invoke", "service", service}; event.outcome = status < 400 ? "success" : "failure";
    event.reason = reason; event.request_id = request_id; store.audit(actor, event);
}
}  // namespace

void Gateway::register_provider(const std::string& id, std::vector<std::string> fields, Provider provider) {
    auto store = security_->store(); std::lock_guard<std::recursive_mutex> lock(store->mutex);
    if (id.empty() || id.size() > 100 || fields.empty() || !provider) throw std::invalid_argument("invalid provider registration");
    for (const auto& field : fields) if (field.empty()) throw std::invalid_argument("empty provider field");
    if (!providers_.emplace(id, Entry{std::move(fields), std::move(provider)}).second) throw std::invalid_argument("duplicate provider");
}
Json Gateway::providers() const {
    auto store = security_->store(); std::lock_guard<std::recursive_mutex> lock(store->mutex);
    auto out = Json::array();
    for (const auto& [id, entry] : providers_) {
        auto item = Json::object(); item["id"] = Json(id); item["fields"] = strings(entry.fields); out.push_back(item);
    } return out;
}
Response Gateway::invoke(const Request& request) {
    auto store = security_->store(); std::lock_guard<std::recursive_mutex> lock(store->mutex);
    const auto service_id = request.path_params.at("id"), request_id = random_token("req_");
    std::string grant_id, actor = "anonymous";
    const auto run = [&]() {
        Transaction tx(store->db);
        const auto auth = request.headers.find("authorization");
        ensure(auth != request.headers.end() && auth->second.rfind("Bearer svc_", 0) == 0,
               401, "unauthorized", "service Bearer token required");
        Statement grant(store->db, "SELECT id,owner_id,scope,expires_at,quota,used,rate_per_minute,mask_level,revoked,bucket,bucket_count,service_id "
            "FROM grants WHERE token_hash=? AND token_hash<>''"); grant.bind(1, digest(auth->second.substr(7)));
        ensure(grant.step(), 401, "unauthorized", "invalid service token");
        grant_id = grant.text(0); actor = grant.text(1);
        ensure(grant.integer(3) > store->now() && grant.integer(8) == 0, 401, "unauthorized", "service token expired or revoked");
        ensure(grant.text(11) == service_id, 403, "forbidden", "token belongs to another service version");
        const auto owner = load_principal(*store, actor);
        ensure(security_->permitted(owner, "grants.apply"), 403, "forbidden", "caller is no longer allowed to share data");
        const auto scope = parse_scope(Json::parse(grant.text(2)));
        ensure(security::contains(owner.scope, scope), 403, "forbidden", "account scope no longer covers grant");
        Statement service(store->db, "SELECT id,name,provider_id,version,fields,scope,rate_per_minute,active,bucket,bucket_count FROM services WHERE id=?"); service.bind(1, service_id);
        ensure(service.step(), 404, "not_found", "service not found");
        ensure(service.integer(7) == 1, 403, "forbidden", "service is suspended");
        ensure(security::contains(parse_scope(Json::parse(service.text(5))), scope), 403, "forbidden", "service scope no longer covers grant");
        ensure(grant.integer(5) < grant.integer(4), 429, "quota_exhausted", "grant quota exhausted");
        const auto minute = store->now() / 60;
        ensure(grant.integer(9) != minute || grant.integer(10) < grant.integer(6), 429, "rate_limited", "grant minute limit reached");
        ensure(service.integer(8) != minute || service.integer(9) < service.integer(6), 429, "rate_limited", "service minute limit reached");
        const auto input = body(request);
        for (const auto& entry : input.as_object()) {
            const auto& key = entry.first;
            if (std::string(key) != "parameters") throw std::invalid_argument("invoke accepts only parameters; scope is determined by the grant");
        }
        const auto parameters = input.contains("parameters") ? input.at("parameters") : Json::object();
        if (!parameters.is_object()) throw std::invalid_argument("parameters must be an object");
        const auto provider = providers_.find(service.text(2));
        ensure(provider != providers_.end(), 503, "provider_unavailable", "data provider has not been registered");
        const auto fields = string_list(Json::parse(service.text(4)));
        for (const auto& field : fields)
            ensure(std::find(provider->second.fields.begin(), provider->second.fields.end(), field) != provider->second.fields.end(),
                   409, "provider_field_mismatch", "published field is not declared by provider");
        Json records;
        try { records = provider->second.provider(Query{owner, scope, fields, parameters}); }
        catch (...) { throw ApiError(503, "provider_unavailable", "data provider failed"); }
        ensure(records.is_array(), 502, "invalid_provider_response", "data provider must return an array");
        std::map<std::string, std::string> rules;
        Statement rule(store->db, "SELECT field,algorithm FROM masking_rules"); while (rule.step()) rules.emplace(rule.text(0), rule.text(1));
        Statement mask_key(store->db, "SELECT value FROM settings WHERE key='mask_key'"); mask_key.step();
        auto output = Json::array();
        for (std::size_t i = 0; i < records.as_array().size(); ++i) {
            const auto row = records.at(i);
            ensure(row.is_object(), 502, "invalid_provider_response", "each provider record must be an object");
            if (!record_in_scope(row, scope)) continue;
            auto projected = Json::object();
            for (const auto& field : fields) if (row.contains(field)) projected[field] = row.at(field);
            output.push_back(mask(projected, rules, grant.text(7), mask_key.text(0)));
        }
        Statement(store->db, "UPDATE grants SET used=used+1,bucket=?,bucket_count=? WHERE id=?")
            .bind(1, minute).bind(2, grant.integer(9) == minute ? grant.integer(10) + 1 : 1).bind(3, grant_id).step();
        Statement(store->db, "UPDATE services SET bucket=?,bucket_count=? WHERE id=?")
            .bind(1, minute).bind(2, service.integer(8) == minute ? service.integer(9) + 1 : 1).bind(3, service_id).step();
        call_log(*store, service_id, grant_id, actor, 200, "ok", request_id);
        auto result = Json::object(); result["service_id"] = Json(service_id); result["version"] = Json(service.text(3));
        result["records"] = output; result["request_id"] = Json(request_id);
        auto response = json(200, result); response.headers["Cache-Control"] = "no-store";
        tx.commit(); return response;
    };
    try { return run(); }
    catch (const ApiError& error) {
        Transaction tx(store->db); call_log(*store, service_id, grant_id, actor, error.status, error.code, request_id); tx.commit();
        return security::api_error(error.status, error.code, error.what());
    } catch (const std::invalid_argument& error) {
        Transaction tx(store->db); call_log(*store, service_id, grant_id, actor, 400, "invalid_request", request_id); tx.commit();
        return security::api_error(400, "invalid_request", error.what());
    } catch (const std::exception&) {
        Transaction tx(store->db); call_log(*store, service_id, grant_id, actor, 500, "internal_error", request_id); tx.commit();
        return security::api_error(500, "internal_error", "service invocation failed");
    }
}

void install_gateway_routes(App& app, const std::shared_ptr<Gateway>& gateway) {
    auto sec = gateway->security(); auto store = sec->store();
    app.add_module_route("data-services", "POST", "/services/:id/invoke", [gateway](const Request& request) { return gateway->invoke(request); });
    app.add_module_route("data-services", "GET", "/providers", guarded(store, [sec, gateway](const Request& request) {
        sec->authorize(request, "services.publish"); return json(200, gateway->providers());
    }));
    app.add_module_route("data-services", "GET", "/masking-rules", guarded(store, [sec, store](const Request& request) {
        sec->authorize(request, "masking.manage"); auto result = Json::array();
        Statement rows(store->db, "SELECT field,algorithm FROM masking_rules ORDER BY field");
        while (rows.step()) {
            auto item = Json::object(); item["field"] = Json(rows.text(0)); item["algorithm"] = Json(rows.text(1)); result.push_back(item);
        } return json(200, result);
    }));
    app.add_module_route("data-services", "PUT", "/masking-rules/:field", guarded(store, [sec, store](const Request& request) {
        const auto p = sec->authorize(request, "masking.manage"); const auto input = body(request);
        const auto field = lower(request.path_params.at("field")), algorithm = text(input, "algorithm", 16);
        if (field.empty() || field.size() > 100 || field.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") != std::string::npos)
            throw std::invalid_argument("invalid field name");
        if (algorithm != "redact" && algorithm != "partial" && algorithm != "hash") throw std::invalid_argument("algorithm must be redact, partial or hash");
        AuditEvent event{"masking.update", "masking_rule", field}; event.after = input;
        Statement old(store->db, "SELECT algorithm FROM masking_rules WHERE field=?"); old.bind(1, field);
        if (old.step()) { event.before = Json::object(); event.before["algorithm"] = Json(old.text(0)); }
        sec->audited_write(p, event, [&](sqlite3* db) {
            Statement(db, "INSERT INTO masking_rules VALUES(?,?) ON CONFLICT(field) DO UPDATE SET algorithm=excluded.algorithm")
                .bind(1, field).bind(2, algorithm).step();
        }); return json(200, Json::object());
    }));
    app.add_module_route("data-services", "GET", "/call-logs", guarded(store, [sec, store](const Request& request) {
        sec->authorize(request, "audit.read"); const auto query = query_parameters(request.query);
        auto sql = std::string("SELECT id,created_at,service_id,grant_id,actor_id,status,reason,request_id FROM call_logs WHERE created_at>=? AND created_at<=?");
        std::vector<std::string> filters;
        for (const auto& key : {"service_id", "actor_id", "status"}) if (query.count(key)) { sql += " AND " + std::string(key) + "=?"; filters.push_back(query.at(key)); }
        sql += " ORDER BY id DESC LIMIT ? OFFSET ?";
        Statement rows(store->db, sql); rows.bind(1, query_number(query, "from", 0, 0, std::numeric_limits<std::int64_t>::max()))
            .bind(2, query_number(query, "to", std::numeric_limits<std::int64_t>::max(), 0, std::numeric_limits<std::int64_t>::max()));
        int index = 3; for (const auto& value : filters) rows.bind(index++, value);
        rows.bind(index++, query_number(query, "limit", 100, 1, 200)); rows.bind(index, query_number(query, "offset", 0, 0, 100000000));
        auto out = Json::array();
        while (rows.step()) {
            auto item = Json::object(); item["id"] = Json(rows.integer(0)); item["created_at"] = Json(rows.integer(1));
            item["service_id"] = Json(rows.text(2)); item["grant_id"] = Json(rows.text(3)); item["actor_id"] = Json(rows.text(4));
            item["status"] = Json(rows.integer(5)); item["reason"] = Json(rows.text(6)); item["request_id"] = Json(rows.text(7)); out.push_back(item);
        } return json(200, out);
    }));
}
}  // namespace platform::data_services
