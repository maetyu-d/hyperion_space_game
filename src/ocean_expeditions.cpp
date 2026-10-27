#include "game.hpp"
#include <sstream>
namespace hyperion {
OceanWreck* findOceanWreck(Economy& e,const std::string& id){for(auto& w:e.oceanWrecks)if(w.contact.id==id)return &w;return nullptr;}
const OceanWreck* findOceanWreck(const Economy& e,const std::string& id){for(const auto& w:e.oceanWrecks)if(w.contact.id==id)return &w;return nullptr;}
void advanceOceanWrecks(Economy& e){for(auto& w:e.oceanWrecks)if(w.crew&&e.hour>w.supportUntil){w.lost+=w.crew;w.crew=0;economicEvent(e,w.contact.name,"Life support exhausted / stranded crew lost");}}
int Game::rescuedCrew()const{int n=0;for(const auto& w:s.economy.oceanWrecks)n+=w.aboard;for(const auto& [id,d]:s.economy.worldDamage)n+=d.recovery.aboard;return n;}
std::vector<Contact> Game::wreckSites()const{
 std::vector<Contact> out;if(!hasExpedition())return out;
 for(const auto& w:s.economy.oceanWrecks)if(objectIntegrity(s.economy.worldDamage,"field:"+w.contact.id)>0&&w.star==s.activeStar&&w.planet==s.activePlanet&&std::abs(w.contact.pos.y-s.oceanAtY)<=131&&std::abs(w.contact.pos.x-s.oceanX)<650)out.push_back(w.contact);
 std::stable_sort(out.begin(),out.end(),[&](const auto& a,const auto& b){return std::abs(a.pos.x-s.oceanX)<std::abs(b.pos.x-s.oceanX);});return out;
}
void Game::discoverOceanWreck(){
 if(s.mode!=Mode::Ocean||s.economy.oceanWrecks.size()>=4096)return;auto p=activePlanet();if(p.water<.2)return;
 int cx=int(std::floor(s.oceanX/390)),cy=int(std::floor(s.oceanAtY/260));std::string id="WRECK:"+p.id+":"+std::to_string(cx)+":"+std::to_string(cy);if(findOceanWreck(s.economy,id))return;
 uint32_t seed=universe.hash(int32_t(p.seed),cx,cy,355);double x=cx*390.+80.+double(seed%230),y=cy*260.+130;
 double floor=universe.floorDepth(p,x,y);if(floor<180)return;
 Settlement* home=nullptr;double best=1e30;for(auto& site:s.economy.settlements)if(site.star==s.activeStar&&site.owner>0&&site.residents>0){double rank=(site.planet==s.activePlanet?0:100000)+(site.depth==0?0:1000)+distance(site.position,{x,y});if(rank<best){best=rank;home=&site;}}
 if(!home)return;OceanWreck w;w.star=s.activeStar;w.planet=s.activePlanet;w.faction=home->owner;w.home=home->id;w.created=s.hours;w.supportUntil=s.hours+72+seed%24;
 w.contact.id=id;w.contact.kind="wreck";w.contact.n=seed;w.contact.pos={x,y};w.contact.depth=std::min(floor-48,220.+(seed>>8)%430);
 static const char* names[]={"Opaline","Violet","Nacre","Prismatic","Saffron","Lattice"};w.contact.name=std::string(names[seed%6])+" survey tender "+std::to_string(10+seed%90);
 w.contact.desc=home->name+" lost this survey tender in "+universe.region(p,w.contact.pos,w.contact.depth).name+". Its emergency beacon still marks the supply route. Cargo belongs to "+s.economy.factions[w.faction].name+"; rescued crew and recovered goods can be returned to "+home->name+".";
 // Materialize the missing shipment from real reserves exactly once per sector.
 for(int g:{Components,Medicine,Alloy,Rations,Propellant}){double amount=std::min(std::floor(std::max(0.,home->stock[g]-reserveFor(*home,g)*.35)),double(g==Components?12:g==Medicine?4:8+int((seed>>(g+2))%7)));w.initial[g]=w.remaining[g]=amount;home->stock[g]-=amount;}
 w.crew=std::min(2+int(seed%3),s.economy.factions[w.faction].reservePopulation);w.initialCrew=w.crew;s.economy.factions[w.faction].reservePopulation-=w.crew;
 s.economy.oceanWrecks.push_back(w);economicEvent(s.economy,home->name,"Overdue survey tender / "+w.contact.name+" / supply cache and missing crew located");
 if(s.economy.missions.size()<256&&home->treasury>=240){Mission m;m.id="DIVEJOB:"+id;m.kind=WreckMission;m.issuer=w.home;m.star=w.star;m.planet=w.planet;m.faction=w.faction;m.seed=seed;m.target=id;m.contact=w.contact;m.created=s.hours;m.deadline=w.supportUntil;m.trip=w.crew?1:0;m.good=Components;m.quantity=w.crew?w.crew:int(std::min(6.,w.remaining[Components]));m.risk=clamp(w.contact.depth/maxDepth()*.7,0.,1.);m.reward=std::min(int64_t(240+w.crew*35),home->treasury/2);m.title=std::string(w.crew?"Rescue / ":"Supply recovery / ")+w.contact.name;m.cause=w.contact.desc;if(m.quantity>0)s.economy.missions.push_back(m);}
}
int Game::wreckCargoGood(const OceanWreck& w)const{if(w.crew>0&&shipStock()[Medicine]<1&&w.remaining[Medicine]>=1)return Medicine;if(wreckGood>=0&&wreckGood<goodCount&&w.remaining[wreckGood]>=1)return wreckGood;for(int g:{Components,Medicine,Alloy,Rations,Propellant,Crystal,Ore,Biomass,Research})if(w.remaining[g]>=1)return g;return Components;}
FieldQuote Game::wreckQuote(const std::string& action,const std::string& id){
 FieldQuote q;auto reject=[&](const std::string& reason){q.reason=reason;return q;};auto* w=findOceanWreck(s.economy,id);if(!w)return reject("Wreck record unavailable");
 if(action=="wreckDeliver"){
  auto* home=findSettlement(s.economy,w->home);if(!home||home->owner!=w->faction||home->residents<=0)return reject("The receiving authority is unavailable");
  if(s.economy.docked!=home->id||!canDock(*home))return reject("Dock at "+home->name+" to unload rescued crew and supplies");
  double room=std::floor(std::max(0.,storageCapacity(*home)-stockTotal(home->stock)));auto cargo=shipStock();for(int g=0;g<goodCount;g++){q.gain[g]=std::min({room,w->recovered[g],std::floor(cargo[g])});room-=q.gain[g];}
  q.move.turns=std::min(w->aboard,10000-home->residents);if(q.move.turns<=0&&stockTotal(q.gain)<1)return reject("No deliverable supplies or crew / check warehouse space");
  q.available=true;q.text="Unload "+std::to_string(q.move.turns)+" crew / "+std::to_string(int(stockTotal(q.gain)))+" supplies / 1 h";return q;
 }
 if(s.mode!=Mode::Ocean||w->star!=s.activeStar||w->planet!=s.activePlanet||std::abs(w->contact.pos.y-s.oceanAtY)>131)return reject("Deploy in this wreck's ocean sector");
 if(s.depth>maxDepth())return reject("Ascend to certified depth before wreck work");
 if(!w->surveyed)return reject("Study the wreck before entering or salvaging");
 if(std::hypot((s.oceanX-w->contact.pos.x)*5,s.depth-w->contact.depth)>80)return reject("Approach within 80 survey units of the hull");
 if(action=="wreckRescue"){
  if(w->crew<=0)return reject("No crew remain inside");if(s.hours+1>w->supportUntil)return reject("Life support cannot last another hour");q.move.turns=std::min({2,w->crew,6-rescuedCrew()});if(q.move.turns<=0)return reject("Six rescue seats occupied / deliver passengers first");
  if(shipStock()[Medicine]<1)return reject("Rescue needs 1 medicine for emergency care");q.battery=10;q.text="Rescue "+std::to_string(q.move.turns)+" crew / 1 medicine / 10 power / 1 h";
 }else if(action=="wreckSalvage"||action=="wreckCut"){
  bool cut=action=="wreckCut";if(cut&&w->crew>0)return reject("Rescue the crew before breaching the hull");int good=wreckCargoGood(*w);double room=std::floor(std::max(0.,cargoCapacity()-stockTotal(shipStock())));q.gain[good]=std::min({cut?16.:6.,w->remaining[good],room});
  if(q.gain[good]<1)return reject(stockTotal(w->remaining)<1?"Wreck cargo exhausted":"No room for more supplies");q.battery=cut?18:8;q.wear=cut?1.2:0;
  q.text=std::string(cut?"Cut ":"Recover ")+std::to_string(int(q.gain[good]))+" "+goods[good].name+" / "+std::to_string(q.battery)+" power / 1 h"+(cut?" / 1.2 hull / habitat disturbance / fauna alerted":" / quiet handling");
 }else return reject("Unknown wreck operation");
 if(s.battery<q.battery)return reject("Need "+std::to_string(q.battery)+" power / wait or move to recharge");q.available=true;return q;
}
bool Game::wreckAction(const std::string& action,const std::string& id){
 if(action=="wreckCargo"){try{size_t used=0;int g=std::stoi(id,&used);if(used==id.size()&&g>=0&&g<goodCount)wreckGood=g;}catch(...){}return true;}
 if(action=="oceanRoute"){
  if(s.mode!=Mode::Ocean)return true;auto route=oceanRoute(id=="shelter");s.oceanTargetX=route.waypoint.x;s.targetDepth=route.waypoint.y;diveState.returning=false;notify(route.detour?"Sheltered waypoint plotted / Enter moves / no time spent":"Direct course plotted / no time spent");return true;
 }
 if(action=="plotWreckHome"){
  auto* w=findOceanWreck(s.economy,id);auto* home=w?findSettlement(s.economy,w->home):nullptr;
  if(home){
   if(s.mode==Mode::Surface&&s.activeStar==home->star&&s.activePlanet==home->planet){s.waypoint=home->position;focusedContact.clear();notify("Receiving settlement waypoint / "+home->name+" / dock to unload crew and supplies");}
   else{system(home->star);s.selectedPlanet=home->planet;if(home->star==s.currentStar){ensureFlight();int port=flightPort(home->id,true);if(port>=0)s.flight.selectTarget(port);}notify("Receiving settlement plotted / "+home->name+" / recover the expedition, then rendezvous and unload");}
  }
  return true;
 }
 if(action!="wreckRescue"&&action!="wreckSalvage"&&action!="wreckCut"&&action!="wreckDeliver")return false;
 auto q=wreckQuote(action,id);if(!q.available){notify(q.reason);return true;}auto* w=findOceanWreck(s.economy,id);double oldBattery=s.battery,oldHull=s.hull;auto cargo=shipStock();std::string result=q.text;s.battery-=q.battery;s.hull=std::max(0.,s.hull-q.wear);
 if(action=="wreckRescue"){w->crew-=q.move.turns;w->aboard+=q.move.turns;cargo[Medicine]-=1;result+=" / deliver to "+findSettlement(s.economy,w->home)->name;}
 else if(action=="wreckDeliver"){
  auto* home=findSettlement(s.economy,w->home);w->aboard-=q.move.turns;w->deliveredCrew+=q.move.turns;home->residents+=q.move.turns;
  for(int g=0;g<goodCount;g++){cargo[g]-=q.gain[g];home->stock[g]+=q.gain[g];w->recovered[g]-=q.gain[g];w->delivered[g]+=q.gain[g];}
  auto& f=s.economy.factions[w->faction];f.standing=std::min(100.,f.standing+q.move.turns*.5+stockTotal(q.gain)*.03);economicEvent(s.economy,home->name,result);
 }else{
  for(int g=0;g<goodCount;g++){cargo[g]+=q.gain[g];w->remaining[g]-=q.gain[g];w->recovered[g]+=q.gain[g];}
  if(action=="wreckCut"){
   w->breached=true;diveState.lastPing=s.hours+1;auto& h=ensureEcosystem(s.economy,universe,activePlanet(),w->contact.pos,w->contact.depth);h.quality=std::max(0.,h.quality-.045);h.pollution=std::min(1.,h.pollution+.06);
   // Physical disturbance moves living carbon into detritus, preserving its budget.
   for(auto& pop:h.populations){double removed=pop.mass*.025;pop.mass-=removed;h.detritus+=removed;}
   economicEvent(s.economy,w->contact.name,"Cutting disturbed the habitat / food-web support and wild harvest impaired until recovery");
  }
 }
 storeShipStock(cargo);advanceTime(1,true);lastTurn.battery=s.battery-oldBattery;lastTurn.hull=s.hull-oldHull;lastTurn.events.insert(lastTurn.events.begin(),result);recentTurns.front()=lastTurn;notify(result,Notice::Important);return true;
}
OceanWater Game::oceanWater(V2 at,double depth)const{
 OceanWater water;if(depth<20)return water;const auto& p=universe;
 auto planet=const_cast<Universe&>(universe).planet(s.activeStar,s.activePlanet);water.biome=p.region(planet,at,depth).name;double floor=p.floorDepth(planet,at.x,at.y);
 water.shelter=clamp((135-(floor-depth))/100.,0.,.85);water.current=clamp((p.noise(at.x*.008,depth*.003+at.y*.0008,int32_t(planet.seed^355u))-.5)*2.2*(1-water.shelter),-1.,1.);
 for(int x=int(std::floor((at.x-20)/130));x<=int(std::floor((at.x+20)/130));x++)for(const auto& c:p.oceanObjects(planet,x,at.y))if(c.kind=="vent")water.vent=std::max(water.vent,clamp(1-std::hypot((c.pos.x-at.x)*5,c.depth-depth)/90.,0.,1.));
 return water;
}
OceanRoute Game::oceanRoute(bool shelter)const{
 OceanRoute best;best.waypoint={diveState.goal.pos.x,diveState.goal.depth};if(s.mode!=Mode::Ocean||diveState.goal.id.empty())return best;
 auto evaluate=[&](V2 waypoint){OceanRoute route;route.waypoint=waypoint;V2 start{s.oceanX,s.depth};auto leg=[&](V2 end){int n=int(std::min(512.,std::ceil(std::hypot((end.x-start.x)/(19+s.upgrades[3]*6),(end.y-start.y)/185.))));for(int i=0;i<n;i++){double f=(i+.5)/std::max(1,n);auto water=oceanWater({start.x+(end.x-start.x)*f,s.oceanAtY},start.y+(end.y-start.y)*f);route.power+=std::ceil(std::max(0.,-water.current*(end.x>start.x?1:end.x<start.x?-1:0))*3);route.wear+=water.vent*.45;}route.turns+=n;start=end;};leg(waypoint);if(distance(waypoint,{diveState.goal.pos.x,diveState.goal.depth})>1)leg({diveState.goal.pos.x,diveState.goal.depth});return route;};
 best=evaluate(best.waypoint);if(!shelter)return best;auto score=[](const OceanRoute& r){return r.turns*.3+r.power*.12+r.wear*12;};
 for(double offset:{-240.,-140.,100.}){double x=(s.oceanX+diveState.goal.pos.x)*.5;auto planet=const_cast<Universe&>(universe).planet(s.activeStar,s.activePlanet);double d=clamp((s.depth+diveState.goal.depth)*.5+offset,35.,std::min(maxDepth(),universe.floorDepth(planet,x,s.oceanAtY)-40));auto candidate=evaluate({x,d});if(score(candidate)+.05<score(best)){candidate.detour=true;best=candidate;}}
 return best;
}
}
