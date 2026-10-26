#include "game.hpp"
#include "flight_storage.hpp"
#include "json.hpp"
#include <iostream>
#include <stdexcept>
using namespace hyperion::flight;
namespace {
int checks=0;void check(bool v,const char* m){++checks;if(!v)throw std::runtime_error(m);}
Game vacuum(){Game f;f.planets.push_back({"Distant","rock",{0,-1e9,0},1000,3.1,{0,1,0},false});f.planets[0].atmosphere=0;f.ports.push_back({"Berth","",{0,-1e9+2000,0},{0,1,0},-1,"port","",0});f.docked=false;f.ship.gear=false;f.orient({0,0,-1},{0,1,0});return f;}
void run(Game& f,double seconds,Input in={},double dt=.01){int count=int(std::round(seconds/dt));for(int i=0;i<count;i++)f.update(dt,in);}
}
int main(){try{
 auto f=vacuum();f.ship.assist=false;f.ship.velocity={87,19,-120};auto velocity=f.ship.velocity;double fuel=f.ship.fuel;run(f,10);
 check(length(f.ship.velocity-velocity)<1e-8,"unpowered vacuum flight conserves linear momentum");check(f.ship.fuel==fuel,"unpowered coasting consumes no propellant at any speed");
 auto turned=f;Input yaw;yaw.yaw=1;turned.update(.02,yaw);check(length(turned.ship.angularVelocity)>0&&length(turned.ship.angularVelocity)<.09,"RCS angular acceleration is finite and ramps into a turn");run(turned,1,yaw);auto spin=turned.ship.angularVelocity;auto heading=turned.ship.forward;run(turned,1);
 check(length(turned.ship.velocity-velocity)<1e-8,"turning in vacuum does not rotate the velocity vector");check(length(turned.ship.angularVelocity-spin)<1e-10&&dot(turned.ship.forward,heading)<.9,"inertial flight retains spin when RCS input is released");
 turned.ship.assist=true;run(turned,3);check(length(turned.ship.angularVelocity)<1e-7,"flight assist damps angular momentum through RCS");
 auto thrust=vacuum();thrust.ship.assist=false;thrust.ship.throttle=1;auto empty=thrust,loaded=thrust,damaged=thrust;loaded.cargoMass=900;damaged.ship.combat.systems[Drive]=25;
 run(thrust,.02);check(thrust.speed()>.01&&thrust.speed()<.2,"main engines spool instead of instantly changing speed");run(empty,1);run(loaded,1);run(damaged,1);
 check(empty.speed()>40&&empty.speed()<53,"main engine acceleration respects its finite force");check(loaded.speed()<empty.speed()*.55,"loaded cargo increases translational inertia");check(damaged.speed()<empty.speed()*.5,"drive damage reduces available thrust");check(empty.ship.fuel<fuel,"burning engines consumes actual propellant");
 auto boosted=vacuum();boosted.ship.assist=false;boosted.ship.throttle=1;Input boost;boost.boost=true;run(boosted,1,boost);check(boosted.speed()>empty.speed()*1.9&&boosted.speed()<empty.speed()*2.4,"boost spools additional physical thrust in inertial flight");check(boosted.ship.fuel<empty.ship.fuel,"inertial boost consumes more propellant than normal thrust");
 auto brake=vacuum();brake.ship.velocity=brake.ship.forward*200;double predicted=brake.stoppingDistance();auto start=brake.ship.position;brake.update(.02);
 check(brake.speed()>199&&brake.speed()<200,"braking cannot erase momentum in a frame");check(predicted>600,"braking distance accounts for reverse thrust rather than main engines");run(brake,12);
 check(brake.speed()<.01&&length(brake.ship.position-start)<predicted*1.1,"assisted reverse thrusters arrest drift within the estimated envelope");
 auto heavy=vacuum();heavy.ship.velocity=heavy.ship.forward*200;heavy.cargoMass=900;check(heavy.stoppingDistance()>predicted*1.9,"braking advice reflects current cargo inertia");
 auto coast=vacuum();coast.ship.assist=false;coast.ship.velocity={1000,0,0};coast.ship.fuel=0;coast.update(.05);check(coast.speed()>999.99,"fuel exhaustion never clamps existing momentum");coast.ship.assist=true;run(coast,120);check(coast.speed()<.02,"reserve thrusters eventually brake a fuel-starved ship");
 auto a=vacuum(),b=a,c=a;for(auto* g:{&a,&b,&c}){g->ship.throttle=.7;g->ship.cruise=true;}Input input;input.yaw=.3;input.pitch=.2;input.strafe=.4;
 run(a,6,input,1./30);run(b,6,input,1./60);run(c,6,input,1./144);
 check(length(a.ship.position-c.ship.position)<1.5&&length(b.ship.position-c.ship.position)<.7,"flight response remains stable across 30, 60 and 144 Hz");check(dot(a.ship.forward,c.ship.forward)>.99999,"angular integration remains stable across frame rates");
 auto atmosphere=vacuum();atmosphere.planets[0].centre={};atmosphere.ship.position={0,1045,0};atmosphere.ship.velocity={50,0,0};atmosphere.ship.assist=false;atmosphere.planets[0].atmosphere=1;auto airless=atmosphere;airless.planets[0].atmosphere=0;run(atmosphere,4);run(airless,4);
 check(atmosphere.ship.velocity.x<airless.ship.velocity.x-.05,"atmospheres apply density-dependent drag");check(airless.ship.velocity.y<-.1,"gravity acts during free flight above an airless surface");
 auto hover=airless;hover.ship.velocity={};hover.ship.assist=true;auto altitude=hover.altitude();run(hover,8);check(std::abs(hover.altitude()-altitude)<.2&&hover.engineLoad>0,"assisted hover spends real thrust cancelling gravity");
 hyperion::Game campaign;campaign.enterFlight();auto& ship=campaign.s.flight.ship;ship.angularVelocity={.13,-.29,.42};ship.thrusters={.23,-.19,.08};campaign.system("HOME");auto saved=hyperion::encodeFlight(campaign.s.flight,campaign.s.economy);
 auto restored=hyperion::decodeFlight(saved,campaign.s,campaign.universe);check(hyperion::encodeFlight(restored,campaign.s.economy)==saved,"spin and spooled thrusters round trip exactly");
 for(int i=0;i<120;i++)campaign.simulate(.05);check(hyperion::encodeFlight(campaign.s.flight,campaign.s.economy)==saved,"planning freezes rotational and engine dynamics as well as position");
 auto legacy=saved;legacy.erase("dynamics");restored=hyperion::decodeFlight(legacy,campaign.s,campaign.universe);check(length(restored.ship.angularVelocity)==0&&length(restored.ship.thrusters)==0,"older campaigns default safely to unspun, idle thrusters");
 for(int field=0;field<4;field++){auto bad=saved;if(field==0)bad["dynamics"]["spin"][0]=7.;if(field==1)bad["dynamics"]["thrusters"]={1.,1.,0.};if(field==2)bad["dynamics"]["spin"]={0,0};if(field==3)bad["dynamics"]=true;bool rejected=false;try{hyperion::decodeFlight(bad,campaign.s,campaign.universe);}catch(...){rejected=true;}check(rejected,"invalid dynamics are rejected transactionally");}
 campaign.s.flight.dockAt(0);check(length(ship.angularVelocity)==0&&length(ship.thrusters)==0,"docking resets residual forces and rotation");
 for(int model=0;model<vesselModelCount;model++){auto stress=vacuum();stress.ship.vessel.model=model;stress.cargoMass=cargoCapacity(stress.ship.vessel);stress.ship.assist=false;stress.ship.throttle=1;Input axes;axes.pitch=.7;axes.yaw=-.6;axes.roll=.5;axes.lift=.4;axes.strafe=.3;axes.boost=true;run(stress,60,axes);check(finite(stress.ship.position)&&finite(stress.ship.velocity)&&finite(stress.ship.angularVelocity),"all loaded hulls retain finite dynamics under sustained mixed thrust");check(std::abs(dot(stress.ship.up,stress.ship.forward))<1e-12&&std::abs(length(stress.ship.forward)-1)<1e-12,"long rotations preserve an orthonormal attitude");}
 for(int model=0;model<vesselModelCount;model++){
  auto land=vacuum();land.planets[0].centre={};land.planets[0].atmosphere=1;land.ship.vessel.model=model;land.cargoMass=cargoCapacity(land.ship.vessel);land.ship.combat.systems[Drive]=50;
  auto up=V3{0,1,0};const auto& p=land.planets[0];land.ports[0]={"Pad","",up*(p.radius+terrain(p,up)+2),up,0,"pad","",0};land.ship.position=land.ports[0].position+up*20+V3{8,0,0};land.ship.gear=true;
  check(land.dock(),"every loaded hull with a damaged drive can request landing");for(int j=0;j<4000&&!land.docked;j++)land.update(.025);
  check(land.docked&&land.ship.hull==100,"navigation lands all loaded hulls safely with reduced drive authority");
  land.ship.angularVelocity={0,1,0};land.docked=false;land.autopilot=false;land.ship.position=land.ports[0].position-up*.1;
  check(!land.landing().safe,"spinning ships cannot secure landing clamps");
 }
 std::cout<<"Handling: "<<checks<<" checks passed / momentum, RCS, engine spool, bounded thrust, mass, braking, boost, gravity, drag, hover, frame rates and saves\n";return 0;
}catch(const std::exception& e){std::cerr<<"Handling failure after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
