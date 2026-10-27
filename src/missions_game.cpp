#include "game.hpp"
namespace hyperion {
void Game::observeMissions(double dt){
 auto& e=s.economy;auto& f=s.flight;
 for(auto& m:e.missions)if(m.state==MissionActive&&s.hours<=m.deadline+1e-8){
  if(m.kind==WreckMission){if(auto* w=findOceanWreck(e,m.target))if((m.trip==1?w->deliveredCrew:w->delivered[m.good])>=m.quantity){m.state=MissionReady;m.result="Rescued crew or recovered supplies received by the real settlement / debrief for reserved payment";}}
  if(m.kind==SurveyMission&&s.scanned.count(m.target)){m.state=MissionReady;m.result="Field evidence catalogued / return with research data";economicEvent(e,m.star,m.title+" / "+m.result);notify(m.result,Notice::Important);}
  if(m.kind!=EscortMission||dt<=0)continue;
  double observed=clamp(s.hours-m.lastObserved,0.,std::min(.1,dt)/55);m.lastObserved=s.hours;
  if(f.deployed||f.docked||f.star!=m.star||observed<=0)continue;
  auto r=std::find_if(e.routes.begin(),e.routes.end(),[&](const auto& r){return r.id==m.target&&r.outbound&&r.trips==m.trip;});if(r==e.routes.end())continue;
  for(const auto& t:f.traffic)if(!t.dead&&t.routeId==m.target&&t.trip==m.trip&&t.positioned&&hyperion::flight::length(t.position-f.ship.position)<650&&f.lineOfSight(f.ship.position,t.position)){
   m.coverage=clamp(m.coverage+observed/std::max(.25,r->duration*(1-m.startProgress)),0.,1.);break;
  }
 }
}
void Game::missionEvent(const hyperion::flight::Event& event){
 for(auto& m:s.economy.missions)if(m.state==MissionActive&&m.kind==InterdictionMission&&m.target==event.victim&&event.kind=="shipLoss"){
  if(event.actor=="NOMA"&&s.hours<=m.deadline+1e-8){m.state=MissionReady;m.result="Named captain defeated / return for debrief";economicEvent(s.economy,m.star,m.title+" / "+m.result);notify(m.result,Notice::Important);}
  else finishMission(s.economy,m,MissionFailed,"Another combatant defeated the named captain / escrow returned");
 }
}
bool Game::missionAction(const std::string& a,const std::string& id){
 if(a!="acceptMission"&&a!="deliverMission"&&a!="debriefMission"&&a!="abortMission"&&a!="plotMission")return false;
 syncEconomy();auto& e=s.economy;auto it=std::find_if(e.missions.begin(),e.missions.end(),[&](const auto& m){return m.id==id;});
 if(it==e.missions.end()){notify("Mission record unavailable");return true;}auto& m=*it;auto* site=findSettlement(e,m.issuer);bool success=false;std::string message;
 if(a=="plotMission"){selectedMinor.clear();
  trackedMission=m.id;
  if(m.kind==WreckMission)if(auto* wreck=findOceanWreck(e,m.target))if(wreck->aboard||stockTotal(wreck->recovered)>0||m.state==MissionReady)return wreckAction("plotWreckHome",m.target);
  if(m.kind>=ReconstructionMission&&m.state!=MissionReady&&m.star==s.currentStar)return recoveryAction("recoveryPlot",m.target);
  if(m.star!=s.currentStar){system(m.star);s.selectedPlanet=m.planet;notify("Mission system plotted / jump before rendezvous");return true;}
  if((m.kind==SurveyMission||m.kind==WreckMission)&&hasExpedition()&&s.activeStar==m.star&&s.activePlanet==m.planet){
   if(expeditionLayer()==Mode::Surface)s.waypoint=m.contact.pos;
   else{if(m.kind==WreckMission)oceanAction("oceanSelect",m.target);else{s.oceanTargetX=m.contact.pos.x;s.targetDepth=std::min(maxDepth(),std::max(20.,m.contact.depth));}}
   notify("Field waypoint / "+m.contact.name+" / scan at the marked position");
  }else{
   ensureFlight();int port=-1;
   if(m.state==MissionReady||m.kind==ReliefMission||(m.kind==WreckMission&&findOceanWreck(e,m.target)&&(findOceanWreck(e,m.target)->aboard||stockTotal(findOceanWreck(e,m.target)->recovered)>0)))port=flightPort(m.issuer,true);
   else if(m.kind==InterdictionMission){auto* c=findCaptain(e,m.target);if(c){s.flight.combatTarget=c->id;s.flight.lock=0;for(int i=0;i<int(s.flight.ports.size());i++)if(s.flight.ports[i].id=="den:"+c->band)port=i;}}
   else if(m.kind==EscortMission){for(const auto& t:s.flight.traffic)if(t.routeId==m.target){s.flight.combatTarget=t.id;s.flight.lock=0;}auto r=std::find_if(e.routes.begin(),e.routes.end(),[&](const auto& r){return r.id==m.target;});if(r!=e.routes.end())port=flightPort(r->from,true);}
   else for(int i=0;i<int(s.flight.ports.size());i++)if(s.flight.ports[i].body==m.planet&&s.flight.ports[i].settlement.empty()&&s.flight.ports[i].id.rfind("relay:",0)==0){port=i;break;}
   if(port>=0&&m.star==s.currentStar)s.flight.selectTarget(port);
   notify(m.kind==SurveyMission?"Survey tracked / fly to the field marker on "+universe.planet(m.star,m.planet).name+" / deploy, then Plot for an expedition waypoint":"Mission tracked / "+missionObjective(e,m));
  }
  return true;
 }
 if(!site||site->owner!=m.faction||site->residents<=0)message="The issuing authority is unavailable";
 else if(m.star!=s.currentStar&&a!="abortMission")message="Return to the mission's star system first";
 else if(s.hours>m.deadline+1e-8)message="Mission deadline has elapsed";
 else if(a=="acceptMission"){
  int active=0;for(const auto& job:e.missions)active+=missionOpen(job);
  if(m.state!=MissionOffered)message="Mission already accepted or resolved";
  else if(active>=6)message="Resolve one of the six active missions before accepting another";
  else if(e.factions[m.faction].warrant>0||e.factions[m.faction].standing<=-45)message="Issuer sanctions or warrants prevent a commission";
  else if(site->treasury<m.reward)message="Issuer can no longer fund this quote";
  else if(m.kind==SurveyMission&&s.scanned.count(m.target))message="This feature is already catalogued / await a new field commission";
  else{
   bool valid=true;
   if(m.kind==EscortMission){auto r=std::find_if(e.routes.begin(),e.routes.end(),[&](const auto& r){return r.id==m.target&&r.outbound&&r.trips==m.trip&&stockTotal(r.cargo)>=5&&r.remaining>=r.duration*.50;});if(r==e.routes.end()){valid=false;message="Quoted shipment already departed too far or resolved / await a new offer";}else{m.initialCargo=stockTotal(r->cargo);m.startProgress=clamp(1-r->remaining/r->duration,0.,1.);}}
   if(m.kind==InterdictionMission){auto* c=findCaptain(e,m.target);if(!c||c->destroyed){valid=false;message="Named captain is no longer operational";}}
   if(valid){site->treasury-=m.reward;m.escrow=m.reward;m.lastObserved=s.hours;m.state=MissionActive;trackedMission=m.id;success=true;message=m.title+" accepted / "+std::to_string(m.reward)+" cr reserved / "+missionObjective(e,m);economicEvent(e,site->name,message);}
  }
 }else if(a=="abortMission"){
  if(!missionOpen(m))message="Only active missions can be abandoned";
  else{finishMission(e,m,MissionFailed,"Commission abandoned / unused escrow returned");success=true;message="Mission abandoned / escrow returned / issuer trust -3";}
 }else if(e.docked!=site->id||!canDock(*site))message="Dock at "+site->name+" to transfer cargo or debrief";
 else if(a=="deliverMission"){
  if(m.kind==EvacuationMission&&m.state==MissionActive)return recoveryAction("recoveryDeliver",m.target);
  if(m.kind==WreckMission&&m.state==MissionActive)return wreckAction("wreckDeliver",m.target);
  if(m.kind!=ReliefMission||m.state!=MissionActive)message="No active relief delivery on this commission";
  else{auto cargo=shipStock();int units=int(std::floor(std::min({double(m.quantity-m.delivered),cargo[m.good],std::max(0.,storageCapacity(*site)-stockTotal(site->stock))})));if(units<=0)message="Required cargo missing or destination warehouse full";
   else{cargo[m.good]-=units;site->stock[m.good]+=units;m.delivered+=units;storeShipStock(cargo);if(m.delivered==m.quantity){m.state=MissionReady;m.result="Relief cargo received by the real warehouse";}success=true;message=std::to_string(units)+" "+goods[m.good].name+" delivered / "+missionObjective(e,m);economicEvent(e,site->name,m.title+" / "+message);}}
 }else if(a=="debriefMission"){
  if(m.state!=MissionReady)message="Complete the physical objective before debriefing";
  else{
   auto cargo=shipStock();if((m.kind==SurveyMission||m.kind==BlackBoxMission)&&(cargo[Research]<m.quantity||stockTotal(site->stock)+m.quantity>storageCapacity(*site)))message="Debrief needs "+std::to_string(m.quantity)+" research data aboard and issuer warehouse space";
   else{if(m.kind==SurveyMission||m.kind==BlackBoxMission){cargo[Research]-=m.quantity;site->stock[Research]+=m.quantity;storeShipStock(cargo);}site->morale=std::min(100.,site->morale+1);auto paid=m.escrow;finishMission(e,m,MissionComplete,"Debrief complete / "+std::to_string(paid)+" cr paid from escrow / standing improved");success=true;message=m.result;}
  }
 }
 if(success&&turnBased())advanceTime(1,true);notify(message.empty()?"Mission order unavailable":message,success?Notice::Important:Notice::Status);return true;
}
}
