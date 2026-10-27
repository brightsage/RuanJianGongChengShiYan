#pragma once
#include "platform/app.hpp"

namespace data_governance {

// 第五部分：指标计算与重算模块
platform::Response handle_recalculate_metrics(const platform::Request& req);

} // namespace data_governance
