#include "data_governance/types.h"

namespace data_governance {

platform::Json rule_to_json(const QualityRule& rule) {
    platform::Json item = platform::Json::object();
    item["rule_id"] = rule.rule_id;
    item["rule_name"] = rule.rule_name;
    item["rule_type"] = rule.rule_type;
    item["target_field"] = rule.target_field;
    item["level"] = rule.level;
    item["enabled"] = rule.enabled;
    return item;
}

platform::Json master_to_json(const StudentMaster& master) {
    platform::Json item = platform::Json::object();
    item["student_id"] = master.student_id;
    item["student_no"] = master.student_no;
    item["name"] = master.name;
    item["major"] = master.major;
    item["class"] = master.class_name;
    item["class_name"] = master.class_name;
    item["org_id"] = master.org_id;
    item["status"] = master.status;
    item["source"] = master.source;
    item["merged_at"] = master.merged_at;
    return item;
}

platform::Json metric_to_json(const MetricDefinition& definition) {
    platform::Json item = platform::Json::object();
    item["metric_code"] = definition.metric_code;
    item["name"] = definition.name;
    item["formula"] = definition.formula;
    item["weight"] = definition.weight;
    item["term"] = definition.term;
    item["version"] = definition.version;
    item["active"] = definition.active;
    return item;
}

}  // namespace data_governance
