#include "game.hpp"
#include "flight_storage.hpp"
#include "json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <limits>
using hyperion::flight::V3;
namespace {
int checks=0;
void require(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
bool close(double a,double b){return std::abs(a-b)<1e-6;}
hyperion::flight::Game arena(){hyperion::flight::Game f;f.valid=true;f.docked=false;f.planets.push_back({"Test","",{10000,0,0},500,3,{0,1,0}});f.ports.push_back({"Relay","",{5000,5000,5000},{0,1,0},-1,"relay:test","",0});f.ship.gear=false;f.orient({0,0,-1},{0,1,0});return f;}
hyperion::flight::Traffic enemy(V3 at,int role=0){hyperion::flight::Traffic t;t.kind=1;t.id="enemy:"+std::to_string(role);t.role=role;t.home=0;t.position=t.previous=at;t.forward={0,0,1};t.positioned=true;t.health=hyperion::flight::enemyDef(role).hull;t.shield=hyperion::flight::enemyDef(role).shield;t.fireClock=100;t.countermeasures=0;return t;}
hyperion::flight::Bolt bolt(V3 at,V3 velocity,bool hostile=false,int weapon=-1,double damage=0){hyperion::flight::Bolt b{at,at,velocity,1,hostile};b.weapon=weapon;b.damage=damage;b.owner=hostile?"enemy":"NOMA";return b;}
}
int main(){try{
 // Layered damage, recovery delay, physical modules, legacy pulse migration.
 {auto f=arena();f.damageShip(20,0,{0,0,-10});require(f.ship.shield<100&&f.ship.hull==100,"shields absorb a front hit before hull");double shield=f.ship.shield;f.update(.05);require(f.ship.shield==shield,"shield recharge waits after a hit");for(int i=0;i<100;i++)f.update(.05);require(f.ship.shield>shield,"shields recover after the delay");
  f.ship.shield=0;f.damageShip(25,1,{0,0,-10});require(f.ship.hull<100&&f.ship.combat.armour<100,"plating wears while reducing hull damage");bool module=false;for(double v:f.ship.combat.systems)module|=v<100;require(module,"hull penetration damages a ship system");
  auto bare=arena();bare.ship.shield=0;bare.ship.combat.armour=0;bare.damageShip(25,1,{0,0,-10});require(f.ship.hull>bare.ship.hull,"intact plating reduces hull loss");
  f=arena();f.ship.shield=8;f.damageShip(12,-1,{0,0,10});require(f.ship.shield==0&&f.ship.hull==96,"legacy projectiles retain their saved damage");auto copy=f.ship;f.damageShip(std::numeric_limits<double>::quiet_NaN(),0,{});require(f.ship.hull==copy.hull,"nonfinite damage cannot corrupt the ship");
  auto healthy=arena(),damaged=arena();damaged.ship.combat.systems[hyperion::flight::Drive]=0;hyperion::flight::Input in;in.throttle=1;for(int i=0;i<30;i++){healthy.update(.05,in);damaged.update(.05,in);}require(healthy.speed()>damaged.speed()*1.5,"drive damage lowers actual acceleration");
  f=arena();f.ship.shield=100;f.ship.combat.systems[hyperion::flight::ShieldGenerator]=20;f.update(0);require(f.ship.shield==20,"generator damage limits shield capacity");
 }
 // Three primaries, finite ammo and power, heat latch, and docked firing guard.
 {hyperion::flight::Input in;in.fire=true;for(int type=0;type<3;type++){auto f=arena();f.ship.combat.weapon=type;f.update(0,in);require(f.bolts.size()==1&&f.bolts[0].weapon==type,"selected primary produces its actual projectile");require(f.ship.combat.energy<100&&f.ship.combat.heat>0,"firing consumes power and adds heat");require(f.ship.combat.rounds==(type==1?299:300),"only kinetic fire consumes rounds");}
  auto f=arena();f.ship.combat.weapon=1;f.ship.combat.rounds=0;f.update(0,in);require(f.bolts.empty(),"empty kinetic magazine cannot fire");f.ship.combat.weapon=2;f.ship.combat.energy=0;f.update(0,in);require(f.bolts.empty(),"empty capacitor cannot fire plasma");f.ship.combat.energy=100;f.ship.combat.heat=99;f.update(0,in);require(f.ship.combat.overheated,"heat limit latches mounts off");auto count=f.bolts.size();f.fireClock=0;f.update(0,in);require(f.bolts.size()==count,"overheated weapons cannot fire");for(int i=0;i<100;i++)f.update(.05);require(!f.ship.combat.overheated&&f.ship.combat.heat<42,"cooling releases the heat latch");
  f=arena();f.docked=true;in.missile=in.decoy=true;f.lock=1;f.update(0,in);require(f.bolts.empty()&&f.ship.combat.decoys==6,"docked weapons and countermeasures are inhibited");
 }
 // Seeker cone / occlusion lock, ammo and input edges; countermeasures break guidance.
 {auto f=arena();f.traffic.push_back(enemy({0,0,-350}));f.combatTarget=f.traffic[0].id;for(int i=0;i<25;i++)f.update(.05);require(f.lock>.99,"sustained clear forward tracking acquires lock");hyperion::flight::Input in;in.missile=true;f.update(0,in);require(f.ship.combat.missiles==5&&f.bolts.back().weapon==3&&!f.bolts.back().target.empty(),"locked missile consumes one seeker and stores guidance target");f.lock=1;f.ship.combat.missileClock=0;f.update(0,in);require(f.ship.combat.missiles==5,"held secondary key cannot repeat across substeps");
  f=arena();f.planets[0].centre={0,0,-550};f.planets[0].radius=180;f.traffic.push_back(enemy({0,0,-1000}));f.combatTarget=f.traffic[0].id;for(int i=0;i<25;i++)f.update(.05);require(f.lock==0,"planet occlusion blocks missile lock");
  f=arena();auto missile=bolt({0,0,-300},{0,0,300},true,3,50);missile.target="NOMA";f.bolts.push_back(missile);in={};in.decoy=true;f.update(0,in);require(f.ship.combat.decoys==5&&f.bolts[0].target.empty(),"decoy consumes stock and clears an incoming seeker lock");f.update(0,in);require(f.ship.combat.decoys==5,"held countermeasure key cannot spend another charge");
  f=arena();f.traffic.push_back(enemy({150,0,-500}));missile=bolt({0,0,0},{0,0,-320},false,3,68);missile.target=f.traffic[0].id;f.bolts.push_back(missile);f.update(.05);require(f.bolts[0].velocity.x>0,"guided missile turns toward its moving target");
 }
 // Secondary weapons respect the same hard stream limit as primaries and AI.
 {auto f=arena();for(int i=0;i<768;i++)f.bolts.push_back(bolt({3000,double(i),0},{},false,0,18));f.lock=1;hyperion::flight::Input input;input.missile=true;f.update(0,input);require(f.bolts.size()==768&&f.ship.combat.missiles==6,"seeker cannot exceed the global projectile population bound");}
 // Swept nearest collision, moving participants, lifetime clipping, terrain occlusion.
 {auto f=arena();for(double z:{-40.,-20.}){hyperion::flight::Traffic t;t.kind=2;t.id=std::to_string(z);t.position=t.previous={0,0,z};t.positioned=true;f.traffic.push_back(t);}f.bolts.push_back(bolt({}, {0,0,-2000}));f.update(.05);require(f.traffic[1].health==66&&f.traffic[0].health==100,"swept collision hits nearest entity independent of array order");
  f=arena();hyperion::flight::Traffic moving;moving.kind=2;moving.positioned=true;moving.id="crossing";moving.previous={-30,0,-25};moving.position={30,0,-25};f.traffic.push_back(moving);f.bolts.push_back(bolt({}, {0,0,-1000}));f.update(.05);require(f.traffic[0].health==66,"relative swept collision catches a crossing convoy");
  f=arena();f.ship.assist=false;f.ship.velocity={800,0,0};f.bolts.push_back(bolt({20,0,-30},{0,0,1200},true));f.update(.05);require(f.ship.shield<100,"relative sweep catches a moving player crossing a projectile");
  f=arena();moving.position=moving.previous={0,0,-40};f.traffic.push_back(moving);auto shortLife=bolt({}, {0,0,-1000});shortLife.life=.01;f.bolts.push_back(shortLife);f.update(.05);require(f.traffic[0].health==100&&f.bolts.empty(),"projectile expires before reaching a distant target");
  f=arena();f.planets[0].centre={0,0,-600};f.planets[0].radius=100;moving.position=moving.previous={0,0,-1000};f.traffic.push_back(moving);f.bolts.push_back(bolt({}, {0,0,-24000}));f.update(.05);require(f.bolts.empty()&&f.traffic[0].health==100,"terrain takes the first hit and protects a target behind the planet");
  f=arena();auto t=enemy({0,0,-40});t.health=1;t.shield=0;f.traffic.push_back(t);f.damageTraffic(0,100,1,t.position,true);f.damageTraffic(0,100,1,t.position,true);require(f.kills==1&&f.events.size()==1&&f.wrecks.size()==1,"destruction emits one bounty and one salvage wreck");
 }
 // Feedback follows actual shield and hull outcomes, including partial penetration.
 {using namespace hyperion::flight;auto f=arena();auto t=enemy({0,0,-60});t.shield=2;f.traffic.push_back(t);
  f.damageTraffic(0,20,Pulse,{0,0,-52},true);
  auto count=[&](int kind){return std::count_if(f.effects.begin(),f.effects.end(),[&](const auto&e){return e.kind==kind;});};
  require(count(ShieldImpact)==1&&count(HullImpact)==1&&count(ShieldBreak)==1,"a penetrating shield break displays both damage layers");
  require(f.feedback.breaks==1&&f.feedback.hullHits==1&&f.feedback.confirmation==ShieldBreak,"break confirmation follows real damage");
  f.damageTraffic(0,1,Kinetic,{0,0,-52},true);require(count(ShieldBreak)==1,"depleted shields cannot repeatedly break");
  require(f.effects[0].normal.z>.99&&f.effects[0].weapon==Pulse,"impact plane and weapon identity survive effect creation");
  f.damageTraffic(0,10000,Seeker,{0,0,-52},true);require(count(ShipExplosion)==1&&f.feedback.destructions==1&&f.feedback.confirmation==ShipExplosion,"destruction supersedes hit feedback once");
  f.damageTraffic(0,10000,Seeker,{},true);require(f.feedback.destructions==1,"dead ships cannot replay the destruction cue");
  f=arena();f.ship.shield=2;f.damageShip(20,Pulse,{0,0,-20});require(f.feedback.breaks==1&&f.feedback.shieldBreak>0&&f.feedback.impact>0,"player shield failure creates directional feedback");
  f=arena();Input input;input.fire=true;auto forward=f.ship.forward;auto velocity=f.ship.velocity;f.updateCombat(0,input);
  require(f.feedback.shots[Pulse]==1&&f.feedback.recoil>0,"successful shot emits recoil and sound sequence");
  require(length(f.ship.forward-forward)==0&&length(f.ship.velocity-velocity)==0,"presentation recoil never changes aim or momentum");
  input.fire=false;for(int i=0;i<60;i++)f.prepareCombat(.05,input);require(f.feedback.recoil<1e-12&&f.effects.empty(),"feedback decays fully on simulation time");
  f=arena();f.ship.combat.energy=0;input.fire=true;f.updateCombat(0,input);require(f.feedback.shots[Pulse]==0&&f.effects.empty(),"rejected firing creates no phantom feedback");
 }
 // Enemy roles, safety buffers, retreat, offscreen reinforcements and bounded effects.
 {auto f=arena();for(int role=0;role<3;role++){auto t=enemy({double(role*60),0,-200},role);t.fireClock=0;f.traffic.push_back(t);}for(int i=0;i<100;i++)f.update(.05);bool fired=false;for(auto& b:f.bolts)fired|=b.hostile;require(fired||f.ship.shield<100,"enemy wing actively attacks outside port protection");require(hyperion::flight::enemyDef(1).speed>hyperion::flight::enemyDef(0).speed&&hyperion::flight::enemyDef(2).hull>hyperion::flight::enemyDef(0).hull,"interceptor and marauder have distinct tactical capabilities");
  f=arena();auto t=enemy({0,0,-200});t.health=10;f.traffic.push_back(t);f.update(.05);require(f.traffic[0].state==4,"crippled raider retreats");
  f=arena();t=enemy({0,0,-200});t.fireClock=0;f.traffic.push_back(t);f.ports[0].position={};f.ports[0].settlement="staffed";f.update(.05);require(f.bolts.empty()&&f.traffic[0].state==0,"staffed port buffer suppresses pirate aggression");
  f=arena();t=enemy({0,0,-2100});t.dead=true;t.respawn=0;f.traffic.push_back(t);f.ports[0].position={0,0,-1700};f.update(.05);require(f.traffic[0].dead,"reinforcements cannot visibly appear at a nearby home port");
  for(int i=0;i<300;i++)f.effect({},2);require(f.effects.size()==128,"combat effects have a hard population bound");for(int i=0;i<60;i++)f.update(.05);require(f.effects.empty(),"transient effects expire on simulation time");
 }
 // Port rearming and repair conserve warehouse stock and money. Salvage depletes one wreck.
 {hyperion::Game g;g.enterFlight();auto& f=g.s.flight;auto& p=g.s.economy.settlements.front();int berth=g.flightPort(p.id,true);f.dockAt(berth);g.processFlightEvents();p.owner=0;f.worldKey.clear();g.ensureFlight();f.dockAt(g.flightPort(p.id,true));g.processFlightEvents();
  p.stock={};p.stock[hyperion::Alloy]=1;p.stock[hyperion::Components]=1.5;g.s.economy.credits=5000;auto& c=f.ship.combat;c.rounds=0;c.missiles=7;c.decoys=7;auto credits=g.s.economy.credits,money=p.treasury;g.action("rearm");require(c.rounds==50&&c.missiles==8&&c.decoys==8,"partial rearming obeys finite alloy and component supply");require(close(p.stock[hyperion::Alloy],0)&&close(p.stock[hyperion::Components],0),"rearming consumes exact warehouse inputs");require(credits-g.s.economy.credits==p.treasury-money,"rearm transfers payment to port treasury");credits=g.s.economy.credits;g.action("rearm");require(g.s.economy.credits==credits,"unavailable ammunition cannot charge the player");
  p.stock[hyperion::Alloy]=30;p.stock[hyperion::Components]=30;c.armour=40;c.systems[hyperion::flight::Drive]=25;g.s.hull=100;g.action("service");require(c.armour==100&&c.systems[hyperion::flight::Drive]==100,"services repair systems and plating even with full hull");require(p.stock[hyperion::Alloy]<30&&p.stock[hyperion::Components]<30,"subsystem repairs consume real supplies");
  c.rounds=0;p.stock[hyperion::Alloy]=10;g.s.economy.credits=0;double alloy=p.stock[hyperion::Alloy];g.action("rearm");require(c.rounds==0&&p.stock[hyperion::Alloy]==alloy,"insufficient funds rejects rearm atomically");
  f.docked=false;f.deployed=false;f.savedDock.clear();f.ship.velocity={};g.s.economy.docked.clear();g.storeShipStock({});f.wrecks.push_back({f.ship.position,{},180,{4,2,3},"salvage"});g.action("salvage");auto stock=g.shipStock();require(stock[hyperion::Alloy]==4&&stock[hyperion::Components]==2&&stock[hyperion::Propellant]==3,"salvage enters canonical cargo channels");g.action("salvage");require(g.shipStock()==stock,"depleted wreck cannot duplicate salvage");
  hyperion::Stock full{};full[hyperion::Ore]=480;g.storeShipStock(full);f.wrecks[0].cargo={4,2,3};g.action("salvage");require(g.shipStock()==full&&f.wrecks[0].cargo[0]==4,"full cargo hold leaves wreck stock untouched");
 }
 // Real strategy commits, idle freeze, equipment persistence, and transactional validation.
 {hyperion::Game g;g.enterFlight();auto& f=g.s.flight;f.docked=false;f.savedDock.clear();f.cancelNavigation();f.ship.velocity={};f.ship.position=f.planets[2].centre+V3{0,2500,0};g.s.economy.docked.clear();g.system("HOME");g.action("combatOrder","1");g.action("weapon","2");auto time=f.time,heat=f.ship.combat.heat;int turn=g.s.turns;for(int i=0;i<100;i++)g.simulate(.1);require(f.time==time&&f.ship.combat.heat==heat&&g.s.turns==turn,"strategy thinking time freezes combat and planning orders cost no turn");g.endTurn();require(f.time>time+54&&g.s.turns==turn+1,"End Turn resolves the shared combat simulation");
  f.ship.combat.systems={71,62,53,44,35};f.ship.combat.armour=26;f.ship.combat.energy=37;f.ship.combat.heat=48;f.ship.combat.rounds=19;f.ship.combat.missiles=3;f.ship.combat.decoys=2;f.ship.combat.weapon=1;f.ship.combat.shieldDelay=3.1;f.combatOrder=2;f.lock=.7;f.combatTarget="persisted";f.wrecks={hyperion::flight::Wreck{f.ship.position,{},130,{4,2,3},"wreck"}};auto b=bolt(f.ship.position+V3{0,300,0},{100,0,0},false,3,68);b.target="persisted";f.bolts={b};
  auto path=(std::filesystem::temp_directory_path()/"hyperion-combat-test.json").string();std::string error;require(hyperion::saveState(g.s,path,error),"save combat state");hyperion::Game restored;require(hyperion::loadState(restored.s,path,error),"load combat state");auto& r=restored.s.flight;require(r.ship.combat.systems==f.ship.combat.systems&&r.ship.combat.rounds==19&&r.ship.combat.missiles==3&&r.ship.combat.decoys==2&&r.ship.combat.weapon==1,"damage, selected weapon and consumables survive save");require(r.ship.combat.shieldDelay==3.1&&r.combatOrder==2&&r.lock==.7&&r.bolts[0].target=="persisted"&&r.wrecks[0].cargo[1]==2,"cooldowns, orders, missile guidance and salvage survive save");
  nlohmann::json j;{std::ifstream in(path);in>>j;}auto valid=j;auto write=[&](){std::ofstream out(path);out<<j;};j["flight"]["combat"]["rounds"]=1.5;write();require(!hyperion::loadState(restored.s,path,error)&&restored.s.flight.ship.combat.rounds==19,"fractional ammunition is rejected without mutating loaded state");j=valid;j["flight"]["combat"]["systems"][0]=-1;write();require(!hyperion::loadState(restored.s,path,error),"negative subsystem condition is rejected");j=valid;j["flight"]["wrecks"][0]["cargo"][0]=1e20;write();require(!hyperion::loadState(restored.s,path,error),"oversized wreck cargo is rejected");
  j=valid;j["flight"]["schema"]=1;for(const char* key:{"combat","combatTarget","combatOrder","lock","wrecks"})j["flight"].erase(key);for(auto& item:j["flight"]["traffic"])for(const char* key:{"id","role","position","velocity","forward","positioned","state","stateClock","shield","armour","shieldDelay","managed","pirate","hostile","assaultReported","name","allegiance","band","reason","enemies"})item.erase(key);for(auto& item:j["flight"]["bolts"])for(const char* key:{"weapon","damage","owner","target"})item.erase(key);write();require(hyperion::loadState(restored.s,path,error),"old flight schema migrates without combat payload");require(restored.s.flight.ship.combat.rounds==300&&restored.s.flight.ship.combat.systems[0]==100&&restored.s.flight.bolts[0].weapon==-1,"legacy saves receive healthy equipment and retain legacy projectile behavior");std::filesystem::remove(path);
 }
 std::cout<<"Combat: "<<checks<<" checks passed / damage, systems, power, heat, weapons, missiles, countermeasures, collision, AI, finite services, salvage, turns and saves\n";return 0;
 }catch(const std::exception& e){std::cerr<<"Combat failure after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
