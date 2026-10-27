#include "alerts.hpp"
#include "json_util.hpp"

#include <map>
#include <string>
#include <utility>
#include <vector>

// ============================================================================
// M4 学业分析与预警 · 接口层（对应 Spring 里的 @RestController）
//
// 每个函数就是一条路由的处理逻辑：读 Request、走领域逻辑、返回 Response。
// 这里只做参数校验与 JSON 组装，数据与判定规则都在 alerts_store.cpp。
// ============================================================================

namespace platform::academic_alerts {

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
// UC-AA-03 配置预警规则
// ---------------------------------------------------------------------------

Response list_rules(const Request&) {
    std::string body = "{\"total\":" + std::to_string(rules().size()) + ",\"rules\":[";
    bool first = true;
    for (const auto& rule : rules()) {
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

Response create_rule(const Request& request) {
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
    rule.id = "R" + std::to_string(rules().size() + 1);
    rule.name = body.str("name", metric + " 规则");
    rule.metric = metric;
    rule.op = op;
    rule.threshold = body.num("threshold");
    rule.level = level;
    rule.notify = body.str("notify", "辅导员");
    rule.enabled = true;
    rules().push_back(rule);
    return json(201, "{\"id\":" + quote(rule.id) + ",\"created\":true}");
}

// ---------------------------------------------------------------------------
// UC-AA-01 / UC-AA-02 学业看板与群体分析
// ---------------------------------------------------------------------------

Response dashboard(const Request& request) {
    const auto query = parse_query(request.query);
    const std::string group = query.count("group") ? query.at("group") : "class";
    const std::string filter_class = query.count("class") ? query.at("class") : "";
    const std::string filter_major = query.count("major") ? query.at("major") : "";

    std::vector<const Student*> selected;
    for (const auto& student : students()) {
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
        const std::string& key = group == "major"   ? student->major
                                 : group == "grade" ? student->grade
                                                    : student->class_name;
        ++buckets[key];
    }
    for (const auto& alert : alerts()) {
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

// ---------------------------------------------------------------------------
// UC-AA-04 生成 / 查看预警名单
// ---------------------------------------------------------------------------

Response generate_alerts(const Request&) {
    int generated = 0;
    int skipped = 0;
    const std::int64_t now = now_epoch();
    for (const auto& rule : rules()) {
        if (!rule.enabled) continue;
        for (const auto& student : students()) {
            double value = 0.0;
            if (!hit(rule, student, &value)) continue;

            bool already = false;
            for (const auto& alert : alerts()) {
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
            alert.id = next_alert_id();
            alert.student_id = student.id;
            alert.rule_id = rule.id;
            alert.level = rule.level;
            alert.status = "open";
            alert.created = now;
            alert.metric_value = value;
            alerts().push_back(std::move(alert));
            ++generated;
        }
    }
    return json(200, "{\"generated\":" + std::to_string(generated) +
                         ",\"skipped\":" + std::to_string(skipped) +
                         ",\"total_alerts\":" + std::to_string(alerts().size()) + "}");
}

Response list_alerts(const Request& request) {
    const auto query = parse_query(request.query);
    const std::string status = query.count("status") ? query.at("status") : "";
    const std::string level = query.count("level") ? query.at("level") : "";
    const std::string class_name = query.count("class") ? query.at("class") : "";

    std::string body = "{\"alerts\":[";
    bool first = true;
    int total = 0;
    int overdue_count = 0;
    for (const auto& alert : alerts()) {
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

// ---------------------------------------------------------------------------
// UC-AA-05 认领 / 处置预警
// ---------------------------------------------------------------------------

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
        return json(409,
                    "{\"error\":\"该预警已被 " + json_escape(alert->assignee) + " 认领（越权）\"}");

    alert->assignee = assignee;
    alert->status = "claimed";
    alert->interventions.push_back({iso_time(now_epoch()), "claim", "", "认领预警", assignee});
    return json(200, alert_json(*alert));
}

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

// ---------------------------------------------------------------------------
// UC-AA-06 跟踪干预记录
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// UC-AA-07 学业趋势
// ---------------------------------------------------------------------------

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

}  // namespace platform::academic_alerts
