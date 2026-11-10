#include "platform/modules.hpp"

#include "catalog.hpp"
#include "platform/security.hpp"

#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <string_view>

namespace platform {
namespace {

constexpr char module_id[] = "career-recommendation";
constexpr char demo_database[] = "data/career-demo.db";

Response json_response(int status, const Json& body) {
    return {status, "application/json; charset=utf-8", body.dump(),
            {{"Cache-Control", "no-store"}}};
}

Response error_response(int status, const std::string& error) {
    return json_response(status, Json::Object{{"error", error}});
}

std::int64_t job_id(const Request& request) {
    const auto& value = request.path_params.at("id");
    std::int64_t id = 0;
    const auto end = value.data() + value.size();
    const auto parsed = std::from_chars(value.data(), end, id);
    if (parsed.ec != std::errc{} || parsed.ptr != end || id <= 0) {
        throw career::CatalogError(400, "岗位 ID 必须是正整数");
    }
    return id;
}

bool is_demo_request(const Request& request) {
    const auto demo_header = request.headers.find("x-career-demo");
    if (demo_header == request.headers.end() || demo_header->second != "1") return false;

    // Reject other hostnames even if DNS resolves them to the loopback listener.
    const auto host = request.headers.find("host");
    if (host == request.headers.end()) return false;
    const auto hostname = host->second.substr(0, host->second.find(':'));
    return hostname == "127.0.0.1" || hostname == "localhost";
}

bool demo_database_is_separate() {
    namespace fs = std::filesystem;
    const fs::path platform_db = default_database_path();
    if (fs::weakly_canonical(demo_database) == fs::weakly_canonical(platform_db)) return false;
    return !fs::exists(demo_database) || !fs::exists(platform_db) ||
           !fs::equivalent(demo_database, platform_db);
}

void record_changes(security::AuditEvent& event, const Json& before, const Json& after) {
    event.before = Json::object();
    event.after = Json::object();
    for (const auto& [key, value] : after.as_object()) {
        if (key == "id" || key == "version") continue;
        if (before.contains(key) && before.at(key).dump() == value.dump()) continue;
        if (before.contains(key)) event.before[key] = before.at(key);
        event.after[key] = value;
    }
}

}  // namespace

void register_career_recommendation(App& app) {
    app.add_module(module_id, "职业发展推荐");
    const char* demo_env = std::getenv("PLATFORM_CAREER_DEMO");
    const bool demo_enabled = demo_env != nullptr && std::string_view(demo_env) == "1";

    const auto access = app.security();
    const auto add = [&](const char* method, const char* path, const char* permission,
                         const char* action, const char* resource, auto handle) {
        app.add_module_route(module_id, method, path,
            [demo_enabled, access, permission, action, resource, handle](const Request& request) -> Response {
                try {
                    security::AuditEvent event(action, resource, "new");
                    if (demo_enabled) {
                        if (!is_demo_request(request) || !demo_database_is_separate()) {
                            return error_response(503, "职业推荐演示未启用或数据库未隔离");
                        }
                        Database db(demo_database);
                        career::Catalog catalog(db);
                        return handle(catalog, request, event);
                    }
                    if (!access) return error_response(503, "统一权限服务不可用");
                    const auto actor = access->authorize(request, permission, security::Scope{});
                    Response response;
                    const auto operation = [&](sqlite3* connection) {
                        Database db(*connection);
                        career::Catalog catalog(db);
                        response = handle(catalog, request, event);
                    };
                    if (*action) access->audited_write(actor, event, operation);
                    else access->with_connection(operation);
                    return response;
                } catch (const career::CatalogError& error) {
                    return error_response(error.status, error.what());
                } catch (const JsonError&) {
                    return error_response(400, "JSON 格式、必填字段或字段类型有误");
                } catch (const DbError&) {
                    return error_response(500, "职业推荐数据库操作失败");
                } catch (const std::filesystem::filesystem_error&) {
                    return error_response(500, "职业推荐数据库路径不可用");
                }
            });
    };

    add("GET", "/jobs", "career.jobs.read", "", "",
        [](career::Catalog& catalog, const Request&, security::AuditEvent&) {
        return json_response(200, catalog.list_jobs());
    });
    add("POST", "/jobs", "career.jobs.write", "career.job.create", "job",
        [](career::Catalog& catalog, const Request& request, security::AuditEvent& event) {
        const auto job = career::validate_job(Json::parse(request.body));
        const auto saved = catalog.create_job(job);
        event.resource_id = std::to_string(saved.at("id").as_int64());
        record_changes(event, {}, saved);
        return json_response(201, saved);
    });
    add("PUT", "/jobs/:id", "career.jobs.write", "career.job.update", "job",
        [](career::Catalog& catalog, const Request& request, security::AuditEvent& event) {
        const auto id = job_id(request);
        event.resource_id = std::to_string(id);
        const auto job = career::validate_job(Json::parse(request.body));
        const auto before = catalog.get_job(id);
        const auto saved = catalog.update_job(id, job);
        record_changes(event, before, saved);
        return json_response(200, saved);
    });
    add("PUT", "/jobs/:id/status", "career.jobs.publish", "career.job.publish", "job",
        [](career::Catalog& catalog, const Request& request, security::AuditEvent& event) {
        const auto id = job_id(request);
        event.resource_id = std::to_string(id);
        const auto status = career::validate_status(Json::parse(request.body));
        const auto before = catalog.get_job(id);
        if (status == "withdrawn") event.action = "career.job.unpublish";
        else if (before.at("status").as_string() == "withdrawn") event.action = "career.job.republish";
        const auto saved = catalog.set_status(id, status);
        record_changes(event, before, saved);
        return json_response(200, saved);
    });
    add("GET", "/strategy", "career.strategy.write", "", "",
        [](career::Catalog& catalog, const Request&, security::AuditEvent&) {
        return json_response(200, catalog.get_strategy());
    });
    add("PUT", "/strategy", "career.strategy.write", "career.strategy.update", "strategy",
        [](career::Catalog& catalog, const Request& request, security::AuditEvent& event) {
        event.resource_id = "1";
        const auto strategy = career::validate_strategy(Json::parse(request.body));
        const auto before = catalog.find_strategy();
        const auto saved = catalog.save_strategy(strategy);
        record_changes(event, before, saved);
        return json_response(200, saved);
    });
}
}  // namespace platform
