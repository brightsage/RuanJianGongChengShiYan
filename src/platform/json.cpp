#include "platform/json.hpp"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <locale>
#include <sstream>
#include <utility>

namespace platform {
namespace {

constexpr int max_depth = 128;

void append_utf8(std::string& out, std::uint32_t code_point) {
    if (code_point <= 0x7F) {
        out += static_cast<char>(code_point);
    } else if (code_point <= 0x7FF) {
        out += static_cast<char>(0xC0 | (code_point >> 6));
        out += static_cast<char>(0x80 | (code_point & 0x3F));
    } else if (code_point <= 0xFFFF) {
        out += static_cast<char>(0xE0 | (code_point >> 12));
        out += static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (code_point & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (code_point >> 18));
        out += static_cast<char>(0x80 | ((code_point >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (code_point & 0x3F));
    }
}

void append_escaped_string(std::string& out, const std::string& value) {
    static const char* hex = "0123456789abcdef";
    out += '"';
    for (unsigned char c : value) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    out += "\\u00";
                    out += hex[c >> 4];
                    out += hex[c & 0x0F];
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    out += '"';
}

void append_number(std::string& out, double value) {
    if (!std::isfinite(value)) throw JsonError("cannot serialize non-finite number");
    if (std::floor(value) == value && std::fabs(value) <= 9007199254740992.0) {
        out += std::to_string(static_cast<std::int64_t>(value));
        return;
    }
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setprecision(15) << value;
    std::string text = stream.str();
    if (text.find('.') != std::string::npos && text.find('e') == std::string::npos &&
        text.find('E') == std::string::npos) {
        while (!text.empty() && text.back() == '0') text.pop_back();
        if (!text.empty() && text.back() == '.') text.pop_back();
    }
    out += text;
}

void dump_into(const Json& value, std::string& out) {
    if (value.is_null()) {
        out += "null";
    } else if (value.is_bool()) {
        out += value.as_bool() ? "true" : "false";
    } else if (value.is_number()) {
        append_number(out, value.as_double());
    } else if (value.is_string()) {
        append_escaped_string(out, value.as_string());
    } else if (value.is_array()) {
        out += '[';
        bool first = true;
        for (const auto& item : value.as_array()) {
            if (!first) out += ',';
            first = false;
            dump_into(item, out);
        }
        out += ']';
    } else {
        out += '{';
        bool first = true;
        for (const auto& [key, item] : value.as_object()) {
            if (!first) out += ',';
            first = false;
            append_escaped_string(out, key);
            out += ':';
            dump_into(item, out);
        }
        out += '}';
    }
}

class Parser {
public:
    explicit Parser(const std::string& text) : text_(text) {}

    Json parse() {
        skip_space();
        Json result = parse_value(0);
        skip_space();
        if (pos_ != text_.size()) fail("trailing characters");
        return result;
    }

private:
    [[noreturn]] void fail(const std::string& message) const {
        throw JsonError(message + " at offset " + std::to_string(pos_));
    }

    void skip_space() {
        while (pos_ < text_.size()) {
            const char c = text_[pos_];
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r') break;
            ++pos_;
        }
    }

    bool consume(char expected) {
        if (pos_ < text_.size() && text_[pos_] == expected) {
            ++pos_;
            return true;
        }
        return false;
    }

    Json parse_value(int depth) {
        if (depth > max_depth) fail("nesting too deep");
        if (pos_ >= text_.size()) fail("unexpected end of input");
        const char c = text_[pos_];
        if (c == '{') return parse_object(depth);
        if (c == '[') return parse_array(depth);
        if (c == '"') return Json(parse_string());
        if (c == 't') return parse_literal("true", Json(true));
        if (c == 'f') return parse_literal("false", Json(false));
        if (c == 'n') return parse_literal("null", Json(nullptr));
        if (c == '-' || (c >= '0' && c <= '9')) return parse_number();
        fail("unexpected character");
    }

    Json parse_literal(const char* literal, Json value) {
        const std::string text(literal);
        if (text_.compare(pos_, text.size(), text) != 0) fail("invalid literal");
        pos_ += text.size();
        return value;
    }

    Json parse_object(int depth) {
        ++pos_;  // '{'
        Json::Object object;
        skip_space();
        if (consume('}')) return Json(std::move(object));
        while (true) {
            skip_space();
            if (pos_ >= text_.size() || text_[pos_] != '"') fail("object key must be a string");
            std::string key = parse_string();
            skip_space();
            if (!consume(':')) fail("expected ':'");
            skip_space();
            object[std::move(key)] = parse_value(depth + 1);
            skip_space();
            if (consume('}')) return Json(std::move(object));
            if (!consume(',')) fail("expected ',' or '}'");
        }
    }

    Json parse_array(int depth) {
        ++pos_;  // '['
        Json::Array array;
        skip_space();
        if (consume(']')) return Json(std::move(array));
        while (true) {
            skip_space();
            array.push_back(parse_value(depth + 1));
            skip_space();
            if (consume(']')) return Json(std::move(array));
            if (!consume(',')) fail("expected ',' or ']'");
        }
    }

    unsigned parse_hex4() {
        if (pos_ + 4 > text_.size()) fail("incomplete unicode escape");
        unsigned value = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = text_[pos_++];
            value <<= 4;
            if (c >= '0' && c <= '9') value |= static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') value |= static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') value |= static_cast<unsigned>(c - 'A' + 10);
            else fail("invalid unicode escape");
        }
        return value;
    }

    std::uint32_t parse_unicode_code_point() {
        const unsigned first = parse_hex4();
        if (first >= 0xD800 && first <= 0xDBFF) {
            if (pos_ + 2 > text_.size() || text_[pos_] != '\\' || text_[pos_ + 1] != 'u')
                fail("missing low surrogate");
            pos_ += 2;
            const unsigned second = parse_hex4();
            if (second < 0xDC00 || second > 0xDFFF) fail("invalid low surrogate");
            return 0x10000 + ((first - 0xD800) << 10) + (second - 0xDC00);
        }
        if (first >= 0xDC00 && first <= 0xDFFF) fail("unexpected low surrogate");
        return first;
    }

    std::string parse_string() {
        ++pos_;  // '"'
        std::string out;
        while (pos_ < text_.size()) {
            const unsigned char c = static_cast<unsigned char>(text_[pos_++]);
            if (c == '"') return out;
            if (c == '\\') {
                if (pos_ >= text_.size()) fail("unfinished escape");
                const char escape = text_[pos_++];
                switch (escape) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    case 'u': append_utf8(out, parse_unicode_code_point()); break;
                    default: fail("invalid escape");
                }
            } else if (c < 0x20) {
                fail("control character in string");
            } else {
                out += static_cast<char>(c);
            }
        }
        fail("unterminated string");
    }

    Json parse_number() {
        const std::size_t start = pos_;
        if (text_[pos_] == '-') ++pos_;
        if (pos_ >= text_.size()) fail("invalid number");
        if (text_[pos_] == '0') {
            ++pos_;
        } else {
            if (text_[pos_] < '1' || text_[pos_] > '9') fail("invalid number");
            while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') ++pos_;
        }
        if (pos_ < text_.size() && text_[pos_] == '.') {
            ++pos_;
            if (pos_ >= text_.size() || text_[pos_] < '0' || text_[pos_] > '9')
                fail("invalid fraction");
            while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') ++pos_;
        }
        if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) ++pos_;
            if (pos_ >= text_.size() || text_[pos_] < '0' || text_[pos_] > '9')
                fail("invalid exponent");
            while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') ++pos_;
        }
        const std::string token = text_.substr(start, pos_ - start);
        char* end = nullptr;
        errno = 0;
        const double value = std::strtod(token.c_str(), &end);
        if (end != token.c_str() + token.size() || errno == ERANGE)
            fail("number out of range");
        return Json(value);
    }

    const std::string& text_;
    std::size_t pos_ = 0;
};

}  // namespace

Json::Json() : value_(nullptr) {}
Json::Json(std::nullptr_t) : value_(nullptr) {}
Json::Json(bool value) : value_(value) {}
Json::Json(int value) : value_(static_cast<double>(value)) {}
Json::Json(std::int64_t value) : value_(static_cast<double>(value)) {}
Json::Json(double value) : value_(value) {}
Json::Json(const char* value)
    : value_(value == nullptr ? std::string{} : std::string(value)) {}
Json::Json(std::string value) : value_(std::move(value)) {}
Json::Json(Array value) : value_(std::move(value)) {}
Json::Json(Object value) : value_(std::move(value)) {}

Json Json::parse(const std::string& text) { return Parser(text).parse(); }

Json Json::object() { return Json(Object{}); }
Json Json::array() { return Json(Array{}); }

std::string Json::dump() const {
    std::string out;
    dump_into(*this, out);
    return out;
}

bool Json::is_null() const { return std::holds_alternative<std::nullptr_t>(value_); }
bool Json::is_bool() const { return std::holds_alternative<bool>(value_); }
bool Json::is_number() const { return std::holds_alternative<double>(value_); }
bool Json::is_string() const { return std::holds_alternative<std::string>(value_); }
bool Json::is_array() const { return std::holds_alternative<Array>(value_); }
bool Json::is_object() const { return std::holds_alternative<Object>(value_); }

bool Json::as_bool() const {
    if (!is_bool()) throw JsonError("json value is not a bool");
    return std::get<bool>(value_);
}

std::int64_t Json::as_int64() const {
    if (!is_number()) throw JsonError("json value is not a number");
    return static_cast<std::int64_t>(std::get<double>(value_));
}

double Json::as_double() const {
    if (!is_number()) throw JsonError("json value is not a number");
    return std::get<double>(value_);
}

const std::string& Json::as_string() const {
    if (!is_string()) throw JsonError("json value is not a string");
    return std::get<std::string>(value_);
}

const Json::Array& Json::as_array() const {
    if (!is_array()) throw JsonError("json value is not an array");
    return std::get<Array>(value_);
}

Json::Array& Json::as_array() {
    if (!is_array()) throw JsonError("json value is not an array");
    return std::get<Array>(value_);
}

const Json::Object& Json::as_object() const {
    if (!is_object()) throw JsonError("json value is not an object");
    return std::get<Object>(value_);
}

Json::Object& Json::as_object() {
    if (!is_object()) throw JsonError("json value is not an object");
    return std::get<Object>(value_);
}

bool Json::contains(const std::string& key) const {
    return is_object() && std::get<Object>(value_).count(key) != 0;
}

const Json& Json::at(const std::string& key) const {
    const auto& object = as_object();
    const auto it = object.find(key);
    if (it == object.end()) throw JsonError("missing key: " + key);
    return it->second;
}

Json& Json::at(const std::string& key) {
    auto& object = as_object();
    const auto it = object.find(key);
    if (it == object.end()) throw JsonError("missing key: " + key);
    return it->second;
}

const Json& Json::at(std::size_t index) const {
    const auto& array = as_array();
    if (index >= array.size()) throw JsonError("array index out of range");
    return array[index];
}

Json& Json::at(std::size_t index) {
    auto& array = as_array();
    if (index >= array.size()) throw JsonError("array index out of range");
    return array[index];
}

Json& Json::operator[](const std::string& key) {
    if (is_null()) value_ = Object{};
    if (!is_object()) throw JsonError("json value is not an object");
    return std::get<Object>(value_)[key];
}

Json& Json::operator[](std::size_t index) {
    if (is_null()) value_ = Array{};
    if (!is_array()) throw JsonError("json value is not an array");
    auto& array = std::get<Array>(value_);
    if (index >= array.size()) array.resize(index + 1);
    return array[index];
}

void Json::push_back(Json value) {
    if (is_null()) value_ = Array{};
    if (!is_array()) throw JsonError("json value is not an array");
    std::get<Array>(value_).push_back(std::move(value));
}

}  // namespace platform
