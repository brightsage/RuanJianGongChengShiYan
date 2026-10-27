#pragma once
#include "platform/app.hpp"

namespace data_governance {

// 第六部分：数据质量报告模块
platform::Response handle_get_quality_report(const platform::Request& req);

} // namespace data_governance
