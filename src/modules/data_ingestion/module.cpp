#include "modules/data_ingestion/serialize.hpp"
#include "modules/data_ingestion/service.hpp"
#include "modules/data_ingestion/sqlite_repository.hpp"
#include "platform/api.hpp"
#include "platform/db.hpp"
#include "platform/modules.hpp"

#include <memory>

namespace platform {
void register_data_ingestion(App& app) {
    app.add_module("data-ingestion", "数据采集与接入");

    auto database = std::make_shared<Database>(default_database_path());
    auto repository = std::make_shared<data_ingestion::SqliteIngestionRepository>(*database);
    auto service = std::make_shared<data_ingestion::IngestionService>(database, repository);

    app.add_module_route(
        "data-ingestion", "POST", "/sources", [service](const Request& request) {
            return guard([&] {
                return json_ok(data_ingestion::to_json(
                                   service->create_data_source(parse_json_body(request))),
                               201);
            });
        });

    app.add_module_route(
        "data-ingestion", "GET", "/sources", [service](const Request&) {
            return guard([&] {
                return json_ok(data_ingestion::to_json_list(service->list_data_sources()));
            });
        });

    app.add_module_route(
        "data-ingestion", "GET", "/sources/:source_id", [service](const Request& request) {
            return guard([&] {
                return json_ok(data_ingestion::to_json(
                    service->get_data_source(path_param_id(request, "source_id"))));
            });
        });

    app.add_module_route(
        "data-ingestion", "PUT", "/sources/:source_id/mappings",
        [service](const Request& request) {
            return guard([&] {
                return json_ok(data_ingestion::to_json_list(service->replace_field_mappings(
                    path_param_id(request, "source_id"), parse_json_body(request))));
            });
        });

    app.add_module_route(
        "data-ingestion", "GET", "/sources/:source_id/mappings",
        [service](const Request& request) {
            return guard([&] {
                return json_ok(data_ingestion::to_json_list(
                    service->list_field_mappings(path_param_id(request, "source_id"))));
            });
        });

    app.add_module_route(
        "data-ingestion", "POST", "/tasks", [service](const Request& request) {
            return guard([&] {
                return json_ok(data_ingestion::to_json(
                                   service->create_import_task(parse_json_body(request))),
                               201);
            });
        });

    app.add_module_route(
        "data-ingestion", "GET", "/tasks", [service](const Request&) {
            return guard([&] {
                return json_ok(data_ingestion::to_json_list(service->list_import_tasks()));
            });
        });

    app.add_module_route(
        "data-ingestion", "GET", "/tasks/:task_id", [service](const Request& request) {
            return guard([&] {
                return json_ok(data_ingestion::to_json(
                    service->get_import_task(path_param_id(request, "task_id"))));
            });
        });

    app.add_module_route(
        "data-ingestion", "PUT", "/tasks/:task_id/schedule",
        [service](const Request& request) {
            return guard([&] {
                return json_ok(data_ingestion::to_json(service->update_schedule(
                    path_param_id(request, "task_id"), parse_json_body(request))));
            });
        });

    app.add_module_route(
        "data-ingestion", "POST", "/tasks/:task_id/run",
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

    app.add_module_route(
        "data-ingestion", "GET", "/runs/:run_id", [service](const Request& request) {
            return guard([&] {
                return json_ok(data_ingestion::to_json(
                    service->get_import_run(path_param_id(request, "run_id"))));
            });
        });

    app.add_module_route(
        "data-ingestion", "GET", "/runs/:run_id/records", [service](const Request& request) {
            return guard([&] {
                const bool only_failed = query_param(request, "failed", "0") == "1" ||
                                         query_param(request, "failed", "0") == "true";
                return json_ok(data_ingestion::to_json_list(
                    service->list_raw_records(path_param_id(request, "run_id"), only_failed)));
            });
        });

    app.add_module_route(
        "data-ingestion", "POST", "/runs/:run_id/retry",
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

    app.add_module_route(
        "data-ingestion", "POST", "/push", [service](const Request& request) {
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
