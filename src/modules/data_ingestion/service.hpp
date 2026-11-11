#pragma once

#include "modules/data_ingestion/model.hpp"
#include "modules/data_ingestion/repository.hpp"
#include "platform/db.hpp"
#include "platform/json.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace platform::data_ingestion {

struct ImportExecution {
    ImportRun run;
    std::vector<RawRecord> failures;
};

class IngestionService {
public:
    IngestionService(std::shared_ptr<Database> database,
                     std::shared_ptr<IngestionRepository> repository);

    DataSource create_data_source(const Json& body);
    std::vector<DataSource> list_data_sources() const;
    DataSource get_data_source(std::int64_t id) const;

    std::vector<FieldMapping> replace_field_mappings(std::int64_t source_id, const Json& body);
    std::vector<FieldMapping> list_field_mappings(std::int64_t source_id) const;

    ImportTask create_import_task(const Json& body);
    std::vector<ImportTask> list_import_tasks() const;
    ImportTask get_import_task(std::int64_t id) const;
    ImportTask update_schedule(std::int64_t id, const Json& body);

    ImportExecution execute_import_task(std::int64_t id, const Json& body);
    ImportExecution retry_import_run(std::int64_t run_id, const Json& body);
    ImportExecution push_records(const Json& body);
    ImportRun get_import_run(std::int64_t id) const;
    std::vector<RawRecord> list_raw_records(std::int64_t run_id, bool only_failed) const;

private:
    std::shared_ptr<Database> database_;
    std::shared_ptr<IngestionRepository> repository_;
};

}  // namespace platform::data_ingestion
