#include "profile.hpp"

#include "catalog.hpp"
#include "modules/student_profile/sqlite_repository.hpp"
#include "platform/crypto.hpp"

#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <tuple>

namespace platform::career {
namespace {

std::string version_digest(const std::string& data) {
    return platform::crypto::sha256_hex(data);
}

std::optional<double> normalize(const student_profile::MetricValue& metric) {
    if (!std::isfinite(metric.value) || metric.value < 0) return std::nullopt;
    if (metric.unit == "ratio_1" && metric.value <= 1) return 100 * metric.value;
    if ((metric.unit == "score_100" || metric.unit == "%") && metric.value <= 100) {
        return metric.value;
    }
    return std::nullopt;
}

}  // namespace

Profile load_profile(Database& db, const std::string& student_no, const std::string& requested_term) {
    student_profile::SqliteProfileRepository profiles(db);
    if (!profiles.find_student(student_no)) {
        throw CatalogError(409, "尚未生成本人画像");
    }
    const auto metrics = profiles.list_metrics(student_no, requested_term);
    std::string term = requested_term;
    if (term.empty()) {
        std::string latest;
        for (const auto& metric : metrics) {
            if (!metric.term.empty() &&
                std::tie(metric.calculated_at, metric.term) > std::tie(latest, term)) {
                latest = metric.calculated_at;
                term = metric.term;
            }
        }
    }
    Profile profile{student_no, term, {}, {}};
    Json revision = Json::object();
    for (const auto& metric : metrics) {
        if (metric.term != term || term.empty()) continue;
        const auto score = normalize(metric);
        if (!score || metric.metric_code.empty()) continue;
        profile.scores.emplace(metric.metric_code, *score);
        std::ostringstream value;
        value.imbue(std::locale::classic());
        value << std::setprecision(std::numeric_limits<double>::max_digits10) << *score;
        revision[metric.metric_code] = Json::Array{value.str(), metric.calculated_at};
    }
    if (profile.scores.empty()) {
        throw CatalogError(409, "所选周期没有可用于职业推荐的画像指标");
    }
    const auto snapshot = profiles.find_snapshot(student_no, term);
    profile.version = version_digest(Json(Json::Array{
        "profile-v1", term, std::move(revision),
        snapshot ? Json(snapshot->id) : Json{}, snapshot ? Json(snapshot->generated_at) : Json{},
    }).dump());
    return profile;
}

Profile demo_profile(const std::string& student_no, const std::string& term) {
    constexpr char demo_term[] = "2026-2027-1";
    if (!term.empty() && term != demo_term) {
        throw CatalogError(409, "演示画像仅提供周期 2026-2027-1");
    }
    if (student_no == "demo-1") return {student_no, demo_term, "demo-1-v1", {{"programming", 80}, {"databases", 40}}};
    if (student_no == "demo-2") return {student_no, demo_term, "demo-2-v1", {{"programming", 80}}};
    throw CatalogError(400, "请选择演示学生");
}

}  // namespace platform::career
