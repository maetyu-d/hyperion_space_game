#include "economy.hpp"
#include <sstream>
namespace hyperion {
const char* missionKindName(int k){static const char* names[]={"Relief delivery","Convoy escort","Captain interdiction","Field research","Ocean recovery","Reconstruction","Evacuation","Black-box retrieval"};return k>=0&&k<8?names[k]:"Unknown mission";}
const char* missionStateName(int s){static const char* names[]={"Available","In progress","Return to issuer","Completed","Failed","Expired"};return s>=0&&s<6?names[s]:"Unknown";}
bool missionOpen(const Mission& m){return m.state==MissionActive||m.state==MissionReady;}
void finishMission(Economy& e,Mission& m,int state,const std::string& cause){
 if(m.state>=MissionComplete)return;
 auto* issuer=findSettlement(e,m.issuer);
 if(state==MissionComplete){e.credits+=m.escrow;e.factions[m.faction].standing=std::min(100.,e.factions[m.faction].standing+3.+m.risk*2.);}
 else if(issuer)issuer->treasury+=m.escrow;
 if(state==MissionFailed&&missionOpen(m))e.factions[m.faction].standing=std::max(-100.,e.factions[m.faction].standing-3.);
 m.escrow=0;m.state=state;m.result=cause;
 economicEvent(e,issuer?issuer->name:m.star,m.title+" / "+cause);
}
namespace {
Contact surveyTarget(const Universe& u,const Planet& p,const Settlement& site,uint32_t seed,const std::set<std::string>* known,const WorldDamageMap& damage){
 Contact best;uint32_t score=0;
 int cx=int(std::floor(site.position.x/86)),cy=int(std::floor(site.position.y/86));
 for(int x=-2;x<=2;x++)for(int y=-2;y<=2;y++){
  auto contacts=site.depth>0?u.oceanObjects(p,int(std::floor(site.position.x/130))+x,site.position.y+y*26):u.surfaceObjects(p,cx+x,cy+y);
  for(const auto& c:contacts){if(c.depth>850||(known&&known->count(c.id))||objectIntegrity(damage,"field:"+c.id)<=0)continue;uint32_t rank=u.hash(int32_t(c.n),int32_t(seed),421);if(best.id.empty()||rank>score){best=c;score=rank;}}
 }
 return best;
}
}
void advanceMissions(Economy& e,Universe& u,const std::string& star,const std::set<std::string>* catalogued){
 for(auto& m:e.missions)if(m.state<=MissionReady){
  auto* issuer=findSettlement(e,m.issuer);
  if(!issuer||issuer->owner!=m.faction||issuer->residents==0){finishMission(e,m,m.state==MissionOffered?MissionExpired:MissionFailed,"Issuing settlement lost its charter or population / unused escrow returned");continue;}
  if(e.hour>m.deadline+1e-8){finishMission(e,m,m.state==MissionOffered?MissionExpired:MissionFailed,"Deadline elapsed / unused escrow returned");continue;}
  if(m.kind==SurveyMission&&m.state<=MissionActive&&objectIntegrity(e.worldDamage,"field:"+m.target)<=0){finishMission(e,m,m.state==MissionOffered?MissionExpired:MissionFailed,"Survey target destroyed / unused escrow returned");continue;}
  if(m.state==MissionOffered&&m.kind==SurveyMission&&catalogued&&catalogued->count(m.target)){finishMission(e,m,MissionExpired,"Field evidence already catalogued / new proposals follow the next shift");continue;}
  if(m.state==MissionOffered&&m.kind==EscortMission){auto r=std::find_if(e.routes.begin(),e.routes.end(),[&](const auto& r){return r.id==m.target;});if(r==e.routes.end()||!r->outbound||r->trips!=m.trip||r->remaining<r->duration*.5){finishMission(e,m,MissionExpired,"Quoted convoy departed or resolved before acceptance");continue;}}
  if(m.kind==InterdictionMission&&m.state<=MissionActive){auto* c=findCaptain(e,m.target);if(!c||c->destroyed){finishMission(e,m,m.state==MissionOffered?MissionExpired:MissionFailed,"Named captain lost before NOMA completed the objective / escrow returned");continue;}}
  if(m.state==MissionOffered&&m.kind==ReliefMission&&issuer->stock[m.good]>=reserveFor(*issuer,m.good)*.98){finishMission(e,m,MissionExpired,"Local shortage resolved by production or other trade");continue;}
  if(m.kind==WreckMission){auto* w=findOceanWreck(e,m.target);if(!w){finishMission(e,m,MissionFailed,"Wreck record unavailable");continue;}if(m.trip==0&&w->remaining[m.good]+w->recovered[m.good]+w->delivered[m.good]<m.quantity){finishMission(e,m,m.state==MissionOffered?MissionExpired:MissionFailed,"Commissioned wreck supplies destroyed / unused escrow returned");continue;}if(m.trip==1&&w->crew+w->aboard+w->deliveredCrew<m.quantity){finishMission(e,m,m.state==MissionOffered?MissionExpired:MissionFailed,"Stranded crew could not survive until rescue");continue;}}
  if(m.state==MissionActive&&m.kind==EscortMission){
   auto r=std::find_if(e.routes.begin(),e.routes.end(),[&](const auto& r){return r.id==m.target;});
   if(r==e.routes.end()){finishMission(e,m,MissionFailed,"Freight route retired / escrow returned");continue;}
   if(r->trips>m.trip){
    if(m.coverage+.0001>=.45){m.state=MissionReady;m.result="Convoy arrived / physical escort verified / return for debrief";economicEvent(e,issuer->name,m.title+" / "+m.result);}
    else finishMission(e,m,MissionFailed,"Shipment arrived without sufficient close escort / at least 45% of the commissioned leg is required");
   }else if(!r->outbound||stockTotal(r->cargo)<m.initialCargo*.65)finishMission(e,m,MissionFailed,"Commissioned cargo lost or more than 35% raided / escrow returned");
  }
 }
 e.missions.erase(std::remove_if(e.missions.begin(),e.missions.end(),[&](const auto& m){return m.state>=MissionComplete&&m.deadline<e.hour-72;}),e.missions.end());
 if(star.empty()||!e.systems.count(star)||e.missions.size()>=256)return;
 auto clock=e.missionClock.find(star);if(clock!=e.missionClock.end()&&e.hour<clock->second-1e-8)return;
 e.missionClock[star]=(std::floor(e.hour/6.)+1)*6.;
 e.missions.erase(std::remove_if(e.missions.begin(),e.missions.end(),[&](const auto& m){return m.state>=MissionComplete&&m.deadline<e.hour-72;}),e.missions.end());
 for(const auto& site:e.settlements)if(site.star==star&&site.owner>0&&site.residents>0&&site.treasury>=180){
  bool busy=false;for(const auto& old:e.missions)if(old.issuer==site.id&&old.state<=MissionReady)busy=true;if(busy)continue;
  uint32_t seed=u.hash(int32_t(stringHash(site.id)),int32_t(uint64_t(std::floor(e.hour/6.))&0xffffffffULL),420);Random rng{seed};
  auto world=u.planet(star,site.planet);auto weather=conditions(u,world,site,e.hour);
  std::vector<std::pair<Mission,double>> candidates;
  Mission base;base.issuer=site.id;base.star=star;base.planet=site.planet;base.faction=site.owner;base.seed=seed;base.created=e.hour;
  double deficit=0;int good=Rations;
  for(int g=0;g<goodCount;g++){double need=reserveFor(site,g),lack=(need-site.stock[g])/std::max(1.,need);if(lack>deficit){deficit=lack;good=g;}}
  if(deficit>.12){auto m=base;m.kind=ReliefMission;m.good=good;m.quantity=clamp(int(std::ceil(reserveFor(site,good)-site.stock[good])),5,40);m.risk=clamp(deficit*.5+weather.storm*.25,0.,1.);m.cause=site.name+" has "+std::to_string(int(site.stock[good]))+" "+goods[good].name+" against a "+std::to_string(int(std::ceil(reserveFor(site,good))))+"-unit reserve. "+site.bottleneck;m.target=site.id;candidates.push_back({m,.3+deficit*1.6});}
  for(const auto& r:e.routes)if(r.outbound&&stockTotal(r.cargo)>=5&&r.remaining>=r.duration*.50){
   auto* from=findSettlement(e,r.from);auto* to=findSettlement(e,r.to);if(!from||!to||from->star!=star||to->star!=star||from->owner!=site.owner)continue;
   auto m=base;m.kind=EscortMission;m.target=r.id;m.trip=r.trips;m.initialCargo=stockTotal(r.cargo);m.startProgress=1-r.remaining/r.duration;m.quantity=int(std::ceil(m.initialCargo));m.good=r.good;
   for(const auto& band:e.pirates)if(band.star==star)m.risk=clamp(.25+band.raids*.06+band.grievance*.004,0.,1.);
   m.cause=from->name+" is shipping "+std::to_string(m.quantity)+" "+goods[r.good].name+" to "+to->name+". "+(m.risk>0?"A local pirate organization threatens the route.":"The unescorted shipment needs a close screen.");candidates.push_back({m,.5+m.risk});break;
  }
  std::vector<const Captain*> pirates;for(const auto& c:e.captains)if(c.star==star&&!c.band.empty()&&!c.destroyed)pirates.push_back(&c);
  if(!pirates.empty()){const auto& captain=*pirates[seed%pirates.size()];auto* band=findPirates(e,captain.band);auto m=base;m.kind=InterdictionMission;m.target=captain.id;m.risk=clamp(.35+captain.role*.12+(band?band->raids*.04:0),0.,1.);m.cause=(band?band->name:"Local raiders")+" operates from "+(band?band->haven:captain.home)+". Interdict "+captain.name+" / "+hyperion::flight::vesselDesignation(captain.genome)+". "+std::to_string(band?band->raids:0)+" recorded raids.";candidates.push_back({m,.35+m.risk*.6});}
  auto contact=surveyTarget(u,world,site,seed,catalogued,e.worldDamage);
  if(!contact.id.empty()){
   bool used=false;for(const auto& old:e.missions)if(old.kind==SurveyMission&&old.target==contact.id)used=true;
   if(!used){auto m=base;m.kind=SurveyMission;m.target=contact.id;m.contact=contact;m.quantity=4+int(seed%5);m.good=Research;m.risk=clamp(weather.storm*.35+world.hazards*.35+contact.depth/3000.,0.,1.);auto region=u.region(world,contact.pos,contact.depth);m.cause=site.name+" requests field evidence from "+contact.name+" in "+region.name+" on "+world.name+". "+contact.desc;m.cause=m.cause.substr(0,1800);candidates.push_back({m,.7+(world.anomaly?.5:0)+world.life*.3});}
  }
  if(candidates.empty())continue;double total=0;for(const auto& c:candidates)total+=c.second;double choice=rng()*total;Mission selected;
  for(const auto& c:candidates){selected=c.first;choice-=c.second;if(choice<=0)break;}
  selected.id="OP:"+site.id+":"+std::to_string(int64_t(std::floor(e.hour/6.)))+":"+std::to_string(seed);
  static const char* calls[]={"Prismatic","Tidal","Veil","Echo","Saffron","Opaline","Lattice","Violet"};
  selected.title=std::string(calls[(seed>>8)%8])+" "+missionKindName(selected.kind)+" / "+world.name+" "+std::to_string(10+seed%90);
  selected.reward=std::min(int64_t(180+selected.risk*420+selected.quantity*(selected.kind==ReliefMission?goods[selected.good].value*1.45:6)),site.treasury/2);
  selected.deadline=e.hour+24+int((seed>>16)%5)*6;if(selected.kind==EscortMission){auto r=std::find_if(e.routes.begin(),e.routes.end(),[&](const auto& r){return r.id==selected.target;});selected.deadline=e.hour+r->remaining+18;}
  if(selected.reward>=90)e.missions.push_back(std::move(selected));if(e.missions.size()>=256)break;
 }
}
std::string missionObjective(const Economy& e,const Mission& m){
 if(m.state==MissionReady)return "Return to "+(findSettlement(e,m.issuer)?findSettlement(e,m.issuer)->name:m.issuer)+" and debrief / "+std::to_string(m.reward)+" cr reserved";
 if(m.state>=MissionComplete)return m.result;
 if(m.kind==WreckMission){auto* w=findOceanWreck(e,m.target);auto* home=findSettlement(e,m.issuer);return m.trip==1?"Rescue "+std::to_string(m.quantity)+" crew / "+std::to_string(w?w->aboard:0)+" aboard / "+std::to_string(w?w->deliveredCrew:0)+" delivered / unload at "+(home?home->name:m.issuer):"Recover and deliver "+std::to_string(m.quantity)+" "+goods[m.good].name+" from "+m.contact.name;}

 if(m.kind>=ReconstructionMission){auto it=e.worldDamage.find(m.target);if(it==e.worldDamage.end())return "Recovery site resolved / await commission status update";const auto& d=it->second;const auto& r=d.recovery;
  if(m.kind==EvacuationMission)return "Rescue "+std::to_string(m.quantity)+" survivors / "+std::to_string(r.aboard)+" aboard / "+std::to_string(r.delivered)+" delivered / life support until hour "+std::to_string(int(r.supportUntil));
  if(m.kind==BlackBoxMission)return "Stabilize, cut open and recover the recorder / return one research datum";
  Stock cost{};if(d.kind==FacilityObject)cost=buildings[d.slot/24].construction;else{cost[Alloy]=d.kind==PortObject?32:8;cost[Components]=d.kind==PortObject?12:3;}std::string text="Rebuild original infrastructure / ";for(int i=0;i<goodCount;i++)if(cost[i]>0)text+=std::to_string(int(cost[i]))+" "+goods[i].name+" / ";return text+"stabilize and evacuate first";
 }
 if(m.kind==ReliefMission)return "Deliver "+std::to_string(m.quantity-m.delivered)+" "+goods[m.good].name+" to the issuer's warehouse / "+std::to_string(m.delivered)+" delivered";
 if(m.kind==EscortMission)return "Rendezvous with "+m.target+" / stay within 650 km for 45% of its commissioned leg / "+std::to_string(int(m.coverage*100))+"% verified";
 if(m.kind==InterdictionMission){auto* c=findCaptain(const_cast<Economy&>(e),m.target);return "Defeat "+(c?c->name:m.target)+" yourself / existing faction consequences apply";}
 return "Catalogue "+m.contact.name+" at "+std::to_string(int(m.contact.pos.x))+" : "+std::to_string(int(m.contact.pos.y))+(m.contact.depth>0?" / "+std::to_string(int(m.contact.depth))+" m":"")+" / return "+std::to_string(m.quantity)+" research data";
}
}
