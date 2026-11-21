#pragma once

#include <mysql.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

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
    DbRow(std::map<std::string, DbValue> values, std::vector<std::string> columns);

    bool has(const std::string& column) const;
    const DbValue& at(const std::string& column) const;
    const DbValue& at(std::size_t index) const;
    std::string get_string(const std::string& column, const std::string& fallback = {}) const;
    std::int64_t get_int64(const std::string& column, std::int64_t fallback = 0) const;
    double get_double(const std::string& column, double fallback = 0.0) const;
    bool get_bool(const std::string& column, bool fallback = false) const;

private:
    std::map<std::string, DbValue> values_;
    std::vector<std::string> columns_;
};

using DbRows = std::vector<DbRow>;

// MySQL 8.x 薄封装：模块只关心 SQL 和参数，不直接处理 MYSQL_STMT。
// 连接串格式：mysql://user:password@host:port/database
class Database {
public:
    explicit Database(const std::string& url);
    explicit Database(MYSQL& connection);
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

    MYSQL* handle() { return db_; }
    const std::string& database_name() const { return database_name_; }

private:
    std::string bind_sql(const std::string& sql, const std::vector<DbValue>& params) const;

    MYSQL* db_ = nullptr;
    bool owns_connection_ = true;
    std::string database_name_;
};

// 默认连接串。PLATFORM_DB_URL 优先；否则由 PLATFORM_DB_HOST/PORT/USER/PASSWORD/NAME 组装。
// PLATFORM_TEST_DB=1 时数据库名追加 _test_<pid>，让每个测试进程使用独立库。
std::string default_database_url();
std::string database_url_for(const std::string& database_name);

// 兼容旧调用名，返回连接串而不是文件路径。
std::string default_database_path();

}  // namespace platform
