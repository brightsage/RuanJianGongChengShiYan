#include "platform/modules.hpp"
#include "store.hpp"

namespace platform {
void register_data_services(App& app, const AppOptions& options) {
    app.add_module("data-services", "数据服务与权限管理");
    auto security = std::make_shared<security::Security>(std::make_shared<data_services::Store>(options));
    app.set_security(security);
    data_services::install_identity_routes(app, security);
    data_services::install_service_routes(app, security);
}
}  // namespace platform
