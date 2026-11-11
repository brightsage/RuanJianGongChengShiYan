#pragma once

#include "platform/app.hpp"
#include "platform/db.hpp"
#include "platform/json.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>

namespace platform {

// 带 HTTP 状态码的业务异常。service 层抛出，路由层统一转成 JSON 错误。
class ApiError : public std::runtime_error {
public:
    ApiError(int status, const std::string& message);
    int status() const noexcept;

private:
    int status_;
};

class BadRequest : public ApiError {
public:
    explicit BadRequest(const std::string& message);
};

class NotFound : public ApiError {
public:
    explicit NotFound(const std::string& message);
};

Response json_ok(const Json& body, int status = 200);
Response json_error(int status, const std::string& message);

// 把 service 调用包起来，统一处理 ApiError / JSON 解析错误 / 数据库错误。
Response guard(const std::function<Response()>& handler);

Json parse_json_body(const Request& request);
std::map<std::string, std::string> parse_query(const std::string& query);
std::string query_param(const Request& request, const std::string& name,
                        const std::string& fallback = {});
std::int64_t path_param_id(const Request& request, const std::string& name);

std::string require_string(const Json& object, const std::string& key);
std::string optional_string(const Json& object, const std::string& key,
                            const std::string& fallback = {});
std::int64_t require_int64(const Json& object, const std::string& key);
std::int64_t optional_int64(const Json& object, const std::string& key,
                            std::int64_t fallback = 0);
double require_number(const Json& object, const std::string& key);
double optional_number(const Json& object, const std::string& key, double fallback = 0.0);
bool optional_bool(const Json& object, const std::string& key, bool fallback = false);

// 用于解析数据库里保存的 JSON 文本，格式不合法时返回 fallback。
Json parse_json_or(const std::string& text, const Json& fallback);

}  // namespace platform
