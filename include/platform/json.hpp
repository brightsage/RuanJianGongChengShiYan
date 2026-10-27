#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace platform {

class JsonError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// 轻量 JSON 值。只覆盖本项目接口需要的类型：null、bool、number、string、
// array、object。数字统一以 double 保存，整数值在 dump 时输出为整数。
class Json {
public:
    using Array = std::vector<Json>;
    using Object = std::map<std::string, Json>;

    Json();
    Json(std::nullptr_t);
    Json(bool value);
    Json(int value);
    Json(std::int64_t value);
    Json(double value);
    Json(const char* value);
    Json(std::string value);
    Json(Array value);
    Json(Object value);

    static Json parse(const std::string& text);
    static Json object();
    static Json array();

    std::string dump() const;

    bool is_null() const;
    bool is_bool() const;
    bool is_number() const;
    bool is_string() const;
    bool is_array() const;
    bool is_object() const;

    bool as_bool() const;
    std::int64_t as_int64() const;
    double as_double() const;
    const std::string& as_string() const;
    const Array& as_array() const;
    Array& as_array();
    const Object& as_object() const;
    Object& as_object();

    bool contains(const std::string& key) const;
    const Json& at(const std::string& key) const;
    Json& at(const std::string& key);
    const Json& at(std::size_t index) const;
    Json& at(std::size_t index);

    Json& operator[](const std::string& key);
    Json& operator[](std::size_t index);
    void push_back(Json value);

private:
    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> value_;
};

}  // namespace platform
