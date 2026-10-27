#include "game.hpp"
#include <numeric>

namespace hyperion {
namespace {
bool commissioned(const Economy& e,const std::string& id){for(const auto& m:e.missions)if(m.target==id&&m.kind>=ReconstructionMission&&(m.state==MissionActive||m.state==MissionReady))return true;return false;}
bool has(const Stock& stock,const Stock& cost){for(int i=0;i<goodCount;i++)if(stock[i]+1e-8<cost[i])return false;return true;}
void returnRoute(RecoveryCrew& c){std::reverse(c.route.begin(),c.route.end());c.route.push_back(c.origin);c.leg=0;c.phase=2;}
}
void Game::initializeClaim(RecoveryState& r,int faction,const std::string& band){r.claimRules=true;r.claim=band.empty()?faction:-1;r.nextDispatch=s.hours+2;}
bool Game::salvageAuthorized(const std::string& id)const{
 auto it=s.economy.worldDamage.find(id);if(it==s.economy.worldDamage.end())return false;const auto& r=it->second.recovery;
 return !r.claimRules||r.claim<=0||r.permit||r.contested||commissioned(s.economy,id);
}
void Game::advanceRecoverySociety(bool allowLegacy){
 auto& e=s.economy;auto& f=s.flight;bool changed=false;
 e.recoveryCrews.erase(std::remove_if(e.recoveryCrews.begin(),e.recoveryCrews.end(),[&](const auto& c){return c.phase>=3&&c.lastHour+72<s.hours;}),e.recoveryCrews.end());
 for(auto it=e.boardings.begin();it!=e.boardings.end();)if(it->second.status!=0&&it->second.defenders==0&&stockTotal(it->second.cargo)<1e-7&&it->second.updated+72<s.hours)it=e.boardings.erase(it);else ++it;
 for(auto& [id,d]:e.worldDamage){auto& r=d.recovery;if(!r.valid)continue;if(!r.claimRules&&allowLegacy){initializeClaim(r,r.faction);changed=true;}
  if(!r.claimRules||r.intact||r.towed||r.claim==0||d.star!=s.currentStar||!f.valid||f.star!=d.star||s.hours+1e-8<r.nextDispatch||commissioned(e,id))continue;
  int active=0;bool assigned=false;for(const auto& c:e.recoveryCrews)if(c.phase<3&&c.star==d.star){active++;assigned|=c.target==id;}if(assigned||active>=2)continue;
  r.nextDispatch=s.hours+2;changed=true;if(r.crew<=0&&stockTotal(r.sealed)+stockTotal(d.salvage)<.01)continue;
  if(e.recoveryCrews.size()>=512)continue;
  int best=-1;double nearest=12000;Settlement* base=nullptr;
  Stock supplies{};supplies[Alloy]=8;supplies[Components]=4;supplies[Crystal]=1;supplies[Propellant]=4;supplies[Rations]=2;
  for(int i=0;i<int(f.ports.size());i++){const auto& p=f.ports[i];auto* site=findSettlement(e,p.settlement);if(p.disabled||p.recovery||!site||site->owner<=0||site->residents<6||site->treasury<80||!has(site->stock,supplies))continue;if(r.claim>0&&site->owner!=r.claim)continue;
   double distance=flight::length(p.position-d.position);if(distance<nearest){nearest=distance;best=i;base=site;}}
  if(best<0)continue;
  RecoveryCrew c;c.id="RESPONSE:"+std::to_string(e.nextId++);c.name=(r.crew>0?"Lifeline ":"Reclaimer ")+std::to_string(100+stringHash(c.id)%900);c.star=d.star;c.home=base->id;c.target=id;c.faction=base->owner;c.lastHour=s.hours;c.cargo=supplies;c.origin=f.ports[best].position+f.ports[best].normal*65;c.position=c.origin;
  auto destination=f.ports[best];destination.position=d.position+d.normal*(d.scale+45);destination.normal=d.normal;destination.disabled=false;c.route=flight::transferRoute(f.navigationBodies(),c.position,destination);if(c.route.empty())c.route.push_back(destination.position);
  base->residents-=c.people;for(int i=0;i<goodCount;i++)base->stock[i]-=supplies[i];base->treasury-=80;e.factions[c.faction].treasury+=80;e.recoveryCrews.push_back(c);e.geometryRevision++;
  economicEvent(e,base->name,c.name+" dispatched with two residents, finite fuel, containment equipment and an 80-credit service fee.");if(d.star==s.currentStar)notify(c.name+" responding / rescue and salvage vessel inbound",Notice::Important);
 }
 for(auto& c:e.recoveryCrews){if(c.phase>=3||s.hours<=c.lastHour+1e-9)continue;double remaining=std::min(96.,s.hours-c.lastHour);c.lastHour=s.hours;changed=true;
  auto found=e.worldDamage.find(c.target);if(c.phase<2&&(found==e.worldDamage.end()||found->second.recovery.towed||found->second.recovery.claim==0))returnRoute(c);
  for(int guard=0;remaining>1e-9&&c.phase<3&&guard<256;guard++){
   if(c.phase==0||c.phase==2){
    if(c.leg<int(c.route.size())){auto delta=c.route[c.leg]-c.position;double distance=flight::length(delta),spent=std::min(remaining,distance/3200.);if(c.cargo[Propellant]<=1e-7){c.health=std::max(1.,c.health);break;}
     spent=std::min(spent,c.cargo[Propellant]/.25);c.forward=flight::unit(delta,c.forward);c.position+=c.forward*(spent*3200);c.cargo[Propellant]=std::max(0.,c.cargo[Propellant]-spent*.25);remaining-=spent;if(distance-spent*3200<1e-5){c.position=c.route[c.leg++];continue;}break;
    }
    if(c.phase==0){c.phase=1;c.work=0;continue;}
    auto* site=findSettlement(e,c.home);if(!site||site->owner!=c.faction||site->residents<=0)break;
    if(f.valid&&f.star==c.star){int berth=-1;double nearest=1e30;for(int i=0;i<int(f.ports.size());i++){const auto& p=f.ports[i];if(p.settlement==c.home&&!p.disabled&&!p.recovery){double distance=flight::length(p.position-c.position);if(distance<nearest){nearest=distance;berth=i;}}}if(berth<0)break;if(nearest>200){auto destination=f.ports[berth];destination.position+=destination.normal*65;c.route=flight::transferRoute(f.navigationBodies(),c.position,destination);c.leg=0;c.origin=destination.position;if(c.route.empty())c.route.push_back(c.origin);continue;}}

    if(c.people||c.rescued){site->residents+=c.people+c.rescued;if(found!=e.worldDamage.end()){auto& r=found->second.recovery;r.npcAboard-=c.rescued;r.npcDelivered+=c.rescued;}c.people=c.rescued=0;e.geometryRevision++;economicEvent(e,site->name,c.name+" returned its crew and delivered rescued residents alive.");}
    double room=std::max(0.,storageCapacity(*site)-stockTotal(site->stock));for(int i=0;i<goodCount;i++){double moved=std::min(room,c.cargo[i]);site->stock[i]+=moved;c.cargo[i]-=moved;room-=moved;}
    if(stockTotal(c.cargo)<1e-7){c.phase=3;economicEvent(e,site->name,c.name+" unloaded recovered equipment into the actual warehouse.");}break;
   }
   if(c.phase==1){double spent=std::min(remaining,.5-c.work);c.work+=spent;remaining-=spent;if(c.work<.5-1e-8)break;
    if(found!=e.worldDamage.end()){auto& d=found->second;auto& r=d.recovery;
     if(!r.stabilized&&c.cargo[Components]>=2&&c.cargo[Crystal]>=1){c.cargo[Components]-=2;c.cargo[Crystal]-=1;r.stabilized=true;}
     if(recoveryHazard(r,s.hours)<=.02){int people=std::min(8,r.crew);r.crew-=people;r.npcAboard+=people;c.rescued+=people;
      if(r.claim!=0&&!commissioned(e,c.target)){double room=std::max(0.,32-stockTotal(c.cargo));for(int i=0;i<goodCount;i++){double amount=std::min(room,r.sealed[i]);r.sealed[i]-=amount;r.recovered[i]+=amount;c.cargo[i]+=amount;room-=amount;amount=std::min(room,d.salvage[i]);d.salvage[i]-=amount;c.cargo[i]+=amount;room-=amount;}if(r.sealed[Research]<.01&&!r.boxRecovered)r.box=false;if(stockTotal(r.sealed)<.01)r.cut=true;}
     }
    }returnRoute(c);
   }
  }
 }
 if(changed){e.damageRevision++;syncWorldDamage();}syncRecoveryTraffic();syncBoarding();
}
void Game::syncRecoveryTraffic(){
 auto& e=s.economy;auto& f=s.flight;if(!f.valid||f.ports.empty())return;
 f.traffic.erase(std::remove_if(f.traffic.begin(),f.traffic.end(),[&](const auto& t){if(t.kind!=3)return false;for(const auto& c:e.recoveryCrews)if(c.id==t.id&&c.star==s.currentStar&&c.phase<3)return false;return true;}),f.traffic.end());
 for(const auto& c:e.recoveryCrews)if(c.star==s.currentStar&&c.phase<3){auto it=std::find_if(f.traffic.begin(),f.traffic.end(),[&](const auto& t){return t.id==c.id;});if(it==f.traffic.end()){flight::Traffic t;t.id=c.id;t.kind=3;t.managed=true;t.hostile=false;t.positioned=true;t.genome={1,stringHash(c.id),false};f.traffic.push_back(t);it=f.traffic.end()-1;}
  auto& t=*it;t.previous=t.position;t.position=c.position;t.forward=c.forward;t.velocity=c.forward*(c.phase==1?0:3200/55.);t.health=std::min(t.health,c.health);t.faction=c.faction;t.dead=false;t.name=c.name;t.band=c.band;t.pirate=!c.band.empty();t.allegiance=e.factions[c.faction].name;t.reason=c.phase==0?"Rescue and salvage response":c.phase==1?"Working wreck / finite supplies":"Returning survivors and equipment";t.home=0;for(int i=0;i<int(f.ports.size());i++)if(f.ports[i].settlement==c.home&&!f.ports[i].disabled){t.home=i;break;}
 }
}
bool Game::recoveryEvent(const flight::Event& event){
 if(event.kind!="recoveryLoss")return false;auto& e=s.economy;auto it=std::find_if(e.recoveryCrews.begin(),e.recoveryCrews.end(),[&](const auto& c){return c.id==event.victim;});if(it==e.recoveryCrews.end()||it->phase>=3)return true;auto& c=*it;
 auto target=e.worldDamage.find(c.target);if(target!=e.worldDamage.end()){auto& r=target->second.recovery;r.npcAboard-=c.rescued;r.lost+=c.rescued;r.nextDispatch=s.hours+6;}
 WorldDamage d;d.kind=SalvageObject;d.star=c.star;d.owner=c.home;d.integrity=0;d.position=c.position;d.scale=11;auto& r=d.recovery;r.valid=true;r.faction=c.faction;r.born=s.hours;r.supportUntil=s.hours;r.sealed=r.initial=c.cargo;r.culprit=event.actor;r.stabilized=true;r.shipSeed=stringHash(c.id);r.shipModel=2+flight::hullVariant({1,r.shipSeed,false})*32;r.shipForward=c.forward;r.shipId=c.id;initializeClaim(r,c.faction);if(e.worldDamage.size()<60000)e.worldDamage.emplace("responder-wreck:"+c.id,d);
 c.cargo={};c.phase=4;c.health=0;c.people=c.rescued=0;e.damageRevision++;e.geometryRevision++;economicEvent(e,c.name,"Recovery ship destroyed / its crew and passengers lost; remaining physical cargo became a wreck.");if(event.actor=="NOMA")for(auto& captain:e.captains)if(captain.faction==c.faction)captain.grudge=std::min(100.,captain.grudge+8);syncWorldDamage();return true;
}
}
