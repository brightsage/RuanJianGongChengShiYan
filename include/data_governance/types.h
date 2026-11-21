#pragma once

#include "platform/json.hpp"

#include <cstdint>
#include <string>

namespace data_governance {

// 质量规则：从 gov_quality_rule 读取，由治理模块维护。
struct QualityRule {
    std::int64_t rule_id = 0;
    std::string rule_name;
    std::string rule_type;
    std::string target_field;
    std::string level = "warn";
    bool enabled = true;
};

// 学生主数据：多源归并后的唯一记录，同时同步到 M3 的 sp_student。
struct StudentMaster {
    std::string student_id;
    std::string student_no;
    std::string name;
    std::string major;
    std::string class_name;
    std::string org_id;
    std::string status = "active";
    std::string source;
    std::string merged_at;
};

// 指标口径：gov_metric_def。formula 使用 avg/sum/count/max/min/pass_rate 聚合。
struct MetricDefinition {
    std::string metric_code;
    std::string name;
    std::string formula;
    double weight = 1.0;
    std::string term;
    int version = 1;
    bool active = true;
};

// 质量报告快照数据（每次请求实时统计，不落表）。
struct QualityStats {
    std::int64_t total_records = 0;
    std::int64_t cleaned_records = 0;
    std::int64_t dirty_records = 0;
    std::int64_t master_students = 0;
    std::int64_t covered_students = 0;
    double quality_score = 0.0;
    double main_data_coverage = 0.0;
    std::string last_clean_time;
};

platform::Json rule_to_json(const QualityRule& rule);
platform::Json master_to_json(const StudentMaster& master);
platform::Json metric_to_json(const MetricDefinition& definition);

}  // namespace data_governance
