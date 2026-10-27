#include "data_governance/quality_report.h"
#include "data_governance/types.h"
#include "data_governance/response_utils.h"

namespace data_governance {

platform::Response handle_get_quality_report(const platform::Request& req) {
    json data;
    data["total_records"] = mock_quality_report.total_records;
    data["cleaned_records"] = mock_quality_report.cleaned_records;
    data["dirty_records"] = mock_quality_report.dirty_records;
    data["quality_score"] = mock_quality_report.quality_score;
    data["main_data_coverage"] = mock_quality_report.main_data_coverage;
    data["last_clean_time"] = mock_quality_report.last_clean_time;
    return success_response(data);
}

} // namespace data_governance
