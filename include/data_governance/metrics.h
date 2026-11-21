#pragma once

#include "platform/app.hpp"

#include <memory>

namespace data_governance {

class GovernanceService;

// 指标重算：读取 M1 原始记录，按 gov_metric_def 计算并写入 M3 指标表。
platform::Response handle_recalculate_metrics(
    const std::shared_ptr<GovernanceService>& service, const platform::Request& request);

}  // namespace data_governance
