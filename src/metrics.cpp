#include "data_governance/metrics.h"
#include "data_governance/response_utils.h"
#include <ctime>

namespace data_governance {

platform::Response handle_recalculate_metrics(const platform::Request& req) {
    json data;
    data["task_id"] = "recalc_" + std::to_string(std::time(nullptr));
    data["estimated_time"] = "5分钟";
    return platform::Response(202, "application/json; charset=utf-8",
        success_response(data, "指标重算任务已提交").body);
}

} // namespace data_governance
