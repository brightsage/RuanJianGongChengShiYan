#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace data_governance {

// 第一部分：公共数据结构定义
struct QualityRule {
    int rule_id;
    std::string rule_name;
    std::string rule_type;
    std::string target_field;
    std::string level;
};

struct StudentMaster {
    int student_id;
    std::string student_no;
    std::string name;
    std::string major;
    std::string class_name;
    std::string status;
};

struct QualityReport {
    int total_records;
    int cleaned_records;
    int dirty_records;
    double quality_score;
    double main_data_coverage;
    std::string last_clean_time;
};

// 内存模拟数据
extern std::vector<QualityRule> mock_quality_rules;
extern std::vector<StudentMaster> mock_student_master;
extern QualityReport mock_quality_report;

} // namespace data_governance
