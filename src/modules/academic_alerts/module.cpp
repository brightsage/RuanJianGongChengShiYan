#include "platform/modules.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

// ============================================================================
// M4 学业分析与预警
//
// 需求对应（实验一需求分析 2.3.5）：
//   UC-AA-01 查看学业看板   GET  /dashboard
//   UC-AA-02 群体分析       GET  /dashboard?group=class|major|grade
//   UC-AA-03 配置预警规则   GET/POST /rules
//   UC-AA-04 生成预警名单   POST /alerts/generate
//   UC-AA-05 处置预警       POST /alerts/:alert_id/claim  /handle
//   UC-AA-06 跟踪干预记录   GET  /alerts/:alert_id/interventions
//   UC-AA-07 分析学业趋势   GET  /students/:student_id/trend
//
// 业务规则（实验一 3.3 UC-AA-05）：
//   - 越权不可认领：已被别人认领的预警，不能再认领
//   - 未填写干预措施不得关闭
//   - 学生转专业/休学可标记「不适用」
//   - 关闭后保留历史，不可删除
//   - 生成预警幂等：同一学生同一规则存在未关闭记录时不重复生成
// ============================================================================

namespace platform {
namespace {

// ---------------------------------------------------------------------------
// JSON 工具（平台没有 JSON 库，这里做一个够用的最小实现）
// ---------------------------------------------------------------------------

std::string json_escape(const std::string& value) {
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

std::string quote(const std::string& value) { return "\"" + json_escape(value) + "\""; }

std::string number(double value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << value;
    std::string text = out.str();
    while (text.size() > 1 && text.back() == '0') text.pop_back();
    if (!text.empty() && text.back() == '.') text.pop_back();
    return text;
}

Response json(int status, std::string body) {
    return Response{status, "application/json; charset=utf-8", std::move(body)};
}

Response bad_request(const std::string& message) {
    return json(400, std::string("{\"error\":") + quote(message) + "}");
}

Response not_found(const std::string& message) {
    return json(404, std::string("{\"error\":") + quote(message) + "}");
}

// 只支持扁平 JSON 对象，足够本模块的请求体使用。
class JsonBody {
public:
    explicit JsonBody(const std::string& text) { parse(text); }

    bool has(const std::string& key) const { return values_.count(key) > 0; }

    std::string str(const std::string& key, const std::string& fallback = {}) const {
        const auto it = values_.find(key);
        if (it == values_.end()) return fallback;
        return it->second;
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
            while (value_at < text.size() && std::isspace(static_cast<unsigned char>(text[value_at])))
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
                while (!raw.empty() && std::isspace(static_cast<unsigned char>(raw.back()))) raw.pop_back();
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
            const char next = raw[++i];
            switch (next) {
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
// 查询串解析（支持 %XX 解码，中文班级名可以正常传参）
// ---------------------------------------------------------------------------

std::string url_decode(const std::string& value) {
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

std::map<std::string, std::string> parse_query(const std::string& query) {
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

// ---------------------------------------------------------------------------
// 时间
// ---------------------------------------------------------------------------

std::int64_t now_epoch() {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

std::string iso_time(std::int64_t epoch) {
    const std::time_t raw = static_cast<std::time_t>(epoch);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &raw);
#else
    gmtime_r(&raw, &tm);
#endif
    char buffer[32]{};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buffer;
}

// ---------------------------------------------------------------------------
// 领域模型（内存版，实验阶段够用；后续可换成 M2 的指标库）
// ---------------------------------------------------------------------------

struct TermScore {
    std::string term;
    double average;
};

struct Student {
    std::string id;
    std::string name;
    std::string class_name;
    std::string major;
    std::string grade;
    double avg_score = 0.0;      // 综合成绩
    int failed_courses = 0;      // 不及格门数
    double credit_rate = 0.0;    // 学分完成率 0~1
    double attendance = 0.0;     // 出勤率 0~1
    std::vector<TermScore> trend;
};

struct Rule {
    std::string id;
    std::string name;
    std::string metric;    // avg_score | failed_courses | credit_rate | attendance
    std::string op;        // lt | lte | gt | gte
    double threshold = 0.0;
    std::string level;     // red | orange | yellow
    std::string notify;    // 通知对象
    bool enabled = true;
};

struct Intervention {
    std::string at;
    std::string action;    // follow | close | not_applicable
    std::string measures;
    std::string note;
    std::string by;
};

struct Alert {
    int id = 0;
    std::string student_id;
    std::string rule_id;
    std::string level;
    std::string status;    // open | claimed | closed | not_applicable
    std::int64_t created = 0;
    std::string assignee;
    double metric_value = 0.0;
    std::vector<Intervention> interventions;
};

struct Store {
    std::vector<Student> students;
    std::vector<Rule> rules;
    std::vector<Alert> alerts;
    int next_alert_id = 1;
};

Store& store() {
    static Store data = [] {
        Store s;
        s.students = {
            {"2021001", "张伟", "计科2101", "计算机科学与技术", "2021", 58.4, 3, 0.52, 0.72,
             {{"2024-2025-1", 61.2}, {"2024-2025-2", 57.8}, {"2025-2026-1", 58.4}}},
            {"2021002", "李娜", "计科2101", "计算机科学与技术", "2021", 76.9, 1, 0.86, 0.93,
             {{"2024-2025-1", 74.1}, {"2024-2025-2", 75.6}, {"2025-2026-1", 76.9}}},
            {"2021003", "王强", "计科2101", "计算机科学与技术", "2021", 52.1, 4, 0.41, 0.65,
             {{"2024-2025-1", 55.6}, {"2024-2025-2", 50.3}, {"2025-2026-1", 52.1}}},
            {"2021004", "赵敏", "计科2101", "计算机科学与技术", "2021", 68.7, 0, 0.74, 0.88,
             {{"2024-2025-1", 70.2}, {"2024-2025-2", 69.5}, {"2025-2026-1", 68.7}}},
            {"2021005", "陈晨", "计科2102", "计算机科学与技术", "2021", 63.5, 2, 0.68, 0.79,
             {{"2024-2025-1", 66.8}, {"2024-2025-2", 64.1}, {"2025-2026-1", 63.5}}},
            {"2021006", "刘洋", "计科2102", "计算机科学与技术", "2021", 81.3, 0, 0.92, 0.96,
             {{"2024-2025-1", 79.4}, {"2024-2025-2", 80.6}, {"2025-2026-1", 81.3}}},
            {"2021007", "周杰", "计科2102", "计算机科学与技术", "2021", 47.8, 5, 0.33, 0.58,
             {{"2024-2025-1", 52.3}, {"2024-2025-2", 49.1}, {"2025-2026-1", 47.8}}},
            {"2021008", "吴静", "计科2102", "计算机科学与技术", "2021", 72.6, 1, 0.81, 0.91,
             {{"2024-2025-1", 71.9}, {"2024-2025-2", 72.0}, {"2025-2026-1", 72.6}}},
            {"2021009", "孙悦", "软工2101", "软件工程", "2021", 66.2, 1, 0.71, 0.77,
             {{"2024-2025-1", 68.0}, {"2024-2025-2", 67.1}, {"2025-2026-1", 66.2}}},
            {"2021010", "郑凯", "软工2101", "软件工程", "2021", 55.3, 3, 0.48, 0.69,
             {{"2024-2025-1", 58.9}, {"2024-2025-2", 56.4}, {"2025-2026-1", 55.3}}},
            {"2021011", "冯雪", "软工2101", "软件工程", "2021", 88.1, 0, 0.95, 0.98,
             {{"2024-2025-1", 86.2}, {"2024-2025-2", 87.4}, {"2025-2026-1", 88.1}}},
            {"2021012", "何磊", "软工2101", "软件工程", "2021", 61.9, 2, 0.63, 0.75,
             {{"2024-2025-1", 64.0}, {"2024-2025-2", 62.7}, {"2025-2026-1", 61.9}}},
        };
        s.rules = {
            {"R1", "综合成绩不达标", "avg_score", "lt", 60.0, "red", "辅导员、学生本人", true},
            {"R2", "多门课程不及格", "failed_courses", "gte", 2.0, "orange", "辅导员", true},
            {"R3", "出勤率偏低", "attendance", "lt", 0.80, "yellow", "辅导员", true},
            {"R4", "学分完成率偏低", "credit_rate", "lt", 0.60, "orange", "辅导员、院系管理员", true},
        };
        // 造一条三天前的历史预警，用来演示 UC-AA-06 的「超时提醒」
        Alert overdue;
        overdue.id = s.next_alert_id++;
        overdue.student_id = "2021005";
        overdue.rule_id = "R2";
        overdue.level = "orange";
        overdue.status = "open";
        overdue.created = now_epoch() - 72 * 3600;
        overdue.metric_value = 2.0;
        s.alerts.push_back(std::move(overdue));
        return s;
    }();
    return data;
}

// ---------------------------------------------------------------------------
// 领域逻辑
// ---------------------------------------------------------------------------

Student* find_student(const std::string& id) {
    for (auto& item : store().students)
        if (item.id == id) return &item;
    return nullptr;
}

Alert* find_alert(int id) {
    for (auto& item : store().alerts)
        if (item.id == id) return &item;
    return nullptr;
}

bool is_metric(const std::string& metric) {
    return metric == "avg_score" || metric == "failed_courses" || metric == "credit_rate" ||
           metric == "attendance";
}

bool is_operator(const std::string& op) { return op == "lt" || op == "lte" || op == "gt" || op == "gte"; }

bool is_level(const std::string& level) {
    return level == "red" || level == "orange" || level == "yellow";
}

double metric_value(const Student& student, const std::string& metric) {
    if (metric == "avg_score") return student.avg_score;
    if (metric == "failed_courses") return static_cast<double>(student.failed_courses);
    if (metric == "credit_rate") return student.credit_rate;
    if (metric == "attendance") return student.attendance;
    return 0.0;
}

bool hit(const Rule& rule, const Student& student, double* value) {
    const double actual = metric_value(student, rule.metric);
    *value = actual;
    if (rule.op == "lt") return actual < rule.threshold;
    if (rule.op == "lte") return actual <= rule.threshold;
    if (rule.op == "gt") return actual > rule.threshold;
    if (rule.op == "gte") return actual >= rule.threshold;
    return false;
}

bool active_status(const std::string& status) { return status == "open" || status == "claimed"; }

int parse_id(const std::string& text) {
    if (text.empty() || text.size() > 9) return -1;
    for (unsigned char c : text)
        if (!std::isdigit(c)) return -1;
    return std::stoi(text);
}

bool overdue(const Alert& alert) {
    return active_status(alert.status) && now_epoch() - alert.created > 48 * 3600;
}

std::string alert_json(const Alert& alert) {
    std::string body = "{";
    body += "\"id\":" + std::to_string(alert.id);
    body += ",\"student_id\":" + quote(alert.student_id);
    body += ",\"rule_id\":" + quote(alert.rule_id);
    body += ",\"level\":" + quote(alert.level);
    body += ",\"status\":" + quote(alert.status);
    body += ",\"created_at\":" + quote(iso_time(alert.created));
    body += ",\"assignee\":" + quote(alert.assignee);
    body += ",\"metric_value\":" + number(alert.metric_value);
    body += ",\"overdue\":" + std::string(overdue(alert) ? "true" : "false");
    if (const Student* student = find_student(alert.student_id)) {
        body += ",\"student_name\":" + quote(student->name);
        body += ",\"class_name\":" + quote(student->class_name);
    }
    body += ",\"interventions\":[";
    bool first = true;
    for (const auto& item : alert.interventions) {
        if (!first) body += ',';
        first = false;
        body += "{\"at\":" + quote(item.at) + ",\"action\":" + quote(item.action) +
                ",\"measures\":" + quote(item.measures) + ",\"note\":" + quote(item.note) +
                ",\"by\":" + quote(item.by) + "}";
    }
    body += "]}";
    return body;
}

// ---------------------------------------------------------------------------
// 路由实现
// ---------------------------------------------------------------------------

// UC-AA-03 配置预警规则：查看
Response list_rules(const Request&) {
    std::string body = "{\"total\":" + std::to_string(store().rules.size()) + ",\"rules\":[";
    bool first = true;
    for (const auto& rule : store().rules) {
        if (!first) body += ',';
        first = false;
        body += "{\"id\":" + quote(rule.id) + ",\"name\":" + quote(rule.name) +
                ",\"metric\":" + quote(rule.metric) + ",\"operator\":" + quote(rule.op) +
                ",\"threshold\":" + number(rule.threshold) + ",\"level\":" + quote(rule.level) +
                ",\"notify\":" + quote(rule.notify) +
                ",\"enabled\":" + std::string(rule.enabled ? "true" : "false") + "}";
    }
    body += "]}";
    return json(200, std::move(body));
}

// UC-AA-03 配置预警规则：新增
Response create_rule(const Request& request) {
    const JsonBody body(request.body);
    const std::string metric = body.str("metric");
    const std::string op = body.str("operator", "lt");
    const std::string level = body.str("level", "yellow");
    if (!is_metric(metric)) return bad_request("metric 必须是 avg_score/failed_courses/credit_rate/attendance");
    if (!is_operator(op)) return bad_request("operator 必须是 lt/lte/gt/gte");
    if (!is_level(level)) return bad_request("level 必须是 red/orange/yellow");
    if (!body.has("threshold")) return bad_request("缺少 threshold");

    Rule rule;
    rule.id = "R" + std::to_string(store().rules.size() + 1);
    rule.name = body.str("name", metric + " 规则");
    rule.metric = metric;
    rule.op = op;
    rule.threshold = body.num("threshold");
    rule.level = level;
    rule.notify = body.str("notify", "辅导员");
    rule.enabled = true;
    store().rules.push_back(rule);
    return json(201, "{\"id\":" + quote(rule.id) + ",\"created\":true}");
}

// UC-AA-01 / UC-AA-02 学业看板与群体分析
Response dashboard(const Request& request) {
    const auto query = parse_query(request.query);
    const std::string group = query.count("group") ? query.at("group") : "class";
    const std::string filter_class = query.count("class") ? query.at("class") : "";
    const std::string filter_major = query.count("major") ? query.at("major") : "";

    const auto& students = store().students;
    std::vector<const Student*> selected;
    for (const auto& student : students) {
        if (!filter_class.empty() && student.class_name != filter_class) continue;
        if (!filter_major.empty() && student.major != filter_major) continue;
        selected.push_back(&student);
    }
    if (selected.empty()) return not_found("没有符合条件的学生");

    double sum = 0.0;
    int failing = 0;
    int alerts_active = 0;
    int alerts_overdue = 0;
    std::map<std::string, int> buckets;
    for (const Student* student : selected) {
        sum += student->avg_score;
        if (student->failed_courses > 0) ++failing;
        const std::string& key = group == "major" ? student->major
                                : group == "grade" ? student->grade
                                                   : student->class_name;
        ++buckets[key];
    }
    for (const auto& alert : store().alerts) {
        if (!active_status(alert.status)) continue;
        const Student* student = find_student(alert.student_id);
        if (student == nullptr) continue;
        if (!filter_class.empty() && student->class_name != filter_class) continue;
        if (!filter_major.empty() && student->major != filter_major) continue;
        ++alerts_active;
        if (overdue(alert)) ++alerts_overdue;
    }

    std::string body = "{";
    body += "\"group_by\":" + quote(group);
    body += ",\"student_count\":" + std::to_string(selected.size());
    body += ",\"avg_score\":" + number(sum / static_cast<double>(selected.size()));
    body += ",\"failing_students\":" + std::to_string(failing);
    body += ",\"active_alerts\":" + std::to_string(alerts_active);
    body += ",\"overdue_alerts\":" + std::to_string(alerts_overdue);
    body += ",\"distribution\":{";
    bool first = true;
    for (const auto& [name, count] : buckets) {
        if (!first) body += ',';
        first = false;
        body += quote(name) + ":" + std::to_string(count);
    }
    body += "}}";
    return json(200, std::move(body));
}

// UC-AA-04 生成预警名单（幂等）
Response generate_alerts(const Request&) {
    int generated = 0;
    int skipped = 0;
    const std::int64_t now = now_epoch();
    for (const auto& rule : store().rules) {
        if (!rule.enabled) continue;
        for (const auto& student : store().students) {
            double value = 0.0;
            if (!hit(rule, student, &value)) continue;
            bool already = false;
            for (const auto& alert : store().alerts) {
                if (alert.student_id == student.id && alert.rule_id == rule.id &&
                    active_status(alert.status)) {
                    already = true;
                    break;
                }
            }
            if (already) {
                ++skipped;
                continue;
            }
            Alert alert;
            alert.id = store().next_alert_id++;
            alert.student_id = student.id;
            alert.rule_id = rule.id;
            alert.level = rule.level;
            alert.status = "open";
            alert.created = now;
            alert.metric_value = value;
            store().alerts.push_back(std::move(alert));
            ++generated;
        }
    }
    return json(200, "{\"generated\":" + std::to_string(generated) +
                         ",\"skipped\":" + std::to_string(skipped) +
                         ",\"total_alerts\":" + std::to_string(store().alerts.size()) + "}");
}

// UC-AA-04 查看预警名单
Response list_alerts(const Request& request) {
    const auto query = parse_query(request.query);
    const std::string status = query.count("status") ? query.at("status") : "";
    const std::string level = query.count("level") ? query.at("level") : "";
    const std::string class_name = query.count("class") ? query.at("class") : "";

    std::string body = "{\"alerts\":[";
    bool first = true;
    int total = 0;
    int overdue_count = 0;
    for (const auto& alert : store().alerts) {
        if (!status.empty() && alert.status != status) continue;
        if (!level.empty() && alert.level != level) continue;
        if (!class_name.empty()) {
            const Student* student = find_student(alert.student_id);
            if (student == nullptr || student->class_name != class_name) continue;
        }
        if (!first) body += ',';
        first = false;
        body += alert_json(alert);
        ++total;
        if (overdue(alert)) ++overdue_count;
    }
    body += "],\"total\":" + std::to_string(total) +
            ",\"overdue\":" + std::to_string(overdue_count) + "}";
    return json(200, std::move(body));
}

// UC-AA-05 认领预警：已被他人认领则越权，返回 409
Response claim_alert(const Request& request) {
    const auto it = request.path_params.find("alert_id");
    if (it == request.path_params.end()) return bad_request("缺少 alert_id");
    Alert* alert = find_alert(parse_id(it->second));
    if (alert == nullptr) return not_found("预警不存在");

    const JsonBody body(request.body);
    const std::string assignee = body.str("assignee");
    if (assignee.empty()) return bad_request("缺少 assignee（认领人）");
    if (alert->status == "closed" || alert->status == "not_applicable")
        return json(409, "{\"error\":\"已关闭或不适用的预警不能再认领\"}");
    if (!alert->assignee.empty() && alert->assignee != assignee)
        return json(409, "{\"error\":\"该预警已被 " + json_escape(alert->assignee) + " 认领（越权）\"}");

    alert->assignee = assignee;
    alert->status = "claimed";
    alert->interventions.push_back(
        {iso_time(now_epoch()), "claim", "", "认领预警", assignee});
    return json(200, alert_json(*alert));
}

// UC-AA-05 处置预警：未填干预措施不得关闭
Response handle_alert(const Request& request) {
    const auto it = request.path_params.find("alert_id");
    if (it == request.path_params.end()) return bad_request("缺少 alert_id");
    Alert* alert = find_alert(parse_id(it->second));
    if (alert == nullptr) return not_found("预警不存在");

    const JsonBody body(request.body);
    const std::string action = body.str("action", "follow");
    const std::string measures = body.str("measures");
    const std::string note = body.str("note");
    const std::string by = body.str("by", alert->assignee);
    if (by.empty()) return bad_request("缺少处理人 by");
    if (alert->status == "closed") return json(409, "{\"error\":\"预警已关闭，历史不可修改\"}");

    if (action == "follow") {
        if (alert->status == "open") alert->status = "claimed";
        alert->assignee = by;
    } else if (action == "close") {
        if (measures.empty()) return bad_request("未填写干预措施，不得关闭预警");
        alert->status = "closed";
    } else if (action == "not_applicable") {
        if (note.empty()) return bad_request("标记不适用需要填写原因（如转专业、休学）");
        alert->status = "not_applicable";
    } else {
        return bad_request("action 必须是 follow/close/not_applicable");
    }

    alert->interventions.push_back({iso_time(now_epoch()), action, measures, note, by});
    return json(200, alert_json(*alert));
}

// UC-AA-06 跟踪干预记录
Response interventions(const Request& request) {
    const auto it = request.path_params.find("alert_id");
    if (it == request.path_params.end()) return bad_request("缺少 alert_id");
    const Alert* alert = find_alert(parse_id(it->second));
    if (alert == nullptr) return not_found("预警不存在");

    std::string body = "{\"alert_id\":" + std::to_string(alert->id) +
                       ",\"status\":" + quote(alert->status) +
                       ",\"overdue\":" + std::string(overdue(*alert) ? "true" : "false") +
                       ",\"records\":[";
    bool first = true;
    for (const auto& item : alert->interventions) {
        if (!first) body += ',';
        first = false;
        body += "{\"at\":" + quote(item.at) + ",\"action\":" + quote(item.action) +
                ",\"measures\":" + quote(item.measures) + ",\"note\":" + quote(item.note) +
                ",\"by\":" + quote(item.by) + "}";
    }
    body += "]}";
    return json(200, std::move(body));
}

// UC-AA-07 学业趋势
Response student_trend(const Request& request) {
    const auto it = request.path_params.find("student_id");
    if (it == request.path_params.end()) return bad_request("缺少 student_id");
    const Student* student = find_student(it->second);
    if (student == nullptr) return not_found("学生不存在");

    std::string body = "{\"student_id\":" + quote(student->id) +
                       ",\"name\":" + quote(student->name) +
                       ",\"class_name\":" + quote(student->class_name) +
                       ",\"terms\":[";
    bool first = true;
    for (const auto& term : student->trend) {
        if (!first) body += ',';
        first = false;
        body += "{\"term\":" + quote(term.term) + ",\"avg_score\":" + number(term.average) + "}";
    }
    body += "]}";
    return json(200, std::move(body));
}

}  // namespace

void register_academic_alerts(App& app) {
    app.add_module("academic-alerts", "学业分析与预警");

    app.add_module_route("academic-alerts", "GET", "/dashboard", dashboard);
    app.add_module_route("academic-alerts", "GET", "/rules", list_rules);
    app.add_module_route("academic-alerts", "POST", "/rules", create_rule);
    app.add_module_route("academic-alerts", "POST", "/alerts/generate", generate_alerts);
    app.add_module_route("academic-alerts", "GET", "/alerts", list_alerts);
    app.add_module_route("academic-alerts", "POST", "/alerts/:alert_id/claim", claim_alert);
    app.add_module_route("academic-alerts", "POST", "/alerts/:alert_id/handle", handle_alert);
    app.add_module_route("academic-alerts", "GET", "/alerts/:alert_id/interventions", interventions);
    app.add_module_route("academic-alerts", "GET", "/students/:student_id/trend", student_trend);
}

}  // namespace platform
