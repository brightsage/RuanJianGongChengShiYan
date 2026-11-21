#include "modules/student_profile/sqlite_repository.hpp"

#include <utility>

namespace platform::student_profile {
namespace {

Student read_student(const DbRow& row) {
    Student student;
    student.student_id = row.get_string("student_id");
    student.name = row.get_string("name");
    student.major = row.get_string("major");
    student.class_name = row.get_string("class_name");
    student.grade = static_cast<int>(row.get_int64("grade"));
    student.updated_at = row.get_string("updated_at");
    student.org_id = row.get_string("org_id");
    return student;
}

MetricValue read_metric(const DbRow& row) {
    MetricValue metric;
    metric.id = row.get_int64("id");
    metric.student_id = row.get_string("student_id");
    metric.metric_code = row.get_string("metric_code");
    metric.term = row.get_string("term");
    metric.value = row.get_double("value");
    metric.unit = row.get_string("unit");
    metric.source = row.get_string("source");
    metric.calculated_at = row.get_string("calculated_at");
    return metric;
}

Tag read_tag(const DbRow& row) {
    Tag tag;
    tag.code = row.get_string("code");
    tag.name = row.get_string("name");
    tag.dimension = row.get_string("dimension");
    tag.description = row.get_string("description");
    tag.rule = row.get_string("rule");
    return tag;
}

StudentTag read_student_tag(const DbRow& row) {
    StudentTag tag;
    tag.id = row.get_int64("id");
    tag.student_id = row.get_string("student_id");
    tag.tag_code = row.get_string("tag_code");
    tag.term = row.get_string("term");
    tag.evidence = row.get_string("evidence");
    tag.calculated_at = row.get_string("calculated_at");
    return tag;
}

ProfileSnapshot read_snapshot(const DbRow& row) {
    ProfileSnapshot snapshot;
    snapshot.id = row.get_int64("id");
    snapshot.student_id = row.get_string("student_id");
    snapshot.term = row.get_string("term");
    snapshot.payload = row.get_string("payload");
    snapshot.generated_at = row.get_string("generated_at");
    return snapshot;
}

}  // namespace

SqliteProfileRepository::SqliteProfileRepository(Database& database) : db_(database) {
    db_.exec(R"sql(
CREATE TABLE IF NOT EXISTS sp_student (
    student_id VARCHAR(64) PRIMARY KEY,
    name VARCHAR(200) NOT NULL,
    major VARCHAR(200) NOT NULL,
    class_name VARCHAR(200) NOT NULL,
    grade INT NOT NULL DEFAULT 0,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    org_id VARCHAR(100) NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS sp_metric_value (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    student_id VARCHAR(64) NOT NULL,
    metric_code VARCHAR(100) NOT NULL,
    term VARCHAR(64) NOT NULL,
    value DOUBLE NOT NULL,
    unit VARCHAR(32) NOT NULL,
    source VARCHAR(64) NOT NULL,
    calculated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uq_sp_metric (student_id, metric_code, term),
    INDEX idx_sp_metric_student (student_id, term),
    CONSTRAINT fk_sp_metric_student FOREIGN KEY (student_id)
        REFERENCES sp_student(student_id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS sp_tag (
    code VARCHAR(100) PRIMARY KEY,
    name VARCHAR(200) NOT NULL,
    dimension VARCHAR(100) NOT NULL,
    description TEXT NOT NULL,
    rule LONGTEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS sp_student_tag (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    student_id VARCHAR(64) NOT NULL,
    tag_code VARCHAR(100) NOT NULL,
    term VARCHAR(64) NOT NULL,
    evidence LONGTEXT NOT NULL,
    calculated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uq_sp_student_tag (student_id, tag_code, term),
    INDEX idx_sp_tag_student_term (student_id, term),
    CONSTRAINT fk_sp_tag_student FOREIGN KEY (student_id)
        REFERENCES sp_student(student_id) ON DELETE CASCADE,
    CONSTRAINT fk_sp_tag_code FOREIGN KEY (tag_code) REFERENCES sp_tag(code)
);

CREATE TABLE IF NOT EXISTS sp_profile_snapshot (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    student_id VARCHAR(64) NOT NULL,
    term VARCHAR(64) NOT NULL,
    payload LONGTEXT NOT NULL,
    generated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uq_sp_snapshot (student_id, term),
    INDEX idx_sp_snapshot_student (student_id, term),
    CONSTRAINT fk_sp_snapshot_student FOREIGN KEY (student_id)
        REFERENCES sp_student(student_id) ON DELETE CASCADE
);
)sql");

    const auto org_column = db_.query(
        "SELECT column_name FROM information_schema.columns "
        "WHERE table_schema = DATABASE() AND table_name = 'sp_student' "
        "AND column_name = 'org_id'");
    if (org_column.empty())
        db_.exec("ALTER TABLE sp_student ADD COLUMN org_id VARCHAR(100) NOT NULL DEFAULT ''");
}

void SqliteProfileRepository::upsert_student(const Student& student) {
    db_.exec(
        "INSERT INTO sp_student (student_id, name, major, class_name, grade, org_id) "
        "VALUES (?, ?, ?, ?, ?, ?) "
        "ON DUPLICATE KEY UPDATE "
        "name = VALUES(name), major = VALUES(major), class_name = VALUES(class_name), "
        "grade = VALUES(grade), org_id = VALUES(org_id), updated_at = NOW()",
        {student.student_id, student.name, student.major, student.class_name,
         static_cast<std::int64_t>(student.grade), student.org_id});
}

std::optional<Student> SqliteProfileRepository::find_student(
    const std::string& student_id) const {
    const auto rows = db_.query(
        "SELECT student_id, name, major, class_name, grade, updated_at, org_id "
        "FROM sp_student WHERE student_id = ?",
        {student_id});
    if (rows.empty()) return std::nullopt;
    return read_student(rows.front());
}

std::vector<Student> SqliteProfileRepository::list_students() const {
    const auto rows = db_.query(
        "SELECT student_id, name, major, class_name, grade, updated_at, org_id "
        "FROM sp_student ORDER BY student_id");
    std::vector<Student> students;
    students.reserve(rows.size());
    for (const auto& row : rows) students.push_back(read_student(row));
    return students;
}

void SqliteProfileRepository::upsert_metric(const MetricValue& metric) {
    db_.exec(
        "INSERT INTO sp_metric_value (student_id, metric_code, term, value, unit, source) "
        "VALUES (?, ?, ?, ?, ?, ?) "
        "ON DUPLICATE KEY UPDATE "
        "value = VALUES(value), unit = VALUES(unit), source = VALUES(source), "
        "calculated_at = NOW()",
        {metric.student_id, metric.metric_code, metric.term, metric.value, metric.unit,
         metric.source});
}

std::vector<MetricValue> SqliteProfileRepository::list_metrics(
    const std::string& student_id, const std::string& term) const {
    std::string sql =
        "SELECT id, student_id, metric_code, term, value, unit, source, calculated_at "
        "FROM sp_metric_value WHERE student_id = ?";
    std::vector<DbValue> params{student_id};
    if (!term.empty()) {
        sql += " AND term = ?";
        params.emplace_back(term);
    }
    sql += " ORDER BY term, metric_code";
    const auto rows = db_.query(sql, params);
    std::vector<MetricValue> metrics;
    metrics.reserve(rows.size());
    for (const auto& row : rows) metrics.push_back(read_metric(row));
    return metrics;
}

void SqliteProfileRepository::upsert_tag(const Tag& tag) {
    const std::string rule = tag.rule.empty() ? "{}" : tag.rule;
    db_.exec(
        "INSERT INTO sp_tag (code, name, dimension, description, rule) "
        "VALUES (?, ?, ?, ?, ?) "
        "ON DUPLICATE KEY UPDATE name = VALUES(name), "
        "dimension = VALUES(dimension), description = VALUES(description), "
        "rule = VALUES(rule)",
        {tag.code, tag.name, tag.dimension, tag.description, rule});
}

std::optional<Tag> SqliteProfileRepository::find_tag(const std::string& code) const {
    const auto rows = db_.query(
        "SELECT code, name, dimension, description, rule FROM sp_tag WHERE code = ?", {code});
    if (rows.empty()) return std::nullopt;
    return read_tag(rows.front());
}

std::vector<Tag> SqliteProfileRepository::list_tags() const {
    const auto rows = db_.query(
        "SELECT code, name, dimension, description, rule FROM sp_tag ORDER BY dimension, code");
    std::vector<Tag> tags;
    tags.reserve(rows.size());
    for (const auto& row : rows) tags.push_back(read_tag(row));
    return tags;
}

void SqliteProfileRepository::replace_student_tags(const std::string& student_id,
                                                   const std::string& term,
                                                   const std::vector<StudentTag>& tags) {
    db_.begin();
    try {
        db_.exec("DELETE FROM sp_student_tag WHERE student_id = ? AND term = ?",
                 {student_id, term});
        for (const auto& tag : tags) {
            db_.exec(
                "INSERT INTO sp_student_tag (student_id, tag_code, term, evidence) "
                "VALUES (?, ?, ?, ?)",
                {student_id, tag.tag_code, term, tag.evidence.empty() ? "{}" : tag.evidence});
        }
        db_.commit();
    } catch (...) {
        db_.rollback();
        throw;
    }
}

std::vector<StudentTag> SqliteProfileRepository::list_student_tags(
    const std::string& student_id, const std::string& term) const {
    std::string sql =
        "SELECT id, student_id, tag_code, term, evidence, calculated_at "
        "FROM sp_student_tag WHERE student_id = ?";
    std::vector<DbValue> params{student_id};
    if (!term.empty()) {
        sql += " AND term = ?";
        params.emplace_back(term);
    }
    sql += " ORDER BY term, tag_code";
    const auto rows = db_.query(sql, params);
    std::vector<StudentTag> tags;
    tags.reserve(rows.size());
    for (const auto& row : rows) tags.push_back(read_student_tag(row));
    return tags;
}

void SqliteProfileRepository::save_snapshot(const ProfileSnapshot& snapshot) {
    db_.exec(
        "INSERT INTO sp_profile_snapshot (student_id, term, payload) VALUES (?, ?, ?) "
        "ON DUPLICATE KEY UPDATE payload = VALUES(payload), "
        "generated_at = NOW()",
        {snapshot.student_id, snapshot.term,
         snapshot.payload.empty() ? "{}" : snapshot.payload});
}

std::optional<ProfileSnapshot> SqliteProfileRepository::find_snapshot(
    const std::string& student_id, const std::string& term) const {
    const auto rows = db_.query(
        "SELECT id, student_id, term, payload, generated_at "
        "FROM sp_profile_snapshot WHERE student_id = ? AND term = ?",
        {student_id, term});
    if (rows.empty()) return std::nullopt;
    return read_snapshot(rows.front());
}

}  // namespace platform::student_profile
