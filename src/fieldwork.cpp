#include "game.hpp"
#include "biosphere.hpp"
#include <sstream>
#include <iomanip>
namespace hyperion {
namespace {
// Collection rights persist in the existing survey catalogue. Older scans
// already collected their sites and cannot be collected again through fieldwork.
std::string recorded(const std::string& id){return "FIELD:record:"+id;}
std::string taken(const std::string& id){return "FIELD:take:"+id;}
bool living(const Contact& c){return c.kind=="life"||c.kind=="flora";}
double fieldDistance(const State& s,const Contact& c){return s.mode==Mode::Ocean?std::hypot((c.pos.x-s.oceanX)*5,c.depth-s.depth):distance(c.pos,s.surface);}
int research(const Contact& c){return c.kind=="relic"?22:c.kind=="signal"?19:c.kind=="life"?12:c.kind=="vent"?13:c.kind=="mineral"?7:9;}
std::string number(double x){std::ostringstream o;o<<std::fixed<<std::setprecision(1)<<x;return o.str();}
std::string gains(const Stock& stock){std::string out;for(int g=0;g<goodCount;g++)if(stock[g]>=1){if(!out.empty())out+=" / ";out+=std::to_string(int(stock[g]))+" "+goods[g].name;}return out.empty()?"record only / hold full":out;}
}
std::vector<Contact> Game::fieldSites(){
 auto sites=targets();std::stable_sort(sites.begin(),sites.end(),[&](const auto& a,const auto& b){return fieldDistance(s,a)<fieldDistance(s,b);});return sites;
}
FieldQuote Game::fieldQuote(const std::string& action,const std::string& id){
 FieldQuote q;auto reject=[&](const std::string& why){q.reason=why;return q;};
 if(s.mode!=Mode::Surface&&s.mode!=Mode::Ocean)return reject("Deploy a surface or ocean expedition first");
 if(action=="fieldRecharge"){
  if(s.battery>=12)return reject("Scanner is ready / choose an order");if(s.mode==Mode::Ocean&&s.depth>maxDepth())return reject("Ascend to certified depth before recharging");
  for(const auto& event:upcoming(1))if(event.priority!=Notice::Status)return reject("Deadline imminent / resolve it or choose a manual hour first");
  q.move.turns=int(std::ceil((12-s.battery)/(4+s.upgrades[3]*1.2)));for(const auto& event:upcoming(q.move.turns))if(event.priority!=Notice::Status)q.move.turns=std::min(q.move.turns,int(std::floor(event.hours+1e-8)));q.text="Recharge toward 12 battery / up to "+std::to_string(q.move.turns)+" h / stops for events and deadlines";
 }else if(action=="fieldApproach"){
  auto sites=fieldSites();auto c=std::find_if(sites.begin(),sites.end(),[&](const auto& c){return c.id==id;});if(c==sites.end())return reject("Contact is outside passive sensor range");
  const double reach=s.mode==Mode::Ocean?260+s.upgrades[1]*190:34+s.upgrades[1]*14;if(fieldDistance(s,*c)<=reach)return reject("Already in study range / choose Study or Collect");
  if(s.mode==Mode::Ocean&&(s.depth>maxDepth()||c->depth>maxDepth()))return reject("Contact lies beyond certified depth / improve the pressure shell");
  for(const auto& event:upcoming(1))if(event.priority!=Notice::Status)return reject("Deadline imminent / take one manual step or resolve it first");
  q.text="Approach at normal pace / up to 6 h / stop in study range / stops for risks";
 }else if(action=="fieldPush"||action=="fieldTravel"){
  q.move=planMove({},true,action=="fieldPush"?1.8:1.);if(!q.move.valid)return reject("Plot a waypoint first");
  if(s.mode==Mode::Ocean&&(s.depth>maxDepth()||q.move.depth>maxDepth()||q.move.impact))return reject("Choose a route within certified depth and clear of the floor");
  if(action=="fieldPush"){
   q.battery=8;auto p=activePlanet();Settlement probe;probe.position=q.move.position;probe.depth=s.mode==Mode::Ocean?q.move.depth:0;
   auto weather=conditions(universe,p,probe,s.hours);double rough=s.mode==Mode::Ocean?q.move.depth/maxDepth():std::min(1.,std::abs(universe.elevation(p,q.move.position.x,q.move.position.y)-universe.elevation(p,s.surface.x,s.surface.y))*20);
   q.wear=.3+rough*1.2+weather.storm;q.text="1.8x stride / 8 battery / "+number(q.wear)+" hull wear / 1 h";
  }else{for(const auto& event:upcoming(1))if(event.priority!=Notice::Status)return reject("Deadline imminent / take one manual step or resolve it first");q.text="Follow up to 6 turns / stops for discoveries, danger and deadlines";}
 }else if(action=="fieldRepair"){
  if(s.mode==Mode::Ocean&&s.depth>maxDepth())return reject("Ascend to certified depth before field repairs");
  if(s.hull>=99.95)return reject("Hull is already sound");auto stock=shipStock();if(stock[Alloy]<2||stock[Components]<1)return reject("Patch needs 2 alloy and 1 component");
  q.battery=8;q.repair=std::min(8.,100-s.hull);q.text="+"+number(q.repair)+" hull / 2 alloy / 1 component / 8 battery / 1 h";
 }else if(action=="fieldStudy"||action=="fieldCollect"){
  auto sites=fieldSites();auto it=std::find_if(sites.begin(),sites.end(),[&](const auto& c){return c.id==id;});if(it==sites.end())return reject("Contact is outside passive sensor range");const auto& c=*it;
  if(s.mode==Mode::Ocean&&s.depth>maxDepth())return reject("Ascend to certified depth before studying or sampling");
  if(s.mode==Mode::Ocean&&!oceanKnown(c))return reject("Resolve the contact with sonar first / Space or Shift+Space");
  const double reach=s.mode==Mode::Ocean?260+s.upgrades[1]*190:34+s.upgrades[1]*14;if(fieldDistance(s,c)>reach)return reject("Move closer / inside survey range");
  double room=std::floor(std::max(0.,cargoCapacity()-stockTotal(shipStock())));
  if(action=="fieldStudy"){
   if(s.scanned.count(id))return reject("Already catalogued / choose another discovery");q.battery=6;int data=research(c)+int(universe.hash(int32_t(c.n?c.n:1),int32_t(c.pos.x),4)%5)+6+s.upgrades[1]*2;
   q.gain[Research]=std::min(room,double(data));q.text=gains(q.gain)+" / 6 battery / 1 h / no specimens removed";
  }else{
   if(!s.scanned.count(recorded(id)))return reject(s.scanned.count(id)?"Standard scan already collected this site":"Study first to assess the site");
   if(s.scanned.count(taken(id)))return reject("This site's collection allowance is exhausted");if(room<1)return reject("Make room in the hold first");q.battery=10;
   if(living(c)){auto p=activePlanet();auto* h=contactHabitat(s.economy,universe,p,c);if(!h)return reject("Study this habitat first");auto copy=*h;q.gain[Biomass]=samplePopulation(copy,c,std::min(room,double(p.generation>=2?phenotype(c).samples:10)));}
   else{Stock stock{};auto region=universe.region(activePlanet(),c.pos,c.depth);
    if(c.kind=="mineral"){stock[Ore]=activePlanet().generation>=2?int(std::round(7+region.ore*6)):14;stock[Crystal]=activePlanet().generation>=2?int(std::round(3+region.crystal*3)):4+int(universe.hash(int32_t(c.n?c.n:1),7)%5);}
    else if(c.kind=="vent"){stock[Ore]=12;stock[Crystal]=8;}else if(c.kind=="relic")stock[Crystal]=14;else return reject("This signature has no extractable materials");
    for(int g:{Crystal,Ore}){q.gain[g]=std::min(room,stock[g]);room-=q.gain[g];}
   }
   if(stockTotal(q.gain)<1)return reject("Population below sampling reserve / leave it to recover");
   q.text=gains(q.gain)+" / 10 battery / 1 h"+(living(c)?" / removes real biomass":" / finite site cache");
  }
 }else return reject("Unknown field order");
 if(q.move.valid){q.battery+=int(q.move.power);q.wear+=q.move.wear;}
 if(s.battery<q.battery)return reject("Need "+std::to_string(q.battery)+" battery / wait or move normally to recharge");q.available=true;return q;
}
bool Game::fieldAction(const std::string& action,const std::string& id){
 if(action!="fieldRecharge"&&action!="fieldApproach"&&action!="fieldStudy"&&action!="fieldCollect"&&action!="fieldRepair"&&action!="fieldPush"&&action!="fieldTravel"&&action!="fieldFocus")return false;
 if(action=="fieldFocus"){for(const auto& c:fieldSites())if(c.id==id){waypoint(c);return true;}notify("Contact is no longer within passive sensor range");return true;}
 auto q=fieldQuote(action,id);if(!q.available){notify(q.reason);return true;}if(s.mode==Mode::Ocean&&!diveState.active)beginDive(false);
 if(action=="fieldRecharge"){waitHours(q.move.turns);notify(!lastTurn.interruption.empty()?"Recharge stopped / "+lastTurn.interruption:s.battery>=12?"Scanner ready / "+lastTurn.summary():"Recharge paused before deadline / "+lastTurn.summary(),Notice::Important);return true;}
 if(action=="fieldApproach"){for(const auto& c:fieldSites())if(c.id==id){waypoint(c);followFieldRoute(id);return true;}return true;}
 if(action=="fieldTravel"){followFieldRoute();return true;}
 if(action=="fieldPush"){if(commitMove(q.move,true,q.battery-q.move.power,q.wear-q.move.wear)){lastTurn.events.insert(lastTurn.events.begin(),"Pushed onward / "+q.text);recentTurns.front()=lastTurn;notify("Pushed onward / "+q.text);}return true;}
 const double battery=s.battery,hull=s.hull;std::string result;s.battery-=q.battery;
 if(action=="fieldRepair"){auto cargo=shipStock();cargo[Alloy]-=2;cargo[Components]-=1;storeShipStock(cargo);s.hull+=q.repair;result="Hull patched / "+q.text;}
 else{
  auto sites=fieldSites();const auto found=std::find_if(sites.begin(),sites.end(),[&](const auto& c){return c.id==id;});if(found==sites.end()){s.battery=battery;notify("Contact is no longer within passive sensor range");return true;}const Contact c=*found;focusedContact=c.id;
  if(action=="fieldStudy"){catalogue(c,false,6+s.upgrades[1]*2);s.scanned.insert(recorded(id));result="Studied "+c.name+" / "+gains(q.gain)+" / ecosystem left intact";}
  else{auto cargo=shipStock();if(living(c)){auto& h=ensureContactHabitat(s.economy,universe,activePlanet(),c);q.gain[Biomass]=samplePopulation(h,c,q.gain[Biomass]);}for(int g=0;g<goodCount;g++)cargo[g]+=q.gain[g];storeShipStock(cargo);s.scanned.insert(taken(id));result="Collected at "+c.name+" / "+gains(q.gain);}
  if(action=="fieldStudy")if(auto* wreck=findOceanWreck(s.economy,c.id))wreck->surveyed=true;
  if(s.mode==Mode::Ocean&&diveState.active){if(action=="fieldStudy")++diveState.studies;else ++diveState.collections;}
 }
 advanceTime(1,true);lastTurn.battery=s.battery-battery;lastTurn.hull=s.hull-hull;lastTurn.events.insert(lastTurn.events.begin(),result);recentTurns.front()=lastTurn;notify(result,Notice::Important);return true;
}
void Game::followFieldRoute(const std::string& approach){
 const double start=s.hours,battery=s.battery,hull=s.hull,shield=s.flight.ship.shield,fuel=s.fuel;const auto credits=s.economy.credits;const int first=s.turns+1,requested=std::min(6,planMove().turns);
 std::set<std::string> known;for(const auto& c:fieldSites())if(c.kind=="signal"||c.kind=="relic")known.insert(c.id);
 std::map<std::string,int> missions;for(const auto& m:s.economy.missions)if(missionOpen(m))missions[m.id]=m.state;
 std::map<std::string,std::pair<int,int>> colonies;for(const auto& site:s.economy.settlements)if(site.owner==0)colonies[site.id]={site.alerts,site.residents};
 struct Before {double food,health,condition;int residents;};std::map<std::string,Before> before;for(const auto& site:s.economy.settlements)if(site.owner==0)before[site.id]={site.stock[Rations],site.health,site.condition,site.residents};
 bool inRange=false;auto initial=outlook().warnings;unsigned critical=criticalSerial;std::string stopped;int steps=0;
 for(;steps<requested;){
  auto q=fieldQuote("fieldTravel");if(!q.available){stopped=q.reason;break;}const auto mode=s.mode;double oldHull=s.hull;
  if(!commitMove(q.move,true))break;++steps;
  if(s.mode!=mode||s.hull<oldHull-.05||criticalSerial!=critical){stopped="Expedition damage or emergency";break;}
  for(const auto& site:s.economy.settlements){auto old=colonies.find(site.id);if(old!=colonies.end()&&(site.owner!=0||(site.alerts&~old->second.first&(1|4|8|16))||site.residents<old->second.second)){stopped="Outpost needs attention / "+site.name;break;}}
  if(outlook().warnings>initial){stopped="New settlement or mission risk";break;}
  for(const auto& m:s.economy.missions){auto old=missions.find(m.id);if(old!=missions.end()&&old->second!=m.state){stopped="Mission status changed / "+m.title;break;}}
  for(const auto& event:lastTurn.events)if(event.rfind("Freight ",0)==0||event.rfind("Research completed",0)==0)stopped=event;
  for(const auto& c:fieldSites())if((c.kind=="signal"||c.kind=="relic")&&!s.scanned.count(c.id)&&!known.count(c.id)){focusedContact=c.id;stopped="Uncharted "+c.kind+" / "+c.name;break;}
  if(!approach.empty()){bool found=false;for(const auto& c:fieldSites())if(c.id==approach){found=true;double reach=s.mode==Mode::Ocean?260+s.upgrades[1]*190:34+s.upgrades[1]*14;if(fieldDistance(s,c)<=reach){inRange=true;focusedContact=approach;}break;}if(!found)stopped="Contact moved beyond passive sensors / replot the approach";}
  if(!stopped.empty()||inRange||!planMove().valid)break;
 }
 if(!steps){notify(stopped.empty()?"No route available":stopped);return;}
 TurnReport combined=lastTurn;combined.startHour=start;combined.endHour=s.hours;combined.hours=s.hours-start;combined.requested=inRange&&stopped.empty()?steps:requested;combined.first=first;combined.battery=s.battery-battery;combined.hull=s.hull-hull;combined.shield=s.flight.ship.shield-shield;combined.fuel=s.fuel-fuel;combined.credits=s.economy.credits-credits;combined.rations=combined.alloy=0;
 for(const auto& report:recentTurns)if(report.endHour>start+1e-8){combined.rations+=report.rations;combined.alloy+=report.alloy;for(const auto& event:report.events)if(std::find(combined.events.begin(),combined.events.end(),event)==combined.events.end())combined.events.push_back(event);}
 combined.colonies.clear();for(const auto& site:s.economy.settlements)if(before.count(site.id)&&combined.colonies.size()<8){const auto& old=before.at(site.id);std::ostringstream text;text<<site.name;auto delta=[&](const char* label,double value){if(std::abs(value)>=.05)text<<" / "<<label<<" "<<(value>0?"+":"")<<std::fixed<<std::setprecision(1)<<value;};delta("food",site.stock[Rations]-old.food);delta("health",site.health-old.health);delta("machinery",(site.condition-old.condition)*100);delta("residents",site.residents-old.residents);text<<" / "<<site.bottleneck;combined.colonies.push_back(text.str());}
 if(!stopped.empty()&&steps<requested)combined.interruption=stopped;
 const std::string result=inRange&&stopped.empty()?"Study range reached / choose Study or Collect":stopped.empty()?(planMove().valid?"Route paused / six-turn limit":"Waypoint reached"):"Route paused / "+stopped;
 combined.events.insert(combined.events.begin(),result);recentTurns.erase(std::remove_if(recentTurns.begin(),recentTurns.end(),[&](const auto& r){return r.endHour>start+1e-8;}),recentTurns.end());recentTurns.insert(recentTurns.begin(),combined);if(recentTurns.size()>12)recentTurns.resize(12);lastTurn=combined;notify(result+" / "+std::to_string(steps)+" turn(s)",Notice::Important);
}
}
