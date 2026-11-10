#include "recommendations.hpp"

#include <algorithm>
#include <cmath>

namespace platform::career {
namespace {

std::string read_text(const Json& input, const char* key, std::size_t limit) {
    const auto& text = input.at(key).as_string();
    const auto start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos || text.size() > limit || text.find('\0') != std::string::npos) {
        throw CatalogError(400, std::string("字段为空、过长或包含空字符：") + key);
    }
    return text.substr(start, text.find_last_not_of(" \t\r\n") - start + 1);
}

bool allowed(const Json& options, const std::string& value) {
    const auto& values = options.as_array();
    return values.empty() || std::any_of(values.begin(), values.end(), [&](const auto& item) {
        return item.as_string() == value;
    });
}

// The normalization by required skills and descending ranking below are adapted from
// Prachi Jha's MIT-licensed matchingengine.cpp at 56817cb. See third_party/career-matching/LICENSE.
struct Match {
    Json job;
    double score;
    Json criteria;
};

Match calculate_match(const Profile& profile, const Json& job, const Json& strategy) {
    const auto& requirements = job.at("requirements").as_array();
    long double total_weight = 0;
    for (const auto& item : requirements) total_weight += item.at("weight").as_double();

    const long double pass_weight = strategy.at("pass_weight").as_double();
    const long double proximity_weight = strategy.at("proximity_weight").as_double();
    const long double component_weight = pass_weight + proximity_weight;
    long double score = 0;
    Json criteria = Json::array();
    for (const auto& skill : requirements) {
        const auto& code = skill.at("competency_code").as_string();
        const auto student = profile.scores.find(code);
        const double threshold = skill.at("threshold").as_double();
        const double weight = skill.at("weight").as_double();
        const bool missing = student == profile.scores.end();
        const bool passed = !missing && student->second >= threshold;
        const double proximity = missing ? 0 : std::min(student->second / threshold, 1.0);
        const auto contribution = 100 * (weight / total_weight) *
            ((passed ? pass_weight : 0) / component_weight + proximity * (proximity_weight / component_weight));
        score += contribution;
        criteria.push_back(Json::Object{
            {"competency_code", code}, {"threshold", threshold}, {"weight", weight},
            {"student_score", missing ? Json{} : Json(student->second)},
            {"status", missing ? "missing" : (passed ? "passed" : "below_threshold")},
            {"gap", missing ? Json{} : Json(std::max(threshold - student->second, 0.0))},
            {"contribution", static_cast<double>(contribution)},
        });
    }
    const double rounded = static_cast<double>(std::round(score * 100)) / 100;
    return {job, rounded, std::move(criteria)};
}

Json current_feedback(const DbRow& row) {
    if (row.get_string("choice").empty()) return {};
    return Json::Object{{"choice", row.get_string("choice")}, {"reason", row.get_string("reason")},
                        {"updated_at", row.get_string("updated_at")}};
}

}  // namespace

RecommendationFilter validate_filter(const Json& input) {
    for (const auto& [key, value] : input.as_object()) {
        (void)value;
        if (key != "industry" && key != "min_score" && key != "term") {
            throw CatalogError(400, "不支持的筛选字段：" + key);
        }
    }
    RecommendationFilter filter;
    if (input.contains("term")) filter.term = read_text(input, "term", 80);
    if (input.contains("industry")) filter.industry = read_text(input, "industry", 120);
    if (input.contains("min_score")) {
        filter.min_score = input.at("min_score").as_double();
        if (!std::isfinite(filter.min_score) || filter.min_score < 0 || filter.min_score > 100) {
            throw CatalogError(400, "最低匹配分必须在 0 到 100 之间");
        }
    }
    return filter;
}

Json validate_feedback(const Json& input) {
    const auto choice = read_text(input, "choice", 30);
    if (choice != "interested" && choice != "not_interested") {
        throw CatalogError(400, "反馈必须为 interested 或 not_interested");
    }
    return Json::Object{{"choice", choice}, {"reason", read_text(input, "reason", 2000)}};
}

Recommendations::Recommendations(Database& database) : db_(database) {
    db_.exec(R"sql(
        CREATE TABLE IF NOT EXISTS cd_recommendations (
            id INTEGER PRIMARY KEY,
            student_no TEXT NOT NULL,
            job_id INTEGER NOT NULL REFERENCES cd_jobs(id),
            job_version INTEGER NOT NULL,
            strategy_version INTEGER NOT NULL,
            profile_version TEXT NOT NULL,
            snapshot TEXT NOT NULL,
            UNIQUE(student_no, job_id, job_version, strategy_version, profile_version)
        );
        CREATE TABLE IF NOT EXISTS cd_feedback (
            recommendation_id INTEGER PRIMARY KEY REFERENCES cd_recommendations(id),
            choice TEXT NOT NULL CHECK(choice IN ('interested', 'not_interested')),
            reason TEXT NOT NULL,
            updated_at TEXT NOT NULL
        );
    )sql");
}

Json Recommendations::query(Catalog& catalog, const Profile& profile, const RecommendationFilter& filter) {
    const auto strategy = catalog.get_strategy();
    const double min_score = std::max(filter.min_score, strategy.at("min_score").as_double());
    const auto jobs = catalog.list_jobs();
    std::vector<Match> matches;
    for (const auto& job : jobs.as_array()) {
        if (job.at("status").as_string() != "published") continue;
        const auto& industry = job.at("industry").as_string();
        if ((!filter.industry.empty() && industry != filter.industry) ||
            !allowed(strategy.at("allowed_industries"), industry) ||
            !allowed(strategy.at("allowed_job_types"), job.at("job_type").as_string())) continue;
        auto match = calculate_match(profile, job, strategy);
        if (match.score >= min_score) matches.push_back(std::move(match));
    }
    std::sort(matches.begin(), matches.end(), [](const auto& a, const auto& b) {
        if (a.score != b.score) return a.score > b.score;
        return a.job.at("id").as_int64() < b.job.at("id").as_int64();
    });

    Json::Array items;
    for (auto& match : matches) {
        const auto job_id = match.job.at("id").as_int64();
        const auto job_version = match.job.at("version").as_int64();
        const auto strategy_version = strategy.at("version").as_int64();
        Json detail = Json::Object{
            {"job", match.job}, {"score", match.score}, {"criteria", std::move(match.criteria)},
            {"term", profile.term}, {"profile_version", profile.version},
            {"strategy_version", strategy_version}, {"algorithm", strategy.at("algorithm")},
        };
        std::vector<DbValue> key{profile.student_no, job_id, job_version, strategy_version, profile.version};
        auto insert = key;
        insert.emplace_back(detail.dump());
        db_.exec("INSERT INTO cd_recommendations(student_no,job_id,job_version,strategy_version,profile_version,snapshot) "
                 "VALUES (?,?,?,?,?,?) ON CONFLICT(student_no,job_id,job_version,strategy_version,profile_version) "
                 "DO NOTHING", insert);
        const auto rows = db_.query(
            "SELECT r.id,f.choice,f.reason,f.updated_at FROM cd_recommendations r "
            "LEFT JOIN cd_feedback f ON r.id=f.recommendation_id "
            "WHERE r.student_no=? AND r.job_id=? AND r.job_version=? AND r.strategy_version=? AND r.profile_version=?", key);
        const auto& row = rows.at(0);
        items.emplace_back(Json::Object{
            {"id", row.get_int64("id")}, {"job_id", job_id}, {"title", match.job.at("title")},
            {"industry", match.job.at("industry")}, {"job_type", match.job.at("job_type")},
            {"score", match.score}, {"feedback", current_feedback(row)},
        });
    }
    return Json::Object{{"term", profile.term}, {"items", std::move(items)}};
}

DbRow Recommendations::find_owned(const std::string& student_no, std::int64_t id) {
    const auto rows = db_.query(
        "SELECT r.snapshot,f.choice,f.reason,f.updated_at FROM cd_recommendations r "
        "LEFT JOIN cd_feedback f ON r.id=f.recommendation_id WHERE r.id=? AND r.student_no=?",
        {id, student_no});
    if (rows.empty()) throw CatalogError(404, "推荐记录不存在");
    return rows.front();
}

Json Recommendations::reason(const std::string& student_no, std::int64_t id) {
    const auto row = find_owned(student_no, id);
    auto result = Json::parse(row.get_string("snapshot"));
    result["id"] = id;
    result["feedback"] = current_feedback(row);
    return result;
}

Json Recommendations::save_feedback(const std::string& student_no, std::int64_t id, const Json& feedback) {
    find_owned(student_no, id);
    const auto& choice = feedback.at("choice").as_string();
    const auto& reason = feedback.at("reason").as_string();
    db_.exec("INSERT INTO cd_feedback VALUES (?,?,?,strftime('%Y-%m-%dT%H:%M:%fZ','now')) "
             "ON CONFLICT(recommendation_id) DO UPDATE SET choice=excluded.choice, reason=excluded.reason, "
             "updated_at=excluded.updated_at WHERE choice<>excluded.choice OR reason<>excluded.reason",
             {id, choice, reason});
    return current_feedback(find_owned(student_no, id));
}

}  // namespace platform::career
