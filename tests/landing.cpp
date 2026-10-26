#include "game.hpp"
#include "json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace hyperion;
namespace {
int checks=0;void check(bool v,const char* m){++checks;if(!v)throw std::runtime_error(m);}
hyperion::flight::Game world(){hyperion::flight::Game f;f.planets.push_back({"Test","stone",{},1000,3.1,{0,1,0},false});auto& p=f.planets[0];hyperion::flight::V3 n{0,1,0};auto at=n*(p.radius+hyperion::flight::terrain(p,n)+2);f.ports.push_back({"Pad","",at,n,0,"pad","",0});f.docked=false;f.ship.gear=true;return f;}
void wilderness(hyperion::flight::Game& f){auto& p=f.planets[0];hyperion::flight::V3 n{1,0,0};auto normal=hyperion::flight::terrainNormal(p,n);f.ship.position=n*(p.radius+hyperion::flight::terrain(p,n)+1.94);f.ship.velocity=-normal*2;f.orient({0,0,-1},normal);}
}
int main(){try{
 auto f=world();const auto& pad=f.ports[0];f.ship.position=pad.position+pad.normal*20+hyperion::flight::V3{8,0,0};f.orient({0,0,-1},pad.normal);auto old=f.ship.position;
 check(f.dock()&&!f.docked&&f.autopilot,"requesting a nearby landing starts a physical approach");check(hyperion::flight::length(f.ship.position-old)<1e-10,"landing request never teleports the ship");
 for(int i=0;i<4000&&!f.docked;i++){f.update(.025);check(hyperion::flight::finite(f.ship.position)&&f.ship.hull>99,"assisted descent remains finite and avoids ground damage");}
 check(f.docked&&f.dockedPort==0&&hyperion::flight::length(f.ship.position-(pad.position+pad.normal*.25))<1e-8,"assist settles onto the actual raised pad");
 old=f.ship.position;auto heading=f.ship.forward,up=f.ship.up;double fuel=f.ship.fuel;f.launch();check(!f.docked&&f.launchAssist>3&&hyperion::flight::length(f.ship.position-old)<1e-10,"surface launch releases the pad without a position jump");
 check(hyperion::flight::length(f.ship.forward-heading)<1e-10&&hyperion::flight::length(f.ship.up-up)<1e-10&&f.speed()==0,"clamp release preserves attitude and begins from rest");
 double previousHeight=f.landing().altitude;for(int i=0;i<200;i++){auto velocity=f.ship.velocity;heading=f.ship.forward;f.update(.02);check(f.landing().altitude>previousHeight-.02,"departure lift remains clear of the pad");check(hyperion::flight::length(f.ship.velocity-velocity)<.6&&hyperion::flight::dot(heading,f.ship.forward)>.999,"spool, rotation and the forward-flight handoff remain continuous");previousHeight=f.landing().altitude;}
 check(f.launchAssist==0&&!f.ship.gear&&previousHeight>15&&f.ship.hull>99,"departure assist clears the ground and retracts gear");check(f.ship.fuel<fuel,"powered departure burns actual flight fuel");
 f.dockAt(0);f.launch();hyperion::flight::Input manual;manual.lift=1;f.update(.02,manual);check(f.launchAssist==0&&!f.docked,"manual thrust takes over immediately");
 f.dockAt(0);f.launch();f.navigate();check(f.launchAssist==0&&f.autopilot,"navigator and departure assist cannot compete");
 for(int mode=0;mode<6;mode++){
  auto manualLanding=world();wilderness(manualLanding);auto c=manualLanding.landing();check(c.safe&&c.port==-1,"level wilderness touchdown starts inside the safe envelope");
  if(mode==1)manualLanding.ship.velocity=-c.normal*9;
  if(mode==2)manualLanding.ship.velocity=hyperion::flight::unit(hyperion::flight::cross(c.normal,{0,0,1}))*16;
  if(mode==3)manualLanding.orient(manualLanding.ship.forward,-c.normal);
  if(mode==4)manualLanding.ship.gear=false;
  if(mode==5)manualLanding.planets[0].giant=true;
  manualLanding.update(.025);
  if(mode==0)check(manualLanding.docked&&manualLanding.dockedPort==-1&&manualLanding.touchdownAge==0,"safe wilderness contact secures gear at the actual site");
  else check(!manualLanding.docked,"excess descent, drift, tilt, raised gear and gas giants cannot dock");
  if(mode==1||mode==2)check(manualLanding.ship.hull<100,"hard or sliding contact damages the real hull");
 }
 auto offset=world();offset.ship.position=offset.ports[0].position+hyperion::flight::V3{7,-.1,0};offset.orient(hyperion::flight::unit({.3,.12,-1}),{0,1,0});heading=offset.ship.forward;up=offset.ship.up;offset.update(.01);check(offset.docked&&std::abs(offset.ship.position.x-7)<1e-8,"manual touchdown preserves its position on the pad");
 check(hyperion::flight::length(offset.ship.forward-heading)<1e-10&&hyperion::flight::length(offset.ship.up-up)<1e-10,"manual touchdown preserves heading and pilot view");
 auto low=world();low.ship.position=low.ports[0].position+low.ports[0].normal*4;low.orient(hyperion::flight::unit({.5,.05,-1}),{0,1,0});check(low.dock()&&low.route.size()==1,"aligned low approaches do not climb to a holding point");double lastAltitude=low.altitude();
 for(int i=0;i<1000&&!low.docked;i++){heading=low.ship.forward;old=low.ship.position;low.update(.02);check(low.altitude()<=lastAltitude+.001,"aligned final approach descends continuously");check(hyperion::flight::dot(heading,low.ship.forward)>.999&&hyperion::flight::length(low.ship.position-old)<.1,"final flare and contact have no attitude or position jump");lastAltitude=low.altitude();}
 check(low.docked&&low.ship.hull==100,"low approach reaches secure contact without damage");
 auto empty=world();empty.ship.position=empty.ports[0].position+empty.ports[0].normal*140+hyperion::flight::V3{45,0,0};empty.ship.fuel=0;empty.autopilot=true;empty.route={empty.ports[0].position+empty.ports[0].normal*.25};
 for(int i=0;i<1500&&!empty.docked;i++){empty.update(.02);check(empty.speed()<=12.00001,"combined lateral and vertical landing commands respect the emergency drive limit");}check(empty.docked&&empty.ship.hull==100,"fuel-starved assisted landing still reaches a safe berth");
 auto orbital=world();orbital.ports[0].planet=-1;orbital.ports[0].position={0,2000,0};orbital.dockAt(0);old=orbital.ship.position;heading=orbital.ship.forward;orbital.launch();check(hyperion::flight::length(orbital.ship.position-old)<1e-10&&orbital.launchAssist==3.2&&hyperion::flight::length(orbital.ship.forward-heading)<1e-10,"orbital undocking preserves the berth pose before a gradual release");
 for(int i=0;i<200;i++){heading=orbital.ship.forward;old=orbital.ship.position;orbital.update(.02);check(hyperion::flight::finite(orbital.ship.up)&&std::abs(hyperion::flight::dot(orbital.ship.up,orbital.ship.forward))<1e-8&&hyperion::flight::dot(heading,orbital.ship.forward)>.998,"orbital release turns continuously and keeps an orthonormal pilot view");}
 check(orbital.launchAssist==0&&hyperion::flight::length(orbital.ship.position-old)>0,"orbital departure completes into ordinary flight");
 orbital.dockAt(0);orbital.orient(-orbital.ports[0].normal,{0,0,1});orbital.launch();for(int i=0;i<180;i++)orbital.update(.02);check(hyperion::flight::dot(orbital.ship.forward,orbital.ports[0].normal)>.98,"an exactly inward-facing orbital departure turns outward instead of stalling at the opposite heading");
 for(int i=0;i<200;i++){
  auto& p=f.planets[0];hyperion::flight::V3 n=hyperion::flight::unit({std::sin(i*.67),std::cos(i*.31),std::sin(i*.17+.4)});auto normal=hyperion::flight::terrainNormal(p,n);double e=hyperion::flight::materialElevation(p,n);
  check(hyperion::flight::finite(normal)&&std::abs(hyperion::flight::length(normal)-1)<1e-8&&hyperion::flight::dot(n,normal)>0,"terrain normals remain outward and finite, including poles");check(e>=0&&e<=1&&e==hyperion::flight::materialElevation(p,n),"surface material fields are bounded and deterministic");
 }
 Game g;g.enterFlight();auto id=g.s.economy.settlements[2].id;g.s.flight.dockAt(g.flightPort(id,false));g.processFlightEvents();g.action("flightDock");g.simulate(.4);double timer=g.s.flight.launchAssist;check(timer>0&&timer<3.2,"shared cockpit advances departure assist on the real clock");
 auto path=std::filesystem::temp_directory_path()/"hyperion-landing-test.json";std::string error;check(saveState(g.s,path.string(),error),"save mid-departure");Game loaded;check(loadState(loaded.s,path.string(),error),"load mid-departure");loaded.universe.reset(loaded.s.seed,loaded.s.generation,loaded.s.legacySystems);loaded.ensureFlight();check(std::abs(loaded.s.flight.launchAssist-timer)<1e-8,"departure phase survives save/reload");
 loaded.simulate(.02);check(loaded.s.flight.launchAssist<timer,"restored departure continues rather than restarting");
 loaded.s.flight.dockAt(loaded.flightPort(id,false));loaded.processFlightEvents();loaded.s.flight.orient(hyperion::flight::unit(loaded.s.flight.ship.forward+hyperion::flight::V3{.01,.02,-.01}),loaded.s.flight.ship.up);heading=loaded.s.flight.ship.forward;up=loaded.s.flight.ship.up;loaded.s.flight.worldKey.clear();loaded.ensureFlight();check(hyperion::flight::length(loaded.s.flight.ship.forward-heading)<1e-10&&hyperion::flight::length(loaded.s.flight.ship.up-up)<1e-10,"rebuilding the flight scene preserves a docked pilot attitude");
 g.system("HOME");auto clock=g.s.hours;timer=g.s.flight.launchAssist;old=g.s.flight.ship.position;for(int i=0;i<100;i++)g.simulate(.1);check(g.s.hours==clock&&g.s.flight.launchAssist==timer&&hyperion::flight::length(old-g.s.flight.ship.position)==0,"strategy idle freezes launch, physics and the common clock");
 std::ifstream input(path);nlohmann::json j;input>>j;j["flight"].erase("launchAssist");{std::ofstream out(path);out<<j;}State legacy;check(loadState(legacy,path.string(),error)&&legacy.flight.launchAssist==0,"older saves without departure metadata remain valid");
 j["flight"]["launchAssist"]=9;{std::ofstream out(path);out<<j;}check(!loadState(legacy,path.string(),error),"out-of-range departure state is rejected");std::filesystem::remove(path);
 std::cout<<"PASS: "<<checks<<" landing checks; gradual approach and departure, pad contact, safe manual landings, hard contact, deterministic detail, save/reload and strategy freeze\n";return 0;
}catch(const std::exception& e){std::cerr<<"Landing regression after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
