#pragma once

#include "platform/db.hpp"
#include "platform/json.hpp"

#include <string_view>

namespace platform::career {

Json employment_outcomes(Database& db, std::string_view query, bool demo);

}  // namespace platform::career
