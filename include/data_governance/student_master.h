#pragma once

#include "platform/app.hpp"

#include <memory>

namespace data_governance {

class GovernanceService;

// 学生主数据：列表、按 student_id 查询、POST /student-master/merge 多源归并。
platform::Response handle_list_student_master(
    const std::shared_ptr<GovernanceService>& service, const platform::Request& request);
platform::Response handle_get_student_master(
    const std::shared_ptr<GovernanceService>& service, const platform::Request& request);
platform::Response handle_merge_student_master(
    const std::shared_ptr<GovernanceService>& service, const platform::Request& request);

}  // namespace data_governance
