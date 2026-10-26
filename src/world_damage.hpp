#pragma once
#include "flight/math.hpp"
#include <map>
#include <string>
#include <array>
namespace hyperion {
enum WorldKind {FacilityObject,PortObject,PanelObject,CargoObject,PlantObject,LifeObject,RockObject,MinorObjectKind,ImpactObject,SalvageObject};
struct WorldDamage {
 std::string star,owner;int kind=RockObject,body=-1,slot=-1,good=-1;double integrity=1,scale=1;
 flight::V3 position,normal{0,1,0};std::array<double,9> salvage{};
};
using WorldDamageMap=std::map<std::string,WorldDamage>;
struct WorldObject {
 std::string id,owner,habitat,habitatBiome;int kind=RockObject,body=-1,slot=-1,good=-1,faction=0;uint32_t population=0;
 flight::V3 habitatAt,position,half{1,1,1},right{1,0,0},up{0,1,0},back{0,0,1};
 double integrity=1,capacity=100,scale=1,mass=0;bool flora=false;
};
inline std::string facilityObjectId(const std::string& site,int kind,int ordinal){return "facility:"+site+":"+std::to_string(kind*24+ordinal);}
inline std::string portObjectId(const std::string& port){return "port:"+port;}
inline std::string propObjectId(const std::string& port,const std::string& part,int slot){return "prop:"+port+":"+part+":"+std::to_string(slot);}
inline double objectIntegrity(const WorldDamageMap& damage,const std::string& id){auto it=damage.find(id);return it==damage.end()?1:it->second.integrity;}
inline int cargoSlots(const WorldDamageMap& damage,const std::string& port,int good,double stock){int lost=0;for(int c=0;c<8;c++)lost+=objectIntegrity(damage,propObjectId(port,"cargo",good*8+c))<=0;return std::min(8,int(std::ceil(stock/20.))+lost);}
}
