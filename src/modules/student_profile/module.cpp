#include "platform/modules.hpp"

namespace platform {
void register_student_profile(App& app) {
    app.add_module("student-profile", "学生画像");
    // 数据层见 model.hpp / repository.hpp / sqlite_repository.hpp。
    // UC-SP-01～07 的 HTTP 接口在 service 层实现后注册；路由前缀为 /api/modules/student-profile。
}
}  // namespace platform
