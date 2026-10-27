#pragma once

#include <cstdint>
#include <string>

namespace platform::student_profile {

// 学生读模型。学生主数据由“数据治理与标准化”模块负责合并，画像模块只读。
struct Student {
    std::string student_id;
    std::string name;
    std::string major;
    std::string class_name;
    int grade = 0;
    std::string updated_at;
};

// 指标值：指标口径由治理模块配置，画像模块按周期读取/写入计算结果。
struct MetricValue {
    std::int64_t id = 0;
    std::string student_id;
    std::string metric_code;
    std::string term;
    double value = 0.0;
    std::string unit;
    std::string source;
    std::string calculated_at;
};

// 标签定义：对应 UC-SP-01“维护标签体系”。
struct Tag {
    std::string code;
    std::string name;
    std::string dimension;
    std::string description;
};

// 学生标签：evidence 保存标签依据（JSON），对应 UC-SP-04“查看标签依据”。
struct StudentTag {
    std::int64_t id = 0;
    std::string student_id;
    std::string tag_code;
    std::string evidence;
    std::string calculated_at;
};

// 画像快照：某学生某周期的指标与标签汇总，供 UC-SP-03/05/06 查询。
struct ProfileSnapshot {
    std::int64_t id = 0;
    std::string student_id;
    std::string term;
    std::string payload;  // JSON 文本
    std::string generated_at;
};

}  // namespace platform::student_profile
