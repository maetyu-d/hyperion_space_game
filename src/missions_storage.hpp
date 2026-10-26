#pragma once
#include "economy.hpp"
#include "json.hpp"
namespace hyperion {nlohmann::json encodeMissions(const Economy&);void decodeMissions(Economy&,const nlohmann::json&,Universe&);}
