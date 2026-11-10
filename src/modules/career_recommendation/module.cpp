#include "platform/modules.hpp"

#include "catalog.hpp"

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

}  // namespace

void register_career_recommendation(App& app) {
    app.add_module(module_id, "职业发展推荐");
    const char* demo_env = std::getenv("PLATFORM_CAREER_DEMO");
    const bool demo_enabled = demo_env != nullptr && std::string_view(demo_env) == "1";

    const auto add = [&](const char* method, const char* path, auto handle) {
        app.add_module_route(module_id, method, path,
            [demo_enabled, handle](const Request& request) -> Response {
                if (!demo_enabled || !is_demo_request(request)) {
                    return error_response(503, "职业推荐管理接口尚未接入统一权限");
                }
                try {
                    if (!demo_database_is_separate()) {
                        return error_response(503, "演示数据库必须与平台数据库分开");
                    }
                    career::Catalog catalog(demo_database);
                    return handle(catalog, request);
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

    add("GET", "/jobs", [](career::Catalog& catalog, const Request&) {
        return json_response(200, catalog.list_jobs());
    });
    add("POST", "/jobs", [](career::Catalog& catalog, const Request& request) {
        const auto job = career::validate_job(Json::parse(request.body));
        return json_response(201, catalog.create_job(job));
    });
    add("PUT", "/jobs/:id", [](career::Catalog& catalog, const Request& request) {
        const auto job = career::validate_job(Json::parse(request.body));
        return json_response(200, catalog.update_job(job_id(request), job));
    });
    add("PUT", "/jobs/:id/status", [](career::Catalog& catalog, const Request& request) {
        const auto status = career::validate_status(Json::parse(request.body));
        return json_response(200, catalog.set_status(job_id(request), status));
    });
    add("GET", "/strategy", [](career::Catalog& catalog, const Request&) {
        return json_response(200, catalog.get_strategy());
    });
    add("PUT", "/strategy", [](career::Catalog& catalog, const Request& request) {
        const auto strategy = career::validate_strategy(Json::parse(request.body));
        return json_response(200, catalog.save_strategy(strategy));
    });
}
}  // namespace platform
