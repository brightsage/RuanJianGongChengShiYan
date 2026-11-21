#include "repository.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

namespace platform::academic_alerts {
namespace {

Rule read_rule(const platform::DbRow& row) {
    Rule rule;
    rule.id = row.get_string("id");
    rule.name = row.get_string("name");
    rule.metric = row.get_string("metric");
    rule.op = row.get_string("operator");
    rule.threshold = row.get_double("threshold");
    rule.level = row.get_string("level");
    rule.notify = row.get_string("notify");
    rule.enabled = row.get_bool("enabled", true);
    return rule;
}

Alert read_alert(const platform::DbRow& row) {
    Alert alert;
    alert.id = row.get_int64("id");
    alert.student_id = row.get_string("student_id");
    alert.rule_id = row.get_string("rule_id");
    alert.level = row.get_string("level");
    alert.status = row.get_string("status");
    alert.metric_value = row.get_double("metric_value");
    alert.metric_term = row.get_string("metric_term");
    alert.created = row.get_int64("created_at");
    alert.assignee = row.get_string("assignee");
    return alert;
}

Intervention read_intervention(const platform::DbRow& row) {
    Intervention item;
    item.epoch = row.get_int64("created_at");
    item.at = iso_time(item.epoch);
    item.action = row.get_string("action");
    item.measures = row.get_string("measures");
    item.note = row.get_string("note");
    item.by = row.get_string("actor");
    return item;
}

int rule_sequence(const std::string& rule_id) {
    if (rule_id.size() < 2 || (rule_id[0] != 'R' && rule_id[0] != 'r')) return 0;
    for (std::size_t i = 1; i < rule_id.size(); ++i)
        if (!std::isdigit(static_cast<unsigned char>(rule_id[i]))) return 0;
    return std::stoi(rule_id.substr(1));
}

}  // namespace

AlertRepository::AlertRepository(std::shared_ptr<platform::Database> database)
    : owner_(std::move(database)), db_(*owner_) {
    // DDL 放在模块启动阶段：MySQL 的 DDL 会隐式提交，不能放进请求事务。
    db_.exec(R"sql(
CREATE TABLE IF NOT EXISTS aa_alert_rule (
    id VARCHAR(32) PRIMARY KEY,
    name VARCHAR(200) NOT NULL,
    metric VARCHAR(64) NOT NULL,
    operator VARCHAR(8) NOT NULL,
    threshold DOUBLE NOT NULL,
    level VARCHAR(16) NOT NULL,
    notify VARCHAR(200) NOT NULL DEFAULT '辅导员',
    enabled TINYINT NOT NULL DEFAULT 1,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS aa_alert_record (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    student_id VARCHAR(64) NOT NULL,
    rule_id VARCHAR(32) NOT NULL,
    level VARCHAR(16) NOT NULL,
    status VARCHAR(32) NOT NULL DEFAULT 'open',
    metric_value DOUBLE NOT NULL,
    metric_term VARCHAR(64) NOT NULL DEFAULT '',
    created_at BIGINT NOT NULL,
    assignee VARCHAR(200) NOT NULL DEFAULT '',
    INDEX idx_aa_record_student (student_id, rule_id, status),
    INDEX idx_aa_record_status (status, level),
    CONSTRAINT fk_aa_record_rule FOREIGN KEY (rule_id) REFERENCES aa_alert_rule(id)
);

CREATE TABLE IF NOT EXISTS aa_intervention (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    alert_id BIGINT NOT NULL,
    action VARCHAR(32) NOT NULL,
    measures TEXT NOT NULL,
    note TEXT NOT NULL,
    actor VARCHAR(200) NOT NULL,
    created_at BIGINT NOT NULL,
    INDEX idx_aa_intervention_alert (alert_id),
    CONSTRAINT fk_aa_intervention_alert FOREIGN KEY (alert_id)
        REFERENCES aa_alert_record(id) ON DELETE CASCADE
);
)sql");

    // 默认规则只在缺失时插入，不覆盖已有配置。
    db_.exec(
        "INSERT IGNORE INTO aa_alert_rule "
        "(id, name, metric, operator, threshold, level, notify, enabled) VALUES "
        "('R1','综合成绩不达标','avg_score','lt',60.0,'red','辅导员、学生本人',1),"
        "('R2','多门课程不及格','failed_courses','gte',2.0,'orange','辅导员',1),"
        "('R3','出勤率偏低','attendance','lt',0.80,'yellow','辅导员',1),"
        "('R4','学分完成率偏低','credit_rate','lt',0.60,'orange','辅导员、院系管理员',1)");
}

std::vector<Rule> AlertRepository::list_rules() const {
    const auto rows = db_.query(
        "SELECT id, name, metric, operator, threshold, level, notify, enabled "
        "FROM aa_alert_rule ORDER BY id");
    std::vector<Rule> rules;
    rules.reserve(rows.size());
    for (const auto& row : rows) rules.push_back(read_rule(row));
    return rules;
}

std::optional<Rule> AlertRepository::find_rule(const std::string& rule_id) const {
    const auto rows = db_.query(
        "SELECT id, name, metric, operator, threshold, level, notify, enabled "
        "FROM aa_alert_rule WHERE id = ?",
        {rule_id});
    if (rows.empty()) return std::nullopt;
    return read_rule(rows.front());
}

std::string AlertRepository::next_rule_id() const {
    int next = 1;
    for (const auto& rule : list_rules()) next = std::max(next, rule_sequence(rule.id) + 1);
    return "R" + std::to_string(next);
}

Rule AlertRepository::create_rule(Rule rule) {
    if (rule.id.empty()) rule.id = next_rule_id();
    db_.exec(
        "INSERT INTO aa_alert_rule (id, name, metric, operator, threshold, level, notify, enabled) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
        {rule.id, rule.name, rule.metric, rule.op, rule.threshold, rule.level, rule.notify,
         static_cast<std::int64_t>(rule.enabled ? 1 : 0)});
    const auto stored = find_rule(rule.id);
    if (!stored) throw platform::DbError("alert rule was not persisted");
    return *stored;
}

std::optional<Rule> AlertRepository::update_rule(const std::string& rule_id, bool enabled,
                                                 double threshold, const std::string& level,
                                                 const std::string& notify) {
    db_.exec(
        "UPDATE aa_alert_rule SET enabled = ?, threshold = ?, level = ?, notify = ? WHERE id = ?",
        {static_cast<std::int64_t>(enabled ? 1 : 0), threshold, level, notify, rule_id});
    return find_rule(rule_id);
}

std::vector<Student> AlertRepository::list_students() const {
    const auto rows = db_.query(
        "SELECT student_id, name, major, class_name, grade, org_id "
        "FROM sp_student ORDER BY student_id");
    std::vector<Student> students;
    students.reserve(rows.size());
    for (const auto& row : rows) {
        Student student;
        student.id = row.get_string("student_id");
        student.name = row.get_string("name");
        student.major = row.get_string("major");
        student.class_name = row.get_string("class_name");
        student.grade = std::to_string(row.get_int64("grade"));
        student.org_id = row.get_string("org_id");
        students.push_back(std::move(student));
    }
    return students;
}

std::optional<Student> AlertRepository::find_student(const std::string& student_id) const {
    for (const auto& student : list_students())
        if (student.id == student_id) return student;
    return std::nullopt;
}

std::map<std::string, std::map<std::string, MetricPoint>>
AlertRepository::metrics_by_student() const {
    // 每个指标取最新学期（term 降序，空学期排最后作为兜底）。
    const auto rows = db_.query(
        "SELECT student_id, metric_code, term, value FROM sp_metric_value "
        "ORDER BY student_id, metric_code, term DESC, calculated_at DESC, id DESC");
    std::map<std::string, std::map<std::string, MetricPoint>> result;
    for (const auto& row : rows) {
        auto& per_student = result[row.get_string("student_id")];
        const std::string code = row.get_string("metric_code");
        if (per_student.count(code) != 0) continue;
        per_student[code] = {row.get_string("term"), row.get_double("value")};
    }
    return result;
}

std::vector<MetricPoint> AlertRepository::metric_history(
    const std::string& student_id, const std::vector<std::string>& codes) const {
    const auto rows = db_.query(
        "SELECT metric_code, term, value FROM sp_metric_value "
        "WHERE student_id = ? AND term <> '' ORDER BY term",
        {student_id});
    std::vector<MetricPoint> points;
    for (const auto& row : rows) {
        const std::string code = row.get_string("metric_code");
        if (std::find(codes.begin(), codes.end(), code) == codes.end()) continue;
        const std::string term = row.get_string("term");
        if (!points.empty() && points.back().term == term) continue;
        points.push_back({term, row.get_double("value")});
    }
    return points;
}

std::vector<Alert> AlertRepository::list_alerts(const std::string& status,
                                                const std::string& level) const {
    std::string sql =
        "SELECT id, student_id, rule_id, level, status, metric_value, metric_term, created_at, "
        "assignee FROM aa_alert_record";
    std::vector<platform::DbValue> params;
    const auto append = [&](const std::string& column, const std::string& value) {
        if (value.empty()) return;
        sql += params.empty() ? " WHERE " : " AND ";
        sql += column + " = ?";
        params.emplace_back(value);
    };
    append("status", status);
    append("level", level);
    sql += " ORDER BY created_at DESC, id DESC";

    const auto rows = db_.query(sql, params);
    std::vector<Alert> alerts;
    alerts.reserve(rows.size());
    for (const auto& row : rows) {
        auto alert = read_alert(row);
        alert.interventions = list_interventions(alert.id);
        alerts.push_back(std::move(alert));
    }
    return alerts;
}

std::optional<Alert> AlertRepository::find_alert(std::int64_t alert_id) const {
    const auto rows = db_.query(
        "SELECT id, student_id, rule_id, level, status, metric_value, metric_term, created_at, "
        "assignee FROM aa_alert_record WHERE id = ?",
        {alert_id});
    if (rows.empty()) return std::nullopt;
    auto alert = read_alert(rows.front());
    alert.interventions = list_interventions(alert.id);
    return alert;
}

bool AlertRepository::has_active_alert(const std::string& student_id,
                                       const std::string& rule_id) const {
    const auto rows = db_.query(
        "SELECT id FROM aa_alert_record WHERE student_id = ? AND rule_id = ? "
        "AND status IN ('open','claimed') LIMIT 1",
        {student_id, rule_id});
    return !rows.empty();
}

std::int64_t AlertRepository::count_alerts() const {
    const auto rows = db_.query("SELECT COUNT(*) AS total FROM aa_alert_record");
    if (rows.empty()) return 0;
    return rows.front().get_int64("total");
}

Alert AlertRepository::create_alert(const Alert& alert) {
    db_.exec(
        "INSERT INTO aa_alert_record "
        "(student_id, rule_id, level, status, metric_value, metric_term, created_at, assignee) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
        {alert.student_id, alert.rule_id, alert.level, alert.status, alert.metric_value,
         alert.metric_term, alert.created, alert.assignee});
    const auto stored = find_alert(db_.last_insert_rowid());
    if (!stored) throw platform::DbError("alert was not persisted");
    return *stored;
}

void AlertRepository::claim(std::int64_t alert_id, const std::string& assignee) {
    db_.exec(
        "UPDATE aa_alert_record SET status = 'claimed', assignee = ? WHERE id = ?",
        {assignee, alert_id});
}

void AlertRepository::update_status(std::int64_t alert_id, const std::string& status,
                                    const std::string& assignee) {
    db_.exec("UPDATE aa_alert_record SET status = ?, assignee = ? WHERE id = ?",
             {status, assignee, alert_id});
}

Intervention AlertRepository::add_intervention(std::int64_t alert_id,
                                               const Intervention& intervention) {
    db_.exec(
        "INSERT INTO aa_intervention (alert_id, action, measures, note, actor, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?)",
        {alert_id, intervention.action, intervention.measures, intervention.note,
         intervention.by, intervention.epoch});
    Intervention stored = intervention;
    if (stored.at.empty()) stored.at = iso_time(stored.epoch);
    return stored;
}

std::vector<Intervention> AlertRepository::list_interventions(std::int64_t alert_id) const {
    const auto rows = db_.query(
        "SELECT action, measures, note, actor, created_at FROM aa_intervention "
        "WHERE alert_id = ? ORDER BY id",
        {alert_id});
    std::vector<Intervention> items;
    items.reserve(rows.size());
    for (const auto& row : rows) items.push_back(read_intervention(row));
    return items;
}

}  // namespace platform::academic_alerts
