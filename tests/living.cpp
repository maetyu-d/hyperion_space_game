#include "game.hpp"
#include "biosphere.hpp"
#include "actor_storage.hpp"
#include "economy_storage.hpp"
#include "flight_storage.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace hyperion;using J=nlohmann::json;
namespace {
int checks=0;void require(bool b,const char* why){checks++;if(!b)throw std::runtime_error(why);}
hyperion::flight::Game arena(){hyperion::flight::Game f;f.valid=true;f.docked=false;f.planets.push_back({"Test","",{10000,0,0},500,3,{0,1,0}});f.ports.push_back({"Relay","",{5000,5000,5000},{0,1,0},-1,"relay:test","",0});f.ship.gear=false;f.orient({0,0,-1},{0,1,0});return f;}
}
int main(){try{
 std::set<std::string> designs,pilots;std::set<int> weapons,shapes,roles;std::set<double> hulls;
 for(uint32_t seed=0;seed<1024;seed++)for(int role=0;role<4;role++){
  hyperion::flight::EnemyGenome g{1,seed,false};auto a=hyperion::flight::enemyDef(role,g),b=hyperion::flight::enemyDef(role,g),base=hyperion::flight::enemyDef(role);
  require(a.hull==b.hull&&a.weapon==b.weapon&&a.speed==b.speed,"actor design is deterministic");
  require(a.hull>=base.hull*.85&&a.hull<=base.hull*1.15&&a.shield<=100&&a.speed>50&&a.radius>4&&a.radius<16&&a.weapon!=hyperion::flight::Seeker,"generated frame and finite primary mounts stay bounded");
  designs.insert(hyperion::flight::vesselDesignation(g));pilots.insert(hyperion::flight::pilotTemperament(g));weapons.insert(a.weapon);shapes.insert(hyperion::flight::hullVariant(g));hulls.insert(a.hull);
  auto original=hyperion::flight::enemyDef(role,{});require(original.hull==base.hull&&original.weapon==base.weapon&&original.aimJitter==0,"revision zero retains fixed frame combat behavior");
  auto legacy=hyperion::flight::enemyDef(role,{1,seed,true});require(legacy.hull==base.hull&&legacy.shield==base.shield&&legacy.weapon==base.weapon,"migrated enemies keep their original fitted equipment");
 }
 require(designs.size()>1000&&pilots.size()==4&&weapons.size()==4&&shapes.size()==8&&hulls.size()>60,"large seed sample has real equipment, hull, pilot and silhouette diversity");
 {Game a,b; a.enterFlight();b.enterFlight();require(encodeEconomy(a.s.economy)==encodeEconomy(b.s.economy),"campaign security generation is repeatable");for(const auto& c:a.s.economy.captains){roles.insert(c.role);require(c.genome.revision==1&&!c.genome.legacyEquipment,"new captains have generated equipment");auto t=std::find_if(a.s.flight.traffic.begin(),a.s.flight.traffic.end(),[&](const auto& t){return t.id==c.id;});require(t!=a.s.flight.traffic.end()&&t->genome==c.genome&&t->health==hyperion::flight::enemyDef(*t).hull,"canonical actor design reaches actual flight");}
  auto before=encodeEconomy(a.s.economy);a.ensureEconomy();a.refreshSecurity();require(before==encodeEconomy(a.s.economy),"inspection cannot regenerate actors or consume supplies");
  auto& c=a.s.economy.captains.back();auto original=c.genome;c.destroyed=true;c.hull=0;c.defeats=3;c.grudge=40;c.readyAt=a.s.economy.hour;auto* band=findPirates(a.s.economy,c.band);band->stock={};band->treasury=240;advanceSecurity(a.s.economy);require(c.destroyed,"procedural enemy cannot replace a ship without materials");band->stock[Alloy]=8;band->stock[Components]=4;band->stock[Propellant]=4;a.s.economy.hour=c.readyAt;advanceSecurity(a.s.economy);require(!c.destroyed&&c.genome==original&&c.hull==hyperion::flight::enemyDef(c.role,c.genome).hull&&c.defeats==3&&c.grudge==40,"funded replacement preserves generated identity and career");require(stockTotal(band->stock)==0&&band->treasury==0,"generated ship replacement consumes finite materials and funds");
 }
 {int fired=0;std::set<int> actualWeapons;for(uint32_t seed=0;seed<40;seed++){auto f=arena();hyperion::flight::Traffic t;t.kind=1;t.role=0;t.genome={1,seed,false};t.id="generated";t.position=t.previous={0,0,-240};t.positioned=true;t.forward={0,0,1};auto spec=hyperion::flight::enemyDef(t);t.health=spec.hull;t.shield=spec.shield;f.traffic.push_back(t);for(int step=0;step<140;step++){f.update(.025);for(const auto& bolt:f.bolts)if(bolt.owner==t.id)actualWeapons.insert(bolt.weapon);}fired+=f.traffic[0].salvos>0;require(f.traffic[0].ammunition>=0&&f.traffic[0].missiles>=0,"procedural AI uses finite magazines");}std::cerr<<"AI audit: "<<fired<<" firing actors / "<<actualWeapons.size()<<" actual primary types\n";require(fired>30&&actualWeapons.size()>=3,"generated weapon choices fire through the actual combat simulation");
  auto resolute=arena(),cautious=arena();uint32_t r=0,c=0;for(uint32_t seed=0;seed<100;seed++){std::string pilot=hyperion::flight::pilotTemperament({1,seed,false});if(pilot=="Resolute")r=seed;if(pilot=="Opportunist")c=seed;}
  for(auto pair:{std::pair<hyperion::flight::Game*,uint32_t>{&resolute,r},{&cautious,c}}){hyperion::flight::Traffic t;t.kind=1;t.genome={1,pair.second,false};t.health=hyperion::flight::enemyDef(t).hull*.25;t.position=t.previous={0,0,-240};t.positioned=true;t.forward={0,0,1};t.id="temperament";pair.first->traffic.push_back(t);pair.first->update(.025);}
  require(resolute.traffic[0].state!=4&&cautious.traffic[0].state==4,"pilot temperament affects an actual withdrawal decision");
 }
 Universe u;auto world=u.planet("HOME",2);std::set<int> plans,features;int organisms=0,sites=0;
 for(int x=-12;x<=12;x++)for(int y=-12;y<=12;y++){
  auto list=u.surfaceObjects(world,x,y);for(const auto& o:list){if(o.kind=="life"||o.kind=="flora"){auto b=phenotype(o);plans.insert(b.plan);organisms++;require(b.limbs>=3&&b.limbs<=8&&b.samples>=8&&b.samples<=14&&o.desc.find("Morphology:")!=std::string::npos,"generated life has bounded morphology and catalogue traits");require(u.region(world,o.pos).biomass>.045,"surface life requires a viable regional habitat");}if(o.feature){sites++;features.insert(o.feature);require(o.id.rfind("F2:",0)==0,"new landmarks use a separate identity channel");}}
  auto a=u.oceanObjects(world,x,y*130+1),b=u.oceanObjects(world,x,y*130+29);require(a.size()==b.size(),"neighboring tracks intersect the same contacts");for(size_t i=0;i<a.size();i++){require(a[i].id==b[i].id&&a[i].n==b[i].n&&a[i].name==b[i].name&&a[i].desc==b[i].desc,"moving within an ocean band cannot reroll species or features");require(a[i].depth>0&&a[i].depth<u.floorDepth(world,a[i].pos.x,a[i].pos.y),"generated sites follow the actual seafloor");if(a[i].feature)features.insert(a[i].feature);}
 }
 require(plans.size()==6&&organisms>100&&sites>60&&features.size()>=6,"survey sample includes diverse body plans and habitat features");
 {Game g;g.s.activeStar="HOME";g.s.activePlanet=2;auto a=u.oceanObjects(world,2,1),b=u.oceanObjects(world,2,20);for(size_t i=0;i<a.size();i++){require(g.catalogue(a[i]),"new regional contact can be catalogued");auto hold=g.shipStock();require(!g.catalogue(b[i])&&hold==g.shipStock(),"moving the submarine cannot duplicate sample rewards");}}
 {Game g;g.enterFlight();g.system("HOME");auto econ=encodeEconomy(g.s.economy),flight=encodeFlight(g.s.flight,g.s.economy);for(int i=0;i<120;i++)g.simulate(.1);require(econ==encodeEconomy(g.s.economy)&&flight==encodeFlight(g.s.flight,g.s.economy),"strategy thinking time freezes generated actors and all simulation clocks");
  auto path=std::filesystem::temp_directory_path()/"hyperion-living-329.json";std::string error;require(saveState(g.s,path.string(),error),"generated universe saves");State s;require(loadState(s,path.string(),error)&&encodeEconomy(s.economy)==econ,"generated identities and live equipment round trip exactly");J saved;{std::ifstream in(path);in>>saved;}
  auto corrupt=[&](J j){std::ofstream out(path);out<<j;out.close();return !loadState(s,path.string(),error)&&encodeEconomy(s.economy)==econ;};
  for(const J& bad:{J(-1),J(1.5),J(uint64_t(4294967296ull))}){auto j=saved;j["economy"]["security"]["captains"][0]["genome"]["seed"]=bad;require(corrupt(j),"invalid genome seeds are rejected transactionally");}
  auto bad=saved;for(auto& t:bad["flight"]["traffic"])if(t.value("managed",false)){t["genome"]["seed"]=123456;break;}require(corrupt(bad),"flight cannot disagree with the canonical generated captain");
  auto old=saved;old.erase("lifeRevision");old["economy"]["security"]["schema"]=1;for(auto& c:old["economy"]["security"]["captains"]){c.erase("genome");c["hull"]=41;c["shield"]=19;c["rounds"]=37;}old["flight"]["combatRevision"]=4;for(auto& t:old["flight"]["traffic"])t.erase("genome");{std::ofstream out(path);out<<old;}require(loadState(s,path.string(),error)&&s.lifeRevision==0,"3.28 profiles remain loadable");Game migration;migration.s=s;migration.universe.reset(s.seed,s.generation,s.legacySystems);migration.ensureEconomy();for(const auto& c:migration.s.economy.captains)require(c.genome.legacyEquipment&&c.hull==41&&c.shield==19&&c.rounds==37,"migration adds identity without healing or refitting existing enemies");std::filesystem::remove(path);
 }
 std::cout<<"Living universe: "<<checks<<" checks / "<<designs.size()<<" ship registries / "<<plans.size()<<" body plans / "<<features.size()<<" habitat features passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
