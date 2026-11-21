#include "modules/data_ingestion/serialize.hpp"
#include "modules/data_ingestion/service.hpp"
#include "modules/data_ingestion/sqlite_repository.hpp"
#include "platform/api.hpp"
#include "platform/db.hpp"
#include "platform/modules.hpp"
#include "platform/security.hpp"

#include <memory>
#include <utility>

namespace platform {
void register_data_ingestion(App& app) {
    app.add_module("data-ingestion", "数据采集与接入");

    auto database = std::make_shared<Database>(default_database_path());
    auto repository = std::make_shared<data_ingestion::SqliteIngestionRepository>(*database);
    auto service = std::make_shared<data_ingestion::IngestionService>(database, repository);
    const auto access = app.security();
    if (access) {
        access->register_permission("ingestion.read", "查看数据接入配置与导入结果");
        access->register_permission("ingestion.write", "登记数据源、配置任务与执行导入");
    }

    const auto add = [&](const char* method, const char* path, const char* permission,
                         Handler handler) {
        app.add_module_route(
            "data-ingestion", method, path,
            [access, permission, handler = std::move(handler)](const Request& request) {
                if (access) {
                    access->authorize(request, permission,
                                      security::Scope{"all", "", "", ""});
                }
                return handler(request);
            });
    };

    add("POST", "/sources", "ingestion.write", [service](const Request& request) {
        return guard([&] {
            return json_ok(
                data_ingestion::to_json(service->create_data_source(parse_json_body(request))),
                201);
        });
    });

    add("GET", "/sources", "ingestion.read", [service](const Request&) {
        return guard([&] {
            return json_ok(data_ingestion::to_json_list(service->list_data_sources()));
        });
    });

    add("GET", "/sources/:source_id", "ingestion.read", [service](const Request& request) {
        return guard([&] {
            return json_ok(data_ingestion::to_json(
                service->get_data_source(path_param_id(request, "source_id"))));
        });
    });

    add("PUT", "/sources/:source_id/mappings", "ingestion.write",
        [service](const Request& request) {
            return guard([&] {
                return json_ok(data_ingestion::to_json_list(service->replace_field_mappings(
                    path_param_id(request, "source_id"), parse_json_body(request))));
            });
        });

    add("GET", "/sources/:source_id/mappings", "ingestion.read",
        [service](const Request& request) {
            return guard([&] {
                return json_ok(data_ingestion::to_json_list(
                    service->list_field_mappings(path_param_id(request, "source_id"))));
            });
        });

    add("POST", "/tasks", "ingestion.write", [service](const Request& request) {
        return guard([&] {
            return json_ok(
                data_ingestion::to_json(service->create_import_task(parse_json_body(request))),
                201);
        });
    });

    add("GET", "/tasks", "ingestion.read", [service](const Request&) {
        return guard([&] {
            return json_ok(data_ingestion::to_json_list(service->list_import_tasks()));
        });
    });

    add("GET", "/tasks/:task_id", "ingestion.read", [service](const Request& request) {
        return guard([&] {
            return json_ok(data_ingestion::to_json(
                service->get_import_task(path_param_id(request, "task_id"))));
        });
    });

    add("PUT", "/tasks/:task_id/schedule", "ingestion.write",
        [service](const Request& request) {
            return guard([&] {
                return json_ok(data_ingestion::to_json(service->update_schedule(
                    path_param_id(request, "task_id"), parse_json_body(request))));
            });
        });

    add("POST", "/tasks/:task_id/run", "ingestion.write",
        [service](const Request& request) {
            return guard([&] {
                const auto execution = service->execute_import_task(
                    path_param_id(request, "task_id"), parse_json_body(request));
                Json output = Json::object();
                output["run"] = data_ingestion::to_json(execution.run);
                output["failures"] = data_ingestion::to_json_list(execution.failures);
                return json_ok(output);
            });
        });

    add("GET", "/runs/:run_id", "ingestion.read", [service](const Request& request) {
        return guard([&] {
            return json_ok(data_ingestion::to_json(
                service->get_import_run(path_param_id(request, "run_id"))));
        });
    });

    add("GET", "/runs/:run_id/records", "ingestion.read",
        [service](const Request& request) {
            return guard([&] {
                const bool only_failed = query_param(request, "failed", "0") == "1" ||
                                         query_param(request, "failed", "0") == "true";
                return json_ok(data_ingestion::to_json_list(
                    service->list_raw_records(path_param_id(request, "run_id"), only_failed)));
            });
        });

    add("POST", "/runs/:run_id/retry", "ingestion.write",
        [service](const Request& request) {
            return guard([&] {
                const Json body =
                    request.body.empty() ? Json::object() : parse_json_body(request);
                const auto execution =
                    service->retry_import_run(path_param_id(request, "run_id"), body);
                Json output = Json::object();
                output["run"] = data_ingestion::to_json(execution.run);
                output["failures"] = data_ingestion::to_json_list(execution.failures);
                return json_ok(output);
            });
        });

    add("POST", "/push", "ingestion.write", [service](const Request& request) {
        return guard([&] {
            const auto execution = service->push_records(parse_json_body(request));
            Json output = Json::object();
            output["run"] = data_ingestion::to_json(execution.run);
            output["failures"] = data_ingestion::to_json_list(execution.failures);
            return json_ok(output);
        });
    });
}
}  // namespace platform
