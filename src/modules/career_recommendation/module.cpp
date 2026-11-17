#include "platform/modules.hpp"

namespace platform {
void register_career_recommendation(App& app) {
    app.add_module("career-recommendation", "职业发展推荐");
}
}  // namespace platform
