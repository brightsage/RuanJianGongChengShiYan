#pragma once

#include "platform/app.hpp"

#include <cstdint>

namespace platform {
void serve(const App& app, std::uint16_t port);
}
