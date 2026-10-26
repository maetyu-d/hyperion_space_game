#include "world_damage_storage.hpp"
#include "universe.hpp"
#include <stdexcept>
namespace hyperion {
using J=nlohmann::json;
J encodeWorldDamage(const WorldDamageMap& map){J out=J::object();for(const auto& [id,d]:map){out[id]={{"star",d.star},{"owner",d.owner},{"kind",d.kind},{"body",d.body},{"slot",d.slot},{"good",d.good},{"integrity",d.integrity},{"scale",d.scale},{"position",{d.position.x,d.position.y,d.position.z}},{"normal",{d.normal.x,d.normal.y,d.normal.z}},{"salvage",d.salvage}};
 if(d.structure.valid){const auto& f=d.structure;auto vec=[](flight::V3 v){return J::array({v.x,v.y,v.z});};out[id]["structure"]={{"profile",f.profile},{"faction",f.faction},{"variant",f.variant},{"ordinal",f.ordinal},{"seed",f.seed},{"orbital",f.orbital},{"born",f.born},{"origin",vec(f.origin)},{"right",vec(f.right)},{"up",vec(f.up)},{"back",vec(f.back)},{"impact",vec(f.impact)}};}
 }return out;}
WorldDamageMap decodeWorldDamage(const J& j,Universe& universe,double hour){
 if(!j.is_object()||j.size()>60000)throw std::runtime_error("Invalid world destruction ledger");WorldDamageMap map;
 for(auto it=j.begin();it!=j.end();++it){const auto& v=it.value();WorldDamage d;auto id=it.key();if(id.empty()||id.size()>512||!v.is_object())throw std::runtime_error("Invalid destructible identity");
  auto number=[&](const char* key,double low,double high){double n=v.at(key).get<double>();if(!std::isfinite(n)||n<low||n>high)throw std::runtime_error(std::string("Invalid world damage field: ")+key);return n;};
  auto integer=[&](const char* key,int low,int high){double n=number(key,low,high);if(n!=std::floor(n))throw std::runtime_error("Fractional world damage index");return int(n);};
  d.star=v.at("star").get<std::string>();d.owner=v.at("owner").get<std::string>();if(d.star.size()>100||d.owner.size()>512||universe.star(d.star).id!=d.star)throw std::runtime_error("Invalid destruction system");
  d.kind=integer("kind",FacilityObject,SalvageObject);d.body=integer("body",-1,int(universe.planets(d.star).size())-1);d.slot=integer("slot",-1,100000);d.good=integer("good",-1,8);d.integrity=number("integrity",0,1);d.scale=number("scale",.01,4096);
  auto vector=[&](const char* key){const auto& a=v.at(key);if(!a.is_array()||a.size()!=3)throw std::runtime_error("Invalid damage vector");flight::V3 p{a[0].get<double>(),a[1].get<double>(),a[2].get<double>()};if(!flight::finite(p)||flight::length(p)>1e8)throw std::runtime_error("Nonfinite damage position");return p;};
  d.position=vector("position");d.normal=vector("normal");if(flight::length(d.normal)<.9||flight::length(d.normal)>1.1)throw std::runtime_error("Invalid damage orientation");
  const auto& cargo=v.at("salvage");if(!cargo.is_array()||cargo.size()!=9)throw std::runtime_error("Invalid debris cargo");for(int i=0;i<9;i++){d.salvage[i]=cargo[i].get<double>();if(!std::isfinite(d.salvage[i])||d.salvage[i]<0||d.salvage[i]>500)throw std::runtime_error("Invalid debris supply");}
  if(v.contains("structure")){
   const auto& f=v.at("structure");auto& shape=d.structure;if(!f.is_object()||d.integrity>0)throw std::runtime_error("Invalid structural ruin");
   auto scalar=[&](const char* key,double low,double high,bool integral=false){double n=f.at(key).get<double>();if(!std::isfinite(n)||n<low||n>high||(integral&&n!=std::floor(n)))throw std::runtime_error("Invalid structural field");return n;};
   auto vector=[&](const char* key){const auto& a=f.at(key);if(!a.is_array()||a.size()!=3)throw std::runtime_error("Invalid structural vector");flight::V3 n{a[0].get<double>(),a[1].get<double>(),a[2].get<double>()};if(!flight::finite(n)||flight::length(n)>1e8)throw std::runtime_error("Invalid structural vector");return n;};
   shape.ordinal=int(scalar("ordinal",0,23,true));shape.profile=int(scalar("profile",0,flight::StructureProfileCount-1,true));shape.faction=int(scalar("faction",0,4,true));shape.variant=int(scalar("variant",-1,100000,true));shape.seed=uint32_t(scalar("seed",0,4294967295.,true));shape.born=scalar("born",0,hour);shape.orbital=f.at("orbital").get<bool>();
   shape.origin=vector("origin");shape.right=vector("right");shape.up=vector("up");shape.back=vector("back");shape.impact=vector("impact");
   if(std::abs(flight::length(shape.right)-1)>1e-5||std::abs(flight::length(shape.up)-1)>1e-5||std::abs(flight::length(shape.back)-1)>1e-5||flight::length(flight::cross(shape.right,shape.up)-shape.back)>1e-5||flight::length(shape.impact)>8192||flight::length(shape.origin-d.position)>2048)throw std::runtime_error("Invalid structural frame");
   bool orbital=shape.profile==flight::StationCore||shape.profile==flight::StationWing;
   bool compatible=d.kind==FacilityObject?shape.profile<14&&d.slot/24==shape.profile:d.kind==PortObject?shape.profile==flight::StationCore||shape.profile==flight::LandingPad:d.kind==CargoObject?shape.profile==flight::FreightCrate:d.kind==PanelObject?shape.profile==flight::StationWing||shape.profile==flight::LoadingGantry||shape.profile==flight::PortMast||shape.profile==flight::PortTerminal:false;
   if(!compatible||shape.orbital!=orbital||(shape.profile==flight::FreightCrate&&(shape.variant<0||shape.variant>8))||(shape.profile==flight::StationWing&&std::abs(shape.variant)!=1)||(shape.profile==flight::StationCore&&(shape.variant<0||shape.variant>80||(shape.variant&15)>8))||(shape.profile<14&&(shape.variant<0||shape.variant>12||shape.ordinal!=d.slot%24)))throw std::runtime_error("Invalid structural identity");shape.valid=true;
  }
  if(d.kind==FacilityObject&&(!d.structure.valid||id!=facilityObjectId(d.owner,d.slot/24,d.slot%24)))throw std::runtime_error("Invalid facility ruin");
  map.emplace(id,std::move(d));
 }return map;
}
}
