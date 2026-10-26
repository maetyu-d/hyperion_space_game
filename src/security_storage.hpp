#pragma once
#include "economy.hpp"
#include "json.hpp"
namespace hyperion {
nlohmann::json encodeSecurity(const Economy&);
void decodeSecurity(Economy&,const nlohmann::json&);
}
