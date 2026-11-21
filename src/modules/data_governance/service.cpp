#include "modules/data_governance/service.hpp"

#include "modules/student_profile/model.hpp"
#include "platform/api.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace data_governance {
namespace {

bool is_one_of(const std::string& value, std::initializer_list<const char*> options) {
    for (const char* option : options) {
        if (value == option) return true;
    }
    return false;
}

std::string trim_copy(std::string value) {
    const auto not_space = [](unsigned char c) { return std::isspace(c) == 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

double round1(double value) { return std::round(value * 10.0) / 10.0; }

// 指标口径只支持一组固定聚合，避免把任意表达式当 SQL/脚本执行。
struct Aggregate {
    enum class Kind { Average, Sum, Count, Maximum, Minimum, PassRate, CountIf, Ratio };
    Kind kind = Kind::Average;
    std::string field;
    double threshold = 60.0;   // pass_rate / count_if 的阈值
    std::string compare_op;    // count_if 的比较符
    double divisor = 1.0;      // ratio 的分母
};

double parse_number(const std::string& text, const std::string& formula) {
    char* end = nullptr;
    errno = 0;
    const double value = std::strtod(text.c_str(), &end);
    if (text.empty() || end != text.c_str() + text.size() || errno == ERANGE)
        throw platform::BadRequest("metric formula argument must be a number: " + formula);
    return value;
}

bool compare(double value, const std::string& op, double threshold) {
    if (op == "lt") return value < threshold;
    if (op == "lte") return value <= threshold;
    if (op == "gt") return value > threshold;
    if (op == "gte") return value >= threshold;
    if (op == "==") return std::fabs(value - threshold) < 1e-9;
    if (op == "!=") return std::fabs(value - threshold) >= 1e-9;
    return false;
}

Aggregate parse_formula(const std::string& raw) {
    const std::string formula = trim_copy(raw);
    const auto open = formula.find('(');
    const auto close = formula.rfind(')');
    if (open == std::string::npos || close == std::string::npos || close < open)
        throw platform::BadRequest("unsupported metric formula: " + raw);

    const std::string name = lower_copy(trim_copy(formula.substr(0, open)));
    const std::string args = trim_copy(formula.substr(open + 1, close - open - 1));
    std::vector<std::string> parts;
    std::string current;
    for (char c : args) {
        if (c == ',') {
            parts.push_back(trim_copy(current));
            current.clear();
        } else {
            current += c;
        }
    }
    if (!args.empty()) parts.push_back(trim_copy(current));
    if (parts.empty() || parts.front().empty())
        throw platform::BadRequest("metric formula requires a field: " + raw);

    Aggregate aggregate;
    aggregate.field = parts.front();
    if (name == "avg" || name == "average") {
        aggregate.kind = Aggregate::Kind::Average;
    } else if (name == "sum") {
        aggregate.kind = Aggregate::Kind::Sum;
    } else if (name == "count") {
        aggregate.kind = Aggregate::Kind::Count;
    } else if (name == "max") {
        aggregate.kind = Aggregate::Kind::Maximum;
    } else if (name == "min") {
        aggregate.kind = Aggregate::Kind::Minimum;
    } else if (name == "pass_rate") {
        aggregate.kind = Aggregate::Kind::PassRate;
        if (parts.size() > 1 && !parts[1].empty())
            aggregate.threshold = parse_number(parts[1], raw);
    } else if (name == "count_if") {
        aggregate.kind = Aggregate::Kind::CountIf;
        if (parts.size() < 3)
            throw platform::BadRequest("count_if requires field, operator and threshold: " + raw);
        const std::string op = lower_copy(parts[1]);
        if (!is_one_of(op, {"lt", "lte", "gt", "gte", "==", "!="}))
            throw platform::BadRequest("count_if operator must be lt/lte/gt/gte/==/!=: " + raw);
        aggregate.compare_op = op;
        aggregate.threshold = parse_number(parts[2], raw);
    } else if (name == "ratio") {
        aggregate.kind = Aggregate::Kind::Ratio;
        if (parts.size() < 2)
            throw platform::BadRequest("ratio requires field and divisor: " + raw);
        aggregate.divisor = parse_number(parts[1], raw);
        if (aggregate.divisor == 0.0)
            throw platform::BadRequest("ratio divisor must not be zero: " + raw);
    } else {
        throw platform::BadRequest("unsupported metric formula function: " + name);
    }
    return aggregate;
}

bool number_from_json(const platform::Json& value, double& number) {
    if (value.is_number()) {
        number = value.as_double();
        return true;
    }
    if (value.is_bool()) {
        number = value.as_bool() ? 1.0 : 0.0;
        return true;
    }
    if (value.is_string()) {
        const std::string& text = value.as_string();
        if (text.empty()) return false;
        char* end = nullptr;
        errno = 0;
        const double parsed = std::strtod(text.c_str(), &end);
        if (end == text.c_str() + text.size() && errno != ERANGE) {
            number = parsed;
            return true;
        }
    }
    return false;
}

std::optional<double> field_number(const platform::Json& row, const std::string& field) {
    if (!row.is_object() || !row.contains(field)) return std::nullopt;
    double number = 0.0;
    if (!number_from_json(row.at(field), number)) return std::nullopt;
    return number;
}

std::optional<double> compute(const Aggregate& aggregate,
                              const std::vector<platform::Json>& rows) {
    std::vector<double> values;
    values.reserve(rows.size());
    for (const auto& row : rows) {
        const auto value = field_number(row, aggregate.field);
        if (value) values.push_back(*value);
    }
    if (values.empty()) return std::nullopt;

    switch (aggregate.kind) {
        case Aggregate::Kind::Count:
            return static_cast<double>(values.size());
        case Aggregate::Kind::Sum:
            return std::accumulate(values.begin(), values.end(), 0.0);
        case Aggregate::Kind::Maximum:
            return *std::max_element(values.begin(), values.end());
        case Aggregate::Kind::Minimum:
            return *std::min_element(values.begin(), values.end());
        case Aggregate::Kind::PassRate: {
            const auto passed = std::count_if(values.begin(), values.end(), [&](double value) {
                return value >= aggregate.threshold;
            });
            return static_cast<double>(passed) * 100.0 / static_cast<double>(values.size());
        }
        case Aggregate::Kind::CountIf: {
            const auto hits = std::count_if(values.begin(), values.end(), [&](double value) {
                return compare(value, aggregate.compare_op, aggregate.threshold);
            });
            return static_cast<double>(hits);
        }
        case Aggregate::Kind::Ratio:
            return std::accumulate(values.begin(), values.end(), 0.0) / aggregate.divisor;
        case Aggregate::Kind::Average:
        default:
            return std::accumulate(values.begin(), values.end(), 0.0) /
                   static_cast<double>(values.size());
    }
}

// 原始记录用 student_id / student_no / 学号 关联到学生。
void collect_student_keys(const platform::Json& payload, std::set<std::string>& keys) {
    if (!payload.is_object()) return;
    for (const char* name : {"student_id", "student_no", "学号"}) {
        if (!payload.contains(name)) continue;
        const auto& value = payload.at(name);
        if (value.is_string() && !value.as_string().empty())
            keys.insert(value.as_string());
        else if (value.is_number())
            keys.insert(std::to_string(value.as_int64()));
    }
}

std::optional<platform::Json> parse_payload(const std::string& text) {
    try {
        return platform::Json::parse(text);
    } catch (const platform::JsonError&) {
        return std::nullopt;
    }
}

// 把 M1 的源字段名（如 xh/cj）按字段映射补成标准字段名（student_id/score），
// 原始 payload 不修改；已有的标准字段优先。
platform::Json standard_view(const platform::Json& payload,
                             const std::vector<FieldRename>& mappings) {
    platform::Json view = payload;
    for (const auto& mapping : mappings) {
        if (mapping.source_field.empty() || mapping.standard_field.empty()) continue;
        if (!view.contains(mapping.source_field)) continue;
        platform::Json value = view.at(mapping.source_field);
        if (value.is_string()) value = platform::Json(trim_copy(value.as_string()));
        if (!view.contains(mapping.standard_field)) view[mapping.standard_field] = value;
    }
    return view;
}

}  // namespace

GovernanceService::GovernanceService(
    std::shared_ptr<platform::Database> database, std::shared_ptr<GovRepository> repository,
    std::shared_ptr<platform::student_profile::ProfileRepository> profile)
    : database_(std::move(database)),
      repository_(std::move(repository)),
      profile_(std::move(profile)) {
    if (!database_ || !repository_ || !profile_)
        throw std::invalid_argument("governance service dependencies");
}

std::vector<QualityRule> GovernanceService::list_quality_rules() const {
    return repository_->list_quality_rules();
}

QualityRule GovernanceService::create_quality_rule(const platform::Json& body) const {
    QualityRule rule;
    rule.rule_name = platform::require_string(body, "rule_name");
    rule.rule_type = platform::require_string(body, "rule_type");
    rule.target_field = platform::require_string(body, "target_field");
    rule.level = platform::optional_string(body, "level", "warn");
    rule.enabled = platform::optional_bool(body, "enabled", true);

    if (!is_one_of(rule.level, {"error", "warn", "info"}))
        throw platform::BadRequest("level must be error, warn or info");
    if (repository_->quality_rule_name_exists(rule.rule_name))
        throw platform::ApiError(409, "quality rule name already exists: " + rule.rule_name);
    return repository_->create_quality_rule(rule);
}

QualityRule GovernanceService::update_quality_rule(std::int64_t rule_id,
                                                   const platform::Json& body) const {
    const auto existing = repository_->find_quality_rule(rule_id);
    if (!existing) throw platform::NotFound("质量规则不存在");

    const bool enabled = platform::optional_bool(body, "enabled", existing->enabled);
    const std::string level = platform::optional_string(body, "level", existing->level);
    if (!is_one_of(level, {"error", "warn", "info"}))
        throw platform::BadRequest("level must be error, warn or info");

    const auto stored = repository_->update_quality_rule(rule_id, enabled, level);
    if (!stored) throw platform::NotFound("质量规则不存在");
    return *stored;
}

platform::Json GovernanceService::list_student_master() const {
    platform::Json rows = platform::Json::array();
    for (const auto& master : repository_->list_student_master())
        rows.push_back(master_to_json(master));
    return rows;
}

platform::Json GovernanceService::get_student_master(const std::string& student_id) const {
    if (student_id.empty()) throw platform::BadRequest("student_id must not be empty");
    const auto master = repository_->find_student_master(student_id);
    if (!master) throw platform::NotFound("学生主数据不存在");
    return master_to_json(*master);
}

void GovernanceService::sync_student(const StudentMaster& master) const {
    platform::student_profile::Student student;
    student.student_id = master.student_id;
    student.name = master.name;
    student.major = master.major;
    student.class_name = master.class_name;
    student.org_id = master.org_id;
    profile_->upsert_student(student);
}

std::vector<std::pair<std::int64_t, platform::Json>>
GovernanceService::normalized_raw_payloads() const {
    const auto mappings = repository_->field_mappings();
    std::vector<std::pair<std::int64_t, platform::Json>> payloads;
    for (const auto& record : repository_->list_valid_raw_records()) {
        const auto parsed = parse_payload(record.payload);
        if (!parsed || !parsed->is_object()) continue;
        const auto mapping = mappings.find(record.data_source_id);
        payloads.emplace_back(
            record.id, mapping == mappings.end() ? *parsed : standard_view(*parsed, mapping->second));
    }
    return payloads;
}

platform::Json GovernanceService::merge_student_master(const platform::Json& body) const {
    const std::string student_id = platform::require_string(body, "student_id");

    StudentMaster incoming;
    incoming.student_no = platform::optional_string(body, "student_no");
    incoming.name = platform::optional_string(body, "name");
    incoming.major = platform::optional_string(body, "major");
    incoming.class_name = platform::optional_string(body, "class_name");
    if (incoming.class_name.empty())
        incoming.class_name = platform::optional_string(body, "class");
    incoming.org_id = platform::optional_string(body, "org_id");
    incoming.status = platform::optional_string(body, "status");
    incoming.source = platform::optional_string(body, "source", "manual");
    if (incoming.source.empty()) incoming.source = "manual";

    // 归并：已有主数据优先，只有新来源提供非空值时才覆盖。
    StudentMaster merged;
    if (const auto existing = repository_->find_student_master(student_id)) merged = *existing;
    merged.student_id = student_id;
    const auto fill = [](std::string& target, const std::string& value) {
        if (!value.empty()) target = value;
    };
    fill(merged.student_no, incoming.student_no);
    fill(merged.name, incoming.name);
    fill(merged.major, incoming.major);
    fill(merged.class_name, incoming.class_name);
    fill(merged.org_id, incoming.org_id);
    fill(merged.status, incoming.status);
    fill(merged.source, incoming.source);
    if (merged.status.empty()) merged.status = "active";
    if (merged.source.empty()) merged.source = "manual";

    const auto stored = repository_->upsert_student_master(merged);
    sync_student(stored);

    platform::Json result = master_to_json(stored);
    result["merged"] = true;
    result["synced_to_profile"] = true;
    return result;
}

platform::Json GovernanceService::recalculate(const platform::Json& body) const {
    const std::string requested = platform::optional_string(body, "term");
    const std::string term =
        requested.empty() ? repository_->latest_metric_term() : requested;

    std::vector<std::pair<MetricDefinition, Aggregate>> metrics;
    for (const auto& definition : repository_->list_metric_definitions(term))
        metrics.emplace_back(definition, parse_formula(definition.formula));
    if (metrics.empty())
        throw platform::BadRequest("no active metric definitions configured");

    std::vector<StudentMaster> students;
    if (body.is_object() && body.contains("student_ids")) {
        const auto& ids = body.at("student_ids");
        if (!ids.is_array()) throw platform::BadRequest("student_ids must be an array");
        for (const auto& item : ids.as_array()) {
            if (!item.is_string() || item.as_string().empty())
                throw platform::BadRequest("student_ids must contain non-empty strings");
            const auto master = repository_->find_student_master(item.as_string());
            if (!master) throw platform::NotFound("学生主数据不存在: " + item.as_string());
            students.push_back(*master);
        }
    } else {
        students = repository_->list_student_master();
    }

    // 原始记录只读一次，按学生建立索引；同一学生可能同时命中 student_id 和
    // student_no 两个键，用记录 id 去重，避免重复计数。
    std::map<std::string, std::map<std::int64_t, platform::Json>> rows_by_student;
    for (const auto& [record_id, payload] : normalized_raw_payloads()) {
        std::set<std::string> keys;
        collect_student_keys(payload, keys);
        for (const auto& key : keys) rows_by_student[key].emplace(record_id, payload);
    }

    const auto task_id = repository_->create_recalc_task(term);
    std::int64_t metric_count = 0;
    platform::Json results = platform::Json::array();
    for (const auto& master : students) {
        sync_student(master);

        std::map<std::int64_t, platform::Json> selected;
        if (const auto by_id = rows_by_student.find(master.student_id);
            by_id != rows_by_student.end())
            selected.insert(by_id->second.begin(), by_id->second.end());
        if (!master.student_no.empty()) {
            if (const auto by_no = rows_by_student.find(master.student_no);
                by_no != rows_by_student.end())
                selected.insert(by_no->second.begin(), by_no->second.end());
        }
        std::vector<platform::Json> rows;
        rows.reserve(selected.size());
        for (const auto& [record_id, payload] : selected) {
            (void)record_id;
            rows.push_back(payload);
        }

        platform::Json values = platform::Json::object();
        for (const auto& [definition, aggregate] : metrics) {
            const auto value = compute(aggregate, rows);
            if (!value) continue;

            platform::student_profile::MetricValue metric;
            metric.student_id = master.student_id;
            metric.metric_code = definition.metric_code;
            metric.term = term;
            metric.value = *value;
            metric.source = "governance";
            profile_->upsert_metric(metric);

            values[definition.metric_code] = *value;
            ++metric_count;
        }

        platform::Json item = platform::Json::object();
        item["student_id"] = master.student_id;
        item["student_no"] = master.student_no;
        item["metrics"] = values;
        results.push_back(item);
    }
    repository_->finish_recalc_task(task_id, static_cast<std::int64_t>(students.size()),
                                    metric_count);

    platform::Json output = platform::Json::object();
    output["task_id"] = "recalc_" + std::to_string(task_id);
    output["status"] = "finished";
    output["term"] = term;
    output["student_count"] = static_cast<std::int64_t>(students.size());
    output["metric_count"] = metric_count;
    output["results"] = results;
    return output;
}

platform::Json GovernanceService::quality_report() const {
    const auto raw = repository_->raw_record_stats();
    QualityStats stats;
    stats.total_records = raw.total;
    stats.cleaned_records = raw.cleaned;
    stats.dirty_records = raw.total - raw.cleaned;
    stats.master_students = repository_->count_student_master();
    stats.last_clean_time = repository_->latest_merge_time();

    std::set<std::string> master_keys;
    for (const auto& master : repository_->list_student_master()) {
        if (!master.student_id.empty()) master_keys.insert(master.student_id);
        if (!master.student_no.empty()) master_keys.insert(master.student_no);
    }

    std::set<std::string> raw_keys;
    for (const auto& [record_id, payload] : normalized_raw_payloads()) {
        (void)record_id;
        collect_student_keys(payload, raw_keys);
    }
    for (const auto& key : raw_keys) {
        if (master_keys.count(key) != 0) ++stats.covered_students;
    }

    stats.quality_score =
        raw.total > 0 ? round1(static_cast<double>(raw.cleaned) * 100.0 /
                               static_cast<double>(raw.total))
                      : 0.0;
    stats.main_data_coverage =
        raw_keys.empty() ? 0.0
                         : round1(static_cast<double>(stats.covered_students) * 100.0 /
                                  static_cast<double>(raw_keys.size()));

    platform::Json report = platform::Json::object();
    report["total_records"] = stats.total_records;
    report["cleaned_records"] = stats.cleaned_records;
    report["dirty_records"] = stats.dirty_records;
    report["quality_score"] = stats.quality_score;
    report["main_data_coverage"] = stats.main_data_coverage;
    report["master_students"] = stats.master_students;
    report["covered_students"] = stats.covered_students;
    report["last_clean_time"] = stats.last_clean_time;
    return report;
}

}  // namespace data_governance
