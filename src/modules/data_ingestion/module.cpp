#include "platform/modules.hpp"

namespace platform {
void register_data_ingestion(App& app) {
    app.add_module("data-ingestion", "数据采集与接入");
    // 数据层见 model.hpp / repository.hpp / sqlite_repository.hpp。
    // UC-DC-01～07 的 HTTP 接口在 service 层实现后，用 app.add_module_route 注册到这里；
    // 路由前缀由框架自动拼成 /api/modules/data-ingestion。
}
}  // namespace platform
