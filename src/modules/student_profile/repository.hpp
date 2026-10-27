#pragma once

#include "modules/student_profile/model.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace platform::student_profile {

// 学生画像模块的持久化边界。画像计算、标签规则、权限判断放在 service 层。
class ProfileRepository {
public:
    virtual ~ProfileRepository() = default;

    virtual void upsert_student(const Student& student) = 0;
    virtual std::optional<Student> find_student(const std::string& student_id) const = 0;
    virtual std::vector<Student> list_students() const = 0;

    virtual void upsert_metric(const MetricValue& metric) = 0;
    virtual std::vector<MetricValue> list_metrics(const std::string& student_id,
                                                  const std::string& term = {}) const = 0;

    virtual void upsert_tag(const Tag& tag) = 0;
    virtual std::optional<Tag> find_tag(const std::string& code) const = 0;
    virtual std::vector<Tag> list_tags() const = 0;

    virtual void replace_student_tags(const std::string& student_id,
                                      const std::vector<StudentTag>& tags) = 0;
    virtual std::vector<StudentTag> list_student_tags(const std::string& student_id) const = 0;

    virtual void save_snapshot(const ProfileSnapshot& snapshot) = 0;
    virtual std::optional<ProfileSnapshot> find_snapshot(const std::string& student_id,
                                                         const std::string& term) const = 0;
};

}  // namespace platform::student_profile
