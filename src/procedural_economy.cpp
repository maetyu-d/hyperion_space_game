#include "economy.hpp"
#include <set>
namespace hyperion {
namespace {
bool produces(const Settlement& p,int good){for(int b=0;b<buildingCount;b++)if(workingBuildings(p,b)&&buildings[b].outputs[good]>0)return true;return false;}
double scoreSite(const Universe& u,const Planet& p,V2 at,int owner){
 auto r=u.region(p,at);bool land=r.elevation>=u.seaLevel(p);double shelter=std::exp(-std::abs(r.temperature-290)/180.)/(1+p.hazards);
 double coast=std::exp(-std::abs(r.elevation-u.seaLevel(p))*30);return shelter+coast*.25+(land?.45:-.8)+(owner==1?r.ore+r.crystal*.4:owner==2?r.biomass*5+r.moisture:owner==4?r.crystal+r.geothermal:coast+r.ore*.2);
}
}
void discoverProceduralEconomy(Economy& e,Universe& u,const std::string& star,double hour){
 auto profile=u.profile(star);auto planets=u.planets(star);std::vector<int> solid;for(const auto& p:planets)if(!p.giant)solid.push_back(p.index);
 if(solid.empty()||!profile.settlements){economicEvent(e,u.star(star).name,"Unsettled system / survey relays only. No chartered markets or invented supply network.");return;}
 Random random{u.star(star).seed^0x318bc7u};std::vector<std::string> ids;
 const char* governors[]={"Iona","Marek","Sela","Tarin","Osei","Neri","Vale","Rin","Asha","Daro","Leto","Suri","Sen","Mira","Ilex","Orin"};
 const char* sites[]={"Foundry","Garden","Exchange","Observatory","Haven","Bore","Terrace","Spindle"};
 int primary=profile.frontier=="Extraction enclave"?1:profile.frontier=="Agrarian network"?2:profile.frontier=="Archive frontier"?4:3;
 for(int slot=0;slot<profile.settlements;slot++){
  int owner=slot<2?primary:1+int(random()*4);if(profile.frontier=="Agrarian network"&&slot==2)owner=1;if(profile.frontier=="Exchange corridor"&&slot>=2&&slot<=4){const int partners[]={1,2,4};owner=partners[slot-2];}if(profile.frontier=="Contested charters")owner=slot+1;double best=-1e9;V2 point;int body=solid.front();
  // A bounded candidate search is deterministic and independent of which other
  // systems were visited. It also keeps charters from occupying the same site.
  for(int candidate=0;candidate<96;candidate++){
   int index=solid[size_t(random()*solid.size())];const auto& p=planets[index];V2 at{(random()-.5)*6500,(random()-.5)*6500};bool overlap=false;
   for(const auto& id:ids){auto* old=findSettlement(e,id);if(old->planet==index&&distance(old->position,at)<240)overlap=true;}if(overlap)continue;
   double score=scoreSite(u,p,at,owner);if(score>best){best=score;point=at;body=index;}
  }
  const auto& p=planets[body];auto region=u.region(p,point);Settlement s;s.id="PORT:"+star+":"+std::to_string(slot+1);s.star=star;s.planet=body;s.planetName=p.name;s.owner=s.authority=owner;s.position=point;
  s.name=region.name+" "+sites[owner==1?0:owner==2?1:owner==4?3:2]+" "+std::to_string(slot+1);s.governor=governors[size_t(random()*16)];
  // Offshore charters use the existing bathylift/pressure/power systems.
  if(region.elevation<u.seaLevel(p)&&p.water>=.2)s.depth=std::min(u.floorDepth(p,point.x,point.y)-80,120+random()*450);
  s.residents=16+int(random()*25);s.built[Habitat]=int(std::ceil(s.residents/12.))+1;s.built[Warehouse]=2;s.built[Solar]=2+int(random()*3);s.built[Reactor]=1;
  s.stock={30+random()*60,32+random()*36,12+random()*22,45+random()*40,65+random()*45,6+random()*10,12+random()*12,55+random()*45,5+random()*15};
  s.treasury=2200+int64_t(random()*3000);s.morale=58+random()*30;s.health=82+random()*14;s.condition=.72+random()*.26;
  s.oreDeposit=(6000+p.metal*2200)*region.ore;s.crystalDeposit=(2200+p.tectonic*5000)*region.crystal;s.ecosystem=80+region.biomass*520;s.nextContract=hour+2+slot;s.nextMigration=hour+24;
  if(owner==1){s.built[Mine]=region.ore>1.2?2:1;s.built[Refinery]=1;s.built[CrystalBore]=1;s.built[Fabricator]=1;s.policy=Industry;s.stock[Ore]+=50;}
  if(owner==2){s.built[Harvester]=2;s.built[Growhouse]=2;s.built[MedLab]=1;s.built[Restoration]=1;s.policy=Stewardship;s.stock[Rations]+=25;}
  if(owner==3){s.built[Harvester]=1;s.built[Growhouse]=1;s.built[FuelPlant]=2;s.built[Fabricator]=1;s.built[Warehouse]=3;s.stock[Propellant]+=30;}
  if(owner==4){s.built[CrystalBore]=1;s.built[Observatory]=1+int(random()*2);s.built[MedLab]=1;s.built[Growhouse]=1;s.stock[Research]+=20;}
  ids.push_back(s.id);e.settlements.push_back(s);economicEvent(e,s.name,e.factions[owner].name+" chartered a "+region.biome+" site in "+profile.frontier+". Local ore, crystal, ecology and climate determine its productive potential.");
 }
 std::set<std::tuple<std::string,std::string,int>> links;
 for(const auto& destination:ids){auto* to=findSettlement(e,destination);int linksMade=0;
  for(int good:{Rations,Propellant,Biomass,Ore,Alloy,Crystal,Components,Medicine}){
   bool demand=good==Rations||good==Propellant;for(int b=0;b<buildingCount;b++)if(to->built[b]&&buildings[b].inputs[good]>0)demand=true;
   if(!demand||produces(*to,good))continue;Settlement* from=nullptr;double best=0;
   for(const auto& source:ids)if(source!=destination){auto* p=findSettlement(e,source);if(!produces(*p,good))continue;
    double score=(p->stock[good]+12)/(1+std::abs(p->planet-to->planet)*.6+distance(p->position,to->position)/5000.);if(score>best){best=score;from=p;}}
   if(!from||!links.insert({from->id,to->id,good}).second)continue;
   TradeRoute route;route.id="NPC-"+std::to_string(e.nextId++);route.from=from->id;route.to=to->id;route.good=good;route.owner=3;route.commercial=true;route.capacity=16+int(random()*17);route.duration=3+std::abs(from->planet-to->planet)*2+distance(from->position,to->position)/2000;route.nextDeparture=hour+2+random()*2;e.routes.push_back(route);if(++linksMade>=4)break;
  }
 }
 economicEvent(e,u.star(star).name,profile.layout+" / "+profile.frontier+". "+std::to_string(ids.size())+" locally specialized charters; freight links connect real producers with dependent consumers. Missing industries must be supplied by expeditions or new outposts.");
}
}
