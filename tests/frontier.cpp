#include "game.hpp"
#include "json.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace hyperion;
namespace {
int checks=0;
void check(bool value,const std::string& message){checks++;if(!value)throw std::runtime_error(message);}
bool close(double a,double b,double tolerance=1e-6){return std::abs(a-b)<tolerance;}
void navigate(Game& g,int target){
 auto& f=g.s.flight;f.selectTarget(target);f.ship.fuel=100;g.s.fuel=g.s.maxFuel;f.ship.hull=g.s.hull=100;
 // Navigation checks isolate the navigator; combat is checked separately.
 for(auto& t:f.traffic)if(t.kind==1){t.dead=true;t.respawn=1e9;}
 f.navigate();double start=g.s.hours;
 for(int step=0;step<40000&&f.autopilot;step++){
  g.s.hours+=.05/55;f.updateMinorBodies(g.s.hours);f.update(.05);g.s.fuel=f.ship.fuel/100*g.s.maxFuel;g.s.hull=f.ship.hull;
  check(hyperion::flight::finite(f.ship.position)&&std::isfinite(g.s.hull),"finite navigation state");
 }
 g.processFlightEvents();g.syncEconomy();
 check(f.docked&&f.dockedPort==target,"navigator reaches "+f.ports[target].name);
 check(g.s.hull>99,"navigator avoids terrain");check(g.s.hours>start,"flight advances colony time");
}
}
int main(){try{
 Game g;g.ensureEconomy();g.enterFlight();auto& f=g.s.flight;
 check(f.planets.size()==g.universe.planets("HOME").size(),"all home planets share flight world");
 for(int i=0;i<int(f.planets.size());i++){auto actual=g.universe.planet("HOME",i);const auto& visual=f.planets[i];check(close(visual.water,actual.water)&&close(visual.life,actual.life)&&close(visual.atmosphere,actual.pressure),"flight materials derive from the actual surveyed world");check((visual.kind==5)==actual.giant,"gas giant material follows real planet type");}
 check(f.ports.size()==f.planets.size()+g.s.economy.settlements.size()*2+g.s.economy.pirates.size()+f.minorBodies.size(),"every settlement has surface and orbit gateways");
 for(V2 p:std::vector<V2>{{0,0},{3200,1800},{-17400,-9300},{9000000,-8000000}}){auto restored=g.surfacePosition(g.surfaceNormal(p));check(close(restored.x,p.x,.01)&&close(restored.y,p.y,.01),"surface chart reversibility");}
 auto home=g.s.economy.settlements.front().id;int surface=g.flightPort(home,false),orbit=g.flightPort(home,true);
 check(surface>=0&&orbit>=0,"live settlement flight links");
 // Visit every surface, orbital and uncrewed port, including the gas giant relay.
 for(int i=0;i<int(f.ports.size());i++)if(f.ports[i].minor<0)navigate(g,i);
 f.dockAt(surface);g.processFlightEvents();check(g.canDock(*findSettlement(g.s.economy,home)),"physical dock authorizes strategy trade");
 auto beforeStock=g.shipStock();auto& port=*findSettlement(g.s.economy,home);port.stock[Rations]=100;port.treasury=5000;g.s.economy.credits=5000;
 g.action("buy",home+"|4");check(g.shipStock()[Rations]==beforeStock[Rations]+5,"flight exchange loads canonical cargo");
 check(port.stock[Rations]==95,"goods removed from actual warehouse");
 g.action("flightDock");check(g.s.economy.docked.empty()&&!g.canDock(port),"launch revokes cargo access immediately");
 beforeStock=g.shipStock();g.action("buy",home+"|4");check(g.shipStock()==beforeStock,"cannot trade while airborne");
 g.system("HOME");g.action("dock",home);check(g.canDock(port)&&f.docked,"strategic rendezvous reaches physical orbital berth");
 g.enterFlight();auto funds=g.s.economy.credits;auto treasury=port.treasury;g.action("courier");check(f.courier.active,"courier accepted at strategic berth");
 check(port.treasury+f.courier.escrow==treasury,"courier reward reserved in actual treasury");check(g.s.economy.credits==funds,"courier acceptance does not pay");
 auto destination=f.courier.destination;auto escrow=f.courier.escrow;int drop=g.flightPort(destination,true);
 f.dockAt(drop);g.processFlightEvents();check(!f.courier.active&&g.s.economy.credits==funds+escrow,"courier pays canonical account");
 f.dockAt(drop);g.processFlightEvents();check(g.s.economy.credits==funds+escrow,"no duplicate courier reward");
 g.action("courier");check(f.courier.active,"second courier accepts");auto issuer=f.courier.issuer;escrow=f.courier.escrow;auto issuerMoney=findSettlement(g.s.economy,issuer)->treasury;
 g.s.hours=f.courier.deadline+1;g.updateFlight(0);check(!f.courier.active,"courier expires by common clock");check(findSettlement(g.s.economy,issuer)->treasury==issuerMoney+escrow,"expired courier refunds escrow");
 // Services use the physical port's finite material stock and shared money.
 f.dockAt(orbit);g.processFlightEvents();port.stock[Propellant]=2;port.stock[Alloy]=2;port.stock[Components]=1;g.s.fuel=100;g.s.hull=60;
 funds=g.s.economy.credits;treasury=port.treasury;g.action("service");check(close(g.s.fuel,116)&&close(g.s.hull,70),"partial services honor finite supplies");check(close(port.stock[Propellant],0)&&close(port.stock[Alloy],0)&&close(port.stock[Components],0),"service inputs consumed exactly");check(funds-g.s.economy.credits==port.treasury-treasury,"service money conserved");
 // Native landing -> exploration -> dive -> same ship and coordinates.
 g.s.mode=Mode::Flight;f.dockAt(surface);g.processFlightEvents();check(g.disembark(),"disembark from surface berth");
 check(g.s.mode==Mode::Surface&&close(g.s.surface.x,port.position.x),"surface position is settlement's actual chart site");
 g.s.surface={3200,1800};g.enterFlight();check(f.docked&&f.dockedPort==-1,"wilderness return parks at expedition location");
 auto pos=g.surfacePosition(f.ship.position-f.planets[g.s.activePlanet].centre);check(close(pos.x,3200)&&close(pos.y,1800),"expedition location reaches cockpit");
 // Manual launch, lateral input cancels navigator, gentle wilderness touchdown.
 g.action("flightDock");auto launchPosition=f.ship.position;g.simulate(.05,{},hyperion::flight::Input{0,0,0,1,1,0,true,false});
 check(!f.docked&&hyperion::flight::length(f.ship.position-launchPosition)>0,"manual thrust moves the shared ship");
 g.action("navigate");check(f.autopilot,"navigation engaged");g.simulate(.05,{},hyperion::flight::Input{0,1,0,0,0,0,false,false});check(!f.autopilot,"steering cancels navigation");
 g.parkFlight(2,{2100,-750});g.enterFlight();g.action("flightDock");f.ship.position=f.planets[2].centre+g.surfaceNormal({2100,-750})*(f.planets[2].radius+hyperion::flight::terrain(f.planets[2],g.surfaceNormal({2100,-750}))+1.8);f.ship.velocity={};f.ship.throttle=0;f.ship.gear=true;f.launchAssist=0;f.orient(f.ship.forward,g.surfaceNormal({2100,-750}));g.updateFlight(.025);if(!f.docked){auto c=f.landing();std::cerr<<"Landing diagnostic: altitude "<<c.altitude<<" slope "<<c.slope<<" tilt "<<c.tilt<<" drift "<<c.drift<<" sink "<<c.sink<<" assist "<<f.launchAssist<<" safe "<<c.safe<<"\n";}check(f.docked&&f.dockedPort==-1,"gentle wilderness landing");check(g.disembark(),"wilderness expedition deploys");
 // Bounty and civilian fines use the same authority balance and sanctions.
 g.enterFlight();f.dockAt(orbit);g.processFlightEvents();port.treasury=100;funds=g.s.economy.credits;f.events.push_back({"bounty",orbit,port.owner,""});g.processFlightEvents();check(g.s.economy.credits==funds+100&&port.treasury==0,"bounty cannot mint missing credits");
 funds=g.s.economy.credits;double standing=g.s.economy.factions[port.owner].standing;f.events.push_back({"civilian",orbit,port.owner,""});g.processFlightEvents();check(g.s.economy.credits==funds-300&&port.treasury==300,"civilian fine conserved");check(g.s.economy.factions[port.owner].standing==standing-18,"civilian damage affects faction standing");
 // Flight combat removes actual freight, not a cosmetic duplicate.
 auto& route=g.s.economy.routes.front();route.outbound=true;route.cargo={};route.cargo[Rations]=12;route.remaining=3;
 f.events.push_back({"freightLoss",orbit,port.owner,route.id});g.processFlightEvents();check(stockTotal(route.cargo)==0&&!route.outbound,"combat removes packet from economic route");
 // Damage/rescue destroys ship cargo but never settlement stores or creates full repair.
 auto colonyStock=port.stock;f.ship.hull=g.s.hull=.01;f.ship.gear=false;f.docked=false;f.ship.position=f.planets[0].centre+hyperion::flight::V3{0,f.planets[0].radius,0};f.ship.velocity={0,-500,0};g.updateFlight(.025);
 check(f.docked&&close(g.s.hull,25)&&stockTotal(g.shipStock())==0,"hull failure rescues with real cargo loss and damaged hull");check(port.stock==colonyStock,"rescue leaves settlement warehouse intact");
 // Isolate legacy pulses from accumulated expedition subsystem damage.
 // Real weapons use swept collision and shield absorption, including convoys.
 {hyperion::flight::Game combat=f;combat.events.clear();combat.traffic.clear();combat.bolts.clear();combat.docked=false;combat.ship.position=combat.planets[0].centre+hyperion::flight::V3{0,3000,0};combat.ship.velocity={};combat.ship.throttle=0;combat.ship.shield=8;combat.ship.hull=100;combat.ship.combat={};combat.fireClock=0;
  combat.bolts.push_back({combat.ship.position,combat.ship.position,{},1,true});combat.update(0);check(combat.ship.shield==0&&combat.ship.hull==96,"hostile pulses consume shields before hull");
  size_t count=combat.bolts.size();hyperion::flight::Input fire;fire.fire=true;combat.update(0,fire);check(combat.bolts.size()>count,"manual pulse cannon fires");
  combat.bolts.clear();hyperion::flight::Traffic pirate;pirate.kind=1;pirate.home=orbit;pirate.health=30;combat.traffic.push_back(pirate);combat.update(0);auto enemy=combat.traffic[0].position;combat.bolts.push_back({enemy-hyperion::flight::V3{0,0,10},enemy-hyperion::flight::V3{0,0,10},{0,0,1200},1,false});combat.update(1./60);check(combat.traffic[0].dead&&combat.kills==f.kills+1,"swept pulse collision neutralises a pirate");
  combat.traffic.clear();combat.bolts.clear();hyperion::flight::Traffic convoy;convoy.kind=2;convoy.home=orbit;convoy.routeId=route.id;convoy.position=combat.ship.position+hyperion::flight::V3{50,0,0};convoy.health=10;combat.traffic.push_back(convoy);combat.bolts.push_back({convoy.position,convoy.position,{},1,true});combat.update(0);check(combat.traffic[0].dead&&combat.events.back().kind=="freightLoss","pirate pulse destruction emits actual freight loss");
 }
 // Prolonged inertial handling and load-dependent propulsion remain bounded.
 {Game manual;manual.enterFlight();manual.s.flight.ship.assist=false;manual.s.flight.docked=false;for(int i=0;i<6000;i++)manual.simulate(1./120,{},hyperion::flight::Input{.2,.3,.1,.1,.5,.3,true,i%2==0});check(hyperion::flight::finite(manual.s.flight.ship.position)&&std::abs(hyperion::flight::dot(manual.s.flight.ship.forward,manual.s.flight.ship.up))<1e-6,"long inertial flight remains stable");check(manual.s.fuel<190&&manual.s.economy.hour>0,"manual propulsion burns canonical fuel while colonies run");}
 // Settle the earlier civilian incident before requesting a new authority commission.
 g.action("restitution",std::to_string(port.owner));check(g.s.economy.factions[port.owner].warrant==0,"civilian warrant settled before courier commission");
 // Persistent physical flight, cargo, colonies, courier escrow, canonical hull/fuel.
 g.action("courier");g.s.hull=73;g.s.fuel=123;g.s.mode=Mode::Flight;g.action("navigate");g.updateFlight(.05);check(f.courier.active&&f.autopilot,"active courier and navigator prepared for persistence");if(!f.traffic.empty())f.traffic.front().health=43;f.bolts.push_back({f.ship.position+hyperion::flight::V3{300,0,0},f.ship.position,{123,45,67},1.67,false});
 auto path=(std::filesystem::temp_directory_path()/"hyperion-frontier-test.json").string();std::string error;check(saveState(g.s,path,error),"save integrated state: "+error);
 Game restored;check(loadState(restored.s,path,error),"load integrated state: "+error);restored.universe.reset(restored.s.seed,restored.s.generation,restored.s.legacySystems);auto position=f.ship.position;restored.ensureFlight();check(hyperion::flight::length(restored.s.flight.ship.position-position)<1e-6,"reload resumes physical position");check(close(restored.s.hull,g.s.hull)&&close(restored.s.fuel,g.s.fuel),"canonical hull and fuel preserved");check(restored.s.economy.settlements.size()==g.s.economy.settlements.size(),"same colonies survive flight save");check(restored.s.flight.courier.escrow==f.courier.escrow,"courier escrow survives save");check(restored.s.flight.autopilot&&!restored.s.flight.route.empty(),"navigator safely resumes after loading");check(restored.s.flight.traffic.front().health==43&&restored.s.flight.bolts.size()==f.bolts.size(),"traffic damage and projectiles survive loading");
 std::ifstream input(path);nlohmann::json json;input>>json;json["flight"]["position"][0]=1e99;{std::ofstream output(path);output<<json;}
 auto seed=restored.s.seed;check(!loadState(restored.s,path,error)&&restored.s.seed==seed,"invalid physical save rejected transactionally");
 // Old v2 saves need no flight payload; the new world derives from their colonies.
 json.erase("flight");json["mode"]="system";json["nativeVersion"]=2;{std::ofstream output(path);output<<json;}
 check(loadState(restored.s,path,error),"v2 societies save migration");restored.universe.reset(restored.s.seed,restored.s.generation,restored.s.legacySystems);restored.enterFlight();check(restored.s.flight.planets.size()==g.s.flight.planets.size(),"migration builds flight around old universe");
 // Boost measurements use clear space; station structures now have physical collision.
 {Game fast;fast.enterFlight();fast.s.flight.ship.position=fast.s.flight.planets[2].centre+hyperion::flight::V3{0,10000,0};fast.s.flight.ship.previous=fast.s.flight.ship.position;fast.storeShipStock({});fast.s.upgrades[3]=6;fast.s.flight.ship.cruise=true;fast.s.flight.ship.throttle=1;fast.s.flight.orient({0,1,0},{0,0,-1});hyperion::flight::Input boost;boost.boost=true;for(int i=0;i<240;i++)fast.simulate(1./60,{},boost);check(fast.s.flight.speed()>400&&fast.s.flight.speed()<900,"upgraded boost accelerates through a bounded thrust ramp");for(int i=0;i<720;i++)fast.simulate(1./60,{},boost);check(fast.s.flight.speed()>1000,"sustained upgraded boost can exceed original cruise envelope");check(saveState(fast.s,path,error),"save upgraded boosted flight");State fastState;check(loadState(fastState,path,error),"restore upgraded boosted velocity");}
 std::filesystem::remove(path);
 // More than one seed/system: the flight world is generated from the actual atlas.
 for(auto seedName:{"GLASS-002","DEEP-BLUE"}){Game generated(seedName);generated.ensureEconomy();auto stars=generated.universe.cell(0,0);for(auto& star:stars)if(star.id!="HOME"&&distance(generated.currentStar().pos,star.pos)<generated.range()){check(generated.travel(star.id),"interstellar travel retained");auto jumpSave=(std::filesystem::temp_directory_path()/"hyperion-jump-test.json").string();check(saveState(generated.s,jumpSave,error),"save immediately after strategic FTL");State jumpState;check(loadState(jumpState,jumpSave,error),"restore immediately after strategic FTL");std::filesystem::remove(jumpSave);generated.enterFlight();check(generated.s.flight.star==star.id&&generated.s.flight.planets.size()==generated.universe.planets(star.id).size(),"generated flight system matches atlas");navigate(generated,0);break;}}
 std::cout<<"Frontier integration: "<<checks<<" checks passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<"Frontier integration failure: "<<e.what()<<" ("<<checks<<" checks)\n";return 1;}}
