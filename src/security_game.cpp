#include "game.hpp"
namespace hyperion {
void Game::storeSecurity(){
 auto& e=s.economy;for(const auto& t:s.flight.traffic)if(t.managed){auto* c=findCaptain(e,t.id);if(!c||c->destroyed||t.dead)continue;
  c->hull=t.health;c->shield=t.shield;c->armour=t.armour;c->modules=t.modules;c->rounds=t.ammunition;c->missiles=t.missiles;c->decoys=t.countermeasures;c->energy=t.energy;c->heat=t.heat;
 }
}
void Game::refreshSecurity(){
 auto& e=s.economy;auto& f=s.flight;if(f.ports.empty())return;for(const auto& faction:e.factions)if(faction.warrant>0)f.portCombat=std::max(f.portCombat,5.);
 for(auto& c:e.captains)if(c.star==s.currentStar){
  int home=-1;std::string id=c.band.empty()?"orbit:"+c.home:"den:"+c.band;
  for(int i=0;i<int(f.ports.size());i++)if(f.ports[i].id==id){home=i;break;}if(home<0)continue;
  auto it=std::find_if(f.traffic.begin(),f.traffic.end(),[&](const auto& t){return t.id==c.id;});
  if(it==f.traffic.end()){hyperion::flight::Traffic t;t.id=c.id;t.kind=1;t.managed=true;t.dead=true;f.traffic.push_back(t);it=f.traffic.end()-1;}
  auto& t=*it;bool revive=t.dead&&!c.destroyed;t.managed=true;t.name=c.name;t.band=c.band;t.pirate=!c.band.empty();t.role=c.role;t.genome=c.genome;t.faction=c.faction;t.home=home;
  auto* base=findSettlement(e,c.home);if(!t.pirate&&(!base||base->owner!=c.faction)){t.dead=true;t.respawn=1e6;continue;}
  if(c.destroyed){t.dead=true;t.respawn=1e6;}else if(revive){t.dead=false;t.respawn=0;t.health=c.hull;t.shield=c.shield;t.armour=c.armour;t.modules=c.modules;t.ammunition=c.rounds;t.missiles=c.missiles;t.countermeasures=c.decoys;t.energy=c.energy;t.heat=c.heat;t.positioned=false;t.fireClock=3;t.charge=0;t.chargeWeapon=-1;t.salvos=0;t.serviceClock=t.decoyClock=t.evadeClock=0;t.assaultReported=false;}
  auto* band=findPirates(e,c.band);t.enemies.fill(false);t.escortDuty=false;
  if(band){t.allegiance=band->name;bool feud=band->grievance>=10||c.grudge>=10;bool truce=band->truceUntil>e.hour&&!feud;
   t.hostile=feud||(!truce&&stockTotal(shipStock())>=20);t.reason=feud?"Vendetta / crew losses attributed to NOMA":truce?"Paid truce / "+std::to_string(int(std::ceil(band->truceUntil-e.hour)))+" h remaining":t.hostile?"Piracy / demanding ship cargo":"Pirate crew / seeking loaded convoys";
  }else{auto& faction=e.factions[c.faction];t.escortDuty=faction.relations[3]>=20&&faction.warrant==0;t.allegiance=faction.name;t.hostile=faction.warrant>0||faction.standing<=-60||c.grudge>=10;
   t.reason=faction.warrant>0?"Authority warrant / "+std::to_string(faction.warrant)+" cr":faction.standing<=-60?"Faction feud / standing below -60":c.grudge>=10?"Personal vendetta / unresolved assault":"Patrol / protects faction and allied shipping";
   t.enemies[0]=t.hostile;for(int i=1;i<factionCount;i++)t.enemies[i]=factionsAtWar(e,c.faction,i);
  }
 }
}
bool Game::securityEvent(const hyperion::flight::Event& event){
 auto& e=s.economy;auto* captain=findCaptain(e,event.victim);
 if((event.kind!="aggression"&&event.kind!="skirmish"&&event.kind!="shipLoss")||!captain)return false;
 auto& c=*captain;auto* band=findPirates(e,c.band);
 if(event.kind=="aggression"||event.kind=="skirmish"){
  c.grudge=std::min(100.,c.grudge+4);
  if(band){band->grievance=std::min(100.,band->grievance+(event.kind=="aggression"?12:4));if(event.kind=="aggression")band->truceUntil=0;}
  else if(event.kind=="aggression"){auto& f=e.factions[c.faction];f.warrant=std::min(int64_t(1000000),f.warrant+75);f.standing=std::max(-100.,f.standing-8);
   economicEvent(e,c.name,"NOMA attacked a neutral patrol. "+f.name+" issued a 75-credit warrant; its patrols now pursue NOMA.");notify("Unprovoked assault / faction warrant issued",Notice::Important);}
  return true;
 }
 if(c.destroyed)return true;c.destroyed=true;c.readyAt=e.hour+24;c.defeats=std::min(1000000,c.defeats+1);c.hull=0;
 if(auto* killer=findCaptain(e,event.actor))killer->victories=std::min(1000000,killer->victories+1);
 if(event.actor=="NOMA"){
  c.grudge=std::min(100.,c.grudge+20);
  if(band){band->grievance=std::min(100.,band->grievance+20);band->truceUntil=0;for(auto& mate:e.captains)if(mate.band==band->id)mate.grudge=std::min(100.,mate.grudge+10);
   Settlement* issuer=nullptr;double best=-1e9;for(auto& p:e.settlements)if(p.star==s.currentStar&&p.owner>0&&e.factions[p.owner].warrant==0&&e.factions[p.owner].standing>-45&&p.residents){double score=e.factions[p.owner].standing;if(score>best){best=score;issuer=&p;}}
   int64_t paid=issuer?std::min(int64_t(240),issuer->treasury):0;if(issuer){issuer->treasury-=paid;e.factions[issuer->owner].standing=std::min(100.,e.factions[issuer->owner].standing+2);}e.credits+=paid;
   economicEvent(e,c.name,"NOMA defeated a captain of "+band->name+". "+(issuer?issuer->name:"No eligible authority")+" paid "+std::to_string(paid)+" credits. The surviving crew declared a vendetta; a replacement requires funded shipyard supplies.");notify("Pirate defeated / bounty "+std::to_string(paid)+" cr / crew vendetta recorded",Notice::Important);
  }else{auto& f=e.factions[c.faction];f.warrant=std::min(int64_t(1000000),f.warrant+300);f.standing=std::max(-100.,f.standing-18);
   economicEvent(e,c.name,"NOMA destroyed a patrol of "+f.name+". Restitution increased by 300 credits; the captain's personal grudge persists.");notify("Faction patrol destroyed / warrant +300 cr",Notice::Important);}
 }else economicEvent(e,c.name,"Ship lost in faction combat. Captain escaped; replacement requires 24 hours, 240 credits, alloy, components and fuel.");
 return true;
}
bool Game::securityAction(const std::string& action,const std::string& arg){
 if(action!="pirateTruce"&&action!="restitution"&&action!="escort"&&action!="inspectContact")return false;
 ensureFlight();auto& e=s.economy;std::string message;bool success=false;
 if(action=="inspectContact"){for(const auto& t:s.flight.traffic)if(t.id==arg&&!t.dead){s.flight.combatTarget=arg;s.flight.lock=0;notify(t.name+" / "+hyperion::flight::vesselDesignation(t.genome)+" / "+hyperion::flight::pilotTemperament(t.genome)+" / "+hyperion::flight::weaponDef(hyperion::flight::enemyDef(t).weapon).name+" / "+t.reason);break;}return true;}
 if(action=="pirateTruce"){
  auto* band=findPirates(e,arg);if(!band||band->star!=s.currentStar)message="Pirate comms require the crew's current star system";
  else if(band->truceUntil>e.hour&&band->grievance<10)message="The existing truce is still in force";
  else {auto price=pirateTrucePrice(*band);if(e.credits<price)message="Settlement of the feud requires "+std::to_string(price)+" credits";
   else{e.credits-=price;band->treasury+=price;band->grievance=0;band->truceUntil=e.hour+24;for(auto& c:e.captains)if(c.band==band->id)c.grudge=0;
    message=band->name+" accepted "+std::to_string(price)+" credits / vendetta settled / 24-hour safe passage for NOMA and its freight";success=true;economicEvent(e,band->name,message);}
  }
 }else if(action=="restitution"){
  int faction=-1;try{size_t used=0;faction=std::stoi(arg,&used);if(used!=arg.size())faction=-1;}catch(...){}
  auto* port=findSettlement(e,e.docked);
  if(faction<1||faction>=factionCount)message="Unknown issuing authority";
  else if(!port||port->owner!=faction||!canDock(*port))message="Dock at a port of the issuing authority to settle its warrant";
  else {auto& f=e.factions[faction];if(f.warrant<=0)message="No outstanding warrant";else if(e.credits<f.warrant)message="Restitution requires "+std::to_string(f.warrant)+" credits";
   else{auto paid=f.warrant;e.credits-=paid;f.treasury+=paid;f.warrant=0;f.standing=std::min(100.,f.standing+10);for(auto& c:e.captains)if(c.band.empty()&&c.faction==faction)c.grudge=0;
    message="Warrant settled / "+std::to_string(paid)+" credits paid / diplomacy may still be needed to end a faction feud";success=true;economicEvent(e,f.name,message);}
  }
 }else{
  auto it=std::find_if(e.routes.begin(),e.routes.end(),[&](const auto& r){auto* from=findSettlement(e,r.from);return r.id==arg&&from&&from->star==s.currentStar;});
  if(it==e.routes.end())message="Select a freight route in this system";
  else if(it->escortUntil>e.hour)message="This convoy already has an active escort commission";
  else if(e.factions[3].standing<=-45||e.factions[3].warrant>0)message="Meridian will not commission an escort while it sanctions or has a warrant for NOMA";
  else if(e.credits<100)message="A 24-hour convoy escort costs 100 credits";
  else{bool available=false;for(const auto& c:e.captains)if(c.star==s.currentStar&&c.band.empty()&&!c.destroyed&&e.factions[c.faction].relations[3]>=20&&e.factions[c.faction].warrant==0)available=true;
   if(!available)message="No allied patrol survives here to provide an escort";
   else{e.credits-=100;e.factions[3].treasury+=100;it->escortUntil=e.hour+24;message="24-hour escort commissioned / allied patrols screen this route / 100 credits transferred to Meridian";economicEvent(e,it->id,message);success=true;}
  }
 }
 refreshSecurity();notify(message);if(success&&turnBased())advanceTime(1,true);return true;
}
}
