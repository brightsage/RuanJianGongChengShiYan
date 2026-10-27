#include "platform/modules.hpp"

#include "alerts.hpp"

// ============================================================================
// M4 学业分析与预警 · 模块注册入口
//
// 这个文件只做一件事：把模块和路由登记到平台。
// 业务实现在 alerts_api.cpp，数据与判定在 alerts_store.cpp，
// 数据结构与函数声明在 alerts.hpp，JSON/查询串工具在 json_util.hpp。
// ============================================================================

namespace platform {

void register_academic_alerts(App& app) {
    namespace m4 = academic_alerts;

    app.add_module("academic-alerts", "学业分析与预警");

    app.add_module_route("academic-alerts", "GET", "/dashboard", m4::dashboard);
    app.add_module_route("academic-alerts", "GET", "/rules", m4::list_rules);
    app.add_module_route("academic-alerts", "POST", "/rules", m4::create_rule);
    app.add_module_route("academic-alerts", "POST", "/alerts/generate", m4::generate_alerts);
    app.add_module_route("academic-alerts", "GET", "/alerts", m4::list_alerts);
    app.add_module_route("academic-alerts", "POST", "/alerts/:alert_id/claim", m4::claim_alert);
    app.add_module_route("academic-alerts", "POST", "/alerts/:alert_id/handle", m4::handle_alert);
    app.add_module_route("academic-alerts", "GET", "/alerts/:alert_id/interventions",
                         m4::interventions);
    app.add_module_route("academic-alerts", "GET", "/students/:student_id/trend",
                         m4::student_trend);
}

}  // namespace platform
