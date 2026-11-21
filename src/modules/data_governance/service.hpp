#pragma once

#include "modules/data_governance/repository.hpp"
#include "modules/student_profile/repository.hpp"
#include "platform/db.hpp"
#include "platform/json.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace data_governance {

// 治理业务逻辑：规则校验、主数据归并、指标计算与跨模块同步都在这一层。
// M2 写 M3 的 sp_student / sp_metric_value 只通过 ProfileRepository，路由层不写跨模块 SQL。
class GovernanceService {
public:
    GovernanceService(std::shared_ptr<platform::Database> database,
                      std::shared_ptr<GovRepository> repository,
                      std::shared_ptr<platform::student_profile::ProfileRepository> profile);

    std::vector<QualityRule> list_quality_rules() const;
    QualityRule create_quality_rule(const platform::Json& body) const;
    QualityRule update_quality_rule(std::int64_t rule_id, const platform::Json& body) const;

    platform::Json get_student_master(const std::string& student_id) const;
    platform::Json list_student_master() const;
    platform::Json merge_student_master(const platform::Json& body) const;

    platform::Json recalculate(const platform::Json& body) const;
    platform::Json quality_report() const;

private:
    void sync_student(const StudentMaster& master) const;
    // 读取有效原始记录，并应用 M1 的字段映射（源字段 → 标准字段）。
    std::vector<std::pair<std::int64_t, platform::Json>> normalized_raw_payloads() const;

    std::shared_ptr<platform::Database> database_;
    std::shared_ptr<GovRepository> repository_;
    std::shared_ptr<platform::student_profile::ProfileRepository> profile_;
};

}  // namespace data_governance
