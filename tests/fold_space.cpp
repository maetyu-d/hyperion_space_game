#include "game.hpp"
#include "economy_storage.hpp"
#include "json.hpp"
#include "minor_bodies.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace hyperion;
namespace hf=hyperion::flight;
namespace {
int checks=0;void require(bool b,const char* message){++checks;if(!b)throw std::runtime_error(message);}
void near(hf::V3 a,hf::V3 b,const char* message){require(hf::length(a-b)<1e-7,message);}
hf::Game arena(){
 hf::Game g;g.planets.push_back({"Distant","",{0,-100000,0},100,1,{0,1,0}});g.ports.push_back({"Berth","",{0,20000,0},{0,1,0},-1,"berth",""});g.docked=false;
 hf::FoldGate a;a.id="A";a.link="pair";a.position={};a.normal={0,0,1};a.radius=80;a.peer=1;
 hf::FoldGate b=a;b.id="B";b.position={10000,0,0};b.normal={1,0,0};b.peer=0;g.folds={a,b};g.ship.position={0,0,20};g.ship.previous=g.ship.position;g.orient({0,0,-1},{0,1,0});return g;
}
hf::Traffic victim(const hf::Game& g){hf::Traffic t;t.id="victim";t.home=0;t.kind=1;t.position=t.previous=g.folds[1].position+g.folds[1].normal*25;t.forward={-1,0,0};t.positioned=true;t.health=120;t.shield=0;t.armour=0;t.fireClock=100;t.modules={0,0,0};return t;}
}
int main(){try{
 Universe u("MERIDIAN-0001");auto generated=generateFoldSpace(u,"HOME");require(generated.gates.size()==4&&generated.nil.size()==1,"Home has an accessible trade fold and a secluded Nil connection");
 auto repeat=generateFoldSpace(u,"HOME");for(size_t i=0;i<generated.gates.size();i++){
  const auto& gate=generated.gates[i];near(gate.position,repeat.gates[i].position,"Fold generation is deterministic");require(gate.peer>=0&&generated.gates[gate.peer].peer==int(i),"Mouths have reciprocal counterparts");require(hf::finite(gate.position)&&std::abs(hf::dot(gate.normal,gate.up))<1e-8,"Gate frames are finite and orthogonal");
  for(const auto& body:u.planets("HOME")){auto c=systemBodyCentre(body.index);require(hf::length(gate.position-hf::V3{c.x,c.y,c.z})>systemBodyRadius(body)+gate.radius+40,"Apertures remain outside all major bodies");}
 }
 int sampled=0,anomalies=0;
 for(int cell=0;cell<64;cell++)for(const auto& star:u.cell(cell-9,cell%4-2)){
  auto space=generateFoldSpace(u,star.id);sampled++;if(!space.nil.empty())anomalies++;
  for(const auto& mouth:space.gates){require(mouth.peer>=0&&space.gates[mouth.peer].peer>=0,"Generated mouths remain reciprocal across seeds");
   require(std::abs(hf::length(mouth.normal)-1)<1e-8&&std::abs(hf::dot(mouth.normal,mouth.up))<1e-8,"Generated mouths retain a rigid frame");
   for(const auto& body:u.planets(star.id)){auto c=systemBodyCentre(body.index);require(hf::length(mouth.position-hf::V3{c.x,c.y,c.z})>systemBodyRadius(body)+mouth.radius+40,"Procedural mouths never intersect a major body");}
  }
 }
 require(sampled>50&&anomalies<sampled/4,"Nil regions remain rare outside the starting-system demonstration");
 auto nav=arena();hf::Port aperture;aperture.id="aperture";aperture.name="Fold";aperture.position={};aperture.normal={0,0,1};aperture.planet=-1;aperture.fold=0;nav.ports.push_back(aperture);nav.target=1;nav.ship.position={0,0,300};nav.ship.previous=nav.ship.position;nav.navigate();
 for(int i=0;i<8000&&nav.events.empty();i++)nav.update(.025,{});
 require(!nav.events.empty()&&nav.events.back().kind=="foldTransit"&&!nav.autopilot,"The normal navigator actually flies through its selected mouth");
 auto detour=hf::avoidFoldRoute(nav.folds,{0,0,300},{{0,0,-300}});hf::V3 last{0,0,300};for(auto next:detour){require(!std::isfinite(hf::foldIntersection(nav.folds[0],last,next)),"Unrelated navigation avoids accidental crossings");last=next;}
 auto f=arena();auto a=f.folds[0],b=f.folds[1];hf::V3 p{13,7,31},v{5,-2,-42};
 near(hf::foldPoint(b,a,hf::foldPoint(a,b,p)),p,"Round trips preserve positions");near(hf::foldVector(b,a,hf::foldVector(a,b,v)),v,"Round trips preserve velocity");require(std::abs(hf::length(v)-hf::length(hf::foldVector(a,b,v)))<1e-8,"Crossings conserve speed");
 require(std::isfinite(hf::foldIntersection(a,{0,0,10},{0,0,-10})),"Front-to-back swept crossings hit the aperture");require(!std::isfinite(hf::foldIntersection(a,{81,0,10},{81,0,-10})),"Shots outside the rim do not teleport");require(!std::isfinite(hf::foldIntersection(a,{0,0,-10},{0,0,10})),"The back of a mouth is not another connection");
 f.ship.position={3,4,-8};f.ship.velocity=v;f.ship.angularVelocity={.2,.3,.4};f.ship.combat.rounds=53;auto forward=f.ship.forward,up=f.ship.up;double fuel=f.ship.fuel;
 require(f.crossFold(0),"The ship crosses a valid free-flight mouth");near(f.ship.velocity,hf::foldVector(a,b,v),"Ship momentum maps with the opening");near(f.ship.forward,hf::foldVector(a,b,forward),"Ship forward orientation maps correctly");near(f.ship.up,hf::foldVector(a,b,up),"Ship up orientation maps correctly");require(f.ship.fuel==fuel&&f.ship.combat.rounds==53,"Crossing invents no fuel or ammunition");require(!f.autopilot&&f.route.empty(),"The navigator does not continue along a chord across the universe");
 for(int weapon=0;weapon<5;weapon++){
  auto battle=arena();battle.traffic.push_back(victim(battle));hf::Bolt bolt;bolt.position=bolt.previous={0,0,10};bolt.velocity={0,0,-1000};bolt.life=2;bolt.weapon=weapon;bolt.damage=40;bolt.owner="NOMA";battle.bolts.push_back(bolt);battle.updateCombat(.05,{});
  require(battle.traffic[0].health<120,"Every weapon can hit a ship through a fold");require(battle.bolts.empty(),"The projectile is consumed exactly once at the destination");
 }
 auto blocked=arena();blocked.traffic.push_back(victim(blocked));WorldObject wall;wall.id="wall";wall.position={0,0,6};wall.half={10,10,1};wall.capacity=200;wall.integrity=1;blocked.worldObjects.push_back(wall);
 hf::Bolt shot;shot.position=shot.previous={0,0,10};shot.velocity={0,0,-1000};shot.life=2;shot.damage=40;shot.owner="NOMA";blocked.bolts.push_back(shot);blocked.updateCombat(.05,{});require(blocked.traffic[0].health==120&&blocked.worldObjects[0].integrity<1,"A real obstacle before the mouth blocks a shot before teleportation");
 auto targetArena=arena();targetArena.traffic.push_back(victim(targetArena));int via=-1;auto image=targetArena.apparentPosition(targetArena.ship.position,targetArena.traffic[0].position,&via);require(via==0&&hf::length(image-targetArena.ship.position)<60,"Through-fold contacts use optical path distance");targetArena.cycleCombatTarget();require(targetArena.combatTarget=="victim","The actual remote enemy can be selected through the aperture");
 hf::Input input;for(int i=0;i<150;i++)targetArena.prepareCombat(.025,input);
 require(targetArena.lock>=.99,"Target lock follows the visible image through the opening");
 for(int i=0;i<32;i++){double t=.3+i*.2;auto point=hf::nilGeodesic(hf::unit(hf::V3{.4,.2,.8}),t);require(hf::finite(point),"Nil light trajectories remain finite");}
 auto crossing=arena();auto t=victim(crossing);t.position={0,0,-2};t.previous={0,0,2};t.velocity={0,0,-50};crossing.foldTraffic(t);require(hf::length(t.position-crossing.folds[1].position)<10,"NPC ships use the same physical crossing");require(hf::length(t.previous-t.position)<1e-8,"NPC crossings cannot create a system-wide collision sweep");
 Game g;g.enterFlight();g.s.flight.traffic.clear();g.s.economy.routes.clear();g.s.flight.ship.position=g.s.flight.folds[0].position+g.s.flight.folds[0].normal*120;g.s.flight.ship.velocity={};g.s.flight.docked=false;g.s.flight.dockedPort=-1;g.s.flight.savedDock.clear();g.s.flight.ship.throttle=0;g.s.flight.ship.gear=false;g.s.economy.docked.clear();
 auto gate=g.s.flight.folds[0];g.prepareWorldObjects();for(const auto& object:g.s.flight.worldObjects)require(object.id.find("fold:")==std::string::npos,"Natural folds do not acquire station geometry or invisible station colliders");g.s.flight.selectTarget(int(std::find_if(g.s.flight.ports.begin(),g.s.flight.ports.end(),[&](const auto& p){return p.id==gate.id;})-g.s.flight.ports.begin()));
 g.openStrategy();auto planningMode=g.s.mode;auto hours=g.s.hours;auto position=g.s.flight.ship.position;for(int i=0;i<50;i++)g.simulate(.1);require(g.s.hours==hours,"Thinking in strategy cannot advance the fold simulation");near(g.s.flight.ship.position,position,"Changing views does not move the ship");
 auto battery=g.s.battery;g.issueOrder("foldSurvey",gate.id,true);require(g.s.hours==hours&&g.s.battery==battery,"Paused orders cannot survey a connection");g.issueOrder("foldSurvey",gate.id);require(g.s.hours==hours+1&&g.s.economy.folds.at(gate.link).surveyed,"A strategic survey resolves one full turn and charts both mouths");
 double afterSurvey=g.s.hours;g.issueOrder("foldSurvey",gate.id);require(g.s.hours==afterSurvey,"Repeated surveys do not duplicate rewards or spend another turn");
 g.s.flight.ship.position=gate.position+gate.normal*30;g.s.flight.ship.velocity={};g.issueOrder("foldTraverse",gate.id);require(g.s.mode==planningMode&&g.s.hours==afterSurvey+1,"A strategic traversal resolves one hour and retains strategy mode");require(g.s.economy.folds.at(gate.link).passages==1,"One physical traversal produces one persistent passage");
 g.s.flight.recordIds();auto file=std::filesystem::temp_directory_path()/"hyperion-fold-test.json";std::string error;require(saveState(g.s,file.string(),error),"Fold state saves");State restored;require(loadState(restored,file.string(),error),error.c_str());require(restored.economy.folds.at(gate.link).surveyed&&restored.economy.folds.at(gate.link).passages==1,"Survey and passage history survive a save round trip");
 auto j=nlohmann::json::parse(std::ifstream(file));j["flight"]["target"]="fold:HOME:999:A";{std::ofstream out(file);out<<j;}require(!loadState(restored,file.string(),error),"Unknown saved fold mouths are rejected");std::filesystem::remove(file);
 auto e=g.s.economy;e.routes.clear();e.contracts.clear();e.folds.at(gate.link).surveyed=true;e.folds.at(gate.link).anchor.clear();e.folds.at(gate.link).authority=3;e.factions[3].standing=40;e.factions[3].warrant=0;
 Settlement* from=nullptr;Settlement* to=nullptr;for(auto& site:e.settlements){site.residents=0;site.built={};site.nextMigration=1e6;site.stock={};site.owner=0;if(site.planet==2&&!from)from=&site;if(site.planet==1&&!to)to=&site;}
 require(from&&to,"Trade fixture has both physical endpoints");from->stock[Rations]=180;from->stock[Propellant]=30;std::string message;require(commissionRoute(e,from->id,to->id,Rations,message),"A player route can be commissioned");double credits=e.credits,food=from->stock[Rations],propellant=from->stock[Propellant];advanceEconomy(e,g.universe,e.hour+.25);require(e.routes[0].fold==gate.link&&e.routes[0].duration<e.routes[0].conventionalDuration,"Surveyed folds shorten actual freight trips");require(std::abs(food-from->stock[Rations]-e.routes[0].cargo[Rations])<1e-8&&from->stock[Propellant]==propellant-1,"Freight packets consume finite source supplies and departure fuel");require(e.credits==credits-6,"Fold tolls and freight handling debit actual credits");

 Game convoy;convoy.enterFlight();convoy.s.economy=e;convoy.s.flight.traffic.clear();convoy.s.flight.docked=false;convoy.s.flight.dockedPort=-1;convoy.s.flight.ship.velocity={};convoy.s.flight.ship.throttle=0;
 auto& packet=convoy.s.economy.routes[0];packet.remaining=packet.duration*.500001;convoy.updateFlight(0);auto visible=std::find_if(convoy.s.flight.traffic.begin(),convoy.s.flight.traffic.end(),[&](const auto& ship){return ship.routeId==packet.id;});require(visible!=convoy.s.flight.traffic.end(),"Economic cargo has a visible physical freighter");auto before=visible->position;auto cargoBefore=packet.cargo;
 packet.remaining=packet.duration*.499999;convoy.updateFlight(0);visible=std::find_if(convoy.s.flight.traffic.begin(),convoy.s.flight.traffic.end(),[&](const auto& ship){return ship.routeId==packet.id;});require(visible->transitLeg==1&&hf::length(visible->position-before)>500,"Freighter changes physical location at the fold seam");require(hf::length(visible->position-visible->previous)<1e-8&&packet.cargo==cargoBefore,"Folded freight cannot sweep through a system or duplicate its cargo");
 e.factions[3].standing=-60;double remaining=e.routes[0].remaining;advanceEconomy(e,g.universe,e.hour+.25);require(e.routes[0].remaining==remaining&&e.routes[0].status.find("embargo")!=std::string::npos,"An authority embargo holds outbound cargo at the fold");
 std::cout<<checks<<" folded-space checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"Folded-space test failed: "<<e.what()<<'\n';return 1;}}
