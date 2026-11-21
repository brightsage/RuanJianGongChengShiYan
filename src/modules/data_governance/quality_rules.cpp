#include "data_governance/quality_rules.h"

#include "modules/data_governance/service.hpp"
#include "data_governance/types.h"
#include "platform/api.hpp"

#include <vector>

namespace data_governance {
namespace {

platform::Json rules_json(const std::vector<QualityRule>& rules) {
    platform::Json array = platform::Json::array();
    for (const auto& rule : rules) array.push_back(rule_to_json(rule));
    return array;
}

}  // namespace

platform::Response handle_list_quality_rules(
    const std::shared_ptr<GovernanceService>& service, const platform::Request&) {
    return platform::guard(
        [&] { return platform::json_ok(rules_json(service->list_quality_rules())); });
}

platform::Response handle_create_quality_rule(
    const std::shared_ptr<GovernanceService>& service, const platform::Request& request) {
    return platform::guard([&] {
        return platform::json_ok(
            rule_to_json(service->create_quality_rule(platform::parse_json_body(request))), 201);
    });
}

platform::Response handle_update_quality_rule(
    const std::shared_ptr<GovernanceService>& service, const platform::Request& request) {
    return platform::guard([&] {
        const auto rule_id = platform::path_param_id(request, "rule_id");
        return platform::json_ok(rule_to_json(
            service->update_quality_rule(rule_id, platform::parse_json_body(request))));
    });
}

}  // namespace data_governance
