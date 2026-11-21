#include "data_governance/metrics.h"

#include "modules/data_governance/service.hpp"
#include "platform/api.hpp"

namespace data_governance {

platform::Response handle_recalculate_metrics(
    const std::shared_ptr<GovernanceService>& service, const platform::Request& request) {
    return platform::guard([&] {
        // 允许空 body：默认按最新指标口径、全部主数据学生重算。
        const platform::Json body =
            request.body.empty() ? platform::Json::object() : platform::parse_json_body(request);
        return platform::json_ok(service->recalculate(body));
    });
}

}  // namespace data_governance
