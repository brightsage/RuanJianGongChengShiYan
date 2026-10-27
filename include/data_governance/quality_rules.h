#pragma once
#include "platform/app.hpp"

namespace data_governance {

// 第三部分：质量规则管理模块
platform::Response handle_list_quality_rules(const platform::Request& req);
platform::Response handle_create_quality_rule(const platform::Request& req);

} // namespace data_governance
