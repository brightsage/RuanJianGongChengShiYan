#include "modules/data_ingestion/service.hpp"

#include "platform/api.hpp"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace platform::data_ingestion {
namespace {

bool is_one_of(const std::string& value, std::initializer_list<const char*> options) {
    for (const char* option : options) {
        if (value == option) return true;
    }
    return false;
}

std::string trim_copy(std::string value) {
    const auto not_space = [](unsigned char c) { return std::isspace(c) == 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

bool number_from_json(const Json& value, double& number) {
    if (value.is_number()) {
        number = value.as_double();
        return true;
    }
    if (value.is_string()) {
        const std::string& text = value.as_string();
        if (text.empty()) return false;
        char* end = nullptr;
        errno = 0;
        const double parsed = std::strtod(text.c_str(), &end);
        if (end == text.c_str() + text.size() && errno != ERANGE) {
            number = parsed;
            return true;
        }
    }
    return false;
}

Json transform_value(const Json& value, const std::string& expression, bool& ok) {
    ok = true;
    if (expression.empty() || expression == "trim") {
        if (value.is_string()) return Json(trim_copy(value.as_string()));
        return value;
    }
    if (expression == "lower" || expression == "upper") {
        if (!value.is_string()) {
            ok = false;
            return Json(nullptr);
        }
        std::string text = value.as_string();
        std::transform(text.begin(), text.end(), text.begin(), [&](unsigned char c) {
            return static_cast<char>(expression == "lower" ? std::tolower(c) : std::toupper(c));
        });
        return Json(std::move(text));
    }
    if (expression == "number") {
        double number = 0.0;
        if (number_from_json(value, number)) return Json(number);
        ok = false;
        return Json(nullptr);
    }
    ok = false;
    return Json(nullptr);
}

Json apply_mappings(const Json& row, const std::vector<FieldMapping>& mappings,
                    std::string& error) {
    if (mappings.empty()) return row;
    Json standard = Json::object();
    for (const auto& mapping : mappings) {
        const Json value =
            row.contains(mapping.source_field) ? row.at(mapping.source_field) : Json(nullptr);
        bool ok = true;
        const Json transformed = transform_value(value, mapping.transform_expr, ok);
        if (!ok) {
            error = "cannot transform field '" + mapping.source_field + "' with '" +
                    mapping.transform_expr + "'";
            return Json(nullptr);
        }
        standard[mapping.standard_field] = transformed;
    }
    return standard;
}

std::string validate_row(const Json& row, const Json& rules) {
    if (!rules.is_object()) return "validation_rules must be a JSON object";
    std::vector<std::string> errors;

    if (rules.contains("required")) {
        const auto& required = rules.at("required");
        if (!required.is_array()) return "validation_rules.required must be an array";
        for (const auto& item : required.as_array()) {
            if (!item.is_string()) return "validation_rules.required must contain strings";
            const std::string& field = item.as_string();
            if (!row.contains(field) || row.at(field).is_null() ||
                (row.at(field).is_string() && row.at(field).as_string().empty())) {
                errors.push_back("missing required field '" + field + "'");
            }
        }
    }

    if (rules.contains("types")) {
        const auto& types = rules.at("types");
        if (!types.is_object()) return "validation_rules.types must be an object";
        for (const auto& [field, type_value] : types.as_object()) {
            if (!type_value.is_string()) {
                errors.push_back("type rule for '" + field + "' must be a string");
                continue;
            }
            if (!row.contains(field) || row.at(field).is_null()) continue;
            const auto& value = row.at(field);
            const std::string& type = type_value.as_string();
            if (type == "number") {
                double number = 0.0;
                if (!number_from_json(value, number))
                    errors.push_back("field '" + field + "' must be a number");
            } else if (type == "string") {
                if (!value.is_string()) errors.push_back("field '" + field + "' must be a string");
            } else if (type == "boolean") {
                const bool ok = value.is_bool() ||
                                (value.is_number() && (value.as_double() == 0.0 ||
                                                       value.as_double() == 1.0)) ||
                                (value.is_string() &&
                                 (value.as_string() == "true" || value.as_string() == "false"));
                if (!ok) errors.push_back("field '" + field + "' must be a boolean");
            }
        }
    }

    if (rules.contains("ranges")) {
        const auto& ranges = rules.at("ranges");
        if (!ranges.is_object()) return "validation_rules.ranges must be an object";
        for (const auto& [field, range] : ranges.as_object()) {
            if (!range.is_object()) {
                errors.push_back("range rule for '" + field + "' must be an object");
                continue;
            }
            if (!row.contains(field) || row.at(field).is_null()) continue;
            double number = 0.0;
            if (!number_from_json(row.at(field), number)) {
                errors.push_back("field '" + field + "' must be a number for range check");
                continue;
            }
            if (range.contains("min") && !range.at("min").is_number()) {
                errors.push_back("minimum for '" + field + "' must be a number");
                continue;
            }
            if (range.contains("max") && !range.at("max").is_number()) {
                errors.push_back("maximum for '" + field + "' must be a number");
                continue;
            }
            if (range.contains("min") && number < range.at("min").as_double())
                errors.push_back("field '" + field + "' is below the minimum");
            if (range.contains("max") && number > range.at("max").as_double())
                errors.push_back("field '" + field + "' is above the maximum");
        }
    }

    std::string result;
    for (const auto& error : errors) {
        if (!result.empty()) result += "; ";
        result += error;
    }
    return result;
}

Json records_from_body(const Json& body) {
    if (body.is_array()) return body;
    if (body.is_object() && body.contains("records") && body.at("records").is_array())
        return body.at("records");
    throw BadRequest("body must be an array of records or an object with a 'records' array");
}

std::string validation_rules_text(const Json& body) {
    if (!body.contains("validation_rules") || body.at("validation_rules").is_null()) return "{}";
    const auto& value = body.at("validation_rules");
    Json parsed = value;
    if (value.is_string()) parsed = Json::parse(value.as_string());
    if (!parsed.is_object()) throw BadRequest("validation_rules must be a JSON object");
    return parsed.dump();
}

std::string execution_status(std::int64_t success, std::int64_t failure) {
    if (failure == 0) return "success";
    if (success > 0) return "partial";
    return "failed";
}

}  // namespace

IngestionService::IngestionService(std::shared_ptr<Database> database,
                                   std::shared_ptr<IngestionRepository> repository)
    : database_(std::move(database)), repository_(std::move(repository)) {
    if (!database_ || !repository_) throw std::invalid_argument("ingestion service dependencies");
}

DataSource IngestionService::create_data_source(const Json& body) {
    DataSource source;
    source.name = require_string(body, "name");
    source.kind = require_string(body, "kind");
    source.access_mode = require_string(body, "access_mode");
    source.owner = require_string(body, "owner");
    source.description = optional_string(body, "description");

    if (!is_one_of(source.kind, {"academic", "research", "practice"}))
        throw BadRequest("kind must be academic, research or practice");
    if (!is_one_of(source.access_mode, {"file", "push", "api"}))
        throw BadRequest("access_mode must be file, push or api");
    for (const auto& existing : repository_->list_data_sources()) {
        if (existing.name == source.name)
            throw ApiError(409, "data source name already exists: " + source.name);
    }
    const auto id = repository_->create_data_source(source);
    return get_data_source(id);
}

std::vector<DataSource> IngestionService::list_data_sources() const {
    return repository_->list_data_sources();
}

DataSource IngestionService::get_data_source(std::int64_t id) const {
    const auto source = repository_->find_data_source(id);
    if (!source) throw NotFound("data source not found: " + std::to_string(id));
    return *source;
}

std::vector<FieldMapping> IngestionService::replace_field_mappings(std::int64_t source_id,
                                                                   const Json& body) {
    get_data_source(source_id);
    Json array;
    if (body.is_array()) {
        array = body;
    } else if (body.is_object() && body.contains("mappings") && body.at("mappings").is_array()) {
        array = body.at("mappings");
    } else {
        throw BadRequest("body must be an array of mappings or an object with 'mappings'");
    }

    std::vector<FieldMapping> mappings;
    std::set<std::string> source_fields;
    for (const auto& item : array.as_array()) {
        if (!item.is_object()) throw BadRequest("each mapping must be a JSON object");
        FieldMapping mapping;
        mapping.data_source_id = source_id;
        mapping.source_field = require_string(item, "source_field");
        mapping.standard_field = require_string(item, "standard_field");
        mapping.transform_expr = optional_string(item, "transform_expr");
        if (!is_one_of(mapping.transform_expr, {"", "trim", "lower", "upper", "number"}))
            throw BadRequest("unsupported transform_expr: " + mapping.transform_expr);
        if (!source_fields.insert(mapping.source_field).second)
            throw BadRequest("duplicate source_field: " + mapping.source_field);
        mappings.push_back(std::move(mapping));
    }
    repository_->replace_field_mappings(source_id, mappings);
    return repository_->list_field_mappings(source_id);
}

std::vector<FieldMapping> IngestionService::list_field_mappings(std::int64_t source_id) const {
    get_data_source(source_id);
    return repository_->list_field_mappings(source_id);
}

ImportTask IngestionService::create_import_task(const Json& body) {
    ImportTask task;
    task.data_source_id = require_int64(body, "data_source_id");
    get_data_source(task.data_source_id);
    task.name = require_string(body, "name");
    task.import_mode = optional_string(body, "import_mode", "manual");
    task.schedule_expr = optional_string(body, "schedule_expr");
    task.validation_rules = validation_rules_text(body);

    if (!is_one_of(task.import_mode, {"manual", "scheduled", "push"}))
        throw BadRequest("import_mode must be manual, scheduled or push");
    if (task.import_mode == "scheduled" && task.schedule_expr.empty())
        throw BadRequest("schedule_expr is required for scheduled tasks");

    const auto id = repository_->create_import_task(task);
    return get_import_task(id);
}

std::vector<ImportTask> IngestionService::list_import_tasks() const {
    return repository_->list_import_tasks();
}

ImportTask IngestionService::get_import_task(std::int64_t id) const {
    const auto task = repository_->find_import_task(id);
    if (!task) throw NotFound("import task not found: " + std::to_string(id));
    return *task;
}

ImportTask IngestionService::update_schedule(std::int64_t id, const Json& body) {
    const auto task = get_import_task(id);
    const std::string import_mode = optional_string(body, "import_mode", task.import_mode);
    const std::string schedule_expr =
        optional_string(body, "schedule_expr", task.schedule_expr);
    if (!is_one_of(import_mode, {"manual", "scheduled", "push"}))
        throw BadRequest("import_mode must be manual, scheduled or push");
    if (import_mode == "scheduled" && schedule_expr.empty())
        throw BadRequest("schedule_expr is required for scheduled tasks");
    repository_->update_import_task_schedule(id, import_mode, schedule_expr);
    return get_import_task(id);
}

ImportExecution IngestionService::execute_import_task(std::int64_t id, const Json& body) {
    const auto task = get_import_task(id);
    const std::string idempotency_key = optional_string(body, "idempotency_key");
    if (!idempotency_key.empty()) {
        if (const auto existing = repository_->find_import_run_by_key(id, idempotency_key)) {
            ImportExecution execution;
            execution.run = *existing;
            execution.failures = repository_->list_raw_records(existing->id, true);
            return execution;
        }
    }

    const Json records = records_from_body(body);
    const auto mappings = repository_->list_field_mappings(task.data_source_id);
    const Json rules = parse_json_or(task.validation_rules, Json::object());

    const auto run_id = repository_->start_import_run(id, idempotency_key);
    repository_->update_import_task_status(id, "running");

    std::int64_t total = 0;
    std::int64_t success = 0;
    std::int64_t failure = 0;
    try {
        const auto& rows = records.as_array();
        for (std::size_t index = 0; index < rows.size(); ++index) {
            const Json& row = rows[index];
            RawRecord record;
            record.run_id = run_id;
            record.source_row_no = static_cast<std::int64_t>(index + 1);
            record.payload = row.dump();

            std::string reason;
            if (!row.is_object()) {
                reason = "row must be a JSON object";
            } else {
                std::string mapping_error;
                const Json standard = apply_mappings(row, mappings, mapping_error);
                if (!mapping_error.empty())
                    reason = mapping_error;
                else
                    reason = validate_row(standard, rules);
            }

            record.valid = reason.empty();
            record.error_reason = reason;
            repository_->add_raw_record(record);
            ++total;
            if (record.valid)
                ++success;
            else
                ++failure;
        }
    } catch (...) {
        repository_->finish_import_run(run_id, "failed", total, success, failure);
        repository_->update_import_task_status(id, "failed");
        throw;
    }

    const std::string status = execution_status(success, failure);
    repository_->finish_import_run(run_id, status, total, success, failure);
    repository_->update_import_task_status(id, status);

    ImportExecution execution;
    execution.run = get_import_run(run_id);
    execution.failures = list_raw_records(run_id, true);
    return execution;
}

ImportExecution IngestionService::retry_import_run(std::int64_t run_id, const Json& body) {
    const auto run = get_import_run(run_id);

    Json records;
    if (body.is_array()) {
        records = body;
    } else if (body.is_object() && body.contains("records")) {
        if (!body.at("records").is_array())
            throw BadRequest("records must be an array when retrying a run");
        records = body.at("records");
    } else {
        const auto failed = repository_->list_raw_records(run_id, true);
        if (failed.empty()) throw BadRequest("no failed records to retry");
        records = Json::array();
        for (const auto& record : failed) {
            try {
                records.push_back(Json::parse(record.payload));
            } catch (const JsonError&) {
                throw BadRequest("stored raw record is not valid JSON");
            }
        }
    }

    Json request = Json::object();
    request["records"] = records;
    if (body.is_object() && body.contains("idempotency_key"))
        request["idempotency_key"] = body.at("idempotency_key");
    return execute_import_task(run.task_id, request);
}

ImportExecution IngestionService::push_records(const Json& body) {
    if (!body.is_object()) throw BadRequest("request body must be a JSON object");
    std::int64_t task_id = optional_int64(body, "task_id", 0);
    if (task_id == 0) {
        const auto source_id = require_int64(body, "source_id");
        const auto source = get_data_source(source_id);
        for (const auto& task : repository_->list_import_tasks()) {
            if (task.data_source_id == source_id && task.import_mode == "push") {
                task_id = task.id;
                break;
            }
        }
        if (task_id == 0) {
            ImportTask task;
            task.data_source_id = source_id;
            task.name = "外部推送：" + source.name;
            task.import_mode = "push";
            task.validation_rules = "{}";
            task_id = repository_->create_import_task(task);
        }
    } else {
        get_import_task(task_id);
    }
    return execute_import_task(task_id, body);
}

ImportRun IngestionService::get_import_run(std::int64_t id) const {
    const auto run = repository_->find_import_run(id);
    if (!run) throw NotFound("import run not found: " + std::to_string(id));
    return *run;
}

std::vector<RawRecord> IngestionService::list_raw_records(std::int64_t run_id,
                                                          bool only_failed) const {
    get_import_run(run_id);
    return repository_->list_raw_records(run_id, only_failed);
}

}  // namespace platform::data_ingestion
