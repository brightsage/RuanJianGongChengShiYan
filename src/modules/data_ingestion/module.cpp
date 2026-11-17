#include "platform/modules.hpp"

namespace platform {
void register_data_ingestion(App& app) {
    app.add_module("data-ingestion", "数据采集与接入");
    // Add this module's routes here when its branch is developed.
}
}  // namespace platform
