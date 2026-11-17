#pragma once

#include <json-c/json.h>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

namespace platform {
// Small ownership wrapper; parsing and serialization are provided by json-c.
class Json {
public:
    Json() = default;
    explicit Json(json_object* value) : value_(value, json_object_put) {}
    static Json object() { return Json(json_object_new_object()); }
    static Json array() { return Json(json_object_new_array()); }
    static Json text(const std::string& s) { return Json(json_object_new_string_len(s.data(), static_cast<int>(s.size()))); }
    static Json number(std::int64_t n) { return Json(json_object_new_int64(n)); }
    static Json boolean(bool b) { return Json(json_object_new_boolean(b)); }
    static Json parse(const std::string& input) {
        const std::string terminated = input + " ";
        auto tok = std::unique_ptr<json_tokener, decltype(&json_tokener_free)>(json_tokener_new_ex(32), json_tokener_free);
        json_tokener_set_flags(tok.get(), JSON_TOKENER_STRICT | JSON_TOKENER_VALIDATE_UTF8);
        Json result(json_tokener_parse_ex(tok.get(), terminated.data(), static_cast<int>(terminated.size())));
        if (json_tokener_get_error(tok.get()) != json_tokener_success ||
            terminated.find_first_not_of(" \t\r\n", json_tokener_get_parse_end(tok.get())) != std::string::npos)
            throw std::invalid_argument("invalid JSON");
        return result;
    }
    std::string dump() const { return value_ ? json_object_to_json_string_ext(value_.get(), JSON_C_TO_STRING_PLAIN) : "null"; }
    bool is(json_type type) const { return json_object_is_type(value_.get(), type); }
    bool has(const std::string& key) const {
        json_object* value = nullptr;
        return is(json_type_object) && json_object_object_get_ex(value_.get(), key.c_str(), &value);
    }
    Json get(const std::string& key) const {
        json_object* value = nullptr;
        if (is(json_type_object)) json_object_object_get_ex(value_.get(), key.c_str(), &value);
        return Json(json_object_get(value));
    }
    Json at(std::size_t index) const { return Json(json_object_get(json_object_array_get_idx(value_.get(), index))); }
    std::size_t size() const { return is(json_type_array) ? json_object_array_length(value_.get()) : 0; }
    std::string string() const {
        if (!is(json_type_string)) throw std::invalid_argument("expected string");
        return std::string(json_object_get_string(value_.get()), json_object_get_string_len(value_.get()));
    }
    std::int64_t integer() const {
        if (!is(json_type_int)) throw std::invalid_argument("expected integer");
        return json_object_get_int64(value_.get());
    }
    bool boolean() const {
        if (!is(json_type_boolean)) throw std::invalid_argument("expected boolean");
        return json_object_get_boolean(value_.get());
    }
    void set(const std::string& key, const Json& value) { json_object_object_add(value_.get(), key.c_str(), json_object_get(value.value_.get())); }
    void push(const Json& value) { json_object_array_add(value_.get(), json_object_get(value.value_.get())); }
    json_object* raw() const { return value_.get(); }
private:
    std::shared_ptr<json_object> value_;
};
}  // namespace platform
