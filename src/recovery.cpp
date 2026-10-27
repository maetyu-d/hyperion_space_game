#include "game.hpp"
#include "flight_scene.hpp"
#include <numeric>
namespace hyperion {
namespace {
bool hasStock(const Stock& a,const Stock& b){for(int i=0;i<goodCount;i++)if(a[i]+1e-8<b[i])return false;return true;}
Stock repairCost(const WorldDamage& d){if(d.kind==FacilityObject)return buildings[d.slot/24].construction;Stock cost{};cost[Alloy]=d.kind==PortObject?32:8;cost[Components]=d.kind==PortObject?12:3;return cost;}
bool job(const Economy& e,const std::string& id,int kind){return std::any_of(e.missions.begin(),e.missions.end(),[&](const auto& m){return m.target==id&&m.kind==kind&&m.state==MissionActive;});}
}
Game::RecoveryQuote Game::recoveryQuote(const std::string& action,const std::string& id)const{
 RecoveryQuote q;auto it=s.economy.worldDamage.find(id);if(it==s.economy.worldDamage.end()||!it->second.recovery.valid){q.reason="No recoverable wreck selected";return q;}const auto& d=it->second;const auto& r=d.recovery;const auto& f=s.flight;auto cargo=shipStock();
 if(action=="recoveryPlot"&&d.star!=s.currentStar){q.reason="Travel to the wreck's system before plotting a local approach";return q;}
 if(action=="recoveryPlan"||action=="recoverySelect"||action=="recoveryPlot"){q.available=true;q.text="Free recovery planning / world paused";return q;}
 if(!turnBased()){q.reason="Open strategy to plan recovery work";return q;}
 if(action=="recoveryDeliver"){
  auto* site=findSettlement(s.economy,s.economy.docked);if(!site||site->owner!=r.faction||!canDock(*site)||r.aboard<=0){q.reason="Dock at a friendly inhabited settlement with evacuees aboard";return q;}q.available=true;q.text="Deliver evacuees / 1 h / actual residents transferred";return q;
 }
 if(action=="recoveryPrize"){
  auto* site=findSettlement(s.economy,s.economy.docked);const PrizeShip* prize=nullptr;for(const auto& p:s.economy.prizes)if(p.source==id&&p.ship.port.empty())prize=&p;
  if(!prize||!r.intact||!site||!canUseShipyard(*site)||s.currentStar!=d.star||flight::length(d.position-f.ship.position)>100){q.reason="Tow the captured hull to a staffed, powered shipyard and dock within 100 km";return q;}q.available=true;q.text="Deliver captured hull / 1 h / stored here for title settlement, sale or commission";return q;
 }
 if(s.currentStar!=d.star||!f.valid||f.star!=d.star||f.deployed||f.docked||flight::length(f.ship.position-d.position)>55||f.speed()>5||f.autopilot){q.reason="Approach within 55 km, below 5 km/s, undocked; disengage navigation";return q;}
 if(action=="recoveryPermit"){
  if(!r.claimRules||r.claim<=0||r.permit){q.reason="No foreign salvage licence needed";return q;}int price=std::min(500,90+int(d.scale*2));if(s.economy.credits<price){q.reason="Salvage licence needs "+std::to_string(price)+" credits";return q;}q.available=true;q.text="Purchase salvage licence / "+std::to_string(price)+" cr to the claimant / 1 h / hull title is separate";return q;
 }
 if(action=="recoveryContest"){
  if(salvageAuthorized(id)){q.reason="Salvage access already available";return q;}q.available=true;q.text="Defy faction claim / 1 h / warrant +150 cr, standing -12 and captain grudges";return q;
 }
 if(action=="recoveryClaim"){
  if(!r.claimRules||r.claim>=0){q.reason="A claim already exists";return q;}q.available=true;q.text="Register NOMA salvage claim / 1 h / lawful rivals defer to your claim";return q;
 }
 if((action=="recoveryCut"||action=="recoveryBox"||action=="recoveryTow")&&!salvageAuthorized(id)){q.reason="Foreign salvage claim / accept a recovery commission, buy a licence or explicitly contest it";return q;}
 if(action=="recoveryDetach"){q.available=r.towed;q.text="Release tow / 1 h / wreck remains at its physical location";q.reason="No tow attached";return q;}
 if(action=="recoveryStabilize"){
  if(r.stabilized){q.reason="Containment already secure";return q;}if(cargo[Components]<2||cargo[Crystal]<1){q.reason="Containment needs 2 components and 1 crystal";return q;}q.available=true;q.text="Quench fuel and isolate circuits / 2 parts + 1 crystal / 1 h";return q;
 }
 if(recoveryHazard(r,s.hours)>.02){q.reason="Stabilize the leaking fuel and live circuits first";return q;}
 if(action=="recoveryCut"){
  if(r.cut){q.reason="Equipment already cut free";return q;}if(cargo[Components]<1){q.reason="Cutting needs 1 component";return q;}q.available=true;q.text="Cut equipment loose / 1 component / 1 h / finite supplies released";return q;
 }
 if(action=="recoveryBox"){
  if(!r.cut||!r.box||r.boxRecovered){q.reason="Cut open the housing; each recorder can be recovered once";return q;}if(cargoCapacity()-stockTotal(cargo)<1){q.reason="Recorder needs one cargo unit";return q;}q.available=true;q.text="Recover black box / 1 h / one research datum";return q;
 }
 if(action=="recoveryRescue"){
  int aboard=rescuedCrew();
  if(r.crew<=0||s.hours>=r.supportUntil){q.reason="No surviving stranded crew";return q;}if(aboard>=12){q.reason="Twelve rescue berths occupied; deliver survivors first";return q;}q.available=true;q.text="Evacuate survivors / 1 h / "+std::to_string(r.crew)+" people / life support "+std::to_string(int(std::ceil(r.supportUntil-s.hours)))+" h";return q;
 }
 if(action=="recoveryTow"){
  if(((!d.structure.valid||!d.structure.orbital)&&r.shipModel<0)||r.towed){q.reason="Only detached orbital wreckage can be towed";return q;}if(!r.cut){q.reason="Cut free the service connections first";return q;}for(const auto& [key,record]:s.economy.worldDamage)if(record.recovery.towed){q.reason="One tow may be attached at a time";return q;}
  double limit=30+f.ship.vessel.model*25+f.ship.vessel.fitted[flight::FlightDrive]*8;if(d.scale>limit){q.reason="Wreck too heavy / fit a stronger drive or a larger hull";return q;}if(s.fuel<3){q.reason="Attaching a tow needs 3 fuel";return q;}q.available=true;q.text="Attach tow / 3 fuel / 1 h / slower flight and continuous fuel use";return q;
 }
 if(action=="recoveryRebuild"){
  if(d.kind!=FacilityObject&&d.kind!=PortObject&&d.kind!=PanelObject){q.reason="This wreck has no infrastructure blueprint to reconstruct";return q;}
  bool atSite=false;for(const auto& port:f.ports)if(port.settlement==d.owner&&!port.recovery&&flight::length(port.position-f.ship.position)<1000)atSite=true;if(!atSite){q.reason="Return wreckage to its original settlement before reconstruction";return q;}
  auto* site=findSettlement(s.economy,d.owner);if(!site||(site->owner!=0&&!job(s.economy,id,ReconstructionMission))){q.reason="Accept the reconstruction commission or hold this charter";return q;}if(r.crew||r.aboard||r.npcAboard){q.reason="Evacuate and deliver all survivors before rebuilding";return q;}auto cost=repairCost(d);if(!hasStock(cargo,cost)){q.reason="Bring the full construction materials / see the commission";return q;}if(site->owner==0&&s.economy.credits<(d.kind==FacilityObject?buildings[d.slot/24].price:d.kind==PortObject?800:120)){q.reason="Insufficient reconstruction credits";return q;}q.available=true;q.text="Reconstruct original lot / real materials / 1 h";return q;
 }
 q.reason="Unknown recovery order";return q;
}
bool Game::recoveryAction(const std::string& action,const std::string& arg){
 if(action.rfind("recovery",0)!=0)return false;
 if(action=="recoveryPlot"){auto it=s.economy.worldDamage.find(arg);if(it==s.economy.worldDamage.end()||!it->second.recovery.valid){notify("Recovery site no longer exists");return true;}if(it->second.star!=s.currentStar){notify("Travel to the wreck's system before plotting a local approach");return true;}ensureFlight();auto& f=s.flight;int target=-1;for(int i=0;i<int(f.ports.size());i++)if(f.ports[i].id=="recovery:"+arg)target=i;if(target<0){flight::Port port;port.id="recovery:"+arg;port.name="Recovery approach";port.description="Fly-by / approach wreck below 5 km/s; open Recovery to work";port.position=it->second.position+it->second.normal*(it->second.scale+80);port.normal=it->second.normal;port.body=it->second.body;port.planet=-1;port.recovery=true;f.ports.push_back(port);target=int(f.ports.size())-1;}f.selectTarget(target);recoveryTarget=arg;notify("Recovery approach plotted / fly to the holding point, then close within 55 km");return true;}
 if(action=="recoveryPlan"){openStrategy();recoveryTarget=arg;notify("Recovery planning / choose an order in Recovery / thinking time is paused");return true;}
 if(action=="recoverySelect"){recoveryTarget=arg;return true;}
 const auto id=arg.empty()?recoveryTarget:arg;auto quote=recoveryQuote(action,id);if(!quote.available){notify(quote.reason);return true;}auto& e=s.economy;auto it=e.worldDamage.find(id);auto& d=it->second;auto& r=d.recovery;auto cargo=shipStock();
 if(action=="recoveryPermit"){int price=std::min(500,90+int(d.scale*2));e.credits-=price;e.factions[r.claim].treasury+=price;r.permit=true;}
 else if(action=="recoveryContest"){r.contested=true;auto& faction=e.factions[r.claim];faction.warrant=std::min(int64_t(1000000),faction.warrant+150);faction.standing=std::max(-100.,faction.standing-12);for(auto& c:e.captains)if(c.faction==r.claim)c.grudge=std::min(100.,c.grudge+8);economicEvent(e,d.owner,"NOMA defied a bonded salvage claim / title remains disputed; authority warrant and vendettas recorded.");}
 else if(action=="recoveryClaim")r.claim=0;
 else if(action=="recoveryPrize"){for(auto& p:e.prizes)if(p.source==id)p.ship.port=e.docked;e.worldDamage.erase(it);e.geometryRevision++;}
 else if(action=="recoveryStabilize"){cargo[Components]-=2;cargo[Crystal]-=1;r.stabilized=true;r.towed=false;}
 else if(action=="recoveryCut"){cargo[Components]-=1;r.cut=true;for(int i=0;i<goodCount;i++)if(i!=Research){d.salvage[i]+=r.sealed[i];r.recovered[i]+=r.sealed[i];r.sealed[i]=0;}}
 else if(action=="recoveryBox"){r.boxRecovered=true;double taken=r.sealed[Research];cargo[Research]+=taken;r.recovered[Research]+=taken;r.sealed[Research]=0;}
 else if(action=="recoveryRescue"){int aboard=rescuedCrew();int moved=std::min(r.crew,12-aboard);r.crew-=moved;r.aboard+=moved;}
 else if(action=="recoveryDeliver"){auto* site=findSettlement(e,e.docked);site->residents+=r.aboard;r.delivered+=r.aboard;r.aboard=0;e.geometryRevision++;economicEvent(e,site->name,"Evacuees delivered alive; residents transferred from the damaged site.");}
 else if(action=="recoveryTow"){s.fuel-=3;s.flight.ship.fuel=s.fuel/s.maxFuel*100;r.towed=true;r.stabilized=true;r.towOffset=d.position-s.flight.ship.position;}
 else if(action=="recoveryDetach")r.towed=false;
 else if(action=="recoveryRebuild"){
  auto* site=findSettlement(e,d.owner);auto cost=repairCost(d);for(int i=0;i<goodCount;i++)cargo[i]-=cost[i];if(site->owner==0){auto price=d.kind==FacilityObject?buildings[d.slot/24].price:d.kind==PortObject?800:120;e.credits-=price;e.factions[site->authority].treasury+=price;}
  for(auto& m:e.missions)if(m.target==id&&m.kind==ReconstructionMission&&m.state==MissionActive){m.state=MissionReady;m.result="Original infrastructure rebuilt using NOMA's actual materials";}
  if(d.kind==FacilityObject)site->facilityIntegrity.erase(d.slot);e.worldDamage.erase(it);e.geometryRevision++;
 }
 storeShipStock(cargo);e.damageRevision++;syncWorldDamage();recoveryMissions();advanceTime(1,true);notify(quote.text,Notice::Important);return true;
}
void Game::updateRecovery(double dt){
 auto& e=s.economy;auto& f=s.flight;bool changed=false;
 for(auto& [id,d]:e.worldDamage){auto& r=d.recovery;if(!r.valid)continue;
  if(r.crew>0&&s.hours>=r.supportUntil){r.lost+=r.crew;r.crew=0;changed=true;if(d.star==s.currentStar||r.faction==0)notify("Wreck life support exhausted / stranded survivors lost",Notice::Critical);economicEvent(e,d.owner,"Emergency life support exhausted; stranded survivors lost.");}
  bool local=f.valid&&f.star==d.star&&s.currentStar==d.star;
  if(r.towed&&local){if(f.docked||f.deployed||f.ship.fuel<=.1){r.towed=false;changed=true;}else if(dt>0){auto at=f.ship.position+r.towOffset;auto delta=at-d.position;d.position=at;d.structure.origin+=delta;f.ship.fuel=std::max(0.,f.ship.fuel-dt*(.035+d.scale*.001));changed=true;}}
  if(!r.failed&&r.failureAt>0&&s.hours>=r.failureAt){r.failed=true;changed=true;if(!r.stabilized){double burned=std::min(r.sealed[Propellant],r.initial[Propellant]*.5);r.sealed[Propellant]-=burned;r.destroyed[Propellant]+=burned;if(auto* site=findSettlement(e,d.owner)){site->condition=std::max(0.,site->condition-.015);site->pollution=std::min(1.,site->pollution+.015);}if(local){f.effect(d.position,flight::ShipExplosion,{}, {},d.normal,flight::Resonance,1,std::min(12.,d.scale));std::set<std::string> struck;for(int i=0;i<int(f.worldObjects.size());i++){const auto& o=f.worldObjects[i];if(o.id!=id&&o.kind!=DebrisObject&&flight::length(o.position-d.position)<35+d.scale*.25&&struck.insert(o.id).second)f.damageWorld(i,40,flight::Plasma,d.position,r.culprit=="NOMA",r.culprit);}}}}
  if(local&&dt>0&&!f.docked&&!f.deployed){double charge=recoveryHazard(r,s.hours),radius=12+std::min(35.,d.scale*.45),distance=flight::length(f.ship.position-d.position);if(charge>.02&&distance<radius){f.damageShip(dt*4*charge*(1-distance/radius),flight::Resonance,d.position);if(f.noticeAge>2)f.message("Live wreck / electrical discharge and leaking fuel / retreat or stabilize");}}
 }
 if(changed){e.damageRevision++;syncWorldDamage();}advanceRecoverySociety(dt>0);recoveryMissions();
}
void Game::recoveryMissions(){
 auto& e=s.economy;
 for(auto& m:e.missions)if(m.kind>=ReconstructionMission&&m.state<=MissionReady){auto it=e.worldDamage.find(m.target);if(it==e.worldDamage.end()){if(m.state!=MissionReady)finishMission(e,m,m.state==MissionOffered?MissionExpired:MissionFailed,"Recovery target rebuilt or removed before the commissioned work / escrow returned");continue;}auto& r=it->second.recovery;
  if(m.kind==BlackBoxMission&&!r.box&&!r.boxRecovered)finishMission(e,m,r.recovered[Research]>0?MissionExpired:m.state==MissionOffered?MissionExpired:MissionFailed,r.recovered[Research]>0?"Recorder secured by another recovery crew / escrow returned":"Recorder destroyed / escrow returned");
  if(m.kind==EvacuationMission&&r.crew+r.aboard+r.delivered<m.quantity)finishMission(e,m,r.npcAboard+r.npcDelivered>0?MissionExpired:m.state==MissionOffered?MissionExpired:MissionFailed,r.npcAboard+r.npcDelivered>0?"Local rescue crew took over / survivors transferred / escrow returned":"Commissioned survivors lost / escrow returned");
  if(m.state==MissionActive&&((m.kind==EvacuationMission&&r.delivered>=m.quantity)||(m.kind==BlackBoxMission&&r.boxRecovered))){m.state=MissionReady;m.result=m.kind==EvacuationMission?"Survivors delivered to a friendly settlement":"Recorder recovered / return its research datum for debrief";}
 }
 if(e.missions.size()>=256)return;
 for(const auto& [id,d]:e.worldDamage){const auto& r=d.recovery;if(!r.valid||r.culprit=="NOMA"||r.faction<=0)continue;const Settlement* issuer=nullptr;for(const auto& site:e.settlements)if(site.star==d.star&&site.owner==r.faction&&site.residents>0&&site.treasury>=180){bool intact=objectIntegrity(e.worldDamage,portObjectId("orbit:"+site.id))>0||objectIntegrity(e.worldDamage,portObjectId("surface:"+site.id))>0;if(intact&&(!issuer||site.id==d.owner))issuer=&site;}
  if(!issuer)continue;for(int kind:{ReconstructionMission,EvacuationMission,BlackBoxMission}){if(e.missions.size()>=256)break;if(kind==EvacuationMission&&r.crew<=0)continue;if(kind==BlackBoxMission&&(!r.box||r.boxRecovered))continue;if(kind==ReconstructionMission&&d.kind!=FacilityObject&&d.kind!=PortObject&&d.kind!=PanelObject)continue;bool exists=false;for(const auto& old:e.missions)if(old.target==id&&old.kind==kind)exists=true;if(exists)continue;
   Mission m;m.id="REC:"+id+":"+std::to_string(kind)+":"+std::to_string(d.structure.seed);m.title=std::string(kind==ReconstructionMission?"Reconstruct":kind==EvacuationMission?"Evacuate":"Recover recorder")+" / "+issuer->name;m.cause="Physical infrastructure lost at "+d.owner+". "+(kind==ReconstructionMission?std::string("Bring construction materials and rebuild the original site."):kind==EvacuationMission?"Stranded residents have finite emergency life support.":"Recover the control recorder from its sealed housing.");m.kind=kind;m.issuer=issuer->id;m.star=d.star;m.target=id;m.faction=r.faction;m.planet=std::max(0,d.body);m.quantity=kind==EvacuationMission?r.initialCrew:1;m.good=kind==BlackBoxMission?Research:Components;m.created=e.hour;m.deadline=kind==EvacuationMission?r.supportUntil+6:s.hours+36;m.risk=r.charge;m.seed=d.structure.seed;m.reward=std::min(issuer->treasury/3,int64_t(kind==ReconstructionMission?800:kind==EvacuationMission?180+m.quantity*90:180));if(m.reward>=90)e.missions.push_back(std::move(m));
  }
 }
}
void Game::registerSpaceWreck(const flight::Event& event){
 auto& e=s.economy;auto& f=s.flight;if(event.victim.empty()||e.worldDamage.size()>=60000)return;auto w=std::find_if(f.wrecks.begin(),f.wrecks.end(),[&](const auto& wreck){return wreck.id==event.victim&&std::accumulate(wreck.cargo.begin(),wreck.cargo.end(),0.)>0;});if(w==f.wrecks.end())return;
 WorldDamage d;d.star=s.currentStar;d.kind=SalvageObject;d.integrity=0;d.body=event.port>=0&&event.port<int(f.ports.size())?f.ports[event.port].body:-1;d.owner=event.port>=0&&event.port<int(f.ports.size())?f.ports[event.port].settlement:"";d.position=w->position;d.scale=w->hull.size;auto& r=d.recovery;r.valid=true;r.born=s.hours;r.charge=.3;r.failureAt=s.hours+.04;r.supportUntil=s.hours+12;r.faction=event.faction;r.culprit=event.actor;r.box=true;r.shipId=w->id;r.shipModel=w->hull.model;r.shipSeed=w->hull.seed;r.shipForward=w->hull.forward;std::string band;for(const auto& t:f.traffic)if(t.id==event.victim&&t.pirate)band=t.band.empty()?"independent":t.band;initializeClaim(r,r.faction,band);r.sealed[Alloy]=w->cargo[0];r.sealed[Components]=w->cargo[1];r.sealed[Propellant]=w->cargo[2];r.sealed[Research]=1;r.initial=r.sealed;
 if(auto* site=findSettlement(e,d.owner))if(site->residents>2){r.initialCrew=r.crew=1;site->residents--;e.geometryRevision++;}w->cargo={};e.worldDamage.emplace("shipwreck:"+event.victim+":"+std::to_string(e.nextId++),d);e.damageRevision++;syncWorldDamage();recoveryMissions();
}

}
