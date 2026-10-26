#pragma once
#include "world_damage.hpp"
#include "json.hpp"
namespace hyperion {class Universe;nlohmann::json encodeWorldDamage(const WorldDamageMap&);WorldDamageMap decodeWorldDamage(const nlohmann::json&,Universe&);}
