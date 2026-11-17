#include "outcomes.hpp"

#include "catalog.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <map>
#include <optional>

namespace platform::career {
namespace {

constexpr std::array categories{"employed", "further_study", "abroad", "unemployed", "other", "unknown"};
using Count = std::int64_t;

struct YearRange {
    std::optional<int> from;
    std::optional<int> to;
};

YearRange parse_range(std::string_view query) {
    YearRange range;
    while (!query.empty()) {
        const auto separator = query.find('&');
        const auto parameter = query.substr(0, separator);
        const auto equals = parameter.find('=');
        if (equals == std::string_view::npos) throw CatalogError(400, "年份参数格式有误");
        const auto name = parameter.substr(0, equals);
        const auto value = parameter.substr(equals + 1);
        int year = 0;
        const auto end = value.data() + value.size();
        const auto parsed = std::from_chars(value.data(), end, year);
        if (value.size() != 4 || parsed.ec != std::errc{} || parsed.ptr != end ||
            year < 1900 || year > 2199) {
            throw CatalogError(400, "年份必须是 1900 至 2199 之间的四位整数");
        }
        if (name == "from_year" && !range.from) range.from = year;
        else if (name == "to_year" && !range.to) range.to = year;
        else throw CatalogError(400, "年份参数未知或重复");
        if (separator == std::string_view::npos) break;
        query.remove_prefix(separator + 1);
        if (query.empty()) throw CatalogError(400, "年份参数格式有误");
    }
    if (range.from && range.to && *range.from > *range.to) {
        throw CatalogError(400, "起始年份不能晚于截止年份");
    }
    return range;
}

double percent(Count count, Count total) {
    return total == 0 ? 0.0 : std::round(10000.0 * count / total) / 100.0;
}

struct Cohort {
    Count total = 0;
    std::map<std::string, Count> by_category;

    void add(const std::string& category, Count count) {
        total += count;
        by_category[category] += count;
    }

    Count employed() const {
        const auto found = by_category.find("employed");
        return found == by_category.end() ? 0 : found->second;
    }

    Json distribution() const {
        Json::Array rows;
        for (const auto* category : categories) {
            const auto found = by_category.find(category);
            const Count count = found == by_category.end() ? 0 : found->second;
            rows.push_back(Json::Object{{"category", category}, {"count", count},
                {"percent", percent(count, total)}});
        }
        return rows;
    }
};

}  // namespace

Json employment_outcomes(Database& db, std::string_view query, bool demo) {
    const auto range = parse_range(query);
    if (!demo && db.query(
        "SELECT 1 FROM main.sqlite_schema WHERE name='cd_outcome_source' AND type='view'").empty()) {
        throw CatalogError(503, "就业去向数据源尚未接入");
    }

    // Demo and formal data follow the same grouping and percentage rules.
    const std::string source = demo ? R"SQL(
        WITH outcome_source(graduation_year, outcome_category, industry) AS (
            VALUES (2023,'employed','信息技术'), (2023,'further_study',NULL),
                   (2024,'employed','制造业'), (2024,'employed',NULL),
                   (2024,'unemployed',NULL), (2025,'employed','信息技术'),
                   (2025,'abroad',NULL), (2025,'unknown',NULL)
        )
    )SQL" : "WITH outcome_source AS (SELECT graduation_year, outcome_category, industry FROM main.cd_outcome_source)";
    const auto grouped = db.query(source + R"SQL(
        SELECT graduation_year, COALESCE(outcome_category, 'unknown') AS category,
               CASE WHEN outcome_category = 'employed' THEN NULLIF(TRIM(industry), '') END AS actual_industry,
               COUNT(*) AS total
        FROM outcome_source
        WHERE graduation_year BETWEEN ? AND ?
        GROUP BY graduation_year, COALESCE(outcome_category, 'unknown'), actual_industry
    )SQL", {Count{range.from.value_or(1900)}, Count{range.to.value_or(2199)}});

    Cohort overall;
    std::map<Count, Cohort> years;
    std::map<std::optional<std::string>, Count> industries;
    for (const auto& row : grouped) {
        const auto category = row.get_string("category");
        if (std::find(categories.begin(), categories.end(), category) == categories.end()) {
            throw CatalogError(503, "就业去向数据包含未映射的类别");
        }
        const auto count = row.get_int64("total");
        overall.add(category, count);
        years[row.get_int64("graduation_year")].add(category, count);
        if (category == "employed") {
            std::optional<std::string> industry;
            if (!std::holds_alternative<std::nullptr_t>(row.at("actual_industry"))) {
                industry = row.get_string("actual_industry");
            }
            industries[industry] += count;
        }
    }
    Json::Array industry_distribution;
    for (const auto& [industry, count] : industries) {
        industry_distribution.push_back(Json::Object{{"industry", industry ? Json(*industry) : Json(nullptr)},
            {"count", count}, {"percent", percent(count, overall.employed())}});
    }
    Json::Array trend;
    for (const auto& [year, cohort] : years) {
        trend.push_back(Json::Object{{"year", year}, {"graduates", cohort.total},
            {"employed", cohort.employed()}, {"employment_percent", percent(cohort.employed(), cohort.total)},
            {"outcome_distribution", cohort.distribution()}});
    }
    return Json::Object{
        {"source", demo ? "demo" : "platform"},
        {"from_year", range.from ? Json(*range.from) : Json(nullptr)},
        {"to_year", range.to ? Json(*range.to) : Json(nullptr)},
        {"graduates", overall.total},
        {"employed", overall.employed()},
        {"outcome_distribution", overall.distribution()},
        {"industry_distribution", std::move(industry_distribution)},
        {"trend", std::move(trend)},
    };
}

}  // namespace platform::career
