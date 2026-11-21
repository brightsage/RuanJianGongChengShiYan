#pragma once

#include "data_governance/types.h"
#include "platform/db.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace data_governance {

// 一条有效原始记录：payload 是 M1 导入时保存的源行 JSON，
// data_source_id 用来取该数据源的字段映射（源字段 → 标准字段）。
struct RawRecordRow {
    std::int64_t id = 0;
    std::int64_t data_source_id = 0;
    std::string payload;
};

struct FieldRename {
    std::string source_field;
    std::string standard_field;
    std::string transform_expr;
};

// 数据治理模块的持久化边界：只负责 SQL，业务规则放在 GovernanceService。
class GovRepository {
public:
    explicit GovRepository(platform::Database& database);

    // 质量规则（gov_quality_rule）
    std::vector<QualityRule> list_quality_rules() const;
    std::optional<QualityRule> find_quality_rule(std::int64_t rule_id) const;
    bool quality_rule_name_exists(const std::string& rule_name) const;
    QualityRule create_quality_rule(const QualityRule& rule);
    std::optional<QualityRule> update_quality_rule(std::int64_t rule_id, bool enabled,
                                                   const std::string& level);

    // 学生主数据（gov_student_master）
    std::vector<StudentMaster> list_student_master() const;
    std::optional<StudentMaster> find_student_master(const std::string& student_id) const;
    std::int64_t count_student_master() const;
    std::string latest_merge_time() const;
    StudentMaster upsert_student_master(const StudentMaster& master);

    // 指标口径（gov_metric_def）
    std::vector<MetricDefinition> list_metric_definitions(const std::string& term = {}) const;
    std::string latest_metric_term() const;

    // M1 原始记录（只读）
    struct RawStats {
        std::int64_t total = 0;
        std::int64_t cleaned = 0;
    };
    std::vector<RawRecordRow> list_valid_raw_records() const;
    std::map<std::int64_t, std::vector<FieldRename>> field_mappings() const;
    RawStats raw_record_stats() const;

    // 重算任务（gov_recalc_task）
    std::int64_t create_recalc_task(const std::string& term);
    void finish_recalc_task(std::int64_t task_id, std::int64_t student_count,
                            std::int64_t metric_count);

private:
    platform::Database& db_;
};

}  // namespace data_governance
