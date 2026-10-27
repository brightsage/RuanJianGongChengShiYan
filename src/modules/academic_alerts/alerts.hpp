#pragma once

#include "platform/app.hpp"

#include <cstdint>
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
// 业务规则（《实验一》3.3 UC-AA-05）：
//   - 越权不可认领；未填干预措施不得关闭
//   - 转专业/休学可标记「不适用」，需填原因
//   - 关闭后保留历史，不可修改
//   - 生成预警幂等
// ============================================================================

namespace platform::academic_alerts {

// ---------------------------------------------------------------------------
// 领域模型
// ---------------------------------------------------------------------------

struct TermScore {
    std::string term;
    double average = 0.0;
};

struct Student {
    std::string id;
    std::string name;
    std::string class_name;
    std::string major;
    std::string grade;
    double avg_score = 0.0;      // 综合成绩
    int failed_courses = 0;      // 不及格门数
    double credit_rate = 0.0;    // 学分完成率 0~1
    double attendance = 0.0;     // 出勤率 0~1
    std::vector<TermScore> trend;
};

struct Rule {
    std::string id;
    std::string name;
    std::string metric;    // avg_score | failed_courses | credit_rate | attendance
    std::string op;        // lt | lte | gt | gte
    double threshold = 0.0;
    std::string level;     // red | orange | yellow
    std::string notify;    // 通知对象
    bool enabled = true;
};

struct Intervention {
    std::string at;
    std::string action;    // claim | follow | close | not_applicable
    std::string measures;
    std::string note;
    std::string by;
};

struct Alert {
    int id = 0;
    std::string student_id;
    std::string rule_id;
    std::string level;
    std::string status;    // open | claimed | closed | not_applicable
    std::int64_t created = 0;
    std::string assignee;
    double metric_value = 0.0;
    std::vector<Intervention> interventions;
};

// ---------------------------------------------------------------------------
// 内存数据（进程内单例，后续接入数据库时只需替换 store 的实现）
// ---------------------------------------------------------------------------

std::vector<Student>& students();
std::vector<Rule>& rules();
std::vector<Alert>& alerts();
int next_alert_id();

// ---------------------------------------------------------------------------
// 查询与判定
// ---------------------------------------------------------------------------

Student* find_student(const std::string& id);
Alert* find_alert(int id);

bool is_metric(const std::string& metric);
bool is_operator(const std::string& op);
bool is_level(const std::string& level);

double metric_value(const Student& student, const std::string& metric);
bool hit(const Rule& rule, const Student& student, double* value);
bool active_status(const std::string& status);
bool overdue(const Alert& alert);

/// 解析正整数 id，非法返回 -1（用于路径参数里的 :alert_id）
int parse_id(const std::string& text);

// ---------------------------------------------------------------------------
// 时间
// ---------------------------------------------------------------------------

std::int64_t now_epoch();
std::string iso_time(std::int64_t epoch);

// ---------------------------------------------------------------------------
// 序列化
// ---------------------------------------------------------------------------

std::string alert_json(const Alert& alert);

// ---------------------------------------------------------------------------
// 路由处理函数（在 alerts_api.cpp 实现）
// ---------------------------------------------------------------------------

Response list_rules(const Request& request);
Response create_rule(const Request& request);
Response dashboard(const Request& request);
Response generate_alerts(const Request& request);
Response list_alerts(const Request& request);
Response claim_alert(const Request& request);
Response handle_alert(const Request& request);
Response interventions(const Request& request);
Response student_trend(const Request& request);

}  // namespace platform::academic_alerts
