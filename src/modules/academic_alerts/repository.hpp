#pragma once

#include "alerts.hpp"
#include "platform/db.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace platform::academic_alerts {

/// 一条指标点：某学期某指标的值。
struct MetricPoint {
    std::string term;
    double value = 0.0;
};

// M4 持久化边界：
//   - aa_alert_rule / aa_alert_record / aa_intervention 读写
//   - sp_student / sp_metric_value 只读（M2/M3 产出）
class AlertRepository {
public:
    // 持有连接的所有权：路由 lambda 只捕获 repository，数据库连接必须一起活着。
    explicit AlertRepository(std::shared_ptr<platform::Database> database);

    // 规则（aa_alert_rule）
    std::vector<Rule> list_rules() const;
    std::optional<Rule> find_rule(const std::string& rule_id) const;
    Rule create_rule(Rule rule);
    std::string next_rule_id() const;
    std::optional<Rule> update_rule(const std::string& rule_id, bool enabled, double threshold,
                                    const std::string& level, const std::string& notify);

    // 学生与指标（只读 sp_student / sp_metric_value）
    std::vector<Student> list_students() const;
    std::optional<Student> find_student(const std::string& student_id) const;
    /// student_id → metric_code → 最新学期的指标点
    std::map<std::string, std::map<std::string, MetricPoint>> metrics_by_student() const;
    /// 某学生在给定指标码上的历史（按学期升序）
    std::vector<MetricPoint> metric_history(const std::string& student_id,
                                            const std::vector<std::string>& codes) const;

    // 预警（aa_alert_record）
    std::vector<Alert> list_alerts(const std::string& status, const std::string& level) const;
    std::optional<Alert> find_alert(std::int64_t alert_id) const;
    bool has_active_alert(const std::string& student_id, const std::string& rule_id) const;
    std::int64_t count_alerts() const;
    Alert create_alert(const Alert& alert);
    void claim(std::int64_t alert_id, const std::string& assignee);
    void update_status(std::int64_t alert_id, const std::string& status,
                       const std::string& assignee);

    // 干预记录（aa_intervention）
    Intervention add_intervention(std::int64_t alert_id, const Intervention& intervention);
    std::vector<Intervention> list_interventions(std::int64_t alert_id) const;

private:
    std::shared_ptr<platform::Database> owner_;
    platform::Database& db_;
};

}  // namespace platform::academic_alerts
