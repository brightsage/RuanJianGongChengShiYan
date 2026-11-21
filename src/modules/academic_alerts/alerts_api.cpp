#include "alerts.hpp"

#include "json_util.hpp"
#include "repository.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

// ============================================================================
// M4 学业分析与预警 · 接口层 + 领域判定
//
// 数据来自 AlertRepository：
//   - 学生/指标只读 M3 的 sp_student、sp_metric_value（指标由 M2 计算）
//   - 规则/预警/干预读写本模块 aa_* 表
// 这里只做参数校验、数据范围过滤与 JSON 组装。
// ============================================================================

namespace platform::academic_alerts {

// ---------------------------------------------------------------------------
// 指标绑定与领域判定
// ---------------------------------------------------------------------------

const std::vector<MetricBinding>& metric_bindings() {
    static const std::vector<MetricBinding> bindings = {
        {"avg_score", "score_avg", 1.0},
        {"failed_courses", "failed_courses", 1.0},
        {"credit_rate", "credit_rate", 1.0},
        // M2 的出勤率按百分比存，规则阈值按 0~1 配。
        {"attendance", "attendance_rate", 0.01},
    };
    return bindings;
}

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

bool is_metric(const std::string& metric) {
    for (const auto& binding : metric_bindings())
        if (metric == binding.metric) return true;
    return false;
}

bool is_operator(const std::string& op) {
    return op == "lt" || op == "lte" || op == "gt" || op == "gte";
}

bool is_level(const std::string& level) {
    return level == "red" || level == "orange" || level == "yellow";
}

bool active_status(const std::string& status) {
    return status == "open" || status == "claimed";
}

bool overdue(const Alert& alert) {
    return active_status(alert.status) && now_epoch() - alert.created > 48 * 3600;
}

double metric_value(const Student& student, const std::string& metric) {
    const auto it = student.metrics.find(metric);
    return it == student.metrics.end() ? 0.0 : it->second;
}

bool hit(const Rule& rule, const Student& student, double* value) {
    const auto it = student.metrics.find(rule.metric);
    if (it == student.metrics.end()) return false;  // 没有指标就不判定
    const double actual = it->second;
    if (value != nullptr) *value = actual;
    if (rule.op == "lt") return actual < rule.threshold;
    if (rule.op == "lte") return actual <= rule.threshold;
    if (rule.op == "gt") return actual > rule.threshold;
    if (rule.op == "gte") return actual >= rule.threshold;
    return false;
}

std::int64_t parse_id(const std::string& text) {
    if (text.empty() || text.size() > 9) return -1;
    for (unsigned char c : text)
        if (!std::isdigit(c)) return -1;
    try {
        return std::stoll(text);
    } catch (const std::exception&) {
        return -1;
    }
}

// ---------------------------------------------------------------------------
// 序列化
// ---------------------------------------------------------------------------

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
    body += ",\"metric_term\":" + quote(alert.metric_term);
    body += ",\"overdue\":" + std::string(overdue(alert) ? "true" : "false");
    if (!alert.student_name.empty()) body += ",\"student_name\":" + quote(alert.student_name);
    if (!alert.class_name.empty()) body += ",\"class_name\":" + quote(alert.class_name);
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
// 内部工具
// ---------------------------------------------------------------------------

namespace {

using platform::security::Principal;
using platform::security::Scope;

std::string rule_json(const Rule& rule) {
    return "{\"id\":" + quote(rule.id) + ",\"name\":" + quote(rule.name) +
           ",\"metric\":" + quote(rule.metric) + ",\"operator\":" + quote(rule.op) +
           ",\"threshold\":" + number(rule.threshold) + ",\"level\":" + quote(rule.level) +
           ",\"notify\":" + quote(rule.notify) +
           ",\"enabled\":" + std::string(rule.enabled ? "true" : "false") + "}";
}

Scope scope_of(const Student& student) {
    return {"self", student.org_id, student.class_name, student.id};
}

bool allowed(const Principal* actor, const Student& student) {
    if (actor == nullptr) return true;
    return platform::security::contains(actor->scope, scope_of(student));
}

/// 把 M2 指标码按绑定关系换算成规则使用的指标名。
void apply_bindings(Student& student, const std::map<std::string, MetricPoint>& metrics) {
    for (const auto& binding : metric_bindings()) {
        if (const auto direct = metrics.find(binding.metric); direct != metrics.end()) {
            student.metrics[binding.metric] = direct->second.value;
            student.metric_terms[binding.metric] = direct->second.term;
            continue;
        }
        if (const auto mapped = metrics.find(binding.code); mapped != metrics.end()) {
            student.metrics[binding.metric] = mapped->second.value * binding.scale;
            student.metric_terms[binding.metric] = mapped->second.term;
        }
    }
}

std::map<std::string, Student> load_students(AlertRepository& repository) {
    const auto metrics = repository.metrics_by_student();
    std::map<std::string, Student> index;
    for (auto student : repository.list_students()) {
        if (const auto it = metrics.find(student.id); it != metrics.end())
            apply_bindings(student, it->second);
        index[student.id] = std::move(student);
    }
    return index;
}

void enrich(Alert& alert, const std::map<std::string, Student>& students) {
    const auto it = students.find(alert.student_id);
    if (it == students.end()) return;
    alert.student_name = it->second.name;
    alert.class_name = it->second.class_name;
    alert.org_id = it->second.org_id;
}

Response forbidden(const std::string& message) {
    return json(403, std::string("{\"error\":") + quote(message) + "}");
}

const char* kAlertIdKey = "alert_id";

}  // namespace

// ---------------------------------------------------------------------------
// UC-AA-03 配置预警规则
// ---------------------------------------------------------------------------

Response list_rules(AlertRepository& repository, const Request&, const Principal*) {
    const auto rules = repository.list_rules();
    std::string body = "{\"total\":" + std::to_string(rules.size()) + ",\"rules\":[";
    bool first = true;
    for (const auto& rule : rules) {
        if (!first) body += ',';
        first = false;
        body += rule_json(rule);
    }
    body += "]}";
    return json(200, std::move(body));
}

Response create_rule(AlertRepository& repository, const Request& request,
                     const Principal*) {
    const JsonBody body(request.body);
    const std::string metric = body.str("metric");
    const std::string op = body.str("operator", "lt");
    const std::string level = body.str("level", "yellow");
    if (!is_metric(metric))
        return bad_request("metric 必须是 avg_score/failed_courses/credit_rate/attendance");
    if (!is_operator(op)) return bad_request("operator 必须是 lt/lte/gt/gte");
    if (!is_level(level)) return bad_request("level 必须是 red/orange/yellow");
    if (!body.has("threshold")) return bad_request("缺少 threshold");

    Rule rule;
    rule.name = body.str("name", metric + " 规则");
    rule.metric = metric;
    rule.op = op;
    rule.threshold = body.num("threshold");
    rule.level = level;
    rule.notify = body.str("notify", "辅导员");
    rule.enabled = true;

    const auto stored = repository.create_rule(rule);
    return json(201, "{\"id\":" + quote(stored.id) + ",\"created\":true}");
}

Response update_rule(AlertRepository& repository, const Request& request, const Principal*) {
    const auto param = request.path_params.find("rule_id");
    if (param == request.path_params.end()) return bad_request("缺少 rule_id");
    const auto rule = repository.find_rule(param->second);
    if (!rule) return not_found("规则不存在");

    const JsonBody body(request.body);
    const std::string level = body.str("level", rule->level);
    if (!is_level(level)) return bad_request("level 必须是 red/orange/yellow");
    const double threshold = body.has("threshold") ? body.num("threshold") : rule->threshold;
    const std::string notify = body.str("notify", rule->notify);
    const bool enabled = body.has("enabled") ? body.str("enabled") == "1" : rule->enabled;

    const auto stored = repository.update_rule(rule->id, enabled, threshold, level, notify);
    if (!stored) return not_found("规则不存在");
    return json(200, rule_json(*stored));
}

// ---------------------------------------------------------------------------
// UC-AA-01 / UC-AA-02 学业看板与群体分析
// ---------------------------------------------------------------------------

Response dashboard(AlertRepository& repository, const Request& request,
                   const Principal* actor) {
    const auto query = parse_query(request.query);
    const std::string group = query.count("group") ? query.at("group") : "class";
    const std::string filter_class = query.count("class") ? query.at("class") : "";
    const std::string filter_major = query.count("major") ? query.at("major") : "";

    const auto students = load_students(repository);
    std::vector<const Student*> selected;
    for (const auto& [id, student] : students) {
        (void)id;
        if (!allowed(actor, student)) continue;
        if (!filter_class.empty() && student.class_name != filter_class) continue;
        if (!filter_major.empty() && student.major != filter_major) continue;
        selected.push_back(&student);
    }
    if (selected.empty()) return not_found("没有符合条件的学生");

    double sum = 0.0;
    int scored = 0;
    int failing = 0;
    std::map<std::string, int> buckets;
    for (const Student* student : selected) {
        if (const auto it = student->metrics.find("avg_score"); it != student->metrics.end()) {
            sum += it->second;
            ++scored;
        }
        if (const auto it = student->metrics.find("failed_courses");
            it != student->metrics.end() && it->second > 0.0)
            ++failing;
        const std::string& key = group == "major"   ? student->major
                                 : group == "grade" ? student->grade
                                                    : student->class_name;
        ++buckets[key];
    }

    int alerts_active = 0;
    int alerts_overdue = 0;
    for (auto alert : repository.list_alerts("", "")) {
        if (!active_status(alert.status)) continue;
        const auto it = students.find(alert.student_id);
        if (it == students.end()) continue;
        if (!allowed(actor, it->second)) continue;
        if (!filter_class.empty() && it->second.class_name != filter_class) continue;
        if (!filter_major.empty() && it->second.major != filter_major) continue;
        ++alerts_active;
        if (overdue(alert)) ++alerts_overdue;
    }

    std::string body = "{";
    body += "\"group_by\":" + quote(group);
    body += ",\"student_count\":" + std::to_string(selected.size());
    body += ",\"avg_score\":" + number(scored == 0 ? 0.0 : sum / static_cast<double>(scored));
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

// ---------------------------------------------------------------------------
// UC-AA-04 生成 / 查看预警名单
// ---------------------------------------------------------------------------

Response generate_alerts(AlertRepository& repository, const Request&,
                         const Principal* actor) {
    const auto students = load_students(repository);
    const auto rules = repository.list_rules();
    const std::int64_t now = now_epoch();
    int generated = 0;
    int skipped = 0;

    for (const auto& rule : rules) {
        if (!rule.enabled) continue;
        for (const auto& [id, student] : students) {
            (void)id;
            if (!allowed(actor, student)) continue;  // 辅导员只生成所辖班级的预警
            double value = 0.0;
            if (!hit(rule, student, &value)) continue;
            if (repository.has_active_alert(student.id, rule.id)) {
                ++skipped;
                continue;
            }

            Alert alert;
            alert.student_id = student.id;
            alert.rule_id = rule.id;
            alert.level = rule.level;
            alert.status = "open";
            alert.created = now;
            alert.metric_value = value;
            const auto term = student.metric_terms.find(rule.metric);
            if (term != student.metric_terms.end()) alert.metric_term = term->second;
            repository.create_alert(alert);
            ++generated;
        }
    }

    return json(200, "{\"generated\":" + std::to_string(generated) +
                         ",\"skipped\":" + std::to_string(skipped) +
                         ",\"total_alerts\":" + std::to_string(repository.count_alerts()) + "}");
}

Response list_alerts(AlertRepository& repository, const Request& request,
                     const Principal* actor) {
    const auto query = parse_query(request.query);
    const std::string status = query.count("status") ? query.at("status") : "";
    const std::string level = query.count("level") ? query.at("level") : "";
    const std::string class_name = query.count("class") ? query.at("class") : "";

    const auto students = load_students(repository);
    std::string body = "{\"alerts\":[";
    bool first = true;
    int total = 0;
    int overdue_count = 0;
    for (auto alert : repository.list_alerts(status, level)) {
        const auto it = students.find(alert.student_id);
        if (it == students.end()) continue;
        if (!allowed(actor, it->second)) continue;
        if (!class_name.empty() && it->second.class_name != class_name) continue;
        enrich(alert, students);
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

// ---------------------------------------------------------------------------
// UC-AA-05 认领 / 处置预警
// ---------------------------------------------------------------------------

Response claim_alert(AlertRepository& repository, const Request& request,
                     const Principal* actor) {
    const auto param = request.path_params.find(kAlertIdKey);
    if (param == request.path_params.end()) return bad_request("缺少 alert_id");
    const auto alert_id = parse_id(param->second);
    const auto stored = repository.find_alert(alert_id);
    if (!stored) return not_found("预警不存在");

    const auto students = load_students(repository);
    const auto student = students.find(stored->student_id);
    if (student != students.end() && !allowed(actor, student->second))
        return forbidden("预警不在当前账号的数据范围内");

    const JsonBody body(request.body);
    const std::string assignee = body.str("assignee");
    if (assignee.empty()) return bad_request("缺少 assignee（认领人）");
    if (stored->status == "closed" || stored->status == "not_applicable")
        return json(409, "{\"error\":\"已关闭或不适用的预警不能再认领\"}");
    if (!stored->assignee.empty() && stored->assignee != assignee)
        return json(409, "{\"error\":\"该预警已被 " + json_escape(stored->assignee) +
                             " 认领（越权）\"}");

    repository.claim(alert_id, assignee);
    Intervention item;
    item.epoch = now_epoch();
    item.at = iso_time(item.epoch);
    item.action = "claim";
    item.note = "认领预警";
    item.by = assignee;
    repository.add_intervention(alert_id, item);

    auto alert = repository.find_alert(alert_id);
    if (!alert) return not_found("预警不存在");
    enrich(*alert, students);
    return json(200, alert_json(*alert));
}

Response handle_alert(AlertRepository& repository, const Request& request,
                      const Principal* actor) {
    const auto param = request.path_params.find(kAlertIdKey);
    if (param == request.path_params.end()) return bad_request("缺少 alert_id");
    const auto alert_id = parse_id(param->second);
    const auto stored = repository.find_alert(alert_id);
    if (!stored) return not_found("预警不存在");

    const auto students = load_students(repository);
    const auto student = students.find(stored->student_id);
    if (student != students.end() && !allowed(actor, student->second))
        return forbidden("预警不在当前账号的数据范围内");

    const JsonBody body(request.body);
    const std::string action = body.str("action", "follow");
    const std::string measures = body.str("measures");
    const std::string note = body.str("note");
    const std::string by = body.str("by", stored->assignee);
    if (by.empty()) return bad_request("缺少处理人 by");
    if (stored->status == "closed") return json(409, "{\"error\":\"预警已关闭，历史不可修改\"}");

    std::string status = stored->status;
    std::string assignee = stored->assignee;
    if (action == "follow") {
        if (status == "open") status = "claimed";
        assignee = by;
    } else if (action == "close") {
        if (measures.empty()) return bad_request("未填写干预措施，不得关闭预警");
        status = "closed";
        assignee = by;
    } else if (action == "not_applicable") {
        if (note.empty()) return bad_request("标记不适用需要填写原因（如转专业、休学）");
        status = "not_applicable";
        assignee = by;
    } else {
        return bad_request("action 必须是 follow/close/not_applicable");
    }

    repository.update_status(alert_id, status, assignee);
    Intervention item;
    item.epoch = now_epoch();
    item.at = iso_time(item.epoch);
    item.action = action;
    item.measures = measures;
    item.note = note;
    item.by = by;
    repository.add_intervention(alert_id, item);

    auto alert = repository.find_alert(alert_id);
    if (!alert) return not_found("预警不存在");
    enrich(*alert, students);
    return json(200, alert_json(*alert));
}

// ---------------------------------------------------------------------------
// UC-AA-06 跟踪干预记录
// ---------------------------------------------------------------------------

Response interventions(AlertRepository& repository, const Request& request,
                       const Principal* actor) {
    const auto param = request.path_params.find(kAlertIdKey);
    if (param == request.path_params.end()) return bad_request("缺少 alert_id");
    const auto alert_id = parse_id(param->second);
    const auto alert = repository.find_alert(alert_id);
    if (!alert) return not_found("预警不存在");

    const auto students = load_students(repository);
    const auto student = students.find(alert->student_id);
    if (student != students.end() && !allowed(actor, student->second))
        return forbidden("预警不在当前账号的数据范围内");

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

// ---------------------------------------------------------------------------
// UC-AA-07 学业趋势
// ---------------------------------------------------------------------------

Response student_trend(AlertRepository& repository, const Request& request,
                       const Principal* actor) {
    const auto param = request.path_params.find("student_id");
    if (param == request.path_params.end()) return bad_request("缺少 student_id");
    const auto students = load_students(repository);
    const auto it = students.find(param->second);
    if (it == students.end()) return not_found("学生不存在");
    if (!allowed(actor, it->second)) return forbidden("学生不在当前账号的数据范围内");

    // 优先 score_avg，没有时退回 avg_score。
    std::map<std::string, double> terms;
    for (const auto& point : repository.metric_history(it->first, {"avg_score"}))
        terms[point.term] = point.value;
    for (const auto& point : repository.metric_history(it->first, {"score_avg"}))
        terms[point.term] = point.value;

    std::string body = "{\"student_id\":" + quote(it->second.id) +
                       ",\"name\":" + quote(it->second.name) +
                       ",\"class_name\":" + quote(it->second.class_name) +
                       ",\"terms\":[";
    bool first = true;
    for (const auto& [term, value] : terms) {
        if (!first) body += ',';
        first = false;
        body += "{\"term\":" + quote(term) + ",\"avg_score\":" + number(value) + "}";
    }
    body += "]}";
    return json(200, std::move(body));
}

}  // namespace platform::academic_alerts
