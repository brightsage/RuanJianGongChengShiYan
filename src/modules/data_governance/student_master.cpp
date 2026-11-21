#include "data_governance/student_master.h"

#include "modules/data_governance/service.hpp"
#include "platform/api.hpp"

namespace data_governance {

platform::Response handle_list_student_master(
    const std::shared_ptr<GovernanceService>& service, const platform::Request&) {
    return platform::guard([&] { return platform::json_ok(service->list_student_master()); });
}

platform::Response handle_get_student_master(
    const std::shared_ptr<GovernanceService>& service, const platform::Request& request) {
    return platform::guard([&] {
        const auto it = request.path_params.find("student_id");
        const std::string student_id = it == request.path_params.end() ? std::string{} : it->second;
        return platform::json_ok(service->get_student_master(student_id));
    });
}

platform::Response handle_merge_student_master(
    const std::shared_ptr<GovernanceService>& service, const platform::Request& request) {
    return platform::guard([&] {
        return platform::json_ok(
            service->merge_student_master(platform::parse_json_body(request)));
    });
}

}  // namespace data_governance
