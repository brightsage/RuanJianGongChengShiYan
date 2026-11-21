#pragma once

#include "platform/app.hpp"
#include "platform/security.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

// ============================================================================
// M4 学业分析与预警 · 领域模型与接口声明
//
// 需求对应（《实验一 需求分析》2.3.5）：
//   UC-AA-01 查看学业看板   GET  /dashboard
//   UC-AA-02 群体分析       GET  /dashboard?group=class|major|grade
//   UC-AA-03 配置预警规则   GET/POST /rules
//   UC-AA-04 生成预警名单   POST /alerts/generate
//   UC-AA-05 处置预警       POST /alerts/:alert_id/claim  /handle
//   UC-AA-06 跟踪干预记录   GET  /alerts/:alert_id/interventions
//   UC-AA-07 分析学业趋势   GET  /students/:student_id/trend
//
// 数据来源（阶段 5 起）：
//   - 学生与指标：只读 M3 的 sp_student / sp_metric_value（指标口径由 M2 产出）
//   - 规则、预警、干预记录：本模块 aa_* 三张表，MySQL 持久化
//
// 业务规则（《实验一》3.3 UC-AA-05）：
//   - 越权不可认领；未填干预措施不得关闭
//   - 转专业/休学可标记「不适用」，需填原因
//   - 关闭后保留历史，不可修改
//   - 生成预警幂等（同学生同规则存在未关闭预警时不重复生成）
// ============================================================================

namespace platform::academic_alerts {

// ---------------------------------------------------------------------------
// 领域模型
// ---------------------------------------------------------------------------

/// 指标绑定：规则里的 metric 名 → M2 指标码 + 换算系数。
/// 例：attendance 绑定 attendance_rate，M2 存百分比，规则阈值按 0~1 比较。
struct MetricBinding {
    const char* metric;
    const char* code;
    double scale;
};

const std::vector<MetricBinding>& metric_bindings();

struct TermScore {
    std::string term;
    double average = 0.0;
};

/// 学生视图：基础信息来自 sp_student，指标来自 sp_metric_value（已按绑定换算）。
struct Student {
    std::string id;
    std::string name;
    std::string class_name;
    std::string major;
    std::string grade;
    std::string org_id;
    std::map<std::string, double> metrics;  // metric 名 → 值
    std::map<std::string, std::string> metric_terms;  // metric 名 → 学期
};

struct Rule {
    std::string id;
    std::string name;
    std::string metric;  // avg_score | failed_courses | credit_rate | attendance
    std::string op;      // lt | lte | gt | gte
    double threshold = 0.0;
    std::string level;   // red | orange | yellow
    std::string notify;  // 通知对象
    bool enabled = true;
};

struct Intervention {
    std::string at;  // ISO-8601
    std::string action;  // claim | follow | close | not_applicable
    std::string measures;
    std::string note;
    std::string by;
    std::int64_t epoch = 0;  // 落库用，序列化时不输出
};

struct Alert {
    std::int64_t id = 0;
    std::string student_id;
    std::string rule_id;
    std::string level;
    std::string status;  // open | claimed | closed | not_applicable
    std::int64_t created = 0;  // epoch seconds
    std::string assignee;
    double metric_value = 0.0;
    std::string metric_term;
    std::vector<Intervention> interventions;
    // 展示字段（从 sp_student 补齐）
    std::string student_name;
    std::string class_name;
    std::string org_id;
};

// ---------------------------------------------------------------------------
// 判定与工具
// ---------------------------------------------------------------------------

bool is_metric(const std::string& metric);
bool is_operator(const std::string& op);
bool is_level(const std::string& level);
bool active_status(const std::string& status);
bool overdue(const Alert& alert);
double metric_value(const Student& student, const std::string& metric);
bool hit(const Rule& rule, const Student& student, double* value);

/// 解析正整数 id，非法返回 -1（用于路径参数里的 :alert_id）
std::int64_t parse_id(const std::string& text);

std::int64_t now_epoch();
std::string iso_time(std::int64_t epoch);

/// 序列化单条预警（含学生信息与干预记录）
std::string alert_json(const Alert& alert);

// ---------------------------------------------------------------------------
// 路由处理函数（在 alerts_api.cpp 实现）
//
// actor 为空表示平台未启用鉴权（单元测试/旧调用方式），此时不做数据范围过滤。
// ---------------------------------------------------------------------------

class AlertRepository;  // 实现在 repository.hpp

Response list_rules(AlertRepository& repository, const Request& request,
                    const security::Principal* actor);
Response create_rule(AlertRepository& repository, const Request& request,
                     const security::Principal* actor);
Response update_rule(AlertRepository& repository, const Request& request,
                     const security::Principal* actor);
Response dashboard(AlertRepository& repository, const Request& request,
                   const security::Principal* actor);
Response generate_alerts(AlertRepository& repository, const Request& request,
                         const security::Principal* actor);
Response list_alerts(AlertRepository& repository, const Request& request,
                     const security::Principal* actor);
Response claim_alert(AlertRepository& repository, const Request& request,
                     const security::Principal* actor);
Response handle_alert(AlertRepository& repository, const Request& request,
                      const security::Principal* actor);
Response interventions(AlertRepository& repository, const Request& request,
                       const security::Principal* actor);
Response student_trend(AlertRepository& repository, const Request& request,
                       const security::Principal* actor);

}  // namespace platform::academic_alerts
