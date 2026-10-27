#include "game.hpp"
#include "world_collision.hpp"
#include "../worldgen.hpp"
#include <istream>
#include <ostream>
#include <utility>

namespace hyperion::flight {
double terrain(const Planet& p,V3 n){
  n=unit(n);
  double h=6+6*std::sin(n.x*13+p.seed)*std::sin(n.z*11+n.y*7)
    +3*std::sin(n.y*29+n.x*9+p.seed)*std::cos(n.z*27-p.seed)
    +1.4*std::sin(n.x*71+n.y*33)*std::sin(n.z*67);
  double flat=std::clamp((dot(n,p.portNormal)-.985)/.012,0.,1.);
  flat=flat*flat*(3-2*flat);
  double height=std::max(.5,h*(1-flat)+3*flat);
  for(const auto& scar:p.scars){double d2=2*(1-dot(n,scar.normal))*p.radius*p.radius;if(d2<scar.radius*scar.radius*1.5){double u=std::sqrt(std::max(0.,d2))/scar.radius;double bowl=std::max(0.,1-u*u);height-=scar.depth*bowl*bowl;height+=scar.depth*.22*std::exp(-std::pow((u-1)*7,2));}}
  return height;
}
V3 terrainNormal(const Planet& p,V3 n){
 n=unit(n);auto east=unit(cross({0,1,0},n),{1,0,0}),north=unit(cross(n,east));double step=.5/p.radius;
 auto point=[&](V3 d){d=unit(d);return d*(p.radius+terrain(p,d)+2);};
 auto dx=point(n+east*step)-point(n-east*step),dy=point(n+north*step)-point(n-north*step);
 auto result=unit(cross(dx,dy),n);return dot(result,n)<0?-result:result;
}
double materialElevation(const Planet& p,V3 n){
 if(p.generation>=2&&!p.giant){n=unit(n);hyperion::proc::Spec spec{p.worldSeed,p.landform,p.tectonic,p.water,p.temperature,p.life};return hyperion::proc::elevation(spec,{n.x,n.y,n.z})+(.28+.44*p.water)-(.36+.285*p.water);}
 // Same periodic lookup and six octaves used by the cached GPU surface field.
 auto hash=[](int x,int y){uint32_t h=uint32_t(x&255)*1973u+uint32_t(y&255)*9277u+89173u;h=(h^(h>>16))*2246822519u;h=(h^(h>>13))*3266489917u;return double((h^(h>>16))>>24)/255.;};
 auto noise=[&](V3 q){int x=int(std::floor(q.x)),y=int(std::floor(q.y)),z=int(std::floor(q.z));V3 f=q-V3{double(x),double(y),double(z)};f={f.x*f.x*(3-2*f.x),f.y*f.y*(3-2*f.y),f.z*f.z*(3-2*f.z)};
  auto plane=[&](int layer){int u=x+37*(z+layer),v=y+17*(z+layer);return (hash(u,v)*(1-f.x)+hash(u+1,v)*f.x)*(1-f.y)+(hash(u,v+1)*(1-f.x)+hash(u+1,v+1)*f.x)*f.y;};return plane(0)*(1-f.z)+plane(1)*f.z;};
 V3 q=unit(n)*3.2+V3{p.seed*.17,p.seed*.073,p.seed*.11};double warp=noise(q*1.9)*.6;q+=V3{warp,warp,warp};double v=0,a=.52;
 for(int i=0;i<6;i++){v+=a*noise(q);q=q*2.03+V3{7.1,13.7,19.3};a*=.49;}return v;
}
void Game::orient(V3 f,V3 u){
  ship.forward=unit(f,{0,0,-1});
  V3 right=unit(cross(ship.forward,u),unit(cross(ship.forward,{1,0,0}),{1,0,0}));
  ship.up=unit(cross(right,ship.forward));
}
void Game::message(std::string text){notice=std::move(text);noticeAge=0;}
int Game::nearestPlanet()const{
  int result=0;double d=1e30;
  for(int i=0;i<int(planets.size());i++){double a=length(ship.position-planets[i].centre)-planets[i].radius;if(a<d){d=a;result=i;}}
  return result;
}
LandingInfo Game::landing()const{
 LandingInfo result;if(planets.empty())return result;result.body=nearestPlanet();const auto& p=planets[result.body];
 result.radial=unit(ship.position-p.centre);result.normal=terrainNormal(p,result.radial);result.ground=p.centre+result.radial*(p.radius+terrain(p,result.radial)+2);
 double closest=32;for(int i=0;i<int(ports.size());i++)if(ports[i].planet==result.body&&dot(result.radial,ports[i].normal)>.9){
  auto d=ship.position-ports[i].position;double side=length(d-ports[i].normal*dot(d,ports[i].normal));
  if(side<closest){closest=side;result.port=i;result.normal=ports[i].normal;auto pad=ports[i].position+result.normal*.25;result.ground=ship.position-result.normal*dot(ship.position-pad,result.normal);}
 }
 result.altitude=dot(ship.position-result.ground,result.normal);result.sink=-dot(ship.velocity,result.normal);result.drift=length(ship.velocity+result.normal*result.sink);
 result.tilt=std::acos(std::clamp(dot(ship.up,result.normal),-1.,1.))*180/pi;result.slope=std::acos(std::clamp(dot(result.normal,result.radial),-1.,1.))*180/pi;
 result.water=p.kind!=2&&!p.giant&&materialElevation(p,result.radial)<.28+.44*p.water;
 result.available=!p.giant;result.safe=result.available&&ship.gear&&result.sink<=6&&result.drift<=10&&result.tilt<=35&&result.slope<=25&&length(ship.angularVelocity)<.35;return result;
}
double Game::altitude()const{return planets.empty()?0:landing().altitude;}
void Game::launch(){
  if(!docked||planets.empty()||ports.empty())return;
  bool surface=dockedPort<0||ports[dockedPort].planet>=0;
  // Clamps release at the existing pose. Spool, attitude and acceleration are
  // resolved by update(), so Enter never shifts or turns the camera in one frame.
  ship.velocity={};resetDynamics();
  launchAssist=3.2;touchdownAge=10;liftThrust=0;
  savedDock.clear();ship.throttle=0;docked=false;ship.gear=surface;ship.cruise=false;
  message(surface?"Departure assist / vertical lift / steer or thrust to take control / J for navigation.":"Docking clamps released / clearing the berth. Clear skies, commander.");
}
void Game::dockAt(int index,bool preserveAttitude){
 if(index>=0&&index<int(ports.size())&&ports[index].recovery){cancelNavigation();ship.throttle=0;ship.velocity={};docked=false;dockedPort=-1;savedDock.clear();message("Recovery approach / hold outside the wreck and close below 5 km/s");return;}
 if(index>=0&&index<int(ports.size())&&ports[index].disabled){index=-1;for(int i=0;i<int(ports.size());i++)if(!ports[i].disabled&&ports[i].minor<0){index=i;break;}if(index<0){docked=false;dockedPort=-1;savedDock.clear();return;}}
  if(index<0||index>=int(ports.size()))return;
  if(ports[index].minor>=0){message("Fly-by destination / no docking facilities. Survey from the destination panel.");return;}
  ship.position=ports[index].position+ports[index].normal*(ports[index].planet<0?90:.25);
  ship.velocity={};ship.throttle=0;ship.cruise=false;docked=true;dockedPort=index;lastBerth=index;autopilot=false;route.clear();
  ship.gear=true;resetDynamics();
  launchAssist=0;liftThrust=0;touchdownAge=0;
  V3 n=ports[index].normal;
  if(!preserveAttitude){V3 tangent=unit(cross({1,0,0},n),{0,0,-1});orient(ports[index].planet<0?tangent*.6-n*.8:tangent+n*.16,n);}
  message("Docked at "+ports[index].name+". E opens the exchange / Enter launches.");
  events.push_back({"dock",index,0,""});recordIds();
}
bool Game::dock(){
  if(ports.empty())return false;
  if(docked){launch();return true;}
  int nearest=-1;double best=1e30;
  for(int i=0;i<int(ports.size());i++)if(ports[i].minor<0&&!ports[i].disabled&&!ports[i].recovery){double d=length(ship.position-ports[i].position);if(d<best){best=d;nearest=i;}}
  if(nearest<0)return false;
  double limit=ports[nearest].planet>=0?55:95;
  if(best<limit&&speed()<55){
   const auto& port=ports[nearest];if(port.planet>=0&&dot(ship.position-port.position,port.normal)<-2){message("Approach the landing pad from above.");return false;}
   target=nearest;cancelNavigation();launchAssist=0;ship.assistCaptureTime=0;ship.assistCapture={};ship.gear=true;ship.cruise=false;
   if(port.planet>=0){auto offset=ship.position-port.position;double height=dot(offset,port.normal),side=length(offset-port.normal*height);
    // An already aligned low approach continues downward without first climbing
    // to the old fixed holding point. Offset approaches still align safely above.
    if(side>10)route.push_back(port.position+port.normal*std::max(18.,std::min(65.,std::max(height,best*.55))));
   }
   route.push_back(port.position+port.normal*(port.planet<0?90:.25));autopilot=true;recordIds();
   message(port.planet>=0?"Landing assist / aligning over the pad / steer to cancel.":"Docking assist / matching the berth / steer to cancel.");return true;
  }
  message(best>=limit?"Move within "+std::to_string(int(limit))+" km of a port, or press J for docking assist.":"Too fast to dock. Hold S or Backspace to brake.");return false;
}
void Game::cancelNavigation(){autopilot=false;route.clear();}
void Game::selectTarget(int i){if(i<0||i>=int(ports.size()))return;target=i;recordIds();cancelNavigation();message("Destination: "+ports[target].name+". Press J to engage navigation.");}
namespace {
bool intersects(V3 a,V3 b,const Planet&p,double clearance){
  V3 d=b-a;double l=dot(d,d);if(l<.01)return false;
  double t=std::clamp(dot(p.centre-a,d)/l,0.,1.);
  return length(a+d*t-p.centre)<p.radius+clearance;
}
}
std::vector<V3> transferRoute(const std::vector<Planet>& planets,V3 start,const Port& port){
  std::vector<V3> route;V3 current=start;
  for(const auto&p:planets)if(length(current-p.centre)<p.radius+295){current=p.centre+unit(current-p.centre)*(p.radius+320);route.push_back(current);}
  V3 end=port.position+port.normal*(port.planet>=0?320:130);
  // Route around actual spherical bodies; departure and final descent are radial.
  for(int pass=0;pass<4;pass++){
    int blocker=-1;double nearest=1e30;
    for(int i=0;i<int(planets.size());i++)if(intersects(current,end,planets[i],130)){
      double d=length(planets[i].centre-current);if(d<nearest){nearest=d;blocker=i;}
    }
    if(blocker<0)break;
    const auto&p=planets[blocker];V3 n=unit(current-p.centre),finish=unit(end-p.centre);
    current=p.centre+n*(p.radius+320);route.push_back(current);
    V3 axis=unit(cross(n,finish),unit(cross(n,{0,0,1}),{1,0,0}));
    for(int step=0;step<16&&intersects(current,end,p,130);step++){
      double angle=std::acos(std::clamp(dot(n,finish),-1.,1.));n=rotate(n,axis,std::min(.25,angle));
      current=p.centre+n*(p.radius+320);route.push_back(current);
    }
  }
  route.push_back(end);route.push_back(port.position+port.normal*(port.planet<0?90:.25));
  return route;
}
RoutePose freightPose(const std::vector<V3>& path,double progress){
 if(path.empty())return {{},{0,0,-1}};
 double total=0;for(size_t i=1;i<path.size();i++)total+=length(path[i]-path[i-1]);
 double u=std::clamp((progress-.05)/.90,0.,1.);u=u*u*(3-2*u);double distance=u*total;
 for(size_t i=1;i<path.size();i++){double segment=length(path[i]-path[i-1]);if(segment<1e-8)continue;if(distance<=segment||i+1==path.size())return {mix(path[i-1],path[i],std::clamp(distance/segment,0.,1.)),unit(path[i]-path[i-1],{0,0,-1})};distance-=segment;}
 return {path.back(),{0,0,-1}};
}
void Game::navigate(bool resume){
  if(ports.empty()||planets.empty())return;
  if(autopilot){cancelNavigation();message("Navigation released. You have the ship.");return;}
  if(docked&&dockedPort==target){message("Already at this port. Tab selects another destination.");return;}
  if(ship.fuel<1&&!resume){message("Refuel before engaging the navigator.");return;}
  if(docked)launch();
  launchAssist=0;ship.assistCaptureTime=0;ship.assistCapture={};
  route=transferRoute(navigationBodies(),ship.position,ports[target]);
  autopilot=true;ship.cruise=true;ship.gear=false;
  message("Navigator engaged: "+ports[target].name+". J cancels / steering returns manual control.");
}
void Game::update(double dt,const Input& supplied){
  Input in=supplied;ship.previous=ship.position;
  if(planets.empty()||ports.empty())return;
  dt=std::clamp(dt,0.,.05);time+=dt;noticeAge+=dt;fireClock-=dt;hitFlash=std::max(0.,hitFlash-dt*2);
  touchdownAge=std::min(10.,touchdownAge+dt);liftThrust=0;prepareCombat(dt,in);
  bool manual=std::abs(in.pitch)+std::abs(in.yaw)+std::abs(in.roll)+std::abs(in.throttle)+std::abs(in.lift)+std::abs(in.strafe)>.01;
  if(!docked&&manual){
    if(launchAssist>0){launchAssist=0;message("Departure assist released / manual control.");}
    if(autopilot){cancelNavigation();message("Manual control.");}
  }
  // Integrate forces, contacts and RCS in steps of at most 10 ms.
  int steps=std::max(1,int(std::ceil(dt/.01)));double step=dt/steps;
  if(dt>0)for(int i=0;i<steps&&!docked;i++)stepHandling(step,in);
  worldContacts(*this);updateCombat(dt,in);
  if(ship.hull<=0){events.push_back({"rescue",lastBerth,0,"",lastAttacker,"NOMA"});bolts.clear();combatTarget.clear();lock=0;ship.combat.energy=25;ship.combat.heat=0;ship.combat.overheated=false;ship.hull=25;ship.shield=0;ship.fuel=std::max(5.,ship.fuel);dockAt(std::clamp(lastBerth,0,int(ports.size())-1));message("Emergency rescue to the last berth. Cargo lost; repair before departure.");}
  recordIds();
}
void Game::recordIds(){
  if(target>=0&&target<int(ports.size()))savedTarget=ports[target].id;
  savedDock=docked&&dockedPort>=0&&dockedPort<int(ports.size())?ports[dockedPort].id:"";
  if(lastBerth>=0&&lastBerth<int(ports.size()))savedBerth=ports[lastBerth].id;
}
}
