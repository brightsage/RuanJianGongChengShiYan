#pragma once

#include "modules/data_ingestion/repository.hpp"
#include "platform/db.hpp"

namespace platform::data_ingestion {

class SqliteIngestionRepository final : public IngestionRepository {
public:
    explicit SqliteIngestionRepository(Database& database);

    std::int64_t create_data_source(const DataSource& source) override;
    std::vector<DataSource> list_data_sources() const override;
    std::optional<DataSource> find_data_source(std::int64_t id) const override;

    std::int64_t add_field_mapping(const FieldMapping& mapping) override;
    std::vector<FieldMapping> list_field_mappings(std::int64_t data_source_id) const override;
    void replace_field_mappings(std::int64_t data_source_id,
                                const std::vector<FieldMapping>& mappings) override;

    std::int64_t create_import_task(const ImportTask& task) override;
    std::vector<ImportTask> list_import_tasks() const override;
    std::optional<ImportTask> find_import_task(std::int64_t id) const override;
    void update_import_task_status(std::int64_t id, const std::string& status) override;

    std::int64_t start_import_run(std::int64_t task_id) override;
    void finish_import_run(std::int64_t run_id, const std::string& status,
                           std::int64_t total_count, std::int64_t success_count,
                           std::int64_t failure_count) override;
    std::optional<ImportRun> find_import_run(std::int64_t run_id) const override;

    std::int64_t add_raw_record(const RawRecord& record) override;
    std::vector<RawRecord> list_raw_records(std::int64_t run_id,
                                            bool only_failed = false) const override;

private:
    Database& db_;
};

}  // namespace platform::data_ingestion
