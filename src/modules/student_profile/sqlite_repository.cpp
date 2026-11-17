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
    return tag;
}

StudentTag read_student_tag(const DbRow& row) {
    StudentTag tag;
    tag.id = row.get_int64("id");
    tag.student_id = row.get_string("student_id");
    tag.tag_code = row.get_string("tag_code");
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
    student_id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    major TEXT NOT NULL DEFAULT '',
    class_name TEXT NOT NULL DEFAULT '',
    grade INTEGER NOT NULL DEFAULT 0,
    updated_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS sp_metric_value (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    student_id TEXT NOT NULL REFERENCES sp_student(student_id) ON DELETE CASCADE,
    metric_code TEXT NOT NULL,
    term TEXT NOT NULL,
    value REAL NOT NULL,
    unit TEXT NOT NULL DEFAULT '',
    source TEXT NOT NULL DEFAULT '',
    calculated_at TEXT NOT NULL DEFAULT (datetime('now')),
    UNIQUE(student_id, metric_code, term)
);

CREATE TABLE IF NOT EXISTS sp_tag (
    code TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    dimension TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS sp_student_tag (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    student_id TEXT NOT NULL REFERENCES sp_student(student_id) ON DELETE CASCADE,
    tag_code TEXT NOT NULL REFERENCES sp_tag(code),
    evidence TEXT NOT NULL DEFAULT '{}',
    calculated_at TEXT NOT NULL DEFAULT (datetime('now')),
    UNIQUE(student_id, tag_code)
);

CREATE TABLE IF NOT EXISTS sp_profile_snapshot (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    student_id TEXT NOT NULL REFERENCES sp_student(student_id) ON DELETE CASCADE,
    term TEXT NOT NULL,
    payload TEXT NOT NULL DEFAULT '{}',
    generated_at TEXT NOT NULL DEFAULT (datetime('now')),
    UNIQUE(student_id, term)
);

CREATE INDEX IF NOT EXISTS idx_sp_metric_student ON sp_metric_value(student_id, term);
CREATE INDEX IF NOT EXISTS idx_sp_tag_student ON sp_student_tag(student_id);
CREATE INDEX IF NOT EXISTS idx_sp_snapshot_student ON sp_profile_snapshot(student_id, term);
)sql");
}

void SqliteProfileRepository::upsert_student(const Student& student) {
    db_.exec(
        "INSERT INTO sp_student (student_id, name, major, class_name, grade) "
        "VALUES (?, ?, ?, ?, ?) "
        "ON CONFLICT(student_id) DO UPDATE SET "
        "name = excluded.name, major = excluded.major, class_name = excluded.class_name, "
        "grade = excluded.grade, updated_at = datetime('now')",
        {student.student_id, student.name, student.major, student.class_name,
         static_cast<std::int64_t>(student.grade)});
}

std::optional<Student> SqliteProfileRepository::find_student(
    const std::string& student_id) const {
    const auto rows = db_.query(
        "SELECT student_id, name, major, class_name, grade, updated_at "
        "FROM sp_student WHERE student_id = ?",
        {student_id});
    if (rows.empty()) return std::nullopt;
    return read_student(rows.front());
}

std::vector<Student> SqliteProfileRepository::list_students() const {
    const auto rows = db_.query(
        "SELECT student_id, name, major, class_name, grade, updated_at "
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
        "ON CONFLICT(student_id, metric_code, term) DO UPDATE SET "
        "value = excluded.value, unit = excluded.unit, source = excluded.source, "
        "calculated_at = datetime('now')",
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
    db_.exec(
        "INSERT INTO sp_tag (code, name, dimension, description) VALUES (?, ?, ?, ?) "
        "ON CONFLICT(code) DO UPDATE SET name = excluded.name, "
        "dimension = excluded.dimension, description = excluded.description",
        {tag.code, tag.name, tag.dimension, tag.description});
}

std::optional<Tag> SqliteProfileRepository::find_tag(const std::string& code) const {
    const auto rows = db_.query(
        "SELECT code, name, dimension, description FROM sp_tag WHERE code = ?", {code});
    if (rows.empty()) return std::nullopt;
    return read_tag(rows.front());
}

std::vector<Tag> SqliteProfileRepository::list_tags() const {
    const auto rows = db_.query(
        "SELECT code, name, dimension, description FROM sp_tag ORDER BY dimension, code");
    std::vector<Tag> tags;
    tags.reserve(rows.size());
    for (const auto& row : rows) tags.push_back(read_tag(row));
    return tags;
}

void SqliteProfileRepository::replace_student_tags(const std::string& student_id,
                                                   const std::vector<StudentTag>& tags) {
    db_.begin();
    try {
        db_.exec("DELETE FROM sp_student_tag WHERE student_id = ?", {student_id});
        for (const auto& tag : tags) {
            db_.exec(
                "INSERT INTO sp_student_tag (student_id, tag_code, evidence) VALUES (?, ?, ?)",
                {student_id, tag.tag_code, tag.evidence.empty() ? "{}" : tag.evidence});
        }
        db_.commit();
    } catch (...) {
        db_.rollback();
        throw;
    }
}

std::vector<StudentTag> SqliteProfileRepository::list_student_tags(
    const std::string& student_id) const {
    const auto rows = db_.query(
        "SELECT id, student_id, tag_code, evidence, calculated_at "
        "FROM sp_student_tag WHERE student_id = ? ORDER BY tag_code",
        {student_id});
    std::vector<StudentTag> tags;
    tags.reserve(rows.size());
    for (const auto& row : rows) tags.push_back(read_student_tag(row));
    return tags;
}

void SqliteProfileRepository::save_snapshot(const ProfileSnapshot& snapshot) {
    db_.exec(
        "INSERT INTO sp_profile_snapshot (student_id, term, payload) VALUES (?, ?, ?) "
        "ON CONFLICT(student_id, term) DO UPDATE SET payload = excluded.payload, "
        "generated_at = datetime('now')",
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
