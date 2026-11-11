#include "modules/student_profile/service.hpp"

#include "modules/student_profile/serialize.hpp"
#include "platform/api.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace platform::student_profile {
namespace {

std::string html_escape(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out += c;
        }
    }
    return out;
}

Json rule_from_body(const Json& body) {
    if (!body.contains("rule") || body.at("rule").is_null()) return Json::object();
    const auto& value = body.at("rule");
    Json rule = value.is_string() ? Json::parse(value.as_string()) : value;
    if (!rule.is_object()) throw BadRequest("rule must be a JSON object");
    if (rule.as_object().empty()) return rule;

    if (!rule.contains("metric") || !rule.at("metric").is_string() ||
        rule.at("metric").as_string().empty())
        throw BadRequest("rule.metric must be a non-empty string");

    const std::string op =
        rule.contains("operator") && rule.at("operator").is_string()
            ? rule.at("operator").as_string()
            : ">=";
    if (op == "between") {
        if (!rule.contains("min") || !rule.at("min").is_number() || !rule.contains("max") ||
            !rule.at("max").is_number())
            throw BadRequest("between rule requires numeric min and max");
    } else if (op == ">=" || op == ">" || op == "<=" || op == "<" || op == "==" || op == "!=") {
        if (!rule.contains("value") || !rule.at("value").is_number())
            throw BadRequest("rule.value must be a number");
    } else {
        throw BadRequest("unsupported rule.operator: " + op);
    }
    return rule;
}

bool evaluate_rule(const Json& rule, const std::map<std::string, MetricValue>& metrics,
                   std::string& metric_code, double& metric_value) {
    if (!rule.is_object() || !rule.contains("metric") || !rule.at("metric").is_string())
        return false;
    metric_code = rule.at("metric").as_string();
    const auto it = metrics.find(metric_code);
    if (it == metrics.end()) return false;
    metric_value = it->second.value;

    const std::string op =
        rule.contains("operator") && rule.at("operator").is_string()
            ? rule.at("operator").as_string()
            : ">=";
    if (op == "between") {
        if (!rule.contains("min") || !rule.contains("max")) return false;
        if (!rule.at("min").is_number() || !rule.at("max").is_number()) return false;
        return metric_value >= rule.at("min").as_double() &&
               metric_value <= rule.at("max").as_double();
    }
    if (!rule.contains("value") || !rule.at("value").is_number()) return false;
    const double threshold = rule.at("value").as_double();
    if (op == ">=") return metric_value >= threshold;
    if (op == ">") return metric_value > threshold;
    if (op == "<=") return metric_value <= threshold;
    if (op == "<") return metric_value < threshold;
    if (op == "==") return std::fabs(metric_value - threshold) < 1e-9;
    if (op == "!=") return std::fabs(metric_value - threshold) >= 1e-9;
    return false;
}

std::set<std::string> tag_code_set(const std::vector<StudentTag>& tags) {
    std::set<std::string> codes;
    for (const auto& tag : tags) codes.insert(tag.tag_code);
    return codes;
}

}  // namespace

ProfileService::ProfileService(std::shared_ptr<Database> database,
                               std::shared_ptr<ProfileRepository> repository)
    : database_(std::move(database)), repository_(std::move(repository)) {
    if (!database_ || !repository_) throw std::invalid_argument("profile service dependencies");
}

Student ProfileService::sync_student(const Json& body) {
    Student student;
    student.student_id = require_string(body, "student_id");
    student.name = require_string(body, "name");
    student.major = optional_string(body, "major");
    student.class_name = optional_string(body, "class_name");
    student.grade = static_cast<int>(optional_int64(body, "grade", 0));
    repository_->upsert_student(student);
    return get_student(student.student_id);
}

std::vector<Student> ProfileService::list_students(const std::string& major,
                                                   const std::string& class_name) const {
    std::vector<Student> result;
    for (const auto& student : repository_->list_students()) {
        if (!major.empty() && student.major != major) continue;
        if (!class_name.empty() && student.class_name != class_name) continue;
        result.push_back(student);
    }
    return result;
}

Student ProfileService::get_student(const std::string& student_id) const {
    const auto student = repository_->find_student(student_id);
    if (!student) throw NotFound("student not found: " + student_id);
    return *student;
}

MetricValue ProfileService::sync_metric(const Json& body) {
    MetricValue metric;
    metric.student_id = require_string(body, "student_id");
    get_student(metric.student_id);
    metric.metric_code = require_string(body, "metric_code");
    metric.term = require_string(body, "term");
    metric.value = require_number(body, "value");
    metric.unit = optional_string(body, "unit");
    metric.source = optional_string(body, "source");
    repository_->upsert_metric(metric);

    for (const auto& stored : repository_->list_metrics(metric.student_id, metric.term)) {
        if (stored.metric_code == metric.metric_code) return stored;
    }
    return metric;
}

std::vector<MetricValue> ProfileService::list_metrics(const std::string& student_id,
                                                      const std::string& term) const {
    get_student(student_id);
    return repository_->list_metrics(student_id, term);
}

Tag ProfileService::upsert_tag(const Json& body) {
    Tag tag;
    tag.code = require_string(body, "code");
    tag.name = require_string(body, "name");
    tag.dimension = require_string(body, "dimension");
    tag.description = optional_string(body, "description");
    tag.rule = rule_from_body(body).dump();
    repository_->upsert_tag(tag);
    const auto stored = repository_->find_tag(tag.code);
    if (!stored) throw NotFound("tag not found: " + tag.code);
    return *stored;
}

std::vector<Tag> ProfileService::list_tags() const { return repository_->list_tags(); }

std::string ProfileService::resolve_term(const std::string& student_id,
                                         const std::string& term) const {
    if (!term.empty()) return term;
    std::string latest;
    for (const auto& metric : repository_->list_metrics(student_id, "")) {
        if (metric.term > latest) latest = metric.term;
    }
    return latest;
}

Json ProfileService::build_profile(const Student& student, const std::string& term) const {
    Json profile = Json::object();
    profile["student"] = to_json(student);
    profile["term"] = term;

    Json metrics = Json::array();
    for (const auto& metric : repository_->list_metrics(student.student_id, term))
        metrics.push_back(to_json(metric));
    profile["metrics"] = metrics;

    std::map<std::string, Tag> definitions;
    for (const auto& tag : repository_->list_tags()) definitions.emplace(tag.code, tag);

    Json tags = Json::array();
    for (const auto& student_tag :
         repository_->list_student_tags(student.student_id, term)) {
        Json item = Json::object();
        item["code"] = student_tag.tag_code;
        const auto definition = definitions.find(student_tag.tag_code);
        if (definition != definitions.end()) {
            item["name"] = definition->second.name;
            item["dimension"] = definition->second.dimension;
            item["description"] = definition->second.description;
            item["rule"] = parse_json_or(definition->second.rule, Json::object());
        } else {
            item["name"] = "";
            item["dimension"] = "";
            item["description"] = "";
            item["rule"] = Json::object();
        }
        item["evidence"] = parse_json_or(student_tag.evidence, Json::object());
        item["calculated_at"] = student_tag.calculated_at;
        tags.push_back(item);
    }
    profile["tags"] = tags;

    profile["snapshot"] = Json(nullptr);
    if (!term.empty()) {
        const auto snapshot = repository_->find_snapshot(student.student_id, term);
        if (snapshot) profile["snapshot"] = to_json(*snapshot);
    }
    return profile;
}

Json ProfileService::get_profile(const std::string& student_id, const std::string& term) const {
    const auto student = repository_->find_student(student_id);
    if (!student) throw NotFound("student not found: " + student_id);
    return build_profile(*student, resolve_term(student_id, term));
}

Json ProfileService::get_tag_evidence(const std::string& student_id,
                                      const std::string& tag_code,
                                      const std::string& term) const {
    const auto student = get_student(student_id);
    const std::string resolved = resolve_term(student_id, term);
    for (const auto& tag : repository_->list_student_tags(student_id, resolved)) {
        if (tag.tag_code != tag_code) continue;
        Json result = Json::object();
        result["student"] = to_json(student);
        const auto definition = repository_->find_tag(tag_code);
        result["tag"] = definition ? to_json(*definition) : Json(nullptr);
        result["evidence"] = parse_json_or(tag.evidence, Json::object());
        result["calculated_at"] = tag.calculated_at;
        return result;
    }
    throw NotFound("tag evidence not found: " + tag_code);
}

Json ProfileService::compare(const std::string& student_id, const std::string& peer_id,
                             const std::string& term) const {
    if (peer_id.empty()) throw BadRequest("peer query parameter is required");
    if (peer_id == student_id) throw BadRequest("peer must be different from student_id");

    const auto student = get_student(student_id);
    const auto peer = get_student(peer_id);
    const std::string resolved = term.empty() ? resolve_term(student_id, "") : term;

    std::map<std::string, double> student_metrics;
    std::map<std::string, double> peer_metrics;
    for (const auto& metric : repository_->list_metrics(student_id, resolved))
        student_metrics[metric.metric_code] = metric.value;
    for (const auto& metric : repository_->list_metrics(peer_id, resolved))
        peer_metrics[metric.metric_code] = metric.value;

    std::set<std::string> codes;
    for (const auto& [code, value] : student_metrics) codes.insert(code);
    for (const auto& [code, value] : peer_metrics) codes.insert(code);

    Json metrics = Json::array();
    for (const auto& code : codes) {
        Json item = Json::object();
        item["metric_code"] = code;
        const auto left = student_metrics.find(code);
        const auto right = peer_metrics.find(code);
        item["student_value"] = left == student_metrics.end() ? Json(nullptr) : Json(left->second);
        item["peer_value"] = right == peer_metrics.end() ? Json(nullptr) : Json(right->second);
        item["delta"] = (left != student_metrics.end() && right != peer_metrics.end())
                            ? Json(left->second - right->second)
                            : Json(nullptr);
        metrics.push_back(item);
    }

    const auto student_tags =
        tag_code_set(repository_->list_student_tags(student_id, resolved));
    const auto peer_tags = tag_code_set(repository_->list_student_tags(peer_id, resolved));
    Json only_student = Json::array();
    Json only_peer = Json::array();
    Json common = Json::array();
    for (const auto& code : student_tags) {
        if (peer_tags.count(code) == 0)
            only_student.push_back(code);
        else
            common.push_back(code);
    }
    for (const auto& code : peer_tags) {
        if (student_tags.count(code) == 0) only_peer.push_back(code);
    }

    Json tags = Json::object();
    tags["only_student"] = only_student;
    tags["only_peer"] = only_peer;
    tags["common"] = common;

    Json result = Json::object();
    result["term"] = resolved;
    result["student"] = to_json(student);
    result["peer"] = to_json(peer);
    result["metrics"] = metrics;
    result["tags"] = tags;
    return result;
}

Json ProfileService::recalculate(const Json& body) {
    return recalculate_one(require_string(body, "student_id"),
                           require_string(body, "term"));
}

Json ProfileService::recalculate_one(const std::string& student_id,
                                     const std::string& term) {
    const auto student = get_student(student_id);

    std::map<std::string, MetricValue> metrics;
    Json metric_values = Json::object();
    for (const auto& metric : repository_->list_metrics(student_id, term)) {
        metrics.emplace(metric.metric_code, metric);
        metric_values[metric.metric_code] = metric.value;
    }

    std::vector<StudentTag> student_tags;
    Json tag_codes = Json::array();
    for (const auto& tag : repository_->list_tags()) {
        const Json rule = parse_json_or(tag.rule, Json::object());
        if (!rule.is_object() || rule.as_object().empty()) continue;

        std::string metric_code;
        double metric_value = 0.0;
        if (!evaluate_rule(rule, metrics, metric_code, metric_value)) continue;

        Json evidence = Json::object();
        evidence["rule"] = rule;
        evidence["metric"] = metric_code;
        evidence["value"] = metric_value;
        evidence["term"] = term;
        const auto metric = metrics.find(metric_code);
        if (metric != metrics.end()) evidence["source"] = metric->second.source;

        StudentTag student_tag;
        student_tag.student_id = student_id;
        student_tag.tag_code = tag.code;
        student_tag.evidence = evidence.dump();
        student_tags.push_back(std::move(student_tag));
        tag_codes.push_back(tag.code);
    }

    repository_->replace_student_tags(student_id, term, student_tags);

    Json snapshot_payload = Json::object();
    snapshot_payload["student_id"] = student_id;
    snapshot_payload["term"] = term;
    snapshot_payload["metrics"] = metric_values;
    snapshot_payload["tags"] = tag_codes;

    ProfileSnapshot snapshot;
    snapshot.student_id = student_id;
    snapshot.term = term;
    snapshot.payload = snapshot_payload.dump();
    repository_->save_snapshot(snapshot);

    return build_profile(student, term);
}

Json ProfileService::recalculate_batch(const Json& body) {
    const std::string term = require_string(body, "term");
    std::vector<std::string> ids;
    if (body.contains("student_ids")) {
        const auto& value = body.at("student_ids");
        if (!value.is_array()) throw BadRequest("student_ids must be an array");
        for (const auto& item : value.as_array()) {
            if (!item.is_string() || item.as_string().empty())
                throw BadRequest("student_ids must contain non-empty strings");
            ids.push_back(item.as_string());
        }
    } else {
        for (const auto& student : repository_->list_students())
            ids.push_back(student.student_id);
    }

    Json students = Json::array();
    for (const auto& id : ids) {
        const Json profile = recalculate_one(id, term);
        Json item = Json::object();
        item["student_id"] = id;
        item["metric_count"] =
            static_cast<std::int64_t>(profile.at("metrics").as_array().size());
        item["tag_count"] = static_cast<std::int64_t>(profile.at("tags").as_array().size());
        Json codes = Json::array();
        for (const auto& tag : profile.at("tags").as_array()) codes.push_back(tag.at("code"));
        item["tags"] = codes;
        students.push_back(item);
    }

    Json result = Json::object();
    result["term"] = term;
    result["count"] = static_cast<std::int64_t>(students.as_array().size());
    result["students"] = students;
    return result;
}

std::string ProfileService::report_html(const std::string& student_id,
                                        const std::string& term) const {
    const Json profile = get_profile(student_id, term);
    const auto& student = profile.at("student");

    std::string html = "<!doctype html><html lang=\"zh-CN\"><head><meta charset=\"utf-8\">";
    html += "<title>学生画像报告</title><style>";
    html += "body{font-family:system-ui,'Microsoft YaHei',sans-serif;margin:32px;color:#222}";
    html += "h1{font-size:24px}h2{font-size:18px;margin-top:28px}";
    html += "table{border-collapse:collapse;width:100%;margin-top:8px}";
    html += "th,td{border:1px solid #ccc;padding:6px 8px;text-align:left}";
    html += "th{background:#f3f5f7}.muted{color:#666}";
    html += "</style></head><body>";

    html += "<h1>" + html_escape(student.at("name").as_string()) + " 的成长画像</h1>";
    html += "<p class=\"muted\">学号：" + html_escape(student.at("student_id").as_string());
    html += "　专业：" + html_escape(student.at("major").as_string());
    html += "　班级：" + html_escape(student.at("class_name").as_string());
    html += "　学期：" + html_escape(profile.at("term").as_string()) + "</p>";

    html += "<h2>指标</h2>";
    const auto& metrics = profile.at("metrics");
    if (metrics.as_array().empty()) {
        html += "<p>暂无指标数据。</p>";
    } else {
        html += "<table><tr><th>指标</th><th>数值</th><th>单位</th><th>来源</th></tr>";
        for (const auto& metric : metrics.as_array()) {
            html += "<tr><td>" + html_escape(metric.at("metric_code").as_string()) + "</td>";
            html += "<td>" + metric.at("value").dump() + "</td>";
            html += "<td>" + html_escape(metric.at("unit").as_string()) + "</td>";
            html += "<td>" + html_escape(metric.at("source").as_string()) + "</td></tr>";
        }
        html += "</table>";
    }

    html += "<h2>标签</h2>";
    const auto& tags = profile.at("tags");
    if (tags.as_array().empty()) {
        html += "<p>暂无标签，请先执行画像重算。</p>";
    } else {
        html += "<ul>";
        for (const auto& tag : tags.as_array()) {
            html += "<li><strong>" + html_escape(tag.at("name").as_string()) + "</strong>";
            html += "（" + html_escape(tag.at("dimension").as_string()) + "）<br>";
            html += "<span class=\"muted\">依据：" +
                    html_escape(tag.at("evidence").dump()) + "</span></li>";
        }
        html += "</ul>";
    }
    html += "</body></html>";
    return html;
}

}  // namespace platform::student_profile
