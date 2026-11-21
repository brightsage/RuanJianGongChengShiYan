#pragma once

#include "platform/app.hpp"

#include <memory>

namespace data_governance {

class GovernanceService;

// 质量报告：实时统计原始记录与主数据覆盖率。
platform::Response handle_get_quality_report(
    const std::shared_ptr<GovernanceService>& service, const platform::Request& request);

}  // namespace data_governance
