#pragma once
#include "game.hpp"
#include "json.hpp"
namespace hyperion {
nlohmann::json encodeCombatState(const hyperion::flight::CombatState&);
hyperion::flight::CombatState decodeCombatState(const nlohmann::json&);
nlohmann::json encodeFlight(const hyperion::flight::Game&,const Economy&);
hyperion::flight::Game decodeFlight(const nlohmann::json&,const State&,Universe&);
}
