#pragma once

#include "modules/student_profile/repository.hpp"
#include "platform/db.hpp"

namespace platform::student_profile {

class SqliteProfileRepository final : public ProfileRepository {
public:
    explicit SqliteProfileRepository(Database& database);

    void upsert_student(const Student& student) override;
    std::optional<Student> find_student(const std::string& student_id) const override;
    std::vector<Student> list_students() const override;

    void upsert_metric(const MetricValue& metric) override;
    std::vector<MetricValue> list_metrics(const std::string& student_id,
                                          const std::string& term = {}) const override;

    void upsert_tag(const Tag& tag) override;
    std::optional<Tag> find_tag(const std::string& code) const override;
    std::vector<Tag> list_tags() const override;

    void replace_student_tags(const std::string& student_id,
                              const std::vector<StudentTag>& tags) override;
    std::vector<StudentTag> list_student_tags(const std::string& student_id) const override;

    void save_snapshot(const ProfileSnapshot& snapshot) override;
    std::optional<ProfileSnapshot> find_snapshot(const std::string& student_id,
                                                 const std::string& term) const override;

private:
    Database& db_;
};

}  // namespace platform::student_profile
