#include "platform/modules.hpp"

namespace platform {
void register_data_governance(App& app) {
    app.add_module("data-governance", "数据治理与标准化");
}
}  // namespace platform
