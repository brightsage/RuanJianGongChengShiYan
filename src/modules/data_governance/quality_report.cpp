#include "data_governance/quality_report.h"

#include "modules/data_governance/service.hpp"
#include "platform/api.hpp"

namespace data_governance {

platform::Response handle_get_quality_report(
    const std::shared_ptr<GovernanceService>& service, const platform::Request&) {
    return platform::guard([&] { return platform::json_ok(service->quality_report()); });
}

}  // namespace data_governance
