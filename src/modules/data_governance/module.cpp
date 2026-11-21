#include "platform/modules.hpp"

#include "data_governance/metrics.h"
#include "data_governance/quality_report.h"
#include "data_governance/quality_rules.h"
#include "modules/data_governance/service.hpp"
#include "data_governance/student_master.h"
#include "modules/student_profile/sqlite_repository.hpp"
#include "platform/db.hpp"
#include "platform/security.hpp"

#include <memory>
#include <utility>

namespace platform {

// 第七部分：模块入口注册
void register_data_governance(App& app) {
    app.add_module("data-governance", "数据治理与标准化");

    auto database = std::make_shared<Database>(default_database_path());
    auto repository = std::make_shared<data_governance::GovRepository>(*database);
    // M2 的主数据/指标统一写到 M3 的 sp_student / sp_metric_value，
    // 复用 M3 repository，跨模块 SQL 只集中在 service 层。
    auto profile = std::make_shared<student_profile::SqliteProfileRepository>(*database);
    auto service = std::make_shared<data_governance::GovernanceService>(database, repository,
                                                                        profile);

    const auto access = app.security();
    if (access) {
        access->register_permission("governance.read", "查看治理规则、学生主数据与质量报告");
        access->register_permission("governance.write", "维护治理规则、归并主数据与重算指标");
    }

    const auto add = [&](const char* method, const char* path, const char* permission,
                         Handler handler) {
        app.add_module_route(
            "data-governance", method, path,
            [access, permission, handler = std::move(handler)](const Request& request) {
                if (access) {
                    access->authorize(request, permission, security::Scope{"all", "", "", ""});
                }
                return handler(request);
            });
    };

    add("GET", "/quality-rules", "governance.read", [service](const Request& request) {
        return data_governance::handle_list_quality_rules(service, request);
    });

    add("POST", "/quality-rules", "governance.write", [service](const Request& request) {
        return data_governance::handle_create_quality_rule(service, request);
    });

    add("PATCH", "/quality-rules/:rule_id", "governance.write",
        [service](const Request& request) {
            return data_governance::handle_update_quality_rule(service, request);
        });

    add("GET", "/student-master", "governance.read", [service](const Request& request) {
        return data_governance::handle_list_student_master(service, request);
    });

    add("GET", "/student-master/:student_id", "governance.read",
        [service](const Request& request) {
            return data_governance::handle_get_student_master(service, request);
        });

    add("POST", "/student-master/merge", "governance.write",
        [service](const Request& request) {
            return data_governance::handle_merge_student_master(service, request);
        });

    add("POST", "/recalculate", "governance.write", [service](const Request& request) {
        return data_governance::handle_recalculate_metrics(service, request);
    });

    add("GET", "/quality-report", "governance.read", [service](const Request& request) {
        return data_governance::handle_get_quality_report(service, request);
    });
}

}  // namespace platform
