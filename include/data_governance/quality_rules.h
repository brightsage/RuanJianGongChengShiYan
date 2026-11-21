#pragma once

#include "platform/app.hpp"

#include <memory>

namespace data_governance {

class GovernanceService;

// 质量规则：GET 列表 / POST 新建 / PATCH 启停与级别（gov_quality_rule）。
platform::Response handle_list_quality_rules(
    const std::shared_ptr<GovernanceService>& service, const platform::Request& request);
platform::Response handle_create_quality_rule(
    const std::shared_ptr<GovernanceService>& service, const platform::Request& request);
platform::Response handle_update_quality_rule(
    const std::shared_ptr<GovernanceService>& service, const platform::Request& request);

}  // namespace data_governance
