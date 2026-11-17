#include "platform/modules.hpp"

namespace platform {
void register_student_profile(App& app) {
    app.add_module("student-profile", "学生画像");
}
}  // namespace platform
