#include "game.hpp"
#include "flight_scene.hpp"
#include "flight/world_objects.hpp"
#include "wildlife.hpp"
namespace hyperion {
namespace {
std::array<double,9> salvageFor(const WorldObject& object){
 std::array<double,9> stock{};
 if(object.kind==RockObject){stock[Ore]=std::min(8.,object.scale*1.3);if(object.good==Crystal)stock[Crystal]=std::min(3.,object.scale*.4);}
 else if(object.kind==MinorObjectKind){stock[Ore]=std::min(100.,object.scale*2);stock[Crystal]=object.scale*.15;stock[Propellant]=object.scale*.3;}
 else if(object.kind==PanelObject||object.kind==PortObject){stock[Alloy]=std::min(20.,object.scale*.15);stock[Components]=std::min(5.,object.scale*.025);}
 for(int good:{Alloy,Crystal,Biomass,Research})stock[good]=std::floor(stock[good]);return stock;
}
}
void Game::clearFieldContact(const Contact& contact){
 ensureFlight();auto& e=s.economy;auto& f=s.flight;auto planet=activePlanet();auto normal=surfaceNormal(contact.pos);
 WorldObject object;object.id="field:"+contact.id;object.kind=contact.kind=="life"||contact.kind=="flora"?LifeObject:contact.kind=="mineral"?RockObject:PanelObject;object.body=planet.index;object.capacity=1;object.scale=1;object.up=normal;object.position=f.planets[planet.index].centre+normal*(f.planets[planet.index].radius+flight::terrain(f.planets[planet.index],normal)+3);
 if(object.kind==LifeObject){
  LifeView view;view.centre=s.mode==Mode::Ocean?V2{s.oceanX,s.oceanAtY}:s.surface;view.observer=view.centre;view.observerDepth=s.depth;view.depth=s.depth;view.ocean=s.mode==Mode::Ocean;view.hours=s.hours;view.radius=100+s.upgrades[1]*25;view.limit=80;
  for(const auto& a:visibleLife(e,universe,planet,view))if(a.contact.id==contact.id){object.id=a.id;object.mass=std::clamp(a.size*.08,.05,.8);break;}
  object.habitat=ecosystemKey(planet,contact.habitatPos,contact.habitatDepth,contact.habitatBiome);object.habitatAt={contact.habitatPos.x,contact.habitatDepth,contact.habitatPos.y};object.habitatBiome=contact.habitatBiome;object.population=contact.n;object.good=contact.kind=="flora"?3:4;object.mass=std::max(.05,object.mass);
 }
 if(e.worldDamage.size()>=60000)return;
 int index=int(f.worldObjects.size());f.worldObjects.push_back(object);flight::Event event{"worldHit",index,0,object.id,"NOMA",object.id};event.amount=1;event.weapon=flight::Kinetic;event.position=object.position;processWorldHit(event);
 auto record=e.worldDamage.find(object.id);if(record!=e.worldDamage.end())record->second.salvage={};e.damageRevision++;syncWorldDamage();
 if(auto* wreck=findOceanWreck(e,contact.id)){for(int good=0;good<goodCount;good++){wreck->destroyed[good]+=wreck->remaining[good];wreck->remaining[good]=0;}wreck->breached=true;wreck->surveyed=true;}
 if(diveState.goal.id==contact.id)diveState.goal={};focusedContact.clear();
}
void Game::syncWorldDamage(){
 auto& f=s.flight;auto& e=s.economy;bool changed=f.damageRevision!=e.damageRevision||f.damageStar!=s.currentStar;
 if(changed){f.damageRevision=e.damageRevision;f.damageStar=s.currentStar;auto previous=std::move(f.worldDamage);f.worldDamage.clear();for(const auto& [id,damage]:e.worldDamage)if(damage.star==s.currentStar){auto copy=damage;if(copy.structure.valid){auto old=previous.find(id);copy.structure.localBorn=old!=previous.end()&&old->second.structure.born==copy.structure.born?old->second.structure.localBorn:f.time-std::max(0.,(s.hours-copy.structure.born)*55.);}f.worldDamage.emplace(id,copy);}
  for(auto& port:f.ports)port.disabled=objectIntegrity(f.worldDamage,portObjectId(port.id))<=0;
  for(auto& body:f.minorBodies)body.destroyed=objectIntegrity(f.worldDamage,"body:"+body.source.id)<=0;
 }
 if(changed||flight::length(f.ship.position-f.scarAnchor)>40){f.scarAnchor=f.ship.position;uint64_t hash=1469598103934665603ull;
  for(int i=0;i<int(f.planets.size());i++){auto& planet=f.planets[i];planet.scars.clear();std::vector<std::pair<double,const WorldDamage*>> near;
   for(const auto& entry:f.worldDamage){const auto& d=entry.second;if(d.kind==ImpactObject&&d.body==i&&d.owner=="planet:"+std::to_string(i)&&flight::length(d.position-f.ship.position)<450)near.push_back({flight::length(d.position-f.ship.position),&d});}
   std::sort(near.begin(),near.end(),[](const auto& a,const auto& b){return a.first<b.first;});if(near.size()>64)near.resize(64);
   for(const auto& entry:near){const auto& d=*entry.second;planet.scars.push_back({d.normal,d.scale,std::min(2.8,d.scale*.18)});for(double n:{d.normal.x,d.normal.y,d.normal.z,d.scale}){hash^=uint64_t(int64_t(n*1e6));hash*=1099511628211ull;}}
  }
  f.scarKey=hash;
 }
 f.geometryRevision=e.geometryRevision^f.scarKey;for(auto& wreck:f.wrecks)if(wreck.id.rfind("world:body:",0)==0)for(const auto& body:f.minorBodies)if(wreck.id=="world:body:"+body.source.id)wreck.position=body.centre;if(!changed)return;
 for(auto& wreck:f.wrecks){if(wreck.id.rfind("world:",0)==0){auto it=e.worldDamage.find(wreck.id.substr(6));if(it!=e.worldDamage.end()){wreck.supplies=it->second.salvage;wreck.debrisKind=it->second.kind+1;}else{wreck.supplies={};wreck.life=0;}}
  if(objectIntegrity(f.worldDamage,"wreck:"+wreck.id)<=0){wreck.life=0;wreck.cargo={};wreck.supplies={};}
 }
 for(const auto& [id,d]:f.worldDamage)if(d.integrity<=0&&stockTotal(d.salvage)>.001){
  if(std::any_of(f.wrecks.begin(),f.wrecks.end(),[wreckId=std::string("world:")+id](const auto&w){return w.id==wreckId;}))continue;
  auto at=d.position;if(d.kind==MinorObjectKind)for(const auto& body:f.minorBodies)if("body:"+body.source.id==id)at=body.centre;
  if(f.wrecks.size()<48){flight::Wreck wreck;wreck.id="world:"+id;wreck.position=at;wreck.debrisKind=d.kind+1;wreck.supplies=d.salvage;f.wrecks.push_back(wreck);}
 }
}
void Game::prepareWorldObjects(){
 syncWorldDamage();auto& f=s.flight;f.worldObjects=flight::worldObjects(f,flightScene(*this));
 for(const auto& wreck:f.wrecks)if(wreck.life>0&&flight::length(wreck.position-f.ship.position)<1500){WorldObject object;object.id="wreck:"+wreck.id;object.owner=wreck.id;object.kind=SalvageObject;object.position=wreck.position;object.half={9,6,12};object.capacity=100;object.scale=10;object.integrity=objectIntegrity(f.worldDamage,object.id);f.worldObjects.push_back(object);}
}
void Game::processWorldHit(const flight::Event& event){
 auto& f=s.flight;auto& e=s.economy;
 if(event.kind=="minorGroundHit"){
  if(event.port<0||event.port>=int(f.minorBodies.size()))return;const auto& body=f.minorBodies[event.port];auto normal=flight::unit(event.position-body.centre);std::string id="impact:"+body.source.id+":"+std::to_string(int(normal.x*32))+":"+std::to_string(int(normal.y*32))+":"+std::to_string(int(normal.z*32));
  if(!e.worldDamage.count(id)&&e.worldDamage.size()>=60000)return;auto& d=e.worldDamage[id];d.star=s.currentStar;d.kind=ImpactObject;d.owner=body.source.id;d.body=body.source.parent;d.position=event.position;d.normal=normal;d.integrity=0;d.scale=event.weapon==flight::Seeker?5:2;e.damageRevision++;e.geometryRevision++;syncWorldDamage();return;
 }
 if(event.kind=="groundHit"){
  int body=event.port;if(body<0||body>=int(f.planets.size()))return;auto& p=f.planets[body];auto delta=event.position-p.centre;if(std::abs(flight::length(delta)-p.radius)>40)return;
  auto cell=[&](double n){return std::to_string(int(std::floor(n/8)));};std::string id="impact:"+s.currentStar+":"+std::to_string(body)+":"+cell(delta.x)+":"+cell(delta.y)+":"+cell(delta.z);
  if(!e.worldDamage.count(id)&&e.worldDamage.size()>=60000)return;auto& damage=e.worldDamage[id];double oldScale=damage.scale;bool fresh=damage.star.empty();damage.star=s.currentStar;damage.kind=ImpactObject;damage.owner="planet:"+std::to_string(body);damage.body=body;damage.position=event.position;damage.normal=flight::unit(delta);damage.integrity=0;damage.scale=std::max(damage.scale,event.weapon==flight::Seeker?8.:event.weapon==flight::Plasma?5.:1.5);
  if(fresh||damage.scale!=oldScale){e.damageRevision++;e.geometryRevision++;syncWorldDamage();}return;
 }
 if(event.port<0||event.port>=int(f.worldObjects.size()))return;const auto& object=f.worldObjects[event.port];if(object.id!=event.victim||event.amount<=0)return;
 auto* site=findSettlement(e,object.owner);double previous=1;WorldDamage* record=nullptr;
 if(object.kind==FacilityObject){if(!site)return;previous=facilityIntegrity(*site,object.slot/24,object.slot%24);}
 else{auto it=e.worldDamage.find(object.id);previous=it==e.worldDamage.end()?1:it->second.integrity;if(it==e.worldDamage.end()&&e.worldDamage.size()>=60000)return;record=&e.worldDamage[object.id];record->star=s.currentStar;record->kind=object.kind;record->body=object.body;record->slot=object.slot;record->owner=object.owner;record->good=object.good;record->position=object.position;record->normal=object.up;record->scale=std::clamp(object.scale,.1,4096.);}
 if(previous<=0)return;double hp=std::max(0.,previous-event.amount/object.capacity);bool collapsed=hp<=0;
 if(object.kind==FacilityObject){site->facilityIntegrity[object.slot]=hp;site->condition=std::max(0.,site->condition-event.amount*.00004);site->bottleneck=collapsed?"Infrastructure destroyed / rebuild required":"Infrastructure damaged";}
 else record->integrity=hp;
 if(previous>=1&&event.actor=="NOMA"&&object.faction>0&&object.kind<=PlantObject){auto& faction=e.factions[object.faction];faction.standing=std::max(-100.,faction.standing-8);faction.warrant=std::min(int64_t(1000000),faction.warrant+100);economicEvent(e,site?site->name:s.currentStar,"NOMA attacked civilian infrastructure / 100-credit warrant issued.");}
 if(collapsed){
  if(object.structureProfile>=0&&object.structureProfile<flight::StructureProfileCount&&(record||e.worldDamage.size()<60000)){
   if(!record){record=&e.worldDamage[object.id];record->star=s.currentStar;record->owner=object.owner;record->kind=object.kind;record->body=object.body;record->slot=object.slot;record->position=object.position;record->normal=object.up;record->scale=object.scale;record->integrity=0;}
   auto& shape=record->structure;shape.valid=true;shape.orbital=object.structureProfile==flight::StationCore||object.structureProfile==flight::StationWing;shape.profile=object.structureProfile;shape.variant=object.structureVariant;shape.ordinal=object.kind==FacilityObject?object.slot%24:0;shape.faction=object.structureFaction;shape.origin=object.structureOrigin;shape.right=object.right;shape.up=object.up;shape.back=object.back;shape.born=s.hours;shape.seed=flight::sceneHash(object.id+":"+std::to_string(s.hours));auto hit=event.position-shape.origin;shape.impact={flight::dot(hit,shape.right),flight::dot(hit,shape.up),flight::dot(hit,shape.back)};
  }
  if(object.kind==PortObject&&object.owner.rfind("den:",0)==0)if(auto* band=findPirates(e,object.owner.substr(4))){band->stock={};band->truceUntil=0;if(event.actor=="NOMA"){band->grievance=std::min(100.,band->grievance+30);for(auto& captain:e.captains)if(captain.band==band->id)captain.grudge=std::min(100.,captain.grudge+25);}economicEvent(e,band->name,"Anchorage destroyed / stored supplies lost / surviving captains retain their vendetta.");}
  if(record)record->salvage=salvageFor(object);
  if(site&&object.kind==FacilityObject){int building=object.slot/24;
   if(building==Habitat){int lost=std::min(site->residents,1+site->residents/std::max(1,site->built[Habitat]*6));site->residents-=lost;site->health=std::max(0.,site->health-6);site->morale=std::max(0.,site->morale-12);}
   if(building==Warehouse)for(auto& stock:site->stock)stock*=.90;
   if(building==Reactor||building==FuelPlant){site->pollution=std::min(1.,site->pollution+.08);if(auto* habitat=settlementHabitat(e,*site)){habitat->pollution=std::min(1.,habitat->pollution+.08);habitat->quality=std::max(0.,habitat->quality-.03);}}
   economicEvent(e,site->name,std::string(buildings[building].name)+" destroyed / production and services reduced / reconstruction required.");
  }
  if(site&&object.kind==CargoObject&&object.good>=0){double removed=std::min(20.,site->stock[object.good]);site->stock[object.good]-=removed;if(record)record->salvage[object.good]=(object.good==Alloy||object.good==Crystal||object.good==Biomass||object.good==Research)?std::floor(removed*.25):removed*.25;}
  if(object.kind==LifeObject){auto planet=universe.planet(s.currentStar,object.body);auto& habitat=ensureEcosystem(e,universe,planet,{object.habitatAt.x,object.habitatAt.z},object.habitatAt.y,object.habitatBiome);auto it=e.ecosystems.find(object.habitat);Ecosystem& ecosystem=it==e.ecosystems.end()?habitat:it->second;
   bool found=false;for(auto& population:ecosystem.populations)if(population.seed==object.population){double killed=std::min(population.mass,object.mass);population.mass-=killed;ecosystem.detritus+=killed;found=true;break;}if(!found)for(auto& population:ecosystem.populations)if(population.flora==(object.good==3)){double killed=std::min(population.mass,object.mass);population.mass-=killed;ecosystem.detritus+=killed;break;}
  }
  if(object.kind==SalvageObject){for(auto& wreck:f.wrecks)if(wreck.id==object.owner){wreck.life=0;wreck.cargo={};wreck.supplies={};}if(object.owner.rfind("world:",0)==0){auto parent=e.worldDamage.find(object.owner.substr(6));if(parent!=e.worldDamage.end())parent->second.salvage={};}}
  if(event.actor=="NOMA"&&object.faction>0&&object.kind<=PlantObject){auto& faction=e.factions[object.faction];faction.standing=std::max(-100.,faction.standing-10);faction.warrant=std::min(int64_t(1000000),faction.warrant+250);}
  e.geometryRevision++;notify(object.kind==LifeObject?"Organism lost / biomass returned to its ecosystem":object.kind==RockObject||object.kind==MinorObjectKind?"Material fractured / finite salvage released":"Object destroyed / consequences recorded",Notice::Important);
 }
 e.damageRevision++;syncWorldDamage();
 if(collapsed&&object.kind==PortObject&&f.docked&&f.dockedPort>=0&&portObjectId(f.ports[f.dockedPort].id)==object.id){f.docked=false;f.dockedPort=-1;f.savedDock.clear();e.docked.clear();f.ship.position+=object.up*8;f.damageShip(25,flight::CollisionDamage,object.position);notify("Berth destroyed / emergency launch",Notice::Critical);}
}
}
