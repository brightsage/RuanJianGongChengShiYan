#include "catalog.hpp"

#include <cmath>
#include <set>

namespace platform::career {
namespace {

std::string checked_text(const Json& field, const std::string& key, std::size_t limit) {
    const auto& value = field.as_string();
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos || value.size() > limit ||
        value.find('\0') != std::string::npos) {
        throw CatalogError(400, "字段为空、过长或包含空字符：" + key);
    }
    return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}

std::string text_field(const Json& input, const std::string& key, std::size_t limit) {
    return checked_text(input.at(key), key, limit);
}

double number_field(const Json& input, const std::string& key, double max,
                    bool allow_zero) {
    const double value = input.at(key).as_double();
    if (!std::isfinite(value) || value < 0 || (!allow_zero && value == 0) || value > max) {
        throw CatalogError(400, "数值超出范围：" + key);
    }
    return value;
}

void check_job_type(const std::string& value) {
    if (value != "campus" && value != "internship") {
        throw CatalogError(400, "岗位类型必须为 campus 或 internship");
    }
}

Json filter_values(const Json& input, const std::string& key, bool is_job_type) {
    const auto& values = input.at(key).as_array();
    if (values.size() > 100) throw CatalogError(400, "筛选值过多：" + key);
    std::set<std::string> unique;
    for (const auto& item : values) {
        const auto value = checked_text(item, key, 120);
        if (is_job_type) check_job_type(value);
        if (!unique.insert(value).second) throw CatalogError(400, "筛选值重复：" + key);
    }
    return Json::Array(unique.begin(), unique.end());
}

Json read_details(const DbRow& row) {
    try {
        return Json::parse(row.get_string("details"));
    } catch (const JsonError&) {
        throw DbError("Invalid career catalog record");
    }
}

Json read_job(const DbRow& row) {
    auto job = read_details(row);
    job["id"] = row.get_int64("id");
    job["status"] = row.get_string("status");
    job["version"] = row.get_int64("version");
    return job;
}

Json read_strategy(const DbRow& row) {
    auto strategy = read_details(row);
    strategy["version"] = row.get_int64("version");
    return strategy;
}

}  // namespace

Json validate_job(const Json& input) {
    const auto type = text_field(input, "job_type", 20);
    check_job_type(type);
    const auto& requirements = input.at("requirements").as_array();
    if (requirements.empty() || requirements.size() > 100) {
        throw CatalogError(400, "岗位需要 1 到 100 项能力要求");
    }
    Json::Array validated;
    std::set<std::string> codes;
    for (const auto& requirement : requirements) {
        const auto code = text_field(requirement, "competency_code", 120);
        if (!codes.insert(code).second) throw CatalogError(400, "能力标识重复：" + code);
        validated.emplace_back(Json::Object{
            {"competency_code", code},
            {"threshold", number_field(requirement, "threshold", 100, false)},
            {"weight", number_field(requirement, "weight", 100, false)},
        });
    }
    return Json::Object{
        {"title", text_field(input, "title", 200)},
        {"industry", text_field(input, "industry", 120)},
        {"job_type", type},
        {"description", text_field(input, "description", 4000)},
        {"requirements", std::move(validated)},
    };
}

Json validate_strategy(const Json& input) {
    const auto algorithm = text_field(input, "algorithm", 40);
    if (algorithm != "weighted_pass" && algorithm != "pass_and_proximity") {
        throw CatalogError(400, "未知匹配算法");
    }
    const auto pass = number_field(input, "pass_weight", 100, true);
    const auto proximity = number_field(input, "proximity_weight", 100, true);
    if (algorithm == "weighted_pass" && (pass != 100 || proximity != 0)) {
        throw CatalogError(400, "加权达标算法的达标权重应为 100，接近度权重应为 0");
    }
    if (pass == 0 && proximity == 0) throw CatalogError(400, "算法权重之和必须大于 0");
    return Json::Object{
        {"algorithm", algorithm},
        {"min_score", number_field(input, "min_score", 100, true)},
        {"pass_weight", pass},
        {"proximity_weight", proximity},
        {"allowed_industries", filter_values(input, "allowed_industries", false)},
        {"allowed_job_types", filter_values(input, "allowed_job_types", true)},
    };
}

std::string validate_status(const Json& input) {
    const auto status = text_field(input, "status", 20);
    if (status != "published" && status != "withdrawn") {
        throw CatalogError(400, "目标状态必须为 published 或 withdrawn");
    }
    return status;
}

void ensure_catalog_schema(Database& db) {
    db.exec(R"sql(
        CREATE TABLE IF NOT EXISTS cd_jobs (
            id BIGINT AUTO_INCREMENT PRIMARY KEY,
            status VARCHAR(16) NOT NULL DEFAULT 'draft'
                CHECK (status IN ('draft', 'published', 'withdrawn')),
            version INT NOT NULL DEFAULT 1 CHECK (version > 0),
            details LONGTEXT NOT NULL
        );
        CREATE TABLE IF NOT EXISTS cd_strategy (
            id INT PRIMARY KEY CHECK (id = 1),
            version INT NOT NULL DEFAULT 1 CHECK (version > 0),
            details LONGTEXT NOT NULL
        );
    )sql");
}

Catalog::Catalog(Database& database) : db_(database) {}

Json Catalog::list_jobs() {
    Json::Array jobs;
    for (const auto& row : db_.query("SELECT * FROM cd_jobs ORDER BY id DESC")) {
        jobs.push_back(read_job(row));
    }
    return jobs;
}

Json Catalog::get_job(std::int64_t id) {
    const auto rows = db_.query("SELECT * FROM cd_jobs WHERE id = ?", {id});
    if (rows.empty()) throw CatalogError(404, "岗位不存在");
    return read_job(rows.front());
}

Json Catalog::create_job(const Json& details) {
    db_.exec("INSERT INTO cd_jobs (details) VALUES (?)", {details.dump()});
    return get_job(db_.last_insert_rowid());
}

Json Catalog::update_job(std::int64_t id, const Json& details) {
    const auto payload = details.dump();
    db_.exec("UPDATE cd_jobs SET version = version + (details <> ?), details = ? WHERE id = ?",
             {payload, payload, id});
    return get_job(id);
}

Json Catalog::set_status(std::int64_t id, const std::string& status) {
    db_.exec(
        "UPDATE cd_jobs SET version = version + (status <> ?), status = ? "
        "WHERE id = ? AND (status <> 'draft' OR ? = 'published')",
        {status, status, id, status});
    const auto job = get_job(id);
    if (job.at("status").as_string() != status) throw CatalogError(409, "草稿须先发布才能下架");
    return job;
}

Json Catalog::get_strategy() {
    auto strategy = find_strategy();
    if (strategy.is_null()) throw CatalogError(409, "尚未配置推荐策略");
    return strategy;
}

Json Catalog::find_strategy() {
    const auto rows = db_.query("SELECT * FROM cd_strategy WHERE id = 1");
    if (rows.empty()) return {};
    return read_strategy(rows.front());
}

Json Catalog::save_strategy(const Json& details) {
    const auto payload = details.dump();
    db_.exec(
        "INSERT INTO cd_strategy (id, details) VALUES (1, ?) "
        "ON DUPLICATE KEY UPDATE version = version + (details <> VALUES(details)), "
        "details = VALUES(details)",
        {payload});
    return read_strategy(db_.query("SELECT * FROM cd_strategy WHERE id = 1").at(0));
}

}  // namespace platform::career
