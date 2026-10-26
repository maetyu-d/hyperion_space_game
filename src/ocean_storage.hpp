#pragma once
#include "economy.hpp"
#include "json.hpp"
namespace hyperion {nlohmann::json encodeOceanWrecks(const Economy&);void decodeOceanWrecks(Economy&,const nlohmann::json&,Universe&);}
