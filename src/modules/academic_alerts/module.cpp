#include "platform/modules.hpp"

#include "alerts.hpp"
#include "platform/db.hpp"
#include "platform/security.hpp"
#include "repository.hpp"

#include <memory>
#include <utility>

// ============================================================================
// M4 学业分析与预警 · 模块注册入口
//
// 只做模块、路由与权限登记；业务在 alerts_api.cpp，持久化在 repository.cpp。
// 权限：
//   alert.read  —— 看板、规则列表、预警列表、干预记录、学业趋势
//   alert.write —— 配置规则（仅全校范围）、生成、认领、处置
// 数据范围由 M6 的 Scope 控制：辅导员本班、院系本学院、数据/系统管理员全校。
// ============================================================================

namespace platform {

void register_academic_alerts(App& app) {
    app.add_module("academic-alerts", "学业分析与预警");

    auto database = std::make_shared<Database>(default_database_path());
    auto repository = std::make_shared<academic_alerts::AlertRepository>(database);

    const auto access = app.security();
    if (access) {
        access->register_permission("alert.read", "查看学业看板、预警名单与干预记录");
        access->register_permission("alert.write", "配置预警规则、生成预警、认领与处置");
    }

    const auto add = [&](const char* method, const char* path, const char* permission,
                         auto handler) {
        app.add_module_route(
            "academic-alerts", method, path,
            [access, permission, repository, handler = std::move(handler)](
                const Request& request) {
                if (!access) return handler(*repository, request, nullptr);
                const auto actor = access->authorize(request, permission);
                return handler(*repository, request, &actor);
            });
    };

    add("GET", "/dashboard", "alert.read", academic_alerts::dashboard);
    add("GET", "/rules", "alert.read", academic_alerts::list_rules);
    add("POST", "/alerts/generate", "alert.write", academic_alerts::generate_alerts);
    add("GET", "/alerts", "alert.read", academic_alerts::list_alerts);
    add("POST", "/alerts/:alert_id/claim", "alert.write", academic_alerts::claim_alert);
    add("POST", "/alerts/:alert_id/handle", "alert.write", academic_alerts::handle_alert);
    add("GET", "/alerts/:alert_id/interventions", "alert.read",
        academic_alerts::interventions);
    add("GET", "/students/:student_id/trend", "alert.read", academic_alerts::student_trend);

    // 规则配置属于全校级操作，辅导员/院系管理员不能改全局阈值。
    app.add_module_route(
        "academic-alerts", "POST", "/rules",
        [access, repository](const Request& request) {
            if (!access) return academic_alerts::create_rule(*repository, request, nullptr);
            const auto actor = access->authorize(request, "alert.write",
                                                 security::Scope{"all", "", "", ""});
            return academic_alerts::create_rule(*repository, request, &actor);
        });

    app.add_module_route(
        "academic-alerts", "PATCH", "/rules/:rule_id",
        [access, repository](const Request& request) {
            if (!access) return academic_alerts::update_rule(*repository, request, nullptr);
            const auto actor = access->authorize(request, "alert.write",
                                                 security::Scope{"all", "", "", ""});
            return academic_alerts::update_rule(*repository, request, &actor);
        });
}

}  // namespace platform
