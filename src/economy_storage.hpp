#pragma once
#include "economy.hpp"
#include "json.hpp"
namespace hyperion {
nlohmann::json encodeEconomy(const Economy&);
Economy decodeEconomy(const nlohmann::json&, Universe&, double currentHour);
}
