#include "security_storage.hpp"
#include "actor_storage.hpp"
#include <stdexcept>
namespace hyperion {
using J=nlohmann::json;
J encodeSecurity(const Economy& e){
 J j={{"schema",2},{"systems",e.securitySystems},{"pirates",J::array()},{"captains",J::array()}};
 for(const auto& b:e.pirates)j["pirates"].push_back({{"id",b.id},{"name",b.name},{"star",b.star},{"haven",b.haven},{"stock",b.stock},{"treasury",b.treasury},{"grievance",b.grievance},{"truceUntil",b.truceUntil},{"nextRaid",b.nextRaid},{"stolen",b.stolen},{"raids",b.raids}});
 for(const auto& c:e.captains)j["captains"].push_back({{"genome",encodeGenome(c.genome)},{"id",c.id},{"name",c.name},{"star",c.star},{"home",c.home},{"band",c.band},{"faction",c.faction},{"role",c.role},{"defeats",c.defeats},{"victories",c.victories},{"destroyed",c.destroyed},{"readyAt",c.readyAt},{"grudge",c.grudge},{"hull",c.hull},{"shield",c.shield},{"armour",c.armour},{"energy",c.energy},{"heat",c.heat},{"modules",c.modules},{"rounds",c.rounds},{"missiles",c.missiles},{"decoys",c.decoys}});
 for(size_t i=0;i<e.captains.size();i++)if(e.captains[i].captured)j["captains"][i]["captured"]=true;
 return j;
}
namespace {
double n(const J& j,const char* k,double lo,double hi){double v=j.at(k).get<double>();if(!std::isfinite(v)||v<lo||v>hi)throw std::runtime_error(std::string("Invalid security field: ")+k);return v;}
int64_t integer(const J& j,const char* k,int64_t lo,int64_t hi){double v=n(j,k,double(lo),double(hi));if(std::floor(v)!=v)throw std::runtime_error("Fractional security count");return int64_t(v);}
std::string text(const J& j,const char* k){auto s=j.at(k).get<std::string>();if(s.size()>240)throw std::runtime_error("Oversized security identity");return s;}
void collection(const J& j,size_t max){if(!j.is_array()||j.size()>max)throw std::runtime_error("Oversized security collection");}
}
void decodeSecurity(Economy& e,const J& j){
 if(!j.is_object()||(j.at("schema")!=1&&j.at("schema")!=2))throw std::runtime_error("Unsupported security save");
 const auto& systems=j.at("systems");collection(systems,20000);for(const auto& v:systems){auto id=v.get<std::string>();if(!e.systems.count(id)||!e.securitySystems.insert(id).second)throw std::runtime_error("Unknown or repeated security system");}
 const auto& bands=j.at("pirates");collection(bands,20000);std::set<std::string> ids,bandStars;
 for(const auto& v:bands){PirateBand b;b.id=text(v,"id");b.name=text(v,"name");b.star=text(v,"star");b.haven=text(v,"haven");auto* p=findSettlement(e,b.haven);
  if(b.id.empty()||!ids.insert(b.id).second||!bandStars.insert(b.star).second||!e.securitySystems.count(b.star)||!p||p->star!=b.star)throw std::runtime_error("Invalid pirate organization graph");
  const auto& stock=v.at("stock");collection(stock,goodCount);if(stock.size()!=goodCount)throw std::runtime_error("Invalid pirate stores");for(int i=0;i<goodCount;i++){b.stock[i]=stock[i].get<double>();if(!std::isfinite(b.stock[i])||b.stock[i]<0||b.stock[i]>600)throw std::runtime_error("Invalid pirate cargo");}if(stockTotal(b.stock)>600.001)throw std::runtime_error("Pirate stores exceed capacity");
  b.treasury=integer(v,"treasury",0,1000000000000);b.grievance=n(v,"grievance",0,100);b.truceUntil=n(v,"truceUntil",0,1e12);b.nextRaid=n(v,"nextRaid",0,1e12);b.stolen=n(v,"stolen",0,1e12);b.raids=int(integer(v,"raids",0,1000000000));e.pirates.push_back(b);
 }
 const auto& captains=j.at("captains");collection(captains,160000);ids.clear();
 for(const auto& v:captains){Captain c;if(j.at("schema")==2)c.genome=decodeGenome(v.at("genome"));c.id=text(v,"id");c.name=text(v,"name");c.star=text(v,"star");c.home=text(v,"home");c.band=text(v,"band");auto* p=findSettlement(e,c.home);auto* band=findPirates(e,c.band);
  if(c.id.empty()||!ids.insert(c.id).second||!e.securitySystems.count(c.star)||!p||p->star!=c.star||(!c.band.empty()&&(!band||band->star!=c.star||band->haven!=c.home)))throw std::runtime_error("Invalid captain allegiance graph");
  c.faction=int(integer(v,"faction",1,4));c.role=int(integer(v,"role",0,3));c.defeats=int(integer(v,"defeats",0,1000000));c.victories=int(integer(v,"victories",0,1000000));c.destroyed=v.at("destroyed").get<bool>();c.captured=v.value("captured",false);if(c.captured&&!c.destroyed)throw std::runtime_error("Captured captain still owns a ship");c.readyAt=n(v,"readyAt",0,1e12);c.grudge=n(v,"grudge",0,100);
  c.hull=n(v,"hull",0,240);c.shield=n(v,"shield",0,100);c.armour=n(v,"armour",0,100);c.energy=n(v,"energy",0,100);c.heat=n(v,"heat",0,100);
  const auto& modules=v.at("modules");collection(modules,3);if(modules.size()!=3)throw std::runtime_error("Invalid captain modules");for(int i=0;i<3;i++){c.modules[i]=modules[i].get<double>();if(!std::isfinite(c.modules[i])||c.modules[i]<0||c.modules[i]>100)throw std::runtime_error("Invalid captain module");}
  c.rounds=int(integer(v,"rounds",0,180));c.missiles=int(integer(v,"missiles",0,2));c.decoys=int(integer(v,"decoys",0,2));
  e.captains.push_back(c);
 }
}
}
