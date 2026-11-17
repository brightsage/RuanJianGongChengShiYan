#pragma once

#include "platform/app.hpp"

namespace platform {
void register_data_ingestion(App& app);
void register_data_governance(App& app);
void register_student_profile(App& app);
void register_academic_alerts(App& app);
void register_career_recommendation(App& app);
void register_data_services(App& app);
}  // namespace platform
