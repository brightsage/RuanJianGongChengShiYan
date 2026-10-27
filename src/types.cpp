#include "data_governance/types.h"

namespace data_governance {

// 初始化模拟数据
std::vector<QualityRule> mock_quality_rules = {
    {1, "学号非空校验", "not_null", "student_no", "error"},
    {2, "成绩值域校验", "range", "score", "warn"},
    {3, "身份证格式校验", "format", "id_card", "error"}
};

std::vector<StudentMaster> mock_student_master = {
    {1001, "2023001", "张三", "软件工程", "软工2301", "active"},
    {1002, "2023002", "李四", "计算机科学", "计科2302", "active"},
    {1003, "2023003", "王五", "软件工程", "软工2301", "active"}
};

QualityReport mock_quality_report = {
    15230, 14980, 250, 98.3, 98.7, "2026-10-27 10:30:00"
};

} // namespace data_governance
