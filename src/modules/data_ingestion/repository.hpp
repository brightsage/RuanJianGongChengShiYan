#pragma once

#include "modules/data_ingestion/model.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace platform::data_ingestion {

// 数据接入模块的持久化边界。业务逻辑（字段校验、导入执行、重试幂等）
// 放在 service 层，仓储只负责读写，便于以后替换存储实现或写测试替身。
class IngestionRepository {
public:
    virtual ~IngestionRepository() = default;

    virtual std::int64_t create_data_source(const DataSource& source) = 0;
    virtual std::vector<DataSource> list_data_sources() const = 0;
    virtual std::optional<DataSource> find_data_source(std::int64_t id) const = 0;

    virtual std::int64_t add_field_mapping(const FieldMapping& mapping) = 0;
    virtual std::vector<FieldMapping> list_field_mappings(std::int64_t data_source_id) const = 0;
    virtual void replace_field_mappings(std::int64_t data_source_id,
                                        const std::vector<FieldMapping>& mappings) = 0;

    virtual std::int64_t create_import_task(const ImportTask& task) = 0;
    virtual std::vector<ImportTask> list_import_tasks() const = 0;
    virtual std::optional<ImportTask> find_import_task(std::int64_t id) const = 0;
    virtual void update_import_task_status(std::int64_t id, const std::string& status) = 0;

    virtual std::int64_t start_import_run(std::int64_t task_id) = 0;
    virtual void finish_import_run(std::int64_t run_id, const std::string& status,
                                   std::int64_t total_count, std::int64_t success_count,
                                   std::int64_t failure_count) = 0;
    virtual std::optional<ImportRun> find_import_run(std::int64_t run_id) const = 0;

    virtual std::int64_t add_raw_record(const RawRecord& record) = 0;
    virtual std::vector<RawRecord> list_raw_records(std::int64_t run_id,
                                                    bool only_failed = false) const = 0;
};

}  // namespace platform::data_ingestion
