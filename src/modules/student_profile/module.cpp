#include "modules/student_profile/serialize.hpp"
#include "modules/student_profile/service.hpp"
#include "modules/student_profile/sqlite_repository.hpp"
#include "platform/api.hpp"
#include "platform/data_services.hpp"
#include "platform/db.hpp"
#include "platform/modules.hpp"
#include "platform/security.hpp"

#include <algorithm>
#include <memory>
#include <utility>

namespace platform {
namespace {

security::Scope scope_of(const student_profile::Student& student) {
    return {"self", student.org_id, student.class_name, student.student_id};
}

void require_scope(const std::shared_ptr<security::Security>& access, const Request& request,
                   const student_profile::Student& student) {
    if (!access) return;
    const auto actor = access->authenticate(request);
    if (!security::contains(actor.scope, scope_of(student)))
        throw security::ApiError(403, "forbidden", "profile is outside the account data scope");
}

}  // namespace

void register_student_profile(App& app) {
    app.add_module("student-profile", "学生画像");

    auto database = std::make_shared<Database>(default_database_path());
    auto repository = std::make_shared<student_profile::SqliteProfileRepository>(*database);
    auto service = std::make_shared<student_profile::ProfileService>(database, repository);

    // 向 M6 数据服务网关注册只读提供方，供 `/services/:id/invoke` 使用。
    // 记录里带上 org_id / class_id / student_no，网关按授权范围过滤。
    if (const auto gateway = app.data_services()) {
        gateway->register_provider(
            "student_profile",
            {"student_id", "student_no", "class_id", "class_name", "name", "major", "org_id",
             "grade"},
            [repository](const data_services::Query& query) {
                std::string only;
                if (query.parameters.is_object() && query.parameters.contains("student_id") &&
                    query.parameters.at("student_id").is_string())
                    only = query.parameters.at("student_id").as_string();

                Json rows = Json::array();
                for (const auto& student : repository->list_students()) {
                    if (!only.empty() && student.student_id != only) continue;
                    Json row = Json::object();
                    row["student_id"] = student.student_id;
                    row["student_no"] = student.student_id;
                    row["class_id"] = student.class_name;
                    row["class_name"] = student.class_name;
                    row["name"] = student.name;
                    row["major"] = student.major;
                    row["org_id"] = student.org_id;
                    row["grade"] = static_cast<std::int64_t>(student.grade);
                    rows.push_back(std::move(row));
                }
                return rows;
            });
    }
    const auto access = app.security();
    if (access) {
        access->register_permission("profile.read", "查看学生画像、指标与标签依据");
        access->register_permission("profile.write", "同步学生/指标、维护标签与重算画像");
    }

    const auto add = [&](const char* method, const char* path, const char* permission,
                         Handler handler) {
        app.add_module_route(
            "student-profile", method, path,
            [access, permission, handler = std::move(handler)](const Request& request) {
                if (access) access->authorize(request, permission);
                return handler(request);
            });
    };

    const auto add_global_write = [&](const char* method, const char* path, Handler handler) {
        app.add_module_route(
            "student-profile", method, path,
            [access, handler = std::move(handler)](const Request& request) {
                if (access) {
                    access->authorize(request, "profile.write",
                                      security::Scope{"all", "", "", ""});
                }
                return handler(request);
            });
    };

    add_global_write("POST", "/students", [service](const Request& request) {
        return guard([&] {
            return json_ok(
                student_profile::to_json(service->sync_student(parse_json_body(request))), 201);
        });
    });

    add("GET", "/students", "profile.read", [service, access](const Request& request) {
        return guard([&] {
            auto students = service->list_students(query_param(request, "major"),
                                                   query_param(request, "class"));
            if (access) {
                const auto actor = access->authenticate(request);
                students.erase(
                    std::remove_if(students.begin(), students.end(),
                                   [&](const student_profile::Student& student) {
                                       return !security::contains(actor.scope, scope_of(student));
                                   }),
                    students.end());
            }
            return json_ok(student_profile::to_json_list(students));
        });
    });

    add("GET", "/students/:student_id", "profile.read",
        [service, access](const Request& request) {
            return guard([&] {
                const auto& id = request.path_params.at("student_id");
                require_scope(access, request, service->get_student(id));
                return json_ok(service->get_profile(id, query_param(request, "term")));
            });
        });

    add_global_write("POST", "/metrics", [service](const Request& request) {
        return guard([&] {
            return json_ok(
                student_profile::to_json(service->sync_metric(parse_json_body(request))), 201);
        });
    });

    add("GET", "/students/:student_id/metrics", "profile.read",
        [service, access](const Request& request) {
            return guard([&] {
                const auto& id = request.path_params.at("student_id");
                require_scope(access, request, service->get_student(id));
                return json_ok(student_profile::to_json_list(
                    service->list_metrics(id, query_param(request, "term"))));
            });
        });

    add_global_write("POST", "/tags", [service](const Request& request) {
        return guard([&] {
            return json_ok(
                student_profile::to_json(service->upsert_tag(parse_json_body(request))), 201);
        });
    });

    add("GET", "/tags", "profile.read", [service](const Request&) {
        return guard([&] {
            return json_ok(student_profile::to_json_list(service->list_tags()));
        });
    });

    add("POST", "/recalculate", "profile.write", [service, access](const Request& request) {
        return guard([&] {
            const auto body = parse_json_body(request);
            const auto& id = require_string(body, "student_id");
            require_scope(access, request, service->get_student(id));
            return json_ok(service->recalculate(body));
        });
    });

    add_global_write("POST", "/recalculate/batch", [service](const Request& request) {
        return guard([&] {
            return json_ok(service->recalculate_batch(parse_json_body(request)));
        });
    });

    add("GET", "/students/:student_id/tags/:tag_code/evidence", "profile.read",
        [service, access](const Request& request) {
            return guard([&] {
                const auto& id = request.path_params.at("student_id");
                require_scope(access, request, service->get_student(id));
                return json_ok(service->get_tag_evidence(
                    id, request.path_params.at("tag_code"), query_param(request, "term")));
            });
        });

    add("GET", "/students/:student_id/compare", "profile.read",
        [service, access](const Request& request) {
            return guard([&] {
                const auto& id = request.path_params.at("student_id");
                const auto& peer = query_param(request, "peer");
                require_scope(access, request, service->get_student(id));
                if (!peer.empty()) require_scope(access, request, service->get_student(peer));
                return json_ok(service->compare(id, peer, query_param(request, "term")));
            });
        });

    add("GET", "/students/:student_id/report", "profile.read",
        [service, access](const Request& request) {
            return guard([&] {
                const auto& id = request.path_params.at("student_id");
                require_scope(access, request, service->get_student(id));
                const std::string html =
                    service->report_html(id, query_param(request, "term"));
                std::map<std::string, std::string> headers;
                if (query_param(request, "download", "0") == "1") {
                    headers["Content-Disposition"] =
                        "attachment; filename=\"profile-report.html\"";
                }
                return Response{200, "text/html; charset=utf-8", html, headers};
            });
        });
}
}  // namespace platform
