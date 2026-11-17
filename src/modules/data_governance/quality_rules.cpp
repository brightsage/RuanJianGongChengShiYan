#include "data_governance/quality_rules.h"
#include "data_governance/types.h"
#include "data_governance/response_utils.h"

namespace data_governance {

platform::Response handle_list_quality_rules(const platform::Request& req) {
    json data = json::array();
    for (const auto& rule : mock_quality_rules) {
        json item;
        item["rule_id"] = rule.rule_id;
        item["rule_name"] = rule.rule_name;
        item["rule_type"] = rule.rule_type;
        item["target_field"] = rule.target_field;
        item["level"] = rule.level;
        data.push_back(item);
    }
    return success_response(data);
}

platform::Response handle_create_quality_rule(const platform::Request& req) {
    try {
        json body = json::parse(req.body);
        QualityRule new_rule;
        new_rule.rule_id = mock_quality_rules.size() + 1;
        new_rule.rule_name = body.value("rule_name", "");
        new_rule.rule_type = body.value("rule_type", "");
        new_rule.target_field = body.value("target_field", "");
        new_rule.level = body.value("level", "warn");
        mock_quality_rules.push_back(new_rule);
        
        json result;
        result["rule_id"] = new_rule.rule_id;
        result["rule_name"] = new_rule.rule_name;
        return platform::Response(201, "application/json; charset=utf-8", 
            success_response(result, "规则创建成功").body);
    } catch (...) {
        return error_response(400, "请求参数格式错误");
    }
}

} // namespace data_governance
