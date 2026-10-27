#pragma once

#include <cstdint>
#include <string>

namespace platform::data_ingestion {

// 数据源：对应 UC-DC-01“登记数据源”。
struct DataSource {
    std::int64_t id = 0;
    std::string name;
    std::string kind;         // academic / research / practice
    std::string access_mode;  // file / push / api
    std::string owner;
    std::string description;
    std::string created_at;
};

// 字段映射：对应 UC-DC-07“维护字段映射规则”。
struct FieldMapping {
    std::int64_t id = 0;
    std::int64_t data_source_id = 0;
    std::string source_field;
    std::string standard_field;
    std::string transform_expr;
};

// 导入任务：对应 UC-DC-02“配置导入任务”和 UC-DC-06“调度采集任务”。
struct ImportTask {
    std::int64_t id = 0;
    std::int64_t data_source_id = 0;
    std::string name;
    std::string import_mode;       // manual / scheduled / push
    std::string schedule_expr;     // 例如 every:3600（秒），具体解释由 service 层决定
    std::string validation_rules;  // JSON 文本，由 service 层解释
    std::string status;            // pending / running / success / partial / failed
    std::string created_at;
};

// 一次导入执行：对应 UC-DC-03“执行导入任务”和 UC-DC-04“查看导入结果”。
struct ImportRun {
    std::int64_t id = 0;
    std::int64_t task_id = 0;
    std::string status;  // running / success / partial / failed
    std::int64_t total_count = 0;
    std::int64_t success_count = 0;
    std::int64_t failure_count = 0;
    std::string started_at;
    std::string finished_at;
};

// 原始记录：导入时保留源数据，失败记录同时保留原因，便于重试与追溯。
struct RawRecord {
    std::int64_t id = 0;
    std::int64_t run_id = 0;
    std::int64_t source_row_no = 0;
    std::string payload;  // JSON 文本，保存源行的原始内容
    bool valid = true;
    std::string error_reason;
};

}  // namespace platform::data_ingestion
