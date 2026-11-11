#pragma once

#include "modules/student_profile/model.hpp"
#include "platform/api.hpp"
#include "platform/json.hpp"

#include <vector>

namespace platform::student_profile {

inline Json to_json(const Student& student) {
    Json value = Json::object();
    value["student_id"] = student.student_id;
    value["name"] = student.name;
    value["major"] = student.major;
    value["class_name"] = student.class_name;
    value["grade"] = student.grade;
    value["updated_at"] = student.updated_at;
    return value;
}

inline Json to_json(const MetricValue& metric) {
    Json value = Json::object();
    value["id"] = metric.id;
    value["student_id"] = metric.student_id;
    value["metric_code"] = metric.metric_code;
    value["term"] = metric.term;
    value["value"] = metric.value;
    value["unit"] = metric.unit;
    value["source"] = metric.source;
    value["calculated_at"] = metric.calculated_at;
    return value;
}

inline Json to_json(const Tag& tag) {
    Json value = Json::object();
    value["code"] = tag.code;
    value["name"] = tag.name;
    value["dimension"] = tag.dimension;
    value["description"] = tag.description;
    value["rule"] = platform::parse_json_or(tag.rule, Json::object());
    return value;
}

inline Json to_json(const StudentTag& tag) {
    Json value = Json::object();
    value["id"] = tag.id;
    value["student_id"] = tag.student_id;
    value["tag_code"] = tag.tag_code;
    value["evidence"] = platform::parse_json_or(tag.evidence, Json::object());
    value["calculated_at"] = tag.calculated_at;
    return value;
}

inline Json to_json(const ProfileSnapshot& snapshot) {
    Json value = Json::object();
    value["id"] = snapshot.id;
    value["student_id"] = snapshot.student_id;
    value["term"] = snapshot.term;
    value["payload"] = platform::parse_json_or(snapshot.payload, Json::object());
    value["generated_at"] = snapshot.generated_at;
    return value;
}

template <typename T>
Json to_json_list(const std::vector<T>& values) {
    Json array = Json::array();
    for (const auto& value : values) array.push_back(to_json(value));
    return array;
}

}  // namespace platform::student_profile
