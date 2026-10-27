#include "platform/modules.hpp"

namespace platform {
void register_academic_alerts(App& app) {
    app.add_module("academic-alerts", "学业分析与预警");
}
}  // namespace platform
