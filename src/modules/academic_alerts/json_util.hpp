#pragma once

#include "platform/app.hpp"

#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <utility>

// ============================================================================
// 极简 JSON / 查询串工具
//
// 平台没有提供 JSON 库，这个头文件只解决本模块需要的问题：
//   - 输出：转义、拼字符串、拼数字
//   - 输入：解析扁平的 JSON 对象（请求体）
//   - 查询串：解析 ?a=1&b=2，支持 %XX 解码（中文班级名）
//
// 全部是 inline，直接在头文件里实现，不产生额外的编译单元。
// ============================================================================

namespace platform::academic_alerts {

// ---------------------------------------------------------------------------
// 输出
// ---------------------------------------------------------------------------

inline std::string json_escape(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (unsigned char c : value) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    static const char* hex = "0123456789abcdef";
                    out += "\\u00";
                    out += hex[c >> 4];
                    out += hex[c & 0x0f];
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

inline std::string quote(const std::string& value) { return "\"" + json_escape(value) + "\""; }

/// 数字转字符串：固定两位小数并去掉末尾多余的 0（66.07 / 58.4 / 12）
inline std::string number(double value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << value;
    std::string text = out.str();
    while (text.size() > 1 && text.back() == '0') text.pop_back();
    if (!text.empty() && text.back() == '.') text.pop_back();
    return text;
}

inline Response json(int status, std::string body) {
    return Response{status, "application/json; charset=utf-8", std::move(body)};
}

inline Response bad_request(const std::string& message) {
    return json(400, std::string("{\"error\":") + quote(message) + "}");
}

inline Response not_found(const std::string& message) {
    return json(404, std::string("{\"error\":") + quote(message) + "}");
}

// ---------------------------------------------------------------------------
// 输入：扁平 JSON 对象解析
//
// 只处理 {"key":"value","n":60} 这种一层结构，够本模块的请求体使用。
// 复杂嵌套（数组、对象）请交给专门的 JSON 库。
// ---------------------------------------------------------------------------

class JsonBody {
public:
    explicit JsonBody(const std::string& text) { parse(text); }

    bool has(const std::string& key) const { return values_.count(key) > 0; }

    std::string str(const std::string& key, const std::string& fallback = {}) const {
        const auto it = values_.find(key);
        return it == values_.end() ? fallback : it->second;
    }

    double num(const std::string& key, double fallback = 0.0) const {
        const auto it = values_.find(key);
        if (it == values_.end() || it->second.empty()) return fallback;
        try {
            return std::stod(it->second);
        } catch (const std::exception&) {
            return fallback;
        }
    }

private:
    void parse(const std::string& text) {
        std::size_t cursor = 0;
        while (cursor < text.size()) {
            const auto key_begin = text.find('"', cursor);
            if (key_begin == std::string::npos) return;
            const auto key_end = text.find('"', key_begin + 1);
            if (key_end == std::string::npos) return;
            const std::string key = text.substr(key_begin + 1, key_end - key_begin - 1);
            const auto colon = text.find(':', key_end + 1);
            if (colon == std::string::npos) return;

            std::size_t value_at = colon + 1;
            while (value_at < text.size() &&
                   std::isspace(static_cast<unsigned char>(text[value_at])))
                ++value_at;
            if (value_at >= text.size()) return;

            if (text[value_at] == '"') {
                const std::size_t end = find_string_end(text, value_at + 1);
                if (end == std::string::npos) return;
                values_[key] = unescape(text.substr(value_at + 1, end - value_at - 1));
                cursor = end + 1;
            } else {
                std::size_t end = value_at;
                while (end < text.size() && text[end] != ',' && text[end] != '}') ++end;
                std::string raw = text.substr(value_at, end - value_at);
                while (!raw.empty() && std::isspace(static_cast<unsigned char>(raw.back())))
                    raw.pop_back();
                if (raw == "true") raw = "1";
                if (raw == "false") raw = "0";
                values_[key] = raw;
                cursor = end;
            }
        }
    }

    static std::size_t find_string_end(const std::string& text, std::size_t from) {
        bool escaped = false;
        for (std::size_t i = from; i < text.size(); ++i) {
            if (escaped) {
                escaped = false;
            } else if (text[i] == '\\') {
                escaped = true;
            } else if (text[i] == '"') {
                return i;
            }
        }
        return std::string::npos;
    }

    static std::string unescape(const std::string& raw) {
        std::string out;
        for (std::size_t i = 0; i < raw.size(); ++i) {
            if (raw[i] != '\\' || i + 1 >= raw.size()) {
                out += raw[i];
                continue;
            }
            switch (const char next = raw[++i]) {
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                default: out += next;
            }
        }
        return out;
    }

    std::map<std::string, std::string> values_;
};

// ---------------------------------------------------------------------------
// 查询串
// ---------------------------------------------------------------------------

inline std::string url_decode(const std::string& value) {
    std::string out;
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%' && i + 2 < value.size()) {
            const auto hex = value.substr(i + 1, 2);
            out += static_cast<char>(std::strtol(hex.c_str(), nullptr, 16));
            i += 2;
        } else if (value[i] == '+') {
            out += ' ';
        } else {
            out += value[i];
        }
    }
    return out;
}

inline std::map<std::string, std::string> parse_query(const std::string& query) {
    std::map<std::string, std::string> out;
    std::size_t cursor = 0;
    while (cursor < query.size()) {
        const auto amp = query.find('&', cursor);
        const std::string pair = query.substr(cursor, amp == std::string::npos ? amp : amp - cursor);
        const auto equal = pair.find('=');
        if (equal != std::string::npos)
            out[url_decode(pair.substr(0, equal))] = url_decode(pair.substr(equal + 1));
        if (amp == std::string::npos) break;
        cursor = amp + 1;
    }
    return out;
}

}  // namespace platform::academic_alerts
