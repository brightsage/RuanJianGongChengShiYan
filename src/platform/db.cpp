#include "platform/db.hpp"

#include <sqlite3.h>

#include <cstdlib>
#include <filesystem>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace platform {
namespace {

class Statement {
public:
    Statement(sqlite3* db, const std::string& sql) : db_(db) {
        const int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt_, nullptr);
        if (rc != SQLITE_OK) throw DbError(error_message());
    }

    ~Statement() {
        if (stmt_ != nullptr) sqlite3_finalize(stmt_);
    }

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    sqlite3_stmt* get() const { return stmt_; }

    void bind(const std::vector<DbValue>& params) {
        for (std::size_t i = 0; i < params.size(); ++i) {
            const int index = static_cast<int>(i + 1);
            int rc = SQLITE_OK;
            std::visit(
                [&](const auto& value) {
                    using T = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<T, std::nullptr_t>) {
                        rc = sqlite3_bind_null(stmt_, index);
                    } else if constexpr (std::is_same_v<T, std::int64_t>) {
                        rc = sqlite3_bind_int64(stmt_, index, value);
                    } else if constexpr (std::is_same_v<T, double>) {
                        rc = sqlite3_bind_double(stmt_, index, value);
                    } else {
                        rc = sqlite3_bind_text(stmt_, index, value.c_str(),
                                               static_cast<int>(value.size()), SQLITE_TRANSIENT);
                    }
                },
                params[i]);
            if (rc != SQLITE_OK) throw DbError("bind failed: " + error_message());
        }
    }

    void execute() {
        while (true) {
            const int rc = sqlite3_step(stmt_);
            if (rc == SQLITE_ROW) continue;
            if (rc == SQLITE_DONE) return;
            throw DbError("statement failed: " + error_message());
        }
    }

    std::string error_message() const {
        return db_ != nullptr ? sqlite3_errmsg(db_) : "sqlite error";
    }

private:
    sqlite3* db_ = nullptr;
    sqlite3_stmt* stmt_ = nullptr;
};

DbValue column_value(sqlite3_stmt* stmt, int column) {
    switch (sqlite3_column_type(stmt, column)) {
        case SQLITE_INTEGER:
            return static_cast<std::int64_t>(sqlite3_column_int64(stmt, column));
        case SQLITE_FLOAT:
            return sqlite3_column_double(stmt, column);
        case SQLITE_TEXT: {
            const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, column));
            const int bytes = sqlite3_column_bytes(stmt, column);
            return std::string(text == nullptr ? "" : text, static_cast<std::size_t>(bytes));
        }
        case SQLITE_BLOB: {
            const auto* data = static_cast<const char*>(sqlite3_column_blob(stmt, column));
            const int bytes = sqlite3_column_bytes(stmt, column);
            return std::string(data == nullptr ? "" : data, static_cast<std::size_t>(bytes));
        }
        case SQLITE_NULL:
        default:
            return nullptr;
    }
}

}  // namespace

DbRow::DbRow(std::map<std::string, DbValue> values) : values_(std::move(values)) {}

bool DbRow::has(const std::string& column) const { return values_.count(column) != 0; }

const DbValue& DbRow::at(const std::string& column) const {
    const auto it = values_.find(column);
    if (it == values_.end()) throw DbError("missing column: " + column);
    return it->second;
}

std::string DbRow::get_string(const std::string& column, const std::string& fallback) const {
    const auto it = values_.find(column);
    if (it == values_.end()) return fallback;
    if (const auto* value = std::get_if<std::string>(&it->second)) return *value;
    return fallback;
}

std::int64_t DbRow::get_int64(const std::string& column, std::int64_t fallback) const {
    const auto it = values_.find(column);
    if (it == values_.end()) return fallback;
    if (const auto* value = std::get_if<std::int64_t>(&it->second)) return *value;
    if (const auto* value = std::get_if<double>(&it->second))
        return static_cast<std::int64_t>(*value);
    return fallback;
}

double DbRow::get_double(const std::string& column, double fallback) const {
    const auto it = values_.find(column);
    if (it == values_.end()) return fallback;
    if (const auto* value = std::get_if<double>(&it->second)) return *value;
    if (const auto* value = std::get_if<std::int64_t>(&it->second))
        return static_cast<double>(*value);
    return fallback;
}

bool DbRow::get_bool(const std::string& column, bool fallback) const {
    const auto it = values_.find(column);
    if (it == values_.end()) return fallback;
    if (const auto* value = std::get_if<std::int64_t>(&it->second)) return *value != 0;
    if (const auto* value = std::get_if<double>(&it->second)) return *value != 0.0;
    return fallback;
}

Database::Database(const std::string& path) {
    if (path.empty()) throw DbError("database path is empty");

    const std::filesystem::path file_path(path);
    if (file_path.has_parent_path()) {
        std::error_code error;
        std::filesystem::create_directories(file_path.parent_path(), error);
        if (error) throw DbError("cannot create database directory: " + error.message());
    }

    const int rc = sqlite3_open_v2(
        path.c_str(), &db_,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
    if (rc != SQLITE_OK) {
        const std::string message = db_ != nullptr ? sqlite3_errmsg(db_) : "open failed";
        if (db_ != nullptr) sqlite3_close_v2(db_);
        db_ = nullptr;
        throw DbError("cannot open database '" + path + "': " + message);
    }

    sqlite3_busy_timeout(db_, 5000);
    exec("PRAGMA foreign_keys = ON;");
    exec("PRAGMA journal_mode = WAL;");
}

Database::~Database() {
    if (db_ != nullptr) sqlite3_close_v2(db_);
}

void Database::exec(const std::string& sql) {
    char* error = nullptr;
    const int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error);
    if (rc != SQLITE_OK) {
        const std::string message = error != nullptr ? error : sqlite3_errmsg(db_);
        sqlite3_free(error);
        throw DbError("sql failed: " + message);
    }
}

void Database::exec(const std::string& sql, const std::vector<DbValue>& params) {
    Statement statement(db_, sql);
    statement.bind(params);
    statement.execute();
}

DbRows Database::query(const std::string& sql, const std::vector<DbValue>& params) {
    Statement statement(db_, sql);
    statement.bind(params);
    DbRows rows;
    while (true) {
        const int rc = sqlite3_step(statement.get());
        if (rc == SQLITE_DONE) break;
        if (rc != SQLITE_ROW) throw DbError("query failed: " + statement.error_message());
        const int columns = sqlite3_column_count(statement.get());
        std::map<std::string, DbValue> values;
        for (int i = 0; i < columns; ++i) {
            const char* name = sqlite3_column_name(statement.get(), i);
            values[name == nullptr ? std::string{} : std::string(name)] =
                column_value(statement.get(), i);
        }
        rows.emplace_back(std::move(values));
    }
    return rows;
}

std::int64_t Database::last_insert_rowid() const { return sqlite3_last_insert_rowid(db_); }

void Database::begin() { exec("BEGIN IMMEDIATE;"); }
void Database::commit() { exec("COMMIT;"); }
void Database::rollback() { exec("ROLLBACK;"); }

std::string default_database_path() {
    const char* from_env = std::getenv("PLATFORM_DB_PATH");
    if (from_env != nullptr && *from_env != '\0') return from_env;
    return "data/platform.db";
}

}  // namespace platform
