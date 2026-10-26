#pragma once
#include "economy.hpp"
#include "json.hpp"
namespace hyperion {
nlohmann::json encodeEcosystems(const Economy&);
void decodeEcosystems(Economy&,const nlohmann::json&,Universe&);
}
