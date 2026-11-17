#pragma once

#include "modules/student_profile/model.hpp"
#include "modules/student_profile/repository.hpp"
#include "platform/db.hpp"
#include "platform/json.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace platform::student_profile {

class ProfileService {
public:
    ProfileService(std::shared_ptr<Database> database,
                   std::shared_ptr<ProfileRepository> repository);

    Student sync_student(const Json& body);
    std::vector<Student> list_students(const std::string& major,
                                       const std::string& class_name) const;
    Student get_student(const std::string& student_id) const;

    MetricValue sync_metric(const Json& body);
    std::vector<MetricValue> list_metrics(const std::string& student_id,
                                          const std::string& term) const;

    Tag upsert_tag(const Json& body);
    std::vector<Tag> list_tags() const;

    Json get_profile(const std::string& student_id, const std::string& term) const;
    Json get_tag_evidence(const std::string& student_id, const std::string& tag_code,
                          const std::string& term) const;
    Json compare(const std::string& student_id, const std::string& peer_id,
                 const std::string& term) const;
    Json recalculate(const Json& body);
    Json recalculate_batch(const Json& body);
    std::string report_html(const std::string& student_id, const std::string& term) const;

private:
    std::string resolve_term(const std::string& student_id, const std::string& term) const;
    Json build_profile(const Student& student, const std::string& term) const;
    Json recalculate_one(const std::string& student_id, const std::string& term);

    std::shared_ptr<Database> database_;
    std::shared_ptr<ProfileRepository> repository_;
};

}  // namespace platform::student_profile
