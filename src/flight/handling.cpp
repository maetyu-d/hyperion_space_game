#include "game.hpp"
#include "weather.hpp"

namespace hyperion::flight {
namespace {
V3 limited(V3 v,double maximum){double n=length(v);return n>maximum?v*(maximum/n):v;}
struct Axes {
 V3 right,up,forward;
 V3 local(V3 v)const{return {dot(v,right),dot(v,up),dot(v,forward)};}
 V3 world(V3 v)const{return right*v.x+up*v.y+forward*v.z;}
};
Axes axes(const Ship& s){return {unit(cross(s.forward,s.up)),s.up,s.forward};}
V3 authority(const Game& g,double boost){
 double drive=g.thrustFactor*speedFactor(g.ship.vessel)*(.25+.75*g.ship.combat.systems[Drive]/100)/g.inertialMass();
 if(g.ship.fuel<=0)return V3{12,14,16}/g.inertialMass();
 return {22*drive*(1+.25*boost),26*drive*(1+.25*boost),52*drive*(1+1.4*boost)};
}
double retroAuthority(const Game& g,V3 caps,double boost){return g.ship.fuel<=0?caps.z:caps.z/(1+1.4*boost)*(1+.25*boost)*32./52.;}
V3 normalizedForce(V3 force,V3 caps,double retro){return {force.x/caps.x,force.y/caps.y,force.z/(force.z>=0?caps.z:retro)};}
V3 allocateForce(V3 support,V3 command,V3 caps,double retro){
 // Reserve the thrust needed to oppose gravity, then fit the pilot's command
 // into the remaining convex force envelope. Heavy lateral braking must not
 // scale the hover jets away. No force is added beyond actual jet authority.
 double supportLoad=length(normalizedForce(support,caps,retro));if(supportLoad>1)support=support/supportLoad;
 auto total=support+command;if(dot(normalizedForce(total,caps,retro),normalizedForce(total,caps,retro))<=1)return total;
 double low=0,high=1;
 for(int i=0;i<18;i++){double t=(low+high)*.5;auto n=normalizedForce(support+command*t,caps,retro);if(dot(n,n)<=1)low=t;else high=t;}
 return support+command*low;
}
V3 angularTarget(const Ship& s,V3 heading,V3 desiredUp,double rate){
 auto forward=unit(heading,s.forward);double angle=std::acos(std::clamp(dot(s.forward,forward),-1.,1.));
 auto axis=unit(cross(s.forward,forward),s.up);
 auto up=unit(desiredUp-s.forward*dot(desiredUp,s.forward),s.up);
 double roll=std::atan2(dot(cross(s.up,up),s.forward),dot(s.up,up));
 return limited(axis*std::min(rate,angle*2.2)+s.forward*std::clamp(roll*2.,-rate,rate),rate);
}
}
double Game::inertialMass()const{
 // Hull strength approximates dry mass in this compressed-scale universe.
 // Cargo and reinforced fittings add inertia, rather than reducing a speed cap.
 return vesselDef(ship.vessel.model).hull/100.+std::max(0.,cargoMass)/900.
       +.035*ship.vessel.fitted[ReinforcedHull]+.025*ship.vessel.fitted[CargoRacks];
}
V3 Game::gravity()const{
 V3 a;for(const auto& p:planets){auto r=p.centre-ship.position;double d2=dot(r,r);
  a+=unit(r)*(.7*p.radius*p.radius/std::max(d2,p.radius*p.radius*.25));
 }return a;
}
double Game::atmosphereDensity()const{
 if(planets.empty())return 0;const auto& p=planets[nearestPlanet()];
 double h=std::max(0.,length(ship.position-p.centre)-p.radius);
 if(h>250||p.atmosphere<=0)return 0;
 return std::min(3.,p.atmosphere)*std::exp(-h/22.);
}
double Game::thrustLimit(V3 direction,bool boost)const{
 auto v=axes(ship).local(unit(direction));auto limit=authority(*this,boost);
 double retro=retroAuthority(*this,limit,boost?1.:0.);
 double z=v.z>=0?limit.z:retro;
 return 1/std::sqrt(v.x*v.x/(limit.x*limit.x)+v.y*v.y/(limit.y*limit.y)+v.z*v.z/(z*z));
}
double Game::manualSpeedLimit(bool precision)const{
 double drive=thrustFactor*speedFactor(ship.vessel)*(.25+.75*ship.combat.systems[Drive]/100);
 double cap=(ship.cruise?360:85)*drive*(1+1.4*ship.boostLevel);
 if(!planets.empty()){
  double u=std::clamp((altitude()-60)/140.,0.,1.);u=u*u*(3-2*u);
  cap=std::min(cap,105+(std::max(105.,cap)-105)*u);
 }
 if(ship.fuel<=0)cap=11.8;
 return precision?std::min(cap*.18,18.):cap;
}
void Game::setAssist(bool enabled){
 if(ship.assist==enabled)return;
 launchAssist=0;cancelNavigation();ship.assist=enabled;ship.assistCaptureTime=0;ship.assistCapture={};
 if(enabled&&!docked){
  // Match forward speed before gently arresting sideways drift. Re-engaging
  // assist is distinct from Backspace, which explicitly commands a full stop.
  ship.throttle=std::clamp(dot(ship.velocity,ship.forward)/std::max(.1,manualSpeedLimit()),0.,1.);
  ship.assistCapture=ship.velocity;ship.assistCaptureTime=.8;
 }
 message(enabled?"Flight assist / matching forward speed / Backspace stops the ship.":"Inertial flight / momentum and rotation retained / X restores assist.");
}
void Game::brake(){
 launchAssist=0;cancelNavigation();ship.throttle=0;ship.assist=true;ship.cruise=false;ship.assistCaptureTime=0;ship.assistCapture={};
 message("Braking / reverse and manoeuvring jets / allow stopping distance.");
}
double Game::stoppingDistance()const{
 if(speed()<.01)return 0;
 auto basis=axes(ship);auto direction=unit(ship.velocity),g=gravity();auto caps=authority(*this,0.);double retro=retroAuthority(*this,caps,0.);
 auto force=allocateForce(basis.local(-g),basis.local(-direction*10000),caps,retro);
 double a=dot(basis.world(force)+g,-direction);
 auto actual=authority(*this,ship.boostLevel);double actualRetro=retroAuthority(*this,actual,ship.boostLevel);
 auto thrust=basis.world({ship.thrusters.x*actual.x,ship.thrusters.y*actual.y,ship.thrusters.z*(ship.thrusters.z>=0?actual.z:actualRetro)});
 double delay=.18+std::max(0.,dot(thrust,direction))/std::max(.01,a)*.12;
 return a>.01?speed()*delay+speed()*speed()/(2*a):1e9;
}
void Game::stepHandling(double dt,const Input& in){
 auto settle=[&](const LandingInfo& c){
  ship.position=c.ground;ship.velocity={};ship.throttle=0;docked=true;dockedPort=-1;cancelNavigation();touchdownAge=0;liftThrust=0;resetDynamics();
  if(c.port>=0){dockAt(c.port,true);ship.position=c.ground;}
  else message("Touchdown / landing gear secure / L deploys / Enter lifts off.");
 };
 if(!autopilot&&launchAssist<=0){auto c=landing();if(c.altitude<0&&c.safe){settle(c);return;}}
 auto basis=axes(ship);double agility=agilityFactor(ship.vessel);
 double drive=thrustFactor*speedFactor(ship.vessel)*(.25+.75*ship.combat.systems[Drive]/100);
 bool navigating=autopilot&&!route.empty(),departing=launchAssist>0;
 precisionActive=in.precision&&!navigating&&!departing;
 double boostTarget=in.boost&&!precisionActive&&!navigating&&!departing&&ship.fuel>0?1.:0.;
 ship.boostLevel+=(boostTarget-ship.boostLevel)*(1-std::exp(-dt/(boostTarget>.5?.18:.12)));
 if(ship.boostLevel<1e-7)ship.boostLevel=0;
 if(std::abs(in.throttle)+std::abs(in.lift)+std::abs(in.strafe)>.01){ship.assistCaptureTime=0;ship.assistCapture={};}
 V3 wanted,spin;bool velocityControl=navigating||departing||ship.assist;
 if(navigating){
  auto delta=route.front()-ship.position;double d=length(delta);bool final=route.size()==1;
  if(final&&ports[target].minor>=0&&d<8&&speed()<4){cancelNavigation();ship.throttle=0;ship.cruise=false;ship.gear=false;message("Fly-by arrival / "+ports[target].name+" / survey available in destinations");return;}
  if(d<(final?.025:15)&&speed()<(final?.15:24.)){
   route.erase(route.begin());if(route.empty()){dockAt(target,true);return;}
   delta=route.front()-ship.position;d=length(delta);final=route.size()==1;
  }
  bool surfaceFinal=final&&ports[target].planet>=0;
  // Begin braking from actual available retro/lateral thrust, including cargo
  // and damaged drives. Every route corner is approached at a controllable speed.
  double braking=std::max(1.,std::min(thrustLimit(-delta),thrustLimit(-ship.velocity))*.65);
  double cap=ship.fuel<=0?11.8:ship.fuel<1?120.:540*drive;
  double finalLimit=surfaceFinal?(d<4?1.8:d<18?5.:22.):18.;
  double desiredSpeed=std::min({final?finalLimit:540*drive,cap,d*(final?.7:1.1),std::sqrt(2*braking*d)});
  wanted=unit(delta)*desiredSpeed;
  if(surfaceFinal){
   auto up=ports[target].normal;double height=-dot(delta,up);auto side=delta-up*dot(delta,up);double offset=length(side);
   double sinkLimit=std::min(cap,std::clamp(height*.55,.08,18.));
   double alignment=std::clamp((25-offset)/20.,0.,1.);alignment=alignment*alignment*(3-2*alignment);
   wanted=side*std::min(.85,std::min(12.,cap)/std::max(.001,offset))+up*std::clamp(dot(delta,up)*.6,-sinkLimit*alignment,6.);
   wanted=limited(wanted,cap);
  }
  const auto& p=planets[nearestPlanet()];auto radial=unit(ship.position-p.centre);
  V3 up=length(ship.position-p.centre)<p.radius+1000?radial:V3{0,1,0};V3 heading=delta;
  if(surfaceFinal){
   up=ports[target].normal;auto horizontal=unit(ship.forward-up*dot(ship.forward,up),unit(cross({1,0,0},up),{0,0,-1}));
   double flare=std::clamp((-dot(delta,up)-1)/32.,0.,1.);flare=flare*flare*(3-2*flare);
   heading=horizontal+up*(.025-.32*flare);
  }
  spin=angularTarget(ship,heading,up,1.4*agility);ship.throttle=desiredSpeed/540.;ship.gear=final;
 }else if(departing){
  auto contact=landing();bool surface=contact.available&&contact.altitude<100;auto up=surface?contact.normal:contact.radial;
  double elapsed=3.2-launchAssist,spool=std::clamp(elapsed/1.1,0.,1.),handoff=std::clamp((elapsed-2.1)/1.1,0.,1.);
  spool=spool*spool*(3-2*spool);handoff=handoff*handoff*(3-2*handoff);
  auto horizontal=unit(ship.forward-up*dot(ship.forward,up),unit(cross({1,0,0},up),{0,0,-1}));
  spin=angularTarget(ship,surface?unit(horizontal+up*.12):up,surface?up:ship.up,(surface?.8:1.4)*agility);
  double cruise=.08*85*drive,lift=std::min(ship.fuel<=0?11.8:14.,14.*spool);
  wanted=surface?up*(lift*(1-handoff))+ship.forward*(cruise*handoff):up*(12.*spool);
  launchAssist=std::max(0.,launchAssist-dt);ship.throttle=0;ship.cruise=false;
  if(surface&&contact.altitude>8&&elapsed>1.8)ship.gear=false;
  if(launchAssist<=0){ship.gear=surface&&contact.altitude<8;ship.throttle=.08;message(surface?"Ground clear / departure complete / W thrust / R climb / J navigate.":"Berth clear / departure complete / W thrust / J navigate.");}
 }else{
  spin=basis.right*(std::clamp(in.pitch,-1.,1.)*1.05*agility)-basis.up*(std::clamp(in.yaw,-1.,1.)*.95*agility)+basis.forward*(std::clamp(in.roll,-1.,1.)*1.2*agility);
  ship.throttle=std::clamp(ship.throttle+in.throttle*dt*.36,0.,1.);
  double fine=precisionActive?.22:1.;spin=spin*(precisionActive?.35:1.);
  wanted=basis.world({std::clamp(in.strafe,-1.,1.)*24*fine,std::clamp(in.lift,-1.,1.)*26*fine,ship.throttle*manualSpeedLimit(precisionActive)});
  if(ship.fuel<=0)wanted=limited(wanted,11.8);
  if(ship.assistCaptureTime>0&&ship.assist){
   double u=std::clamp(1-ship.assistCaptureTime/.8,0.,1.);u=u*u*(3-2*u);wanted=mix(ship.assistCapture,wanted,u);
   ship.assistCaptureTime=std::max(0.,ship.assistCaptureTime-dt);if(ship.assistCaptureTime==0)ship.assistCapture={};
  }
 }
 // Assisted RCS regulates angular rate. With assist disabled, no input means
 // no torque: the ship retains its spin and linear momentum in vacuum.
 double torque=4.2*agility/std::sqrt(inertialMass())*(.4+.6*ship.combat.systems[Drive]/100);
 auto oldSpin=ship.angularVelocity;
 V3 angularAcceleration=velocityControl?limited((spin-oldSpin)*9,torque):limited(spin*3.5,torque);
 if(!velocityControl&&length(oldSpin)>2.2*agility){auto along=unit(oldSpin);double added=dot(angularAcceleration,along);if(added>0)angularAcceleration=angularAcceleration-along*added;}
 auto nextSpin=oldSpin+angularAcceleration*dt;
 // At the RCS governor, tangential torque must not accumulate numerical spin.
 // Preserve an already higher rate; only further acceleration is constrained.
 if(!velocityControl)nextSpin=limited(nextSpin,std::max(length(oldSpin),2.2*agility));
 ship.angularVelocity=nextSpin*std::exp(-dt*atmosphereDensity()*.12);
 V3 midSpin=(oldSpin+ship.angularVelocity)*.5;double angle=length(midSpin)*dt;
 if(angle>1e-12)orient(rotate(ship.forward,midSpin,angle),rotate(ship.up,midSpin,angle));
 basis=axes(ship);
 auto gravityForce=gravity();double density=atmosphereDensity();
 if(weatherHours>=0&&!planets.empty()){const auto& p=planets[nearestPlanet()];auto weather=weatherAt(p,unit(ship.position-p.centre),weatherHours);gravityForce+=weather.wind*std::min(1.,density);}
 auto localVelocity=basis.local(ship.velocity);double dragScale=speed()*density*(ship.gear?1.8:1);
 V3 drag=basis.world({-localVelocity.x*.00012,-localVelocity.y*.00015,-localVelocity.z*.000055})*dragScale;
 V3 support,requested;
 if(velocityControl){support=-gravityForce;requested=(wanted-ship.velocity)*(navigating?3.:departing?2.2:1.6)-drag;}
 else requested=basis.world({std::clamp(in.strafe,-1.,1.)*16*drive,std::clamp(in.lift,-1.,1.)*16*drive,ship.throttle*52*drive*(1+1.4*ship.boostLevel)})*((precisionActive?.22:1.)/inertialMass());
 if(!velocityControl&&speed()>1){
  double envelope=ship.fuel<=0?11.8:900*drive;
  auto along=unit(ship.velocity);double forwardWork=dot(requested,along);
  if(forwardWork>0)requested=requested-along*(forwardWork*std::clamp((speed()-envelope*.85)/(envelope*.15),0.,1.));
 }
 auto caps=authority(*this,ship.boostLevel);double retro=retroAuthority(*this,caps,ship.boostLevel);
 auto allocated=allocateForce(basis.local(support),basis.local(requested),caps,retro);
 V3 demand=normalizedForce(allocated,caps,retro);
 ship.thrusters=mix(ship.thrusters,demand,1-std::exp(-dt/ .12));
 // Passive drag cannot reverse momentum, even at extreme atmospheric speeds.
 double maxDrag=speed()/std::max(.001,dt)*.5;drag=limited(drag,maxDrag);
 auto oldVelocity=ship.velocity;auto previousPosition=ship.position;
 auto thrust=basis.world({ship.thrusters.x*caps.x,ship.thrusters.y*caps.y,ship.thrusters.z*(ship.thrusters.z>=0?caps.z:retro)});
 acceleration=thrust+gravityForce+drag;ship.velocity+=acceleration*dt;
 ship.position+=(oldVelocity+ship.velocity)*(.5*dt);
 minorContacts(previousPosition);
 engineLoad=length(ship.thrusters);liftThrust=std::max(0.,ship.thrusters.y);
 double previousFuel=ship.fuel;
 // Propellant follows engine/RCS effort. Fast, unpowered coasting is free.
 double propellantLoad=std::hypot(ship.thrusters.x,ship.thrusters.y)*(1+.25*ship.boostLevel)
                      +std::abs(ship.thrusters.z)*(ship.thrusters.z>0?1+1.4*ship.boostLevel:1+.25*ship.boostLevel);
 ship.fuel=std::max(0.,ship.fuel-dt*vesselDef(ship.vessel.model).burn*(propellantLoad*.12+length(angularAcceleration)*.002));
 if(previousFuel>0&&ship.fuel<=0)message("Fuel exhausted. Reserve thrusters available / assisted speed 12 km/s / brake early.");
 auto contact=landing();
 if(contact.altitude<0){
  if(departing&&std::max(0.,contact.sink)<.3){
   // Pad support while the departure engines spool; no bounce or free lift.
   ship.position=contact.ground;if(contact.sink>0)ship.velocity+=contact.normal*contact.sink;
  }else if(contact.safe&&launchAssist<=0){
   settle(contact);
  }else{
   ship.position=contact.ground+contact.normal*.08;double impact=std::max(0.,contact.sink);
   ship.velocity=ship.velocity+contact.normal*(impact*1.25);ship.velocity=ship.velocity*.92;
   damageShip(impact*.35+std::max(0.,contact.drift-10)*.08,CollisionDamage,contact.ground);hitFlash=std::min(.6,impact*.06);
   if(noticeAge>.8)message("Unsafe contact / reduce descent and drift / level the ship / G lowers gear.");
  }
 }
}
}
