#include "platform/modules.hpp"

namespace platform {
void register_data_services(App& app) {
    app.add_module("data-services", "数据服务与权限管理");
}
}  // namespace platform
