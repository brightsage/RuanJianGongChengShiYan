#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

struct sqlite3;

namespace platform {

using DbValue = std::variant<std::nullptr_t, std::int64_t, double, std::string>;

class DbError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class DbRow {
public:
    DbRow() = default;
    explicit DbRow(std::map<std::string, DbValue> values);

    bool has(const std::string& column) const;
    const DbValue& at(const std::string& column) const;
    std::string get_string(const std::string& column, const std::string& fallback = {}) const;
    std::int64_t get_int64(const std::string& column, std::int64_t fallback = 0) const;
    double get_double(const std::string& column, double fallback = 0.0) const;
    bool get_bool(const std::string& column, bool fallback = false) const;

private:
    std::map<std::string, DbValue> values_;
};

using DbRows = std::vector<DbRow>;

// 对 SQLite 的薄封装：模块只关心 SQL 和参数，不需要直接处理 sqlite3_stmt。
class Database {
public:
    explicit Database(const std::string& path);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&&) = delete;
    Database& operator=(Database&&) = delete;

    void exec(const std::string& sql);
    void exec(const std::string& sql, const std::vector<DbValue>& params);
    DbRows query(const std::string& sql, const std::vector<DbValue>& params = {});
    std::int64_t last_insert_rowid() const;

    void begin();
    void commit();
    void rollback();

    sqlite3* handle() { return db_; }

private:
    sqlite3* db_ = nullptr;
};

// 默认数据库路径为 data/platform.db，可用环境变量 PLATFORM_DB_PATH 覆盖。
std::string default_database_path();

}  // namespace platform
