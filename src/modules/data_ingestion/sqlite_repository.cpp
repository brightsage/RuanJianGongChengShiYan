#include "modules/data_ingestion/sqlite_repository.hpp"

#include <utility>

namespace platform::data_ingestion {
namespace {

DataSource read_data_source(const DbRow& row) {
    DataSource source;
    source.id = row.get_int64("id");
    source.name = row.get_string("name");
    source.kind = row.get_string("kind");
    source.access_mode = row.get_string("access_mode");
    source.owner = row.get_string("owner");
    source.description = row.get_string("description");
    source.created_at = row.get_string("created_at");
    return source;
}

FieldMapping read_field_mapping(const DbRow& row) {
    FieldMapping mapping;
    mapping.id = row.get_int64("id");
    mapping.data_source_id = row.get_int64("data_source_id");
    mapping.source_field = row.get_string("source_field");
    mapping.standard_field = row.get_string("standard_field");
    mapping.transform_expr = row.get_string("transform_expr");
    return mapping;
}

ImportTask read_import_task(const DbRow& row) {
    ImportTask task;
    task.id = row.get_int64("id");
    task.data_source_id = row.get_int64("data_source_id");
    task.name = row.get_string("name");
    task.import_mode = row.get_string("import_mode");
    task.schedule_expr = row.get_string("schedule_expr");
    task.validation_rules = row.get_string("validation_rules");
    task.status = row.get_string("status");
    task.created_at = row.get_string("created_at");
    return task;
}

ImportRun read_import_run(const DbRow& row) {
    ImportRun run;
    run.id = row.get_int64("id");
    run.task_id = row.get_int64("task_id");
    run.status = row.get_string("status");
    run.total_count = row.get_int64("total_count");
    run.success_count = row.get_int64("success_count");
    run.failure_count = row.get_int64("failure_count");
    run.started_at = row.get_string("started_at");
    run.finished_at = row.get_string("finished_at");
    return run;
}

RawRecord read_raw_record(const DbRow& row) {
    RawRecord record;
    record.id = row.get_int64("id");
    record.run_id = row.get_int64("run_id");
    record.source_row_no = row.get_int64("source_row_no");
    record.payload = row.get_string("payload");
    record.valid = row.get_bool("valid");
    record.error_reason = row.get_string("error_reason");
    return record;
}

}  // namespace

SqliteIngestionRepository::SqliteIngestionRepository(Database& database) : db_(database) {
    db_.exec(R"sql(
CREATE TABLE IF NOT EXISTS ing_data_source (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL UNIQUE,
    kind TEXT NOT NULL,
    access_mode TEXT NOT NULL,
    owner TEXT NOT NULL DEFAULT '',
    description TEXT NOT NULL DEFAULT '',
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS ing_field_mapping (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    data_source_id INTEGER NOT NULL REFERENCES ing_data_source(id) ON DELETE CASCADE,
    source_field TEXT NOT NULL,
    standard_field TEXT NOT NULL,
    transform_expr TEXT NOT NULL DEFAULT '',
    UNIQUE(data_source_id, source_field)
);

CREATE TABLE IF NOT EXISTS ing_import_task (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    data_source_id INTEGER NOT NULL REFERENCES ing_data_source(id),
    name TEXT NOT NULL,
    import_mode TEXT NOT NULL DEFAULT 'manual',
    schedule_expr TEXT NOT NULL DEFAULT '',
    validation_rules TEXT NOT NULL DEFAULT '{}',
    status TEXT NOT NULL DEFAULT 'pending',
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS ing_import_run (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    task_id INTEGER NOT NULL REFERENCES ing_import_task(id),
    status TEXT NOT NULL DEFAULT 'running',
    total_count INTEGER NOT NULL DEFAULT 0,
    success_count INTEGER NOT NULL DEFAULT 0,
    failure_count INTEGER NOT NULL DEFAULT 0,
    started_at TEXT NOT NULL DEFAULT (datetime('now')),
    finished_at TEXT
);

CREATE TABLE IF NOT EXISTS ing_raw_record (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    run_id INTEGER NOT NULL REFERENCES ing_import_run(id) ON DELETE CASCADE,
    source_row_no INTEGER NOT NULL,
    payload TEXT NOT NULL,
    valid INTEGER NOT NULL DEFAULT 1,
    error_reason TEXT NOT NULL DEFAULT '',
    UNIQUE(run_id, source_row_no)
);

CREATE INDEX IF NOT EXISTS idx_ing_mapping_source ON ing_field_mapping(data_source_id);
CREATE INDEX IF NOT EXISTS idx_ing_task_source ON ing_import_task(data_source_id);
CREATE INDEX IF NOT EXISTS idx_ing_run_task ON ing_import_run(task_id);
CREATE INDEX IF NOT EXISTS idx_ing_raw_run ON ing_raw_record(run_id, valid);
)sql");
}

std::int64_t SqliteIngestionRepository::create_data_source(const DataSource& source) {
    db_.exec(
        "INSERT INTO ing_data_source (name, kind, access_mode, owner, description) "
        "VALUES (?, ?, ?, ?, ?)",
        {source.name, source.kind, source.access_mode, source.owner, source.description});
    return db_.last_insert_rowid();
}

std::vector<DataSource> SqliteIngestionRepository::list_data_sources() const {
    const auto rows = db_.query(
        "SELECT id, name, kind, access_mode, owner, description, created_at "
        "FROM ing_data_source ORDER BY id");
    std::vector<DataSource> sources;
    sources.reserve(rows.size());
    for (const auto& row : rows) sources.push_back(read_data_source(row));
    return sources;
}

std::optional<DataSource> SqliteIngestionRepository::find_data_source(std::int64_t id) const {
    const auto rows = db_.query(
        "SELECT id, name, kind, access_mode, owner, description, created_at "
        "FROM ing_data_source WHERE id = ?",
        {id});
    if (rows.empty()) return std::nullopt;
    return read_data_source(rows.front());
}

std::int64_t SqliteIngestionRepository::add_field_mapping(const FieldMapping& mapping) {
    db_.exec(
        "INSERT INTO ing_field_mapping (data_source_id, source_field, standard_field, "
        "transform_expr) VALUES (?, ?, ?, ?)",
        {mapping.data_source_id, mapping.source_field, mapping.standard_field,
         mapping.transform_expr});
    return db_.last_insert_rowid();
}

std::vector<FieldMapping> SqliteIngestionRepository::list_field_mappings(
    std::int64_t data_source_id) const {
    const auto rows = db_.query(
        "SELECT id, data_source_id, source_field, standard_field, transform_expr "
        "FROM ing_field_mapping WHERE data_source_id = ? ORDER BY id",
        {data_source_id});
    std::vector<FieldMapping> mappings;
    mappings.reserve(rows.size());
    for (const auto& row : rows) mappings.push_back(read_field_mapping(row));
    return mappings;
}

void SqliteIngestionRepository::replace_field_mappings(
    std::int64_t data_source_id, const std::vector<FieldMapping>& mappings) {
    db_.begin();
    try {
        db_.exec("DELETE FROM ing_field_mapping WHERE data_source_id = ?", {data_source_id});
        for (const auto& mapping : mappings) {
            db_.exec(
                "INSERT INTO ing_field_mapping (data_source_id, source_field, standard_field, "
                "transform_expr) VALUES (?, ?, ?, ?)",
                {data_source_id, mapping.source_field, mapping.standard_field,
                 mapping.transform_expr});
        }
        db_.commit();
    } catch (...) {
        db_.rollback();
        throw;
    }
}

std::int64_t SqliteIngestionRepository::create_import_task(const ImportTask& task) {
    const std::string import_mode = task.import_mode.empty() ? "manual" : task.import_mode;
    const std::string validation_rules =
        task.validation_rules.empty() ? "{}" : task.validation_rules;
    const std::string status = task.status.empty() ? "pending" : task.status;
    db_.exec(
        "INSERT INTO ing_import_task (data_source_id, name, import_mode, schedule_expr, "
        "validation_rules, status) VALUES (?, ?, ?, ?, ?, ?)",
        {task.data_source_id, task.name, import_mode, task.schedule_expr, validation_rules,
         status});
    return db_.last_insert_rowid();
}

std::vector<ImportTask> SqliteIngestionRepository::list_import_tasks() const {
    const auto rows = db_.query(
        "SELECT id, data_source_id, name, import_mode, schedule_expr, validation_rules, "
        "status, created_at FROM ing_import_task ORDER BY id");
    std::vector<ImportTask> tasks;
    tasks.reserve(rows.size());
    for (const auto& row : rows) tasks.push_back(read_import_task(row));
    return tasks;
}

std::optional<ImportTask> SqliteIngestionRepository::find_import_task(std::int64_t id) const {
    const auto rows = db_.query(
        "SELECT id, data_source_id, name, import_mode, schedule_expr, validation_rules, "
        "status, created_at FROM ing_import_task WHERE id = ?",
        {id});
    if (rows.empty()) return std::nullopt;
    return read_import_task(rows.front());
}

void SqliteIngestionRepository::update_import_task_status(std::int64_t id,
                                                          const std::string& status) {
    db_.exec("UPDATE ing_import_task SET status = ? WHERE id = ?", {status, id});
}

std::int64_t SqliteIngestionRepository::start_import_run(std::int64_t task_id) {
    db_.exec("INSERT INTO ing_import_run (task_id, status) VALUES (?, 'running')", {task_id});
    return db_.last_insert_rowid();
}

void SqliteIngestionRepository::finish_import_run(std::int64_t run_id,
                                                  const std::string& status,
                                                  std::int64_t total_count,
                                                  std::int64_t success_count,
                                                  std::int64_t failure_count) {
    db_.exec(
        "UPDATE ing_import_run SET status = ?, total_count = ?, success_count = ?, "
        "failure_count = ?, finished_at = datetime('now') WHERE id = ?",
        {status, total_count, success_count, failure_count, run_id});
}

std::optional<ImportRun> SqliteIngestionRepository::find_import_run(std::int64_t run_id) const {
    const auto rows = db_.query(
        "SELECT id, task_id, status, total_count, success_count, failure_count, started_at, "
        "finished_at FROM ing_import_run WHERE id = ?",
        {run_id});
    if (rows.empty()) return std::nullopt;
    return read_import_run(rows.front());
}

std::int64_t SqliteIngestionRepository::add_raw_record(const RawRecord& record) {
    db_.exec(
        "INSERT INTO ing_raw_record (run_id, source_row_no, payload, valid, error_reason) "
        "VALUES (?, ?, ?, ?, ?)",
        {record.run_id, record.source_row_no, record.payload,
         static_cast<std::int64_t>(record.valid ? 1 : 0), record.error_reason});
    return db_.last_insert_rowid();
}

std::vector<RawRecord> SqliteIngestionRepository::list_raw_records(std::int64_t run_id,
                                                                   bool only_failed) const {
    std::string sql =
        "SELECT id, run_id, source_row_no, payload, valid, error_reason "
        "FROM ing_raw_record WHERE run_id = ?";
    if (only_failed) sql += " AND valid = 0";
    sql += " ORDER BY source_row_no, id";
    const auto rows = db_.query(sql, {run_id});
    std::vector<RawRecord> records;
    records.reserve(rows.size());
    for (const auto& row : rows) records.push_back(read_raw_record(row));
    return records;
}

}  // namespace platform::data_ingestion
