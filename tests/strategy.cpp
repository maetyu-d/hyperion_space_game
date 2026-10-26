#include "game.hpp"
#include "mode_handoff.hpp"
#include "json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace hyperion;
namespace {
int checks=0;
void check(bool ok,const char* text){++checks;if(!ok)throw std::runtime_error(text);}
bool near(double a,double b){return std::abs(a-b)<1e-6;}
nlohmann::json snapshot(const State& state){auto path=(std::filesystem::temp_directory_path()/"hyperion-strategy-snapshot.json").string();std::string error;check(saveState(state,path,error),"save snapshot");std::ifstream in(path);nlohmann::json data;in>>data;in.close();std::filesystem::remove(path);return data;}
}
int main(){try{
 // Returning to a chart and back is a free camera handoff, including inertial
 // flight, controls, equipment, traffic, projectiles, targets and deadlines.
 Game g;g.enterFlight();auto& f=g.s.flight;f.docked=false;f.dockedPort=-1;f.ship.throttle=.73;f.ship.assist=false;f.ship.cruise=true;f.ship.velocity={12,3,-9};f.ship.gear=false;f.recordIds();g.s.economy.docked.clear();
 auto before=snapshot(g.s);g.system(g.s.currentStar);double hours=g.s.hours;for(int i=0;i<240;i++)g.simulate(.1);g.enterFlight();auto after=snapshot(g.s);
 // Chart selection intentionally follows the physical ship's nearest world.
 before["selectedPlanet"]=after["selectedPlanet"];check(before==after,"chart/cockpit round trip retains the whole campaign and helm");
 check(g.s.hours==hours,"handoff and planning spend no time");g.enterFlight();check(snapshot(g.s)==after,"repeated cockpit entry is idempotent");
 g.system(g.s.currentStar);auto helm=f.ship;double speed=f.speed();g.endTurn();check(near(f.ship.throttle,helm.throttle)&&f.ship.assist==helm.assist&&f.ship.cruise==helm.cruise,"strategic hold restores manual settings after resolution");check(f.speed()<speed*.1,"strategic hold damps inertial flight during the committed turn");
 check(g.lastTurn.valid&&g.lastTurn.last==g.s.turns&&g.lastTurn.hours==1,"turn report describes resolved time");check(g.lastTurn.summary().find("Turn ")!=std::string::npos,"turn report has readable feedback");
 // Preview is a read-only query and must predict the actual resolved endpoint.
 for(const char* seed:{"MERIDIAN-0001","GLASS-002","DEEP-BLUE"}){
  Game world(seed);world.system("HOME");world.s.selectedPlanet=2;check(world.descend(),"deploy surface");
  for(int i=0;i<20;i++){
   world.s.waypoint={world.s.surface.x+17+i*9.,world.s.surface.y-11-i*3.};auto frozen=snapshot(world.s);auto plan=world.planMove();check(snapshot(world.s)==frozen,"surface preview cannot change campaign");check(plan.valid&&plan.turns>=1,"surface preview available");int turn=world.s.turns;check(world.moveTurn({},true),"commit planned move");check(near(world.s.surface.x,plan.position.x)&&near(world.s.surface.y,plan.position.y)&&world.s.turns==turn+1,"surface endpoint equals preview and costs one turn");
  }
  check(world.dive(),"deploy ocean");
  for(int i=0;i<20;i++){
   world.s.oceanTargetX=world.s.oceanX+80;world.s.targetDepth=std::min(world.maxDepth(),world.s.depth+200);auto frozen=snapshot(world.s);auto plan=world.planMove();check(snapshot(world.s)==frozen,"ocean preview cannot change campaign");check(plan.valid,"ocean preview available");int turn=world.s.turns;check(world.moveTurn({},true),"commit dive step");check(near(world.s.oceanX,plan.position.x)&&near(world.s.depth,plan.depth)&&world.s.turns==turn+1,"dive endpoint equals preview and costs one turn");
  }
 }
 Game boundary;boundary.system("HOME");boundary.s.selectedPlanet=2;boundary.descend();boundary.s.surface=boundary.s.waypoint={1e7,1e7};auto frozen=snapshot(boundary.s);check(!boundary.moveTurn({1,1})&&snapshot(boundary.s)==frozen,"blocked map-boundary movement is free");
 // A long wait resolves complete hours only and stops on a mission event.
 Game wait;wait.system("HOME");wait.ensureEconomy();auto& issuer=wait.s.economy.settlements.front();wait.s.flight.courier={true,issuer.id,wait.s.economy.settlements[1].id,70,wait.s.hours+.5};hours=wait.s.hours;int turn=wait.s.turns;wait.waitHours(6);check(near(wait.s.hours,hours+1)&&wait.s.turns==turn+1,"long wait stops after courier resolution at the first complete hour");check(!wait.s.flight.courier.active&&wait.lastTurn.hours==1&&!wait.lastTurn.events.empty(),"interrupted report retains actual duration and events");
 auto& r=wait.lastTurn;check(std::isfinite(r.rations)&&std::isfinite(r.alloy)&&std::isfinite(r.hull),"report deltas finite");auto saved=snapshot(wait.s);for(int i=0;i<120;i++)wait.simulate(.1);check(snapshot(wait.s)==saved,"interruption returns to frozen planning");
 // A brief shield impact must interrupt even if the shield fully recharges
 // before the hour ends. Inspect the actual per-step damage, not just net loss.
 Game hit;hit.enterFlight();auto& hf=hit.s.flight;hf.traffic.clear();hf.docked=false;hf.dockedPort=-1;hf.ship.velocity={};hf.ship.throttle=0;hf.ship.gear=false;hf.ship.shield=100;hit.s.economy.docked.clear();
 auto start=hf.ship.position+hf.ship.forward*12;hf.bolts.push_back({start,start,-hf.ship.forward*600,1,true,hyperion::flight::Pulse,10,"test-hostile","NOMA",0});hit.system(hit.s.currentStar);hours=hit.s.hours;turn=hit.s.turns;hit.waitHours(6);check(near(hit.s.hours,hours+1)&&hit.s.turns==turn+1,"brief shield hit interrupts a long wait despite later regeneration");check(hf.ship.shield>99&&hit.toast.find("interrupted")!=std::string::npos,"interrupt reports a hit even after shield recovers");
 // Handoff waits for both the visual settle and control release, regardless of
 // frame rate. A new mode cannot receive a held scan/fire/launch gesture.
 for(double dt:{1./30,1./60,1./144}){
  ModeHandoff handoff;handoff.observe(1,dt,false);check(handoff.ready(),"initial view starts ready");check(handoff.observe(4,dt,true)&&!handoff.ready(),"new mode latches held input");
  for(int i=0;i<200;i++)handoff.observe(4,dt,true);check(!handoff.ready()&&handoff.opacity()==0,"elapsed fade cannot authorize held controls");handoff.observe(4,dt,false);check(handoff.ready(),"releasing controls completes settled handoff");handoff.observe(1,0,false);check(!handoff.ready(),"early release cannot skip visual settle");for(int i=0;i<200;i++)handoff.observe(1,dt,false);check(handoff.ready(),"handoff settles at varying frame rates");
 }
 std::cout<<"PASS: "<<checks<<" strategy checks; exact helm handoff, fixed-time hold, read-only move previews, ocean/surface endpoints, blocked orders, interruptible waits, reports and held-input release\n";
 return 0;
}catch(const std::exception& e){std::cerr<<"Strategy failure after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
