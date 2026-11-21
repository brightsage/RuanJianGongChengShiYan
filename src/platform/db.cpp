#include "platform/db.hpp"

#include <errmsg.h>

#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace platform {
namespace {

const char* env_value(const char* name, const char* fallback) {
    const char* value = std::getenv(name);
    return value != nullptr && *value != '\0' ? value : fallback;
}

long process_id() {
#ifdef _WIN32
    return static_cast<long>(_getpid());
#else
    return static_cast<long>(getpid());
#endif
}

struct ConnectionInfo {
    std::string host = "127.0.0.1";
    unsigned port = 3307;
    std::string user = "platform";
    std::string password = "PlatformPass123!";
    std::string database = "student_platform";
};

ConnectionInfo default_info() {
    ConnectionInfo info;
    info.host = env_value("PLATFORM_DB_HOST", info.host.c_str());
    const std::string port = env_value("PLATFORM_DB_PORT", "3307");
    try {
        info.port = static_cast<unsigned>(std::stoul(port));
    } catch (const std::exception&) {
        throw DbError("invalid PLATFORM_DB_PORT: " + port);
    }
    info.user = env_value("PLATFORM_DB_USER", info.user.c_str());
    info.password = env_value("PLATFORM_DB_PASSWORD", info.password.c_str());
    info.database = env_value("PLATFORM_DB_NAME", info.database.c_str());
    if (std::string(env_value("PLATFORM_TEST_DB", "0")) == "1") {
        // 每个进程一个测试库：后缀只计算一次，保证同一进程内的所有模块
        // （各自持有连接）落在同一个库上，和生产环境一致。
        static const std::string test_suffix = [] {
            const auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
            return "_test_" + std::to_string(process_id()) + "_" + std::to_string(stamp);
        }();
        info.database += test_suffix;
    }
    return info;
}

std::string build_url(const ConnectionInfo& info) {
    return "mysql://" + info.user + ":" + info.password + "@" + info.host + ":" +
           std::to_string(info.port) + "/" + info.database;
}

ConnectionInfo parse_url(const std::string& url) {
    if (url.empty() || url == ":memory:") return default_info();
    ConnectionInfo info = default_info();
    if (url.rfind("mysql://", 0) != 0) {
        info.database = url;
        return info;
    }

    const std::string rest = url.substr(8);
    const auto at = rest.rfind('@');
    const std::string credentials = at == std::string::npos ? std::string{} : rest.substr(0, at);
    const std::string host_and_database =
        at == std::string::npos ? rest : rest.substr(at + 1);
    const auto slash = host_and_database.find('/');
    if (slash == std::string::npos || slash + 1 >= host_and_database.size())
        throw DbError("database URL must include a database name");
    const std::string host_and_port = host_and_database.substr(0, slash);
    info.database = host_and_database.substr(slash + 1);

    if (!credentials.empty()) {
        const auto colon = credentials.find(':');
        if (colon == std::string::npos) {
            info.user = credentials;
            info.password.clear();
        } else {
            info.user = credentials.substr(0, colon);
            info.password = credentials.substr(colon + 1);
        }
    }

    const auto port_separator = host_and_port.rfind(':');
    if (port_separator == std::string::npos) {
        info.host = host_and_port;
    } else {
        info.host = host_and_port.substr(0, port_separator);
        const std::string port = host_and_port.substr(port_separator + 1);
        try {
            info.port = static_cast<unsigned>(std::stoul(port));
        } catch (const std::exception&) {
            throw DbError("invalid database port: " + port);
        }
    }
    if (info.host.empty() || info.user.empty() || info.database.empty())
        throw DbError("incomplete database URL");
    return info;
}

void validate_identifier(const std::string& name) {
    if (name.empty() || name.size() > 64 ||
        name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") !=
            std::string::npos)
        throw DbError("invalid database name: " + name);
}

DbValue column_value(MYSQL_ROW row, const MYSQL_FIELD& field, unsigned long length) {
    if (row == nullptr || row[0] == nullptr) return nullptr;
    switch (field.type) {
        case MYSQL_TYPE_TINY:
        case MYSQL_TYPE_SHORT:
        case MYSQL_TYPE_LONG:
        case MYSQL_TYPE_LONGLONG:
        case MYSQL_TYPE_INT24:
        case MYSQL_TYPE_YEAR:
            return static_cast<std::int64_t>(std::strtoll(row[0], nullptr, 10));
        case MYSQL_TYPE_FLOAT:
        case MYSQL_TYPE_DOUBLE:
        case MYSQL_TYPE_DECIMAL:
        case MYSQL_TYPE_NEWDECIMAL:
            return std::strtod(row[0], nullptr);
        case MYSQL_TYPE_NULL:
            return nullptr;
        default:
            return std::string(row[0], static_cast<std::size_t>(length));
    }
}

}  // namespace

DbRow::DbRow(std::map<std::string, DbValue> values) : values_(std::move(values)) {}

DbRow::DbRow(std::map<std::string, DbValue> values, std::vector<std::string> columns)
    : values_(std::move(values)), columns_(std::move(columns)) {}

bool DbRow::has(const std::string& column) const { return values_.count(column) != 0; }

const DbValue& DbRow::at(const std::string& column) const {
    const auto it = values_.find(column);
    if (it == values_.end()) throw DbError("missing column: " + column);
    return it->second;
}

const DbValue& DbRow::at(std::size_t index) const {
    if (index >= columns_.size()) throw DbError("column index out of range");
    return at(columns_[index]);
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

Database::Database(const std::string& url) {
    const ConnectionInfo info = parse_url(url);
    validate_identifier(info.database);
    db_ = mysql_init(nullptr);
    if (db_ == nullptr) throw DbError("mysql_init failed");

    mysql_options(db_, MYSQL_SET_CHARSET_NAME, "utf8mb4");
    unsigned int timeout = 5;
    mysql_options(db_, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);

    if (mysql_real_connect(db_, info.host.c_str(), info.user.c_str(), info.password.c_str(),
                           nullptr, info.port, nullptr, CLIENT_MULTI_STATEMENTS) == nullptr) {
        const std::string message = mysql_error(db_);
        mysql_close(db_);
        db_ = nullptr;
        throw DbError("cannot connect to MySQL: " + message);
    }

    try {
        if (mysql_select_db(db_, info.database.c_str()) != 0) {
            if (mysql_errno(db_) != 1049)
                throw DbError("cannot select database '" + info.database + "': " + mysql_error(db_));
            const std::string create = "CREATE DATABASE `" + info.database +
                                       "` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci";
            if (mysql_real_query(db_, create.c_str(), create.size()) != 0)
                throw DbError("cannot create database '" + info.database + "': " + mysql_error(db_));
            if (mysql_select_db(db_, info.database.c_str()) != 0)
                throw DbError("cannot select database '" + info.database + "': " + mysql_error(db_));
        }
        mysql_set_character_set(db_, "utf8mb4");
        database_name_ = info.database;
    } catch (...) {
        mysql_close(db_);
        db_ = nullptr;
        throw;
    }
}

Database::Database(MYSQL& connection) : db_(&connection), owns_connection_(false) {}

Database::~Database() {
    if (owns_connection_ && db_ != nullptr) mysql_close(db_);
}

std::string Database::bind_sql(const std::string& sql,
                               const std::vector<DbValue>& params) const {
    std::string out;
    out.reserve(sql.size() + 32);
    std::size_t parameter = 0;
    char quote = 0;
    for (std::size_t i = 0; i < sql.size(); ++i) {
        const char c = sql[i];
        if (quote != 0) {
            out += c;
            if (c == '\\' && i + 1 < sql.size()) {
                out += sql[++i];
            } else if (c == quote) {
                quote = 0;
            }
            continue;
        }
        if (c == '\'' || c == '"' || c == '`') {
            quote = c;
            out += c;
            continue;
        }
        if (c != '?') {
            out += c;
            continue;
        }
        if (parameter >= params.size()) throw DbError("too few SQL parameters");
        std::visit(
            [&](const auto& value) {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, std::nullptr_t>) {
                    out += "NULL";
                } else if constexpr (std::is_same_v<T, std::int64_t>) {
                    out += std::to_string(value);
                } else if constexpr (std::is_same_v<T, double>) {
                    std::ostringstream stream;
                    stream.imbue(std::locale::classic());
                    stream << std::setprecision(17) << value;
                    out += stream.str();
                } else {
                    std::string escaped(value.size() * 2 + 1, '\0');
                    const auto length = mysql_real_escape_string(
                        db_, escaped.data(), value.data(), static_cast<unsigned long>(value.size()));
                    escaped.resize(length);
                    out += '\'';
                    out += escaped;
                    out += '\'';
                }
            },
            params[parameter]);
        ++parameter;
    }
    if (parameter != params.size()) throw DbError("too many SQL parameters");
    return out;
}

void Database::exec(const std::string& sql) {
    if (mysql_real_query(db_, sql.c_str(), sql.size()) != 0)
        throw DbError("sql failed: " + std::string(mysql_error(db_)));
    while (true) {
        MYSQL_RES* result = mysql_store_result(db_);
        if (result != nullptr) {
            mysql_free_result(result);
        } else if (mysql_field_count(db_) != 0) {
            throw DbError("sql failed: " + std::string(mysql_error(db_)));
        }
        const int next = mysql_next_result(db_);
        if (next > 0) throw DbError("sql failed: " + std::string(mysql_error(db_)));
        if (next != 0) break;
    }
}

void Database::exec(const std::string& sql, const std::vector<DbValue>& params) {
    exec(bind_sql(sql, params));
}

DbRows Database::query(const std::string& sql, const std::vector<DbValue>& params) {
    const std::string bound = bind_sql(sql, params);
    if (mysql_real_query(db_, bound.c_str(), bound.size()) != 0)
        throw DbError("query failed: " + std::string(mysql_error(db_)));
    MYSQL_RES* result = mysql_store_result(db_);
    if (result == nullptr) {
        if (mysql_field_count(db_) == 0) return {};
        throw DbError("query failed: " + std::string(mysql_error(db_)));
    }

    const unsigned int fields = mysql_num_fields(result);
    MYSQL_FIELD* definitions = mysql_fetch_fields(result);
    std::vector<std::string> columns;
    columns.reserve(fields);
    for (unsigned int i = 0; i < fields; ++i)
        columns.emplace_back(definitions[i].name != nullptr ? definitions[i].name : "");

    DbRows rows;
    MYSQL_ROW row = nullptr;
    while ((row = mysql_fetch_row(result)) != nullptr) {
        const unsigned long* lengths = mysql_fetch_lengths(result);
        std::map<std::string, DbValue> values;
        for (unsigned int i = 0; i < fields; ++i) {
            DbValue value = column_value(row + i, definitions[i], lengths[i]);
            values[columns[i]] = std::move(value);
        }
        rows.emplace_back(std::move(values), columns);
    }
    mysql_free_result(result);
    return rows;
}

std::int64_t Database::last_insert_rowid() const {
    return static_cast<std::int64_t>(mysql_insert_id(db_));
}

void Database::begin() { exec("START TRANSACTION"); }
void Database::commit() { exec("COMMIT"); }
void Database::rollback() { exec("ROLLBACK"); }

std::string default_database_url() {
    const char* full = std::getenv("PLATFORM_DB_URL");
    if (full != nullptr && *full != '\0') return full;
    return build_url(default_info());
}

std::string database_url_for(const std::string& database_name) {
    validate_identifier(database_name);
    ConnectionInfo info = parse_url(default_database_url());
    info.database = database_name;
    return build_url(info);
}

std::string default_database_path() { return default_database_url(); }

}  // namespace platform

