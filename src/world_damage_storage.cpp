#include "world_damage_storage.hpp"
#include "universe.hpp"
#include <stdexcept>
namespace hyperion {
using J=nlohmann::json;
J encodeWorldDamage(const WorldDamageMap& map){J out=J::object();for(const auto& [id,d]:map)out[id]={{"star",d.star},{"owner",d.owner},{"kind",d.kind},{"body",d.body},{"slot",d.slot},{"good",d.good},{"integrity",d.integrity},{"scale",d.scale},{"position",{d.position.x,d.position.y,d.position.z}},{"normal",{d.normal.x,d.normal.y,d.normal.z}},{"salvage",d.salvage}};return out;}
WorldDamageMap decodeWorldDamage(const J& j,Universe& universe){
 if(!j.is_object()||j.size()>60000)throw std::runtime_error("Invalid world destruction ledger");WorldDamageMap map;
 for(auto it=j.begin();it!=j.end();++it){const auto& v=it.value();WorldDamage d;auto id=it.key();if(id.empty()||id.size()>512||!v.is_object())throw std::runtime_error("Invalid destructible identity");
  auto number=[&](const char* key,double low,double high){double n=v.at(key).get<double>();if(!std::isfinite(n)||n<low||n>high)throw std::runtime_error(std::string("Invalid world damage field: ")+key);return n;};
  auto integer=[&](const char* key,int low,int high){double n=number(key,low,high);if(n!=std::floor(n))throw std::runtime_error("Fractional world damage index");return int(n);};
  d.star=v.at("star").get<std::string>();d.owner=v.at("owner").get<std::string>();if(d.star.size()>100||d.owner.size()>512||universe.star(d.star).id!=d.star)throw std::runtime_error("Invalid destruction system");
  d.kind=integer("kind",PortObject,SalvageObject);d.body=integer("body",-1,int(universe.planets(d.star).size())-1);d.slot=integer("slot",-1,100000);d.good=integer("good",-1,8);d.integrity=number("integrity",0,1);d.scale=number("scale",.01,4096);
  auto vector=[&](const char* key){const auto& a=v.at(key);if(!a.is_array()||a.size()!=3)throw std::runtime_error("Invalid damage vector");flight::V3 p{a[0].get<double>(),a[1].get<double>(),a[2].get<double>()};if(!flight::finite(p)||flight::length(p)>1e8)throw std::runtime_error("Nonfinite damage position");return p;};
  d.position=vector("position");d.normal=vector("normal");if(flight::length(d.normal)<.9||flight::length(d.normal)>1.1)throw std::runtime_error("Invalid damage orientation");
  const auto& cargo=v.at("salvage");if(!cargo.is_array()||cargo.size()!=9)throw std::runtime_error("Invalid debris cargo");for(int i=0;i<9;i++){d.salvage[i]=cargo[i].get<double>();if(!std::isfinite(d.salvage[i])||d.salvage[i]<0||d.salvage[i]>500)throw std::runtime_error("Invalid debris supply");}
  map.emplace(id,std::move(d));
 }return map;
}
}
