#include "platform/modules.hpp"
#include "data_governance/quality_rules.h"
#include "data_governance/student_master.h"
#include "data_governance/metrics.h"
#include "data_governance/quality_report.h"

namespace platform {

// 第七部分：模块入口注册
void register_data_governance(App& app) {
    app.add_module("data-governance", "数据治理与标准化");

    // 注册质量规则接口
    app.add_module_route("data-governance", "GET", "/quality-rules",
        [](const Request& req) -> Response {
            return data_governance::handle_list_quality_rules(req);
        });
    
    app.add_module_route("data-governance", "POST", "/quality-rules",
        [](const Request& req) -> Response {
            return data_governance::handle_create_quality_rule(req);
        });

    // 注册学生主数据接口
    app.add_module_route("data-governance", "GET", "/student-master/:student_id",
        [](const Request& req) -> Response {
            return data_governance::handle_get_student_master(req);
        });

    // 注册指标重算接口
    app.add_module_route("data-governance", "POST", "/recalculate",
        [](const Request& req) -> Response {
            return data_governance::handle_recalculate_metrics(req);
        });

    // 注册质量报告接口
    app.add_module_route("data-governance", "GET", "/quality-report",
        [](const Request& req) -> Response {
            return data_governance::handle_get_quality_report(req);
        });
}

} // namespace platform
