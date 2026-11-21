#include "modules/data_governance/repository.hpp"

#include <utility>

namespace data_governance {
namespace {

QualityRule read_rule(const platform::DbRow& row) {
    QualityRule rule;
    rule.rule_id = row.get_int64("id");
    rule.rule_name = row.get_string("rule_name");
    rule.rule_type = row.get_string("rule_type");
    rule.target_field = row.get_string("target_field");
    rule.level = row.get_string("level", "warn");
    rule.enabled = row.get_bool("enabled", true);
    return rule;
}

StudentMaster read_master(const platform::DbRow& row) {
    StudentMaster master;
    master.student_id = row.get_string("student_id");
    master.student_no = row.get_string("student_no");
    master.name = row.get_string("name");
    master.major = row.get_string("major");
    master.class_name = row.get_string("class_name");
    master.org_id = row.get_string("org_id");
    master.status = row.get_string("status", "active");
    master.source = row.get_string("source");
    master.merged_at = row.get_string("merged_at");
    return master;
}

MetricDefinition read_metric(const platform::DbRow& row) {
    MetricDefinition definition;
    definition.metric_code = row.get_string("metric_code");
    definition.name = row.get_string("name");
    definition.formula = row.get_string("formula");
    definition.weight = row.get_double("weight", 1.0);
    definition.term = row.get_string("term");
    definition.version = static_cast<int>(row.get_int64("version", 1));
    definition.active = row.get_bool("active", true);
    return definition;
}

}  // namespace

GovRepository::GovRepository(platform::Database& database) : db_(database) {
    // DDL 放在模块启动阶段：MySQL 的 DDL 会隐式提交，不能放进请求事务。
    db_.exec(R"sql(
CREATE TABLE IF NOT EXISTS gov_quality_rule (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    rule_name VARCHAR(200) NOT NULL,
    rule_type VARCHAR(64) NOT NULL,
    target_field VARCHAR(100) NOT NULL,
    level VARCHAR(32) NOT NULL DEFAULT 'warn',
    enabled TINYINT NOT NULL DEFAULT 1,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uq_gov_rule_name (rule_name)
);

CREATE TABLE IF NOT EXISTS gov_student_master (
    student_id VARCHAR(64) PRIMARY KEY,
    student_no VARCHAR(64) NOT NULL DEFAULT '',
    name VARCHAR(200) NOT NULL DEFAULT '',
    major VARCHAR(200) NOT NULL DEFAULT '',
    class_name VARCHAR(200) NOT NULL DEFAULT '',
    org_id VARCHAR(100) NOT NULL DEFAULT '',
    status VARCHAR(32) NOT NULL DEFAULT 'active',
    source VARCHAR(128) NOT NULL DEFAULT '',
    merged_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_gov_master_no (student_no)
);

CREATE TABLE IF NOT EXISTS gov_metric_def (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    metric_code VARCHAR(100) NOT NULL,
    name VARCHAR(200) NOT NULL,
    formula VARCHAR(255) NOT NULL,
    weight DOUBLE NOT NULL DEFAULT 1,
    term VARCHAR(64) NOT NULL DEFAULT '',
    version INT NOT NULL DEFAULT 1,
    active TINYINT NOT NULL DEFAULT 1,
    UNIQUE KEY uq_gov_metric_def (metric_code, term, version)
);

CREATE TABLE IF NOT EXISTS gov_recalc_task (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    term VARCHAR(64) NOT NULL,
    status VARCHAR(32) NOT NULL DEFAULT 'running',
    student_count BIGINT NOT NULL DEFAULT 0,
    metric_count BIGINT NOT NULL DEFAULT 0,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    finished_at DATETIME NULL
);
)sql");

    // 演示用种子数据：只在缺失时插入，不覆盖已有配置。
    db_.exec(
        "INSERT IGNORE INTO gov_quality_rule (rule_name, rule_type, target_field, level, enabled) "
        "VALUES ('学号非空校验','not_null','student_no','error',1),"
        "('成绩值域校验','range','score','warn',1),"
        "('身份证格式校验','format','id_card','error',1)");

    db_.exec(
        "INSERT IGNORE INTO gov_student_master "
        "(student_id, student_no, name, major, class_name, org_id, status, source) VALUES "
        "('1001','2023001','张三','软件工程','软工2301','college-cs','active','seed'),"
        "('1002','2023002','李四','计算机科学','计科2302','college-cs','active','seed'),"
        "('1003','2023003','王五','软件工程','软工2301','college-cs','active','seed')");

    db_.exec(
        "INSERT IGNORE INTO gov_metric_def "
        "(metric_code, name, formula, weight, term, version, active) VALUES "
        "('score_avg','平均成绩','avg(score)',1.0,'',1,1),"
        "('score_pass_rate','及格率','pass_rate(score,60)',1.0,'',1,1),"
        "('failed_courses','不及格门数','count_if(score,lt,60)',1.0,'',1,1),"
        "('attendance_rate','出勤率','avg(attendance)',1.0,'',1,1),"
        "('credit_total','已修学分','sum(credit)',1.0,'',1,1),"
        "('credit_rate','学分完成率','ratio(credit,20)',1.0,'',1,1)");
}

std::vector<QualityRule> GovRepository::list_quality_rules() const {
    const auto rows = db_.query(
        "SELECT id, rule_name, rule_type, target_field, level, enabled "
        "FROM gov_quality_rule ORDER BY id");
    std::vector<QualityRule> rules;
    rules.reserve(rows.size());
    for (const auto& row : rows) rules.push_back(read_rule(row));
    return rules;
}

std::optional<QualityRule> GovRepository::find_quality_rule(std::int64_t rule_id) const {
    const auto rows = db_.query(
        "SELECT id, rule_name, rule_type, target_field, level, enabled "
        "FROM gov_quality_rule WHERE id = ?",
        {rule_id});
    if (rows.empty()) return std::nullopt;
    return read_rule(rows.front());
}

bool GovRepository::quality_rule_name_exists(const std::string& rule_name) const {
    const auto rows = db_.query(
        "SELECT id FROM gov_quality_rule WHERE rule_name = ? LIMIT 1", {rule_name});
    return !rows.empty();
}

QualityRule GovRepository::create_quality_rule(const QualityRule& rule) {
    db_.exec(
        "INSERT INTO gov_quality_rule (rule_name, rule_type, target_field, level, enabled) "
        "VALUES (?, ?, ?, ?, ?)",
        {rule.rule_name, rule.rule_type, rule.target_field, rule.level,
         static_cast<std::int64_t>(rule.enabled ? 1 : 0)});
    const auto stored = find_quality_rule(db_.last_insert_rowid());
    if (!stored) throw platform::DbError("quality rule was not persisted");
    return *stored;
}

std::optional<QualityRule> GovRepository::update_quality_rule(std::int64_t rule_id, bool enabled,
                                                             const std::string& level) {
    db_.exec("UPDATE gov_quality_rule SET enabled = ?, level = ? WHERE id = ?",
             {static_cast<std::int64_t>(enabled ? 1 : 0), level, rule_id});
    return find_quality_rule(rule_id);
}

std::vector<StudentMaster> GovRepository::list_student_master() const {
    const auto rows = db_.query(
        "SELECT student_id, student_no, name, major, class_name, org_id, status, source, "
        "merged_at FROM gov_student_master ORDER BY student_id");
    std::vector<StudentMaster> students;
    students.reserve(rows.size());
    for (const auto& row : rows) students.push_back(read_master(row));
    return students;
}

std::optional<StudentMaster> GovRepository::find_student_master(
    const std::string& student_id) const {
    const auto rows = db_.query(
        "SELECT student_id, student_no, name, major, class_name, org_id, status, source, "
        "merged_at FROM gov_student_master WHERE student_id = ?",
        {student_id});
    if (rows.empty()) return std::nullopt;
    return read_master(rows.front());
}

std::int64_t GovRepository::count_student_master() const {
    const auto rows = db_.query("SELECT COUNT(*) AS total FROM gov_student_master");
    if (rows.empty()) return 0;
    return rows.front().get_int64("total");
}

std::string GovRepository::latest_merge_time() const {
    const auto rows = db_.query("SELECT MAX(merged_at) AS last_merge FROM gov_student_master");
    if (rows.empty()) return {};
    return rows.front().get_string("last_merge");
}

StudentMaster GovRepository::upsert_student_master(const StudentMaster& master) {
    // 归并规则：新值非空才覆盖，避免低质量来源清空主数据。
    db_.exec(
        "INSERT INTO gov_student_master "
        "(student_id, student_no, name, major, class_name, org_id, status, source) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?) "
        "ON DUPLICATE KEY UPDATE "
        "student_no = IF(VALUES(student_no) = '', student_no, VALUES(student_no)), "
        "name = IF(VALUES(name) = '', name, VALUES(name)), "
        "major = IF(VALUES(major) = '', major, VALUES(major)), "
        "class_name = IF(VALUES(class_name) = '', class_name, VALUES(class_name)), "
        "org_id = IF(VALUES(org_id) = '', org_id, VALUES(org_id)), "
        "status = IF(VALUES(status) = '', status, VALUES(status)), "
        "source = VALUES(source), merged_at = NOW()",
        {master.student_id, master.student_no, master.name, master.major, master.class_name,
         master.org_id, master.status, master.source});
    const auto stored = find_student_master(master.student_id);
    if (!stored) throw platform::DbError("student master was not persisted");
    return *stored;
}

std::vector<MetricDefinition> GovRepository::list_metric_definitions(
    const std::string& term) const {
    std::string sql =
        "SELECT metric_code, name, formula, weight, term, version, active FROM gov_metric_def "
        "WHERE active = 1";
    std::vector<platform::DbValue> params;
    if (!term.empty()) {
        sql += " AND (term = ? OR term = '')";
        params.emplace_back(term);
    }
    sql += " ORDER BY metric_code, version DESC";

    std::vector<MetricDefinition> definitions;
    std::string last_code;
    for (const auto& row : db_.query(sql, params)) {
        auto definition = read_metric(row);
        // 同一指标取最高版本。
        if (definition.metric_code == last_code) continue;
        last_code = definition.metric_code;
        definitions.push_back(std::move(definition));
    }
    return definitions;
}

std::string GovRepository::latest_metric_term() const {
    const auto rows = db_.query(
        "SELECT term FROM gov_metric_def WHERE active = 1 AND term <> '' "
        "ORDER BY term DESC LIMIT 1");
    if (rows.empty()) return {};
    return rows.front().get_string("term");
}

std::vector<RawRecordRow> GovRepository::list_valid_raw_records() const {
    // 带上数据源 id：M2 只读 M1 的字段映射，把源字段名还原成标准字段名。
    const auto rows = db_.query(
        "SELECT r.id, r.payload, COALESCE(t.data_source_id, 0) AS data_source_id "
        "FROM ing_raw_record r "
        "LEFT JOIN ing_import_run run ON run.id = r.run_id "
        "LEFT JOIN ing_import_task t ON t.id = run.task_id "
        "WHERE r.valid = 1 ORDER BY r.id");
    std::vector<RawRecordRow> records;
    records.reserve(rows.size());
    for (const auto& row : rows) {
        records.push_back({row.get_int64("id"), row.get_int64("data_source_id"),
                           row.get_string("payload")});
    }
    return records;
}

std::map<std::int64_t, std::vector<FieldRename>> GovRepository::field_mappings() const {
    const auto rows = db_.query(
        "SELECT data_source_id, source_field, standard_field, transform_expr "
        "FROM ing_field_mapping ORDER BY data_source_id, id");
    std::map<std::int64_t, std::vector<FieldRename>> mappings;
    for (const auto& row : rows) {
        mappings[row.get_int64("data_source_id")].push_back(
            {row.get_string("source_field"), row.get_string("standard_field"),
             row.get_string("transform_expr")});
    }
    return mappings;
}

GovRepository::RawStats GovRepository::raw_record_stats() const {
    const auto rows = db_.query(
        "SELECT COUNT(*) AS total, COALESCE(SUM(valid = 1), 0) AS cleaned FROM ing_raw_record");
    if (rows.empty()) return {};
    return {rows.front().get_int64("total"), rows.front().get_int64("cleaned")};
}

std::int64_t GovRepository::create_recalc_task(const std::string& term) {
    db_.exec("INSERT INTO gov_recalc_task (term, status) VALUES (?, 'running')", {term});
    return db_.last_insert_rowid();
}

void GovRepository::finish_recalc_task(std::int64_t task_id, std::int64_t student_count,
                                       std::int64_t metric_count) {
    db_.exec(
        "UPDATE gov_recalc_task SET status = 'finished', student_count = ?, metric_count = ?, "
        "finished_at = NOW() WHERE id = ?",
        {student_count, metric_count, task_id});
}

}  // namespace data_governance
