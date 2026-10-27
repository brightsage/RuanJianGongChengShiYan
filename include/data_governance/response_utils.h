#pragma once
#include "platform/app.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace data_governance {

// 第二部分：统一响应工具函数
inline platform::Response success_response(const json& data, const std::string& message = "success") {
    json result;
    result["code"] = 0;
    result["message"] = message;
    result["data"] = data;
    return platform::Response(200, "application/json; charset=utf-8", result.dump());
}

inline platform::Response error_response(int status_code, const std::string& message) {
    json result;
    result["code"] = status_code;
    result["message"] = message;
    return platform::Response(status_code, "application/json; charset=utf-8", result.dump());
}

} // namespace data_governance
