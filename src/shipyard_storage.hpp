#pragma once
#include "economy.hpp"
#include "json.hpp"
namespace hyperion {
nlohmann::json encodeShipyard(const Economy&);
void decodeShipyard(Economy&,const nlohmann::json&);
}
