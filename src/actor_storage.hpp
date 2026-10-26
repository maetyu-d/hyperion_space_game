#pragma once
#include "flight/threat.hpp"
#include "json.hpp"
#include <stdexcept>
#include <cmath>
namespace hyperion {
inline nlohmann::json encodeGenome(const hyperion::flight::EnemyGenome& g){return {{"revision",g.revision},{"seed",g.seed},{"legacyEquipment",g.legacyEquipment}};}
inline hyperion::flight::EnemyGenome decodeGenome(const nlohmann::json& j){
 if(!j.is_object()||!j.at("revision").is_number_integer()||(j.at("revision")!=0&&j.at("revision")!=1)||(!j.at("seed").is_number_unsigned()&&!j.at("seed").is_number_integer()))throw std::runtime_error("Invalid actor genome");
 auto seed=j.at("seed").get<double>();if(seed<0||seed>4294967295.||std::floor(seed)!=seed)throw std::runtime_error("Invalid actor seed");
 hyperion::flight::EnemyGenome g{j.at("revision").get<int>(),uint32_t(seed),j.at("legacyEquipment").get<bool>()};
 if(!g.revision&&(g.seed||g.legacyEquipment))throw std::runtime_error("Invalid original actor profile");return g;
}
}
