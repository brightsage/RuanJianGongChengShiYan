#pragma once

#include "catalog.hpp"
#include "profile.hpp"

namespace platform::career {

struct RecommendationFilter {
    std::string term;
    std::string industry;
    double min_score = 0;
};

RecommendationFilter validate_filter(const Json& input);
Json validate_feedback(const Json& input);

class Recommendations {
public:
    explicit Recommendations(Database& database);

    Json query(Catalog& catalog, const Profile& profile, const RecommendationFilter& filter);
    Json reason(const std::string& student_no, std::int64_t id);
    Json save_feedback(const std::string& student_no, std::int64_t id, const Json& feedback);

private:
    DbRow find_owned(const std::string& student_no, std::int64_t id);
    Database& db_;
};

}  // namespace platform::career
