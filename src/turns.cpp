#include "game.hpp"
#include <sstream>
#include <iomanip>
namespace hyperion {
std::string TurnReport::summary()const {
 if(!valid)return "Awaiting orders";
 std::ostringstream out;out<<(strategic?"Turn "+std::to_string(last):"Shift")<<" / "<<hours<<" h resolved";
 auto delta=[&](const char* label,double value){if(std::abs(value)>=.05)out<<" / "<<label<<" "<<(value>0?"+":"")<<std::fixed<<std::setprecision(1)<<value;};
 delta("hull",hull);delta("shield",shield);delta("battery",battery);delta("fuel",fuel);if(credits)out<<" / "<<(credits>0?"+":"")<<credits<<" cr";
 if(std::abs(hull)<.05&&std::abs(shield)<.05&&std::abs(battery)<.05&&!credits){delta("rations",rations);delta("alloy",alloy);}
 return out.str();
}
void Game::advanceTime(double hours,bool strategy,bool resolveFlight,bool interruptible) {
 if(!std::isfinite(hours)||hours<=0)return;
 ensureEconomy();registerExpeditionHabitat();hours=clamp(hours,0.,96.);
 if(strategy)hours=std::ceil(hours-1e-9);
 double start=s.hours,oldHull=s.hull,oldShield=s.flight.ship.shield,oldBattery=s.battery,oldFuel=s.fuel,oldRations=0,oldAlloy=0;
 auto credits=s.economy.credits;int first=s.turns;unsigned critical=criticalSerial;bool courier=s.flight.courier.active;
 for(const auto& site:s.economy.settlements)if(site.owner==0){oldRations+=site.stock[Rations];oldAlloy+=site.stock[Alloy];}
 std::map<std::string,std::pair<int,int>> colonyWatch;std::set<std::string> contractWatch;
 std::map<std::string,int> missionWatch;for(const auto& m:s.economy.missions)if(missionOpen(m))missionWatch[m.id]=m.state;
 struct ColonyBefore {double food,health,condition;int residents;};std::map<std::string,ColonyBefore> colonyBefore;
 for(const auto& site:s.economy.settlements)if(site.owner==0)colonyBefore[site.id]={site.stock[Rations],site.health,site.condition,site.residents};
 for(const auto& site:s.economy.settlements)if(site.owner==0)colonyWatch[site.id]={site.alerts,site.residents};
 for(const auto& contract:s.economy.contracts)if(contract.accepted&&!contract.closed)contractWatch.insert(contract.id);
 struct RouteBefore {int trips;bool held;};std::map<std::string,RouteBefore> routeWatch;
 for(const auto& r:s.economy.routes){auto* to=findSettlement(s.economy,r.to);if(to&&(r.owner==0||to->owner==0))routeWatch[r.id]={r.trips,r.outbound&&r.remaining<=1e-8&&stockTotal(r.cargo)>.001};}
 const auto researched=s.economy.researched;const bool navigating=s.flight.valid&&!s.flight.deployed&&s.flight.autopilot&&!s.flight.docked;
 auto contacts=[&](){std::set<std::string> ids;const auto& f=s.flight;if(f.valid&&!f.deployed&&!f.docked)for(size_t i=0;i<f.traffic.size();i++){const auto& t=f.traffic[i];if(!t.dead&&t.kind==1&&t.hostile&&hyperion::flight::length(t.position-f.ship.position)<=std::max(300.,hyperion::flight::enemyDef(t).range*1.5))ids.insert(t.id.empty()?"contact:"+std::to_string(i):t.id);}return ids;};
 const auto knownContacts=contacts();std::vector<std::string> milestones;
 bool interrupted=false,damaged=false;std::string stopReason;
 auto milestone=[&](const std::string& text){if(std::find(milestones.begin(),milestones.end(),text)==milestones.end())milestones.push_back(text);if(interruptible&&stopReason.empty())stopReason=text;};
 // A committed turn resolves the same simulation used by the cockpit. No
 // rendering frames enter this loop; thinking time cannot change its result.
 double remaining=hours;
 while(remaining>1e-8){
  double chunk=std::min(.25,remaining),end=s.hours+chunk;
  // Events see their actual arrival time, including arrivals just before a deadline.
  if(resolveFlight&&s.flight.valid&&!s.flight.deployed){
   double flightSeconds=chunk*55;
   while(flightSeconds>1e-8){double step=std::min(.1,flightSeconds);s.hours+=step/55;double previousHull=s.hull,previousShield=s.flight.ship.shield;updateFlight(step);if(s.hull<previousHull-1e-6||s.flight.ship.shield<previousShield-1e-6)damaged=true;syncEconomy();flightSeconds-=step;}
  }
  s.hours=end;
  s.battery=clamp(s.battery+chunk*(4+s.upgrades[3]*1.2),0.,100.);
  if(hasExpedition()&&expeditionLayer()==Mode::Ocean){
   if(s.depth>maxDepth())s.hull=std::max(0.,s.hull-(s.depth-maxDepth())/100*22*chunk);
   if(s.hull<=0){
    bool chart=s.mode==Mode::System||s.mode==Mode::Galaxy;auto view=s.mode;
    s.hull=18;s.resources[0]=std::max(0,s.resources[0]-15);s.mode=Mode::Ocean;surface(false);if(chart)s.mode=view;
    log("EMERGENCY AUTOBUOY DEPLOYED. Pressure exposure during the turn breached the shell.");notify("Emergency surface / hull breach",Notice::Critical);
   }
  }
  syncEconomy();expireCourier();remaining-=chunk;
  if(interruptible&&stopReason.empty()){
   for(const auto& [id,before]:colonyWatch){auto* site=findSettlement(s.economy,id);if(!site||site->owner!=0){stopReason=(site?site->name:id)+" / charter lost";break;}
    int raised=site->alerts&~before.first&(1|4|8|16);if(raised||site->residents<before.second){stopReason=site->name+" / "+(site->residents<before.second?"Resident lost":raised&1?"Food shortage":raised&4?"Medical emergency":raised&8?"Work stoppage":"Ecological damage");break;}}
   if(stopReason.empty())for(const auto& id:contractWatch){auto found=std::find_if(s.economy.contracts.begin(),s.economy.contracts.end(),[&](const auto& c){return c.id==id;});if(found==s.economy.contracts.end()||found->closed){stopReason="Accepted delivery contract resolved";break;}}
  }
  if(interruptible&&stopReason.empty())for(const auto& watchedMission:missionWatch){const auto& id=watchedMission.first;const int state=watchedMission.second;auto m=std::find_if(s.economy.missions.begin(),s.economy.missions.end(),[&](const auto& m){return m.id==id;});if(m==s.economy.missions.end()||m->state!=state){stopReason="Mission objective or status changed / "+(m==s.economy.missions.end()?id:m->title);break;}}
  for(const auto& r:s.economy.routes){auto watched=routeWatch.find(r.id);if(watched==routeWatch.end())continue;auto* site=findSettlement(s.economy,r.to);const auto name=site?site->name:r.to;
   if(r.trips>watched->second.trips)milestone("Freight arrived / "+name);
   else if(!watched->second.held&&r.outbound&&r.remaining<=1e-8&&stockTotal(r.cargo)>.001)milestone("Freight held / warehouse full / "+name);
  }
  for(int i=0;i<techCount;i++)if(!researched[i]&&s.economy.researched[i])milestone("Research completed / "+std::string(technologies[i].name));
  if(navigating&&s.flight.docked)milestone("Navigator reached berth");
  if(interruptible){auto now=contacts();for(const auto& id:now)if(!knownContacts.count(id)){milestone("New armed contact in range");break;}}
  if(interruptible&&remaining>1e-8&&std::abs((s.hours-start)-std::round(s.hours-start))<1e-7&&(!stopReason.empty()||damaged||criticalSerial!=critical||s.hull<oldHull-.05||s.flight.ship.shield<oldShield-.5||(courier&&!s.flight.courier.active))){interrupted=true;break;}
 }
 {
  if(strategy)s.turns=std::min(1000000000,s.turns+int(std::ceil(s.hours-start-1e-9)));
  lastTurn={};lastTurn.valid=true;lastTurn.strategic=strategy;lastTurn.first=first+1;lastTurn.last=s.turns;lastTurn.startHour=start;lastTurn.endHour=s.hours;lastTurn.requested=hours;lastTurn.hours=s.hours-start;
  lastTurn.hull=s.hull-oldHull;lastTurn.shield=s.flight.ship.shield-oldShield;lastTurn.battery=s.battery-oldBattery;lastTurn.fuel=s.fuel-oldFuel;lastTurn.credits=s.economy.credits-credits;
  for(const auto& site:s.economy.settlements)if(site.owner==0){lastTurn.rations+=site.stock[Rations];lastTurn.alloy+=site.stock[Alloy];}
  lastTurn.rations-=oldRations;lastTurn.alloy-=oldAlloy;
  for(auto it=s.economy.history.rbegin();it!=s.economy.history.rend()&&lastTurn.events.size()<6;++it)if(it->hour>start+1e-8)lastTurn.events.push_back(it->place+" / "+it->cause);
  for(const auto& site:s.economy.settlements)if(colonyBefore.count(site.id)&&lastTurn.colonies.size()<8){const auto& before=colonyBefore.at(site.id);std::ostringstream text;text<<site.name;
   auto change=[&](const char* label,double value){if(std::abs(value)>=.05)text<<" / "<<label<<" "<<(value>0?"+":"")<<std::fixed<<std::setprecision(1)<<value;};
   change("food",site.stock[Rations]-before.food);change("health",site.health-before.health);change("machinery",(site.condition-before.condition)*100);change("residents",site.residents-before.residents);
   text<<" / "<<site.bottleneck;lastTurn.colonies.push_back(text.str());
  }
  lastTurn.events.insert(lastTurn.events.begin(),milestones.begin(),milestones.end());
  if(interrupted){lastTurn.interruption=!stopReason.empty()?stopReason:damaged?"Ship took damage":criticalSerial!=critical?"Critical event":courier&&!s.flight.courier.active?"Courier commission resolved":"Ship took damage";lastTurn.events.insert(lastTurn.events.begin(),"Wait interrupted / "+lastTurn.interruption);}
 }
 recentTurns.insert(recentTurns.begin(),lastTurn);if(recentTurns.size()>12)recentTurns.resize(12);
 if(!s.economy.docked.empty()){auto* site=findSettlement(s.economy,s.economy.docked);if(!site||!canDock(*site))s.economy.docked.clear();}
}
void Game::endTurn(){
 if(!turnBased()){notify("Flight is real time / open system strategy with O for turns");return;}
 advanceTime(1,true);notify(lastTurn.summary());
}
MovePlan Game::planMove(V2 input,bool follow,double stride){
 MovePlan plan;plan.position=s.mode==Mode::Ocean?V2{s.oceanX,s.oceanAtY}:s.surface;plan.depth=s.depth;
 if(s.mode!=Mode::Surface&&s.mode!=Mode::Ocean)return plan;
 if(!std::isfinite(input.x)||!std::isfinite(input.y)||!std::isfinite(stride)||stride<=0||stride>2)return plan;
 double length=std::hypot(input.x,input.y);
 if(s.mode==Mode::Surface){
  double step=(22+s.upgrades[3]*7)*stride;
  if(follow){input={s.waypoint.x-s.surface.x,s.waypoint.y-s.surface.y};length=std::hypot(input.x,input.y);}
  if(!std::isfinite(length)||length<1e-6)return plan;
  plan.turns=int(std::min(1000000000.,std::ceil(length/step)));if(follow)step=std::min(step,length);
  plan.position={clamp(s.surface.x+input.x/length*step,-1.e7,1.e7),clamp(s.surface.y+input.y/length*step,-1.e7,1.e7)};
  plan.valid=distance(plan.position,s.surface)>1e-6;
 }else{
  double horizontal=19+s.upgrades[3]*6;
  double targetX=clamp(s.oceanTargetX,-1.e7,1.e7),targetDepth=clamp(s.targetDepth,20.,std::min(maxDepth(),universe.floorDepth(activePlanet(),targetX,s.oceanAtY)-36));
  if(follow){input={(targetX-s.oceanX)/horizontal,(targetDepth-s.depth)/185};length=std::hypot(input.x,input.y);}
  if(!std::isfinite(length)||length<1e-6)return plan;
  plan.turns=int(std::min(1000000000.,std::ceil(length/stride)));double scale=follow?std::min(stride,length):stride;
  plan.position.x=clamp(s.oceanX+input.x/length*horizontal*scale,-1.e7,1.e7);plan.depth=s.depth+input.y/length*185*scale;
  double limit=std::min(maxDepth()+110,universe.floorDepth(activePlanet(),plan.position.x,s.oceanAtY)-36);
  plan.impact=plan.depth>limit&&input.y>0;plan.depth=clamp(std::min(plan.depth,limit),20.,double(activePlanet().oceanMax));
  plan.valid=std::hypot(plan.position.x-s.oceanX,plan.depth-s.depth)>1e-6;
  auto water=oceanWater({(s.oceanX+plan.position.x)*.5,s.oceanAtY},(s.depth+plan.depth)*.5);plan.power=std::ceil(std::max(0.,-water.current*(plan.position.x>s.oceanX?1:plan.position.x<s.oceanX?-1:0))*3);plan.wear=water.vent*.45;
 }
 return plan;
}
bool Game::moveTurn(V2 input,bool follow){return commitMove(planMove(input,follow),follow);}
bool Game::commitMove(const MovePlan& plan,bool follow,double battery,double wear){
 const double oldHull=s.hull,oldBattery=s.battery;if(!plan.valid){notify("No movement available / choose a new waypoint");return false;}
 battery+=plan.power;wear+=plan.wear;
 if(battery<0||wear<0)return false;if(s.battery<battery){notify("Current needs "+std::to_string(int(std::ceil(battery)))+" power / N waits to recharge; E ascends");return false;}s.battery-=battery;s.hull=std::max(0.,s.hull-wear);
 if(s.mode==Mode::Surface){heading=std::atan2(plan.position.y-s.surface.y,plan.position.x-s.surface.x);s.surface=plan.position;if(!follow||distance(s.surface,s.waypoint)<1e-6)s.waypoint=s.surface;}
 else{
  s.oceanX=plan.position.x;s.depth=plan.depth;if(plan.impact)s.hull=std::max(0.,s.hull-.9);
  s.oceanTargetX=clamp(s.oceanTargetX,-1.e7,1.e7);s.targetDepth=clamp(s.targetDepth,20.,std::min(maxDepth(),universe.floorDepth(activePlanet(),s.oceanTargetX,s.oceanAtY)-36));
  if(!follow||std::hypot(s.oceanTargetX-s.oceanX,(s.targetDepth-s.depth)/10)<1e-6||plan.impact){s.oceanTargetX=s.oceanX;s.targetDepth=s.depth;}
 }
 if(!s.economy.docked.empty()){auto* p=findSettlement(s.economy,s.economy.docked);if(!p||!canDock(*p))s.economy.docked.clear();}
 advanceTime(1,true);lastTurn.hull=s.hull-oldHull;lastTurn.battery=s.battery-oldBattery;if(!recentTurns.empty())recentTurns.front()=lastTurn;return true;
}
}
