#include "game.hpp"
#include "flight/controls.hpp"
#include "flight_storage.hpp"
#include "json.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace hyperion::flight;
namespace {
int checks=0;void check(bool b,const char* m){++checks;if(!b)throw std::runtime_error(m);}
Game vacuum(){Game f;f.planets.push_back({"Distant","rock",{0,-1e9,0},1000,3.1,{0,1,0},false});f.planets[0].atmosphere=0;f.ports.push_back({"Berth","",{0,-1e9+2000,0},{0,1,0},-1,"port","",0});f.docked=false;f.ship.gear=false;f.orient({0,0,-1},{0,1,0});return f;}
void run(Game& f,double seconds,Input in={},double dt=.01){for(int i=0;i<int(std::round(seconds/dt));i++)f.update(dt,in);}
Game surface(){auto f=vacuum();f.planets[0].centre={};f.ship.position={0,1050,0};return f;}
}
int main(){try{
 for(int hz:{30,60,144}){auto look=pointerLook(90./hz,-60./hz,1./hz);check(std::abs(look.yaw-.225)<1e-12&&std::abs(look.pitch+.15)<1e-12,"mouse rate is independent of render frequency");}
 check(pointerLook(100,100,0).yaw==0&&pointerLook(std::numeric_limits<double>::infinity(),1,.01).pitch==0,"invalid pointer samples cannot poison helm commands");
 auto regular=vacuum(),fine=regular;regular.ship.throttle=fine.ship.throttle=.7;Input lateral;lateral.strafe=.7;lateral.lift=.5;auto precise=lateral;precise.precision=true;run(regular,4,lateral);run(fine,4,precise);
 check(fine.speed()<regular.speed()*.3&&fine.precisionActive,"precision allows deliberate close-range translation");
 auto rcs=vacuum(),fineRcs=rcs;Input turn;turn.yaw=.8;auto fineTurn=turn;fineTurn.precision=true;run(rcs,1,turn);run(fineRcs,1,fineTurn);check(length(fineRcs.ship.angularVelocity)<length(rcs.ship.angularVelocity)*.4,"precision reduces steering rate without snapping orientation");
 auto inertial=vacuum(),fineInertial=inertial;inertial.ship.assist=fineInertial.ship.assist=false;inertial.ship.throttle=fineInertial.ship.throttle=1;run(inertial,1);Input precision;precision.precision=precision.boost=true;run(fineInertial,1,precision);
 check(fineInertial.speed()<inertial.speed()*.25&&fineInertial.ship.boostLevel==0,"inertial precision scales force and inhibits boost");
 fineInertial.ship.throttle=0;fineInertial.ship.thrusters={};fineInertial.ship.velocity={500,0,0};fineInertial.update(.01,precision);check(fineInertial.speed()>499.9,"precision never clamps existing momentum");
 auto engine=vacuum();engine.ship.assist=false;engine.ship.throttle=1;run(engine,2);double old=length(engine.acceleration);Input boost;boost.boost=true;engine.update(.01,boost);
 check(engine.ship.boostLevel>0&&engine.ship.boostLevel<.1&&std::abs(length(engine.acceleration)-old)<6,"boost engages with a bounded pressure ramp");run(engine,2,boost);old=length(engine.acceleration);engine.update(.01);
 check(engine.ship.boostLevel>.8&&engine.ship.boostLevel<1&&std::abs(length(engine.acceleration)-old)<10,"boost release fades force rather than cutting it in one frame");
 double predicted=engine.stoppingDistance();auto start=engine.ship.position,velocity=engine.ship.velocity,heading=engine.ship.forward;engine.brake();
 check(length(engine.ship.velocity-velocity)==0&&length(engine.ship.position-start)==0&&dot(engine.ship.forward,heading)>.999999,"brake command preserves pose and momentum");
 check(engine.ship.assist&&!engine.ship.cruise&&engine.ship.throttle==0,"brake commits a full stop in local flight");
 for(int i=0;i<4000&&engine.speed()>.1;i++)engine.update(.01);
 check(engine.speed()<=.1&&length(engine.ship.position-start)<predicted*1.12,"stopping-distance advice includes reversal from a boosted forward burn");
 auto loaded=surface();loaded.ship.vessel.model=2;loaded.cargoMass=900;loaded.ship.combat.systems[Drive]=50;loaded.ship.velocity={300,0,0};loaded.ship.assist=true;run(loaded,.5);
 check(loaded.ship.velocity.x<300&&std::abs(loaded.ship.velocity.y)<.13,"loaded lateral braking retains gravity support with a damaged drive");check(length(loaded.ship.thrusters)<=1.000001,"support allocation cannot exceed the combined jet budget");
 auto nose=surface();nose.planets[0].atmosphere=1;nose.ship.velocity={0,0,-70};nose.ship.assist=false;auto broadside=nose;broadside.orient({1,0,0},{0,1,0});run(nose,2);run(broadside,2);
 check(broadside.ship.velocity.z>nose.ship.velocity.z+.05,"broadside atmospheric drag exceeds nose-first drag");
 auto spinAir=surface();spinAir.planets[0].atmosphere=1;spinAir.ship.assist=false;spinAir.ship.angularVelocity={0,.7,0};auto spinVacuum=spinAir;spinVacuum.planets[0].atmosphere=0;run(spinAir,2);run(spinVacuum,2);
 check(length(spinVacuum.ship.angularVelocity)-length(spinAir.ship.angularVelocity)>.005,"atmosphere damps spin while airless flight preserves it");
 auto low=surface();low.ship.cruise=true;auto& p=low.planets[0];double ground=p.radius+terrain(p,{0,1,0})+2;low.ship.position={0,ground+59.9,0};double cap=low.manualSpeedLimit();low.ship.position.y+=.2;
 check(std::abs(low.manualSpeedLimit()-cap)<.01,"low-altitude speed restriction has no threshold step");low.ship.position.y=ground+220;check(low.manualSpeedLimit()>350,"clear altitude restores cruise authority");
 auto handover=vacuum();handover.ship.assist=false;handover.ship.velocity={30,0,-80};handover.ship.throttle=.1;handover.ship.angularVelocity={0,.35,0};velocity=handover.ship.velocity;start=handover.ship.position;handover.setAssist(true);
 check(handover.ship.assistCaptureTime==.8&&handover.ship.throttle>.9,"assist re-entry matches forward speed and begins a drift capture");check(length(handover.ship.velocity-velocity)==0&&length(handover.ship.position-start)==0,"assist switch never moves or accelerates the ship instantly");handover.update(.01);
 check(length(handover.ship.velocity-velocity)<.02,"capture does not immediately apply a large lateral braking force");run(handover,1);
 check(length(handover.ship.angularVelocity)<.01&&handover.ship.assistCaptureTime==0&&std::abs(handover.ship.velocity.x)<30,"capture damps rotation and resolves lateral drift");handover.setAssist(false);handover.setAssist(true);Input lift;lift.lift=1;handover.update(.01,lift);check(handover.ship.assistCaptureTime==0,"explicit translational input overrides assist capture");
 auto governed=vacuum();governed.ship.assist=false;governed.ship.angularVelocity={0,2.2,0};Input offAxis;offAxis.roll=1;offAxis.pitch=.6;offAxis.yaw=-.1;run(governed,60,offAxis);
 check(length(governed.ship.angularVelocity)<=2.20000000001,"sustained off-axis RCS input cannot add numerical spin above the governor");
 auto mouse30=vacuum(),mouse144=mouse30;
 for(auto pair:{std::pair<Game*,int>{&mouse30,30},{&mouse144,144}})for(int i=0;i<pair.second*3;i++){auto look=pointerLook(90./pair.second,-60./pair.second,1./pair.second);Input helm;helm.yaw=look.yaw;helm.pitch=look.pitch;pair.first->update(1./pair.second,helm);}
 check(dot(mouse30.ship.forward,mouse144.ship.forward)>.99999,"identical physical mouse movement produces matching flight attitude across frame rates");
 hyperion::Game campaign;campaign.enterFlight();auto& f=campaign.s.flight;f.docked=false;f.savedDock.clear();campaign.s.economy.docked.clear();f.ship.assist=false;f.ship.velocity=f.ship.forward*55;f.setAssist(true);f.ship.boostLevel=.42;campaign.system("HOME");auto saved=hyperion::encodeFlight(f,campaign.s.economy);auto restored=hyperion::decodeFlight(saved,campaign.s,campaign.universe);
 check(hyperion::encodeFlight(restored,campaign.s.economy)==saved,"boost pressure and unfinished drift capture save exactly");for(int i=0;i<120;i++)campaign.simulate(.05);check(hyperion::encodeFlight(f,campaign.s.economy)==saved,"planning freezes boost and capture timers");
 for(int invalid=0;invalid<5;invalid++){auto bad=saved;if(invalid==0)bad["dynamics"]["boost"]=1.01;if(invalid==1)bad["dynamics"]["capture"]["remaining"]=.9;if(invalid==2)bad["dynamics"]["capture"]["velocity"]={1e9,0,0};if(invalid==3)bad["assist"]=false;if(invalid==4)bad["autopilot"]=true;bool rejected=false;try{hyperion::decodeFlight(bad,campaign.s,campaign.universe);}catch(...){rejected=true;}check(rejected,"invalid boost or capture fields are rejected");}
 auto oldSave=saved;oldSave["dynamics"].erase("boost");oldSave["dynamics"].erase("capture");restored=hyperion::decodeFlight(oldSave,campaign.s,campaign.universe);check(restored.ship.boostLevel==0&&restored.ship.assistCaptureTime==0,"established saves default to idle boost and no capture");
 f.navigate();check(f.autopilot&&f.ship.assistCaptureTime==0,"navigation clears manual assist capture");f.ship.assistCaptureTime=.5;f.brake();check(!f.autopilot&&f.ship.assistCaptureTime==0&&f.launchAssist==0,"explicit brake clears all competing flight controllers");
 std::cout<<"Helm: "<<checks<<" checks passed / precision, pointer rates, boost spool, reversal braking, gravity allocation, atmospheric drag, capture, persistence and controller ownership\n";return 0;
}catch(const std::exception& e){std::cerr<<"Helm failure after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
