#pragma once
#include "platform/app.hpp"
#include "platform/json.hpp"
#include <string>

namespace data_governance {

class JsonWrapper : public platform::Json {
public:
    using platform::Json::Json;
    JsonWrapper() : platform::Json() {}
    JsonWrapper(platform::Json j) : platform::Json(std::move(j)) {}

    std::string value(const std::string& key, const std::string& default_value) const {
        if (contains(key)) {
            const auto& val = at(key);
            if (val.is_string()) return val.as_string();
        }
        return default_value;
    }

    static JsonWrapper parse(const std::string& text) {
        return JsonWrapper(platform::Json::parse(text));
    }

    static JsonWrapper array() {
        return JsonWrapper(platform::Json::array());
    }

    static JsonWrapper object() {
        return JsonWrapper(platform::Json::object());
    }
};

using json = JsonWrapper;

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
