#include "shipyard_storage.hpp"
#include "flight_storage.hpp"
#include <stdexcept>
namespace hyperion {
using J=nlohmann::json;
namespace {
J vessel(const hyperion::flight::Vessel& v){return {{"serial",v.serial},{"model",v.model},{"fitted",v.fitted}};}
double number(const J& j,const char* key,double low,double high){double v=j.at(key).get<double>();if(!std::isfinite(v)||v<low||v>high)throw std::runtime_error(std::string("Invalid shipyard field: ")+key);return v;}
std::string text(const J& j,const char* key){auto value=j.at(key).get<std::string>();if(value.empty()||value.size()>240)throw std::runtime_error("Invalid vessel identity");return value;}
void array(const J& j,size_t max){if(!j.is_array()||j.size()>max)throw std::runtime_error("Oversized shipyard collection");}
hyperion::flight::Vessel vessel(const J& j){hyperion::flight::Vessel v;v.serial=text(j,"serial");double model=number(j,"model",0,hyperion::flight::vesselModelCount-1);if(std::floor(model)!=model)throw std::runtime_error("Fractional hull class");v.model=int(model);const auto& fitted=j.at("fitted");array(fitted,hyperion::flight::vesselUpgradeCount);if(fitted.size()!=hyperion::flight::vesselUpgradeCount)throw std::runtime_error("Invalid refit slots");for(int i=0;i<hyperion::flight::vesselUpgradeCount;i++){double tier=fitted[i].get<double>();if(!std::isfinite(tier)||tier<0||tier>3||std::floor(tier)!=tier)throw std::runtime_error("Invalid refit tier");v.fitted[i]=int(tier);}return v;}
}
J encodeShipyard(const Economy& e){
 J j={{"schema",1},{"initialized",e.shipyardsInitialized},{"vessel",vessel(e.vessel)},{"yards",J::array()},{"offers",J::array()}};
 for(const auto& [id,next]:e.shipyardClock)j["yards"].push_back({{"port",id},{"nextBuild",next}});
 for(const auto& o:e.shipOffers)j["offers"].push_back({{"port",o.port},{"vessel",vessel(o.vessel)},{"equipment",encodeCombatState(o.equipment)},{"hull",o.hull},{"fuel",o.fuel},{"tank",o.tank},{"shield",o.shield}});return j;
}
void decodeShipyard(Economy& e,const J& j){
 if(!j.is_object()||j.at("schema")!=1)throw std::runtime_error("Unsupported shipyard save");e.shipyardsInitialized=j.at("initialized").get<bool>();e.vessel=vessel(j.at("vessel"));
 auto& yards=j.at("yards");array(yards,80000);for(const auto& yard:yards){auto id=text(yard,"port");if(!findSettlement(e,id)||e.shipyardClock.count(id))throw std::runtime_error("Invalid shipyard port");e.shipyardClock[id]=number(yard,"nextBuild",0,1e12);}
 std::set<std::string> serials{e.vessel.serial};std::map<std::string,int> count;const auto& offers=j.at("offers");array(offers,160000);
 for(const auto& item:offers){ShipOffer o;o.port=text(item,"port");o.vessel=vessel(item.at("vessel"));if(!findSettlement(e,o.port)||!e.shipyardClock.count(o.port)||!serials.insert(o.vessel.serial).second||++count[o.port]>2)throw std::runtime_error("Invalid ship dealer inventory");
  o.hull=number(item,"hull",0,100);o.tank=number(item,"tank",1,100000);o.fuel=number(item,"fuel",0,o.tank);o.shield=number(item,"shield",0,hyperion::flight::shieldCapacity(o.vessel));o.equipment=decodeCombatState(item.at("equipment"));e.shipOffers.push_back(o);
 }
 if(!e.shipyardsInitialized&&(!e.shipyardClock.empty()||!e.shipOffers.empty()))throw std::runtime_error("Uninitialized shipyard owns inventory");
}
}
