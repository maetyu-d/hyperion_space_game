#include "game.hpp"
#include "flight/destruction.hpp"

namespace hyperion {
namespace {
void returnParty(Economy& e,Boarding& b){int home=std::min(b.party,120-e.colonists);e.colonists+=home;e.factions[0].reservePopulation+=b.party-home;b.partyReturned+=b.party;b.party=0;}
void returnDefenders(Economy& e,Boarding& b){auto* home=findSettlement(e,b.home);if(!b.reserve&&home&&home->owner==b.faction)home->residents+=b.defenders;else e.factions[b.faction].reservePopulation+=b.defenders;b.defendersReturned+=b.defenders;b.defenders=0;e.geometryRevision++;}
int danger(const Boarding& b,int room){return b.surrendered||(b.rooms&1)?0:(room==2?1+int(b.seed%3==0):room==4?1:0);}
}
bool Game::boardingActive()const{for(const auto& [id,b]:s.economy.boardings)if(b.status==0)return true;return false;}
Game::RecoveryQuote Game::boardingQuote(const std::string& action,const std::string& id)const{
 RecoveryQuote q;const auto& e=s.economy;const auto& f=s.flight;auto found=e.boardings.find(id);const Boarding* b=found==e.boardings.end()?nullptr:&found->second;
 if(action=="boardingPlan"){q.available=true;q.text="Free boarding planning / world paused";return q;}
 if(!turnBased()){q.reason="Open strategy to plan boarding";return q;}
 if(action=="boardingRelease"){auto* site=findSettlement(e,e.docked);if(!b||b->status!=2||b->defenders<=0||!site||site->owner<=0||!canDock(*site)){q.reason="Dock at an inhabited faction port with captives aboard";return q;}q.available=true;q.text="Disembark captives / 1 h / people transferred to the receiving authority";return q;}
 auto t=std::find_if(f.traffic.begin(),f.traffic.end(),[&](const auto& t){return t.id==id&&!t.dead&&t.kind==1;});
 if(t==f.traffic.end()||!f.valid||f.deployed||f.docked||flight::length(t->position-f.ship.position)>55||f.speed()>5||f.autopilot){q.reason="Approach a disabled ship within 55 km, below 5 km/s, undocked; disengage navigation";return q;}
 if(action=="boardingBegin"){
  if(t->faction<=0){q.reason="No independent crew can be boarded here";return q;}
  if(t->shield>std::max(5.,flight::enemyDef(*t).shield*.15)){q.reason="Suppress the target shields before attaching a boarding party";return q;}if(t->modules[0]>2){q.reason="Disable the target drive with focused fire first";return q;}if(boardingActive()){q.reason="Finish or withdraw the current boarding party first";return q;}if(b&&s.hours<b->updated+6){q.reason="The target is on alert / wait six hours before another boarding attempt";return q;}if(e.colonists<3){q.reason="Boarding needs three available volunteers";return q;}if(e.boardings.size()>=512&&!b){q.reason="Boarding operation limit reached";return q;}q.available=true;q.text="Board disabled hull / 3 volunteers assigned / 1 h / neutral boarding incurs a warrant";return q;
 }
 if(!b||b->status!=0||b->star!=s.currentStar){q.reason="Deploy a boarding party first";return q;}
 if(action=="boardingWithdraw"){q.available=true;q.text="Withdraw party / 1 h / surviving people return to their original populations";return q;}
 if(action=="boardingNegotiate"){if(b->surrendered){q.reason="The defenders have surrendered";return q;}if(!(b->rooms&1)&&t->health>=flight::enemyDef(*t).hull*.35&&t->modules[1]>10){q.reason="Isolate engineering, disable weapons or reduce the hull below 35% before demanding surrender";return q;}q.available=true;q.text="Negotiate surrender / 1 h / engineering, weapon or hull pressure provides leverage";return q;}
 int room=action=="boardingPower"?1:action=="boardingBridge"?2:action=="boardingCargo"?4:0;auto cargo=shipStock();
 if(room){if(b->rooms&room){q.reason="Compartment already secured";return q;}int parts=room==1?2:1;if(cargo[Components]<parts||(room==1&&cargo[Crystal]<1)){q.reason=room==1?"Power isolation needs 2 components and 1 crystal":"Compartment access needs 1 component";return q;}if(b->party<=0){q.reason="No boarding volunteers remain";return q;}int risk=danger(*b,room);q.available=true;q.text=std::string(room==1?"Isolate engineering / 2 parts + 1 crystal":room==2?"Secure bridge / 1 component":"Secure cargo bay / 1 component")+" / 1 h"+(risk?" / "+std::to_string(risk)+" medicine prevents "+std::to_string(risk)+" volunteer casualties":" / controlled access");return q;}
 if(action=="boardingLoot"){if(!(b->rooms&4)||stockTotal(b->cargo)<=0||stockTotal(cargo)>=cargoCapacity()){q.reason="Secure the cargo bay and make room in the hold";return q;}q.available=true;q.text="Transfer finite ship supplies / 1 h / seizure affects the original faction";return q;}
 if(action=="boardingCapture"){if((b->rooms&3)!=3&&!b->surrendered){q.reason="Secure engineering and the bridge, or negotiate surrender";return q;}if(e.worldDamage.size()>=60000){q.reason="Clear existing wrecks before capturing another hull";return q;}if(e.prizes.size()>=64){q.reason="Sell or commission stored prizes before capturing another hull";return q;}if(rescuedCrew()+b->defenders>12){q.reason="Deliver passengers first / captives share the twelve rescue berths";return q;}q.available=true;q.text="Capture hull / 1 h / crew returns; captives and disputed title persist / tow to a shipyard";return q;}
 q.reason="Unknown boarding order";return q;
}
bool Game::boardingAction(const std::string& action,const std::string& arg){
 if(action.rfind("boarding",0)!=0)return false;std::string id=arg.empty()?s.flight.combatTarget:arg;
 if(action=="boardingPlan"){openStrategy();s.flight.combatTarget=id;notify("Boarding planning / choose an order in Recovery / world paused");return true;}
 auto quote=boardingQuote(action,id);if(!quote.available){notify(quote.reason);return true;}auto& e=s.economy;auto& f=s.flight;auto cargo=shipStock();
 if(action=="boardingRelease"){auto& b=e.boardings.at(id);auto* site=findSettlement(e,e.docked);if(site->owner==b.faction)site->residents+=b.defenders;else e.factions[site->owner].reservePopulation+=b.defenders;b.defendersReturned+=b.defenders;b.defenders=0;if(auto* captain=findCaptain(e,id)){captain->captured=false;captain->readyAt=e.hour+24;}e.geometryRevision++;economicEvent(e,site->name,"Captured crew disembarked alive / no population duplicated.");}
 else{
  auto ti=std::find_if(f.traffic.begin(),f.traffic.end(),[&](const auto& t){return t.id==id;});auto& t=*ti;
  if(action=="boardingBegin"){
   Boarding b;b.id=id;b.star=s.currentStar;b.home=t.home>=0&&t.home<int(f.ports.size())?f.ports[t.home].settlement:"";if(auto* c=findCaptain(e,id))b.home=c->home;b.band=t.band;b.faction=t.faction;b.started=b.updated=s.hours;b.seed=t.genome.seed?t.genome.seed:stringHash(id);b.initialParty=b.party=3;e.colonists-=3;
   auto* home=findSettlement(e,b.home);int people=2+t.role+int(b.seed%2);if(home&&home->owner==b.faction){people=std::min(people,std::max(0,home->residents-2));home->residents-=people;}else{b.reserve=true;people=std::min(people,e.factions[b.faction].reservePopulation);e.factions[b.faction].reservePopulation-=people;}b.initialDefenders=b.defenders=people;
   Stock* stores=home?&home->stock:nullptr;if(auto* band=findPirates(e,b.band))stores=&band->stock;if(stores)for(int g:{Alloy,Components,Propellant,Rations}){double amount=std::min((*stores)[g],g==Rations?4.:g==Components?2.:6.);(*stores)[g]-=amount;b.cargo[g]=amount;}b.initialCargo=b.cargo;
   if(!t.pirate){auto& faction=e.factions[b.faction];faction.warrant=std::min(int64_t(1000000),faction.warrant+150);faction.standing=std::max(-100.,faction.standing-10);b.crime=true;for(auto& c:e.captains)if(c.faction==b.faction)c.grudge=std::min(100.,c.grudge+5);}
   e.boardings[id]=b;f.cancelNavigation();f.ship.throttle=0;f.ship.velocity={};f.combatOrder=0;t.boarding=true;t.velocity={};e.geometryRevision++;economicEvent(e,t.name,"NOMA boarding party attached to the disabled hull / three actual volunteers assigned.");
  }else{
   auto& b=e.boardings.at(id);b.updated=s.hours;
   if(action=="boardingWithdraw"){returnParty(e,b);returnDefenders(e,b);for(int i=0;i<goodCount;i++){if(auto* home=findSettlement(e,b.home))home->stock[i]+=b.cargo[i];else if(auto* band=findPirates(e,b.band))band->stock[i]+=b.cargo[i];else continue;b.transferred[i]+=b.cargo[i];b.cargo[i]=0;}b.status=1;t.boarding=false;}
   else if(action=="boardingNegotiate"){if((b.rooms&1)||t.health<flight::enemyDef(t).hull*.35||t.modules[1]<=10){b.surrendered=true;b.rooms=7;t.charge=0;t.chargeWeapon=-1;economicEvent(e,t.name,"Defenders surrendered under engineering or hull pressure; crew kept alive.");}else notify("Defenders refused / isolate engineering or disable their weapons first",Notice::Important);}
   else if(action=="boardingPower"||action=="boardingBridge"||action=="boardingCargo"){int room=action=="boardingPower"?1:action=="boardingBridge"?2:4;int risk=danger(b,room);cargo[Components]-=room==1?2:1;if(room==1)cargo[Crystal]-=1;int treated=std::min(risk,int(std::floor(cargo[Medicine])));cargo[Medicine]-=treated;int loss=std::min(b.party,risk-treated);b.party-=loss;b.partyLost+=loss;b.rooms|=room;if(room==1){t.modules[1]=0;t.charge=0;t.chargeWeapon=-1;}if(loss)economicEvent(e,t.name,std::to_string(loss)+" boarding volunteers lost securing an armed compartment.");}
   else if(action=="boardingLoot"){double room=std::max(0.,cargoCapacity()-stockTotal(cargo));for(int i=0;i<goodCount;i++){double amount=std::min(room,b.cargo[i]);if(i==Alloy||i==Crystal||i==Biomass||i==Research)amount=std::floor(amount);cargo[i]+=amount;b.cargo[i]-=amount;b.looted[i]+=amount;room-=amount;}}
   else if(action=="boardingCapture"){
    PrizeShip prize;prize.id="PRIZE:"+id+":"+std::to_string(e.nextId++);prize.source="captured:"+prize.id;prize.faction=t.pirate?-1:t.faction;prize.disputed=!t.pirate;prize.ship.vessel.serial=prize.id;prize.ship.vessel.model=t.role==2?1:t.role==1?2:3;prize.ship.hull=std::clamp(t.health/flight::enemyDef(t).hull*100,1.,100.);prize.ship.tank=flight::vesselDef(prize.ship.vessel.model).tank;prize.ship.fuel=0;prize.ship.shield=0;auto& equipment=prize.ship.equipment;equipment.rounds=t.ammunition;equipment.missiles=t.missiles;equipment.decoys=t.countermeasures;equipment.armour=t.armour;equipment.energy=t.energy;equipment.heat=t.heat;equipment.systems={t.modules[0],50,t.modules[2],t.modules[1],50};
    WorldDamage d;d.star=s.currentStar;d.owner=b.home;d.kind=SalvageObject;d.integrity=0;d.position=t.position;auto hull=flight::hullRemains(t);d.scale=hull.size;auto& r=d.recovery;r.valid=true;r.stabilized=r.cut=r.intact=true;r.born=s.hours;r.supportUntil=s.hours;r.shipId=t.id;r.shipModel=hull.model;r.shipSeed=hull.seed;r.shipForward=hull.forward;r.faction=t.faction;initializeClaim(r,prize.faction,t.band);r.contested=prize.disputed;if(t.pirate)r.claim=0;d.salvage=b.cargo;for(int i=0;i<goodCount;i++){b.transferred[i]+=b.cargo[i];b.cargo[i]=0;}
    e.worldDamage.emplace(prize.source,d);e.prizes.push_back(prize);b.prize=prize.id;b.status=2;returnParty(e,b);t.captured=t.dead=true;t.boarding=false;t.respawn=1e6;t.ammunition=t.missiles=t.countermeasures=0;
    if(auto* captain=findCaptain(e,id)){captain->captured=b.defenders>0;captain->destroyed=true;captain->readyAt=e.hour+24;captain->hull=0;captain->defeats=std::min(1000000,captain->defeats+1);captain->grudge=std::min(100.,captain->grudge+15);if(auto* band=findPirates(e,captain->band)){band->grievance=std::min(100.,band->grievance+15);band->truceUntil=0;}}
    missionEvent({"shipCaptured",t.home,t.faction,"","NOMA",id});recoveryTarget=prize.source;e.damageRevision++;e.geometryRevision++;economicEvent(e,t.name,"Hull captured intact / finite ship equipment transferred to an owned prize; prisoners aboard and title recorded.");
   }
  }
 }
 storeShipStock(cargo);syncWorldDamage();syncBoarding();advanceTime(1,true);notify(quote.text,Notice::Important);return true;
}
void Game::syncBoarding(){
 auto& e=s.economy;auto& f=s.flight;for(auto& t:f.traffic)t.boarding=false;
 for(auto it=e.prizes.begin();it!=e.prizes.end();)if(it->ship.port.empty()&&(!e.worldDamage.count(it->source)||objectIntegrity(e.worldDamage,"wreck:world:"+it->source)<=0)){auto r=e.worldDamage.find(it->source);if(r!=e.worldDamage.end())r->second.recovery.intact=false;it=e.prizes.erase(it);notify("Captured hull destroyed before delivery / prize written off",Notice::Important);}else ++it;
 for(auto& [id,b]:e.boardings)if(b.status==0){auto t=std::find_if(f.traffic.begin(),f.traffic.end(),[&id=id](const auto& t){return t.id==id&&!t.dead;});bool safe=t!=f.traffic.end()&&f.valid&&!f.docked&&!f.deployed&&f.star==b.star&&b.star==s.currentStar&&flight::length(t->position-f.ship.position)<=100&&b.party>0&&s.hull>0;
  if(safe){t->boarding=true;t->velocity={};t->charge=0;t->chargeWeapon=-1;continue;}
  if(t==f.traffic.end()||s.hull<=0){b.partyLost+=b.party;b.party=0;if(t==f.traffic.end()){b.defendersLost+=b.defenders;b.defenders=0;for(int i=0;i<goodCount;i++){b.destroyed[i]+=b.cargo[i];b.cargo[i]=0;}}else returnDefenders(e,b);b.status=3;}else{returnParty(e,b);returnDefenders(e,b);b.status=1;}
  if(auto* home=findSettlement(e,b.home))for(int i=0;i<goodCount;i++){home->stock[i]+=b.cargo[i];b.transferred[i]+=b.cargo[i];b.cargo[i]=0;}b.updated=s.hours;notify("Boarding operation interrupted / consult the recovery log",Notice::Critical);
 }
}
bool Game::prizeAction(const std::string& action,const std::string& id){
 if(action!="prizeCommission"&&action!="prizeSell"&&action!="prizeTitle")return false;auto& e=s.economy;auto& f=s.flight;auto it=std::find_if(e.prizes.begin(),e.prizes.end(),[&](const auto& p){return p.id==id;});auto* site=findSettlement(e,e.docked);std::string message;bool success=false;
 if(!turnBased())message="Open strategy before commissioning prize work";
 else if(it==e.prizes.end()||!site||it->ship.port!=site->id||!canUseShipyard(*site))message="Deliver the captured hull to this staffed, powered shipyard first";
 else if(site->owner>0&&(e.factions[site->owner].warrant>0||e.factions[site->owner].standing<=-45))message="Settle the receiving authority's warrant and sanctions first";
 else if(action=="prizeTitle"){int64_t price=std::max(180,flight::vesselDef(it->ship.vessel.model).price/5);if(!it->disputed)message="Hull title is already clear";else if(e.credits<price)message="Title settlement needs "+std::to_string(price)+" credits";else{e.credits-=price;e.factions[it->faction].treasury+=price;it->disputed=false;success=true;message="Original faction paid / prize title cleared; outstanding warrants remain separate";}}
 else if(it->disputed)message="Disputed hull / settle its title before sale or commission";
 else if(action=="prizeSell"){auto price=vesselValue(it->ship,false);if(site->treasury<price)message="The shipyard cannot fund this purchase";else{site->treasury-=price;e.credits+=price;e.prizes.erase(it);success=true;message="Captured hull sold / "+std::to_string(price)+" credits paid from the actual yard treasury";}}
 else if(stockTotal(shipStock())>flight::cargoCapacity(it->ship.vessel))message="Unload cargo to fit the captured hull";
 else{Stock cost{};cost[Alloy]=3+std::ceil((100-it->ship.hull)/8);cost[Components]=3+std::ceil((100-it->ship.equipment.systems[flight::Drive])/20);cost[Crystal]=2;cost[Propellant]=it->ship.tank*.2/8;bool supplies=true;for(int g=0;g<goodCount;g++)supplies&=site->stock[g]+1e-8>=cost[g];
  if(!supplies||e.credits<200)message="Commission needs 200 credits, repair alloy, drive components, 2 crystal and fuel from the yard";
  else{for(int g=0;g<goodCount;g++)site->stock[g]-=cost[g];e.credits-=200;site->treasury+=200;auto old=currentVessel(),ship=it->ship;ship.hull=std::max(70.,ship.hull);ship.fuel=std::min(ship.tank,ship.fuel+ship.tank*.2);for(auto& condition:ship.equipment.systems)condition=std::max(70.,condition);ship.equipment.armour=std::max(60.,ship.equipment.armour);it->ship=old;it->id=old.vessel.serial;it->source.clear();it->faction=-1;it->disputed=false;
   e.vessel=ship.vessel;s.hull=ship.hull;s.fuel=ship.fuel;s.maxFuel=ship.tank;f.ship.vessel=e.vessel;f.ship.hull=s.hull;f.ship.fuel=s.fuel/s.maxFuel*100;f.ship.shield=ship.shield;f.ship.combat=ship.equipment;f.combatTarget.clear();f.cancelNavigation();f.ship.throttle=0;f.ship.velocity={};f.resetDynamics();success=true;message="Captured hull commissioned / actual repairs and fuel supplied; previous ship stored here";}
 }
 if(success){economicEvent(e,site->name,message);advanceTime(1,true);}notify(message,success?Notice::Important:Notice::Status);return true;
}
}
