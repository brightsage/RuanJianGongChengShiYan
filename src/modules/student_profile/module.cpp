#include "modules/student_profile/serialize.hpp"
#include "modules/student_profile/service.hpp"
#include "modules/student_profile/sqlite_repository.hpp"
#include "platform/api.hpp"
#include "platform/db.hpp"
#include "platform/modules.hpp"

#include <memory>
#include <string>

namespace platform {
void register_student_profile(App& app) {
    app.add_module("student-profile", "学生画像");

    auto database = std::make_shared<Database>(default_database_path());
    auto repository = std::make_shared<student_profile::SqliteProfileRepository>(*database);
    auto service = std::make_shared<student_profile::ProfileService>(database, repository);

    app.add_module_route(
        "student-profile", "POST", "/students", [service](const Request& request) {
            return guard([&] {
                return json_ok(student_profile::to_json(
                                   service->sync_student(parse_json_body(request))),
                               201);
            });
        });

    app.add_module_route(
        "student-profile", "GET", "/students", [service](const Request& request) {
            return guard([&] {
                return json_ok(student_profile::to_json_list(service->list_students(
                    query_param(request, "major"), query_param(request, "class"))));
            });
        });

    app.add_module_route(
        "student-profile", "GET", "/students/:student_id",
        [service](const Request& request) {
            return guard([&] {
                return json_ok(service->get_profile(
                    request.path_params.at("student_id"), query_param(request, "term")));
            });
        });

    app.add_module_route(
        "student-profile", "POST", "/metrics", [service](const Request& request) {
            return guard([&] {
                return json_ok(student_profile::to_json(
                                   service->sync_metric(parse_json_body(request))),
                               201);
            });
        });

    app.add_module_route(
        "student-profile", "GET", "/students/:student_id/metrics",
        [service](const Request& request) {
            return guard([&] {
                return json_ok(student_profile::to_json_list(service->list_metrics(
                    request.path_params.at("student_id"), query_param(request, "term"))));
            });
        });

    app.add_module_route(
        "student-profile", "POST", "/tags", [service](const Request& request) {
            return guard([&] {
                return json_ok(student_profile::to_json(
                                   service->upsert_tag(parse_json_body(request))),
                               201);
            });
        });

    app.add_module_route(
        "student-profile", "GET", "/tags", [service](const Request&) {
            return guard([&] {
                return json_ok(student_profile::to_json_list(service->list_tags()));
            });
        });

    app.add_module_route(
        "student-profile", "POST", "/recalculate", [service](const Request& request) {
            return guard([&] {
                return json_ok(service->recalculate(parse_json_body(request)));
            });
        });

    app.add_module_route(
        "student-profile", "POST", "/recalculate/batch", [service](const Request& request) {
            return guard([&] {
                return json_ok(service->recalculate_batch(parse_json_body(request)));
            });
        });

    app.add_module_route(
        "student-profile", "GET", "/students/:student_id/tags/:tag_code/evidence",
        [service](const Request& request) {
            return guard([&] {
                return json_ok(service->get_tag_evidence(
                    request.path_params.at("student_id"),
                    request.path_params.at("tag_code"), query_param(request, "term")));
            });
        });

    app.add_module_route(
        "student-profile", "GET", "/students/:student_id/compare",
        [service](const Request& request) {
            return guard([&] {
                return json_ok(service->compare(request.path_params.at("student_id"),
                                                query_param(request, "peer"),
                                                query_param(request, "term")));
            });
        });

    app.add_module_route(
        "student-profile", "GET", "/students/:student_id/report",
        [service](const Request& request) {
            return guard([&] {
                const std::string html = service->report_html(
                    request.path_params.at("student_id"), query_param(request, "term"));
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
