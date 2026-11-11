#pragma once

#include "modules/data_ingestion/model.hpp"
#include "platform/api.hpp"
#include "platform/json.hpp"

#include <vector>

namespace platform::data_ingestion {

inline Json to_json(const DataSource& source) {
    Json value = Json::object();
    value["id"] = source.id;
    value["name"] = source.name;
    value["kind"] = source.kind;
    value["access_mode"] = source.access_mode;
    value["owner"] = source.owner;
    value["description"] = source.description;
    value["created_at"] = source.created_at;
    return value;
}

inline Json to_json(const FieldMapping& mapping) {
    Json value = Json::object();
    value["id"] = mapping.id;
    value["data_source_id"] = mapping.data_source_id;
    value["source_field"] = mapping.source_field;
    value["standard_field"] = mapping.standard_field;
    value["transform_expr"] = mapping.transform_expr;
    return value;
}

inline Json to_json(const ImportTask& task) {
    Json value = Json::object();
    value["id"] = task.id;
    value["data_source_id"] = task.data_source_id;
    value["name"] = task.name;
    value["import_mode"] = task.import_mode;
    value["schedule_expr"] = task.schedule_expr;
    value["validation_rules"] = platform::parse_json_or(task.validation_rules, Json::object());
    value["status"] = task.status;
    value["created_at"] = task.created_at;
    return value;
}

inline Json to_json(const ImportRun& run) {
    Json value = Json::object();
    value["id"] = run.id;
    value["task_id"] = run.task_id;
    value["idempotency_key"] = run.idempotency_key;
    value["status"] = run.status;
    value["total_count"] = run.total_count;
    value["success_count"] = run.success_count;
    value["failure_count"] = run.failure_count;
    value["started_at"] = run.started_at;
    value["finished_at"] = run.finished_at;
    return value;
}

inline Json to_json(const RawRecord& record) {
    Json value = Json::object();
    value["id"] = record.id;
    value["run_id"] = record.run_id;
    value["source_row_no"] = record.source_row_no;
    value["payload"] = platform::parse_json_or(record.payload, Json::object());
    value["valid"] = record.valid;
    value["error_reason"] = record.error_reason;
    return value;
}

template <typename T>
Json to_json_list(const std::vector<T>& values) {
    Json array = Json::array();
    for (const auto& value : values) array.push_back(to_json(value));
    return array;
}

}  // namespace platform::data_ingestion
