#pragma once
#include "economy.hpp"
#include "json.hpp"
namespace hyperion {
nlohmann::json encodeRecoverySociety(const Economy&);
void decodeRecoverySociety(Economy&,const nlohmann::json&,double);
void validateRecoverySociety(const Economy&);
}
