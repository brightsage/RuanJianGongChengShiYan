#include "platform/api.hpp"

#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <string>
#include <utility>

namespace platform {
namespace {

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string url_decode(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '+') {
            out += ' ';
        } else if (c == '%' && i + 2 < text.size()) {
            const int high = hex_value(text[i + 1]);
            const int low = hex_value(text[i + 2]);
            if (high >= 0 && low >= 0) {
                out += static_cast<char>((high << 4) | low);
                i += 2;
            } else {
                out += c;
            }
        } else {
            out += c;
        }
    }
    return out;
}

std::int64_t value_to_int64(const Json& value, const std::string& name) {
    if (value.is_number()) {
        const double number = value.as_double();
        if (std::floor(number) != number) throw BadRequest("field '" + name + "' must be an integer");
        return static_cast<std::int64_t>(number);
    }
    if (value.is_string()) {
        const std::string& text = value.as_string();
        if (text.empty()) throw BadRequest("field '" + name + "' must be an integer");
        std::size_t consumed = 0;
        try {
            const long long number = std::stoll(text, &consumed);
            if (consumed == text.size()) return static_cast<std::int64_t>(number);
        } catch (const std::exception&) {
            // fall through to the uniform error below
        }
    }
    throw BadRequest("field '" + name + "' must be an integer");
}

double value_to_number(const Json& value, const std::string& name) {
    if (value.is_number()) return value.as_double();
    if (value.is_string()) {
        const std::string& text = value.as_string();
        if (!text.empty()) {
            char* end = nullptr;
            errno = 0;
            const double number = std::strtod(text.c_str(), &end);
            if (end == text.c_str() + text.size() && errno != ERANGE) return number;
        }
    }
    throw BadRequest("field '" + name + "' must be a number");
}

}  // namespace

ApiError::ApiError(int status, const std::string& message)
    : std::runtime_error(message), status_(status) {}

int ApiError::status() const noexcept { return status_; }

BadRequest::BadRequest(const std::string& message) : ApiError(400, message) {}

NotFound::NotFound(const std::string& message) : ApiError(404, message) {}

Response json_ok(const Json& body, int status) {
    return {status, "application/json; charset=utf-8", body.dump()};
}

Response json_error(int status, const std::string& message) {
    Json body = Json::object();
    body["error"] = message;
    return {status, "application/json; charset=utf-8", body.dump()};
}

Response guard(const std::function<Response()>& handler) {
    try {
        return handler();
    } catch (const ApiError& error) {
        return json_error(error.status(), error.what());
    } catch (const JsonError& error) {
        return json_error(400, std::string("invalid JSON: ") + error.what());
    } catch (const DbError& error) {
        return json_error(500, std::string("database error: ") + error.what());
    } catch (const std::exception& error) {
        return json_error(500, error.what());
    }
}

Json parse_json_body(const Request& request) {
    if (request.body.empty()) throw BadRequest("request body is required");
    try {
        return Json::parse(request.body);
    } catch (const JsonError& error) {
        throw BadRequest(std::string("invalid JSON body: ") + error.what());
    }
}

std::map<std::string, std::string> parse_query(const std::string& query) {
    std::map<std::string, std::string> params;
    std::size_t start = 0;
    while (start <= query.size()) {
        const auto end = query.find('&', start);
        const std::string pair =
            query.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!pair.empty()) {
            const auto equals = pair.find('=');
            const std::string key = url_decode(pair.substr(0, equals));
            const std::string value =
                equals == std::string::npos ? std::string{} : url_decode(pair.substr(equals + 1));
            if (!key.empty()) params[key] = value;
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return params;
}

std::string query_param(const Request& request, const std::string& name,
                        const std::string& fallback) {
    const auto params = parse_query(request.query);
    const auto it = params.find(name);
    return it == params.end() ? fallback : it->second;
}

std::int64_t path_param_id(const Request& request, const std::string& name) {
    const auto it = request.path_params.find(name);
    if (it == request.path_params.end() || it->second.empty())
        throw BadRequest("missing path parameter: " + name);
    std::size_t consumed = 0;
    try {
        const long long value = std::stoll(it->second, &consumed);
        if (consumed == it->second.size()) return static_cast<std::int64_t>(value);
    } catch (const std::exception&) {
        // fall through
    }
    throw BadRequest("path parameter '" + name + "' must be an integer");
}

std::string require_string(const Json& object, const std::string& key) {
    if (!object.is_object()) throw BadRequest("request body must be a JSON object");
    if (!object.contains(key)) throw BadRequest("missing field: " + key);
    const auto& value = object.at(key);
    if (!value.is_string()) throw BadRequest("field '" + key + "' must be a string");
    if (value.as_string().empty()) throw BadRequest("field '" + key + "' must not be empty");
    return value.as_string();
}

std::string optional_string(const Json& object, const std::string& key,
                            const std::string& fallback) {
    if (!object.contains(key) || object.at(key).is_null()) return fallback;
    const auto& value = object.at(key);
    if (!value.is_string()) throw BadRequest("field '" + key + "' must be a string");
    return value.as_string();
}

std::int64_t require_int64(const Json& object, const std::string& key) {
    if (!object.is_object()) throw BadRequest("request body must be a JSON object");
    if (!object.contains(key)) throw BadRequest("missing field: " + key);
    return value_to_int64(object.at(key), key);
}

std::int64_t optional_int64(const Json& object, const std::string& key,
                            std::int64_t fallback) {
    if (!object.contains(key) || object.at(key).is_null()) return fallback;
    return value_to_int64(object.at(key), key);
}

double require_number(const Json& object, const std::string& key) {
    if (!object.is_object()) throw BadRequest("request body must be a JSON object");
    if (!object.contains(key)) throw BadRequest("missing field: " + key);
    return value_to_number(object.at(key), key);
}

double optional_number(const Json& object, const std::string& key, double fallback) {
    if (!object.contains(key) || object.at(key).is_null()) return fallback;
    return value_to_number(object.at(key), key);
}

bool optional_bool(const Json& object, const std::string& key, bool fallback) {
    if (!object.contains(key) || object.at(key).is_null()) return fallback;
    const auto& value = object.at(key);
    if (value.is_bool()) return value.as_bool();
    if (value.is_number()) return value.as_double() != 0.0;
    if (value.is_string()) {
        const std::string& text = value.as_string();
        if (text == "true" || text == "1") return true;
        if (text == "false" || text == "0") return false;
    }
    throw BadRequest("field '" + key + "' must be a boolean");
}

Json parse_json_or(const std::string& text, const Json& fallback) {
    if (text.empty()) return fallback;
    try {
        return Json::parse(text);
    } catch (const JsonError&) {
        return fallback;
    }
}

}  // namespace platform
