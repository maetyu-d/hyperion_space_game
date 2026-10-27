#include "game.hpp"
#include <sstream>
#include <iomanip>
namespace hyperion {
namespace {std::string number(double value){std::ostringstream out;out<<std::fixed<<std::setprecision(1)<<value;return out.str();}}
TurnOutlook Game::outlook(double hours)const {
 TurnOutlook out;if(!std::isfinite(hours))hours=1;out.hours=clamp(hours,0.,24.);out.battery=clamp(s.battery+out.hours*(4+s.upgrades[3]*1.2),0.,100.);
 auto add=[&](Notice priority,const std::string& text,const std::string& site=std::string{}){out.items.push_back({priority,text,site});if(priority!=Notice::Status)++out.warnings;};
 if(hasExpedition()&&expeditionLayer()==Mode::Ocean&&s.depth>maxDepth()){
  out.pressureLoss=(s.depth-maxDepth())/100*22*out.hours;
  add(Notice::Critical,"Pressure exposure: "+number(out.pressureLoss)+" hull over "+number(out.hours)+" h at this depth"+(out.pressureLoss>=s.hull?" / shell breach expected":""));
 }
 const auto& f=s.flight;
 if(f.valid&&!f.deployed&&!f.docked){int enemies=0;for(const auto& ship:f.traffic)if(!ship.dead&&ship.kind==1&&ship.hostile&&hyperion::flight::length(ship.position-f.ship.position)<=std::max(300.,hyperion::flight::enemyDef(ship).range*1.5))++enemies;
  if(enemies)add(Notice::Important,std::to_string(enemies)+" armed hostile contact(s) nearby / combat advances with the order");
  if(f.autopilot)add(Notice::Status,"Navigator continues toward the selected berth during the order");
 }
 for(const auto& [id,d]:s.economy.worldDamage)if(d.recovery.valid&&(d.star==s.currentStar||d.recovery.faction==0)){const auto& r=d.recovery;if(r.crew>0&&r.supportUntil<=s.hours+out.hours)add(Notice::Critical,"Wreck life support expires in "+number(std::max(0.,r.supportUntil-s.hours))+" h / "+std::to_string(r.crew)+" survivors",d.owner);if(!r.failed&&!r.stabilized&&r.failureAt>s.hours&&r.failureAt<=s.hours+out.hours)add(Notice::Important,"Unstable wreck / secondary failure during this order",d.owner);}
 if(f.courier.active&&f.courier.deadline<=s.hours+out.hours+1e-8)add(Notice::Important,"Courier deadline in "+number(std::max(0.,f.courier.deadline-s.hours))+" h / reserved payment at risk",f.courier.destination);
 for(const auto& contract:s.economy.contracts)if(contract.accepted&&!contract.closed&&contract.deadline<=s.hours+out.hours+1e-8){auto* site=findSettlement(s.economy,contract.settlement);add(Notice::Important,"Delivery deadline: "+(site?site->name:contract.settlement)+" / "+number(std::max(0.,contract.deadline-s.hours))+" h",contract.settlement);}
 for(const auto& m:s.economy.missions)if(missionOpen(m)){
  if(m.state==MissionReady)add(Notice::Important,m.title+": objective complete / debrief before "+number(std::max(0.,m.deadline-s.hours))+" h",m.issuer);
  else if(m.deadline<=s.hours+std::max(6.,out.hours))add(Notice::Important,m.title+": deadline in "+number(std::max(0.,m.deadline-s.hours))+" h / "+std::to_string(m.escrow)+" cr reserved",m.issuer);
  if(m.kind==EscortMission&&m.state==MissionActive){auto r=std::find_if(s.economy.routes.begin(),s.economy.routes.end(),[&](const auto& r){return r.id==m.target;});if(r!=s.economy.routes.end()&&r->remaining<=out.hours&&m.coverage<.45)add(Notice::Important,"Commissioned convoy arrives within this order / close escort still required / "+number(m.coverage*100)+"% verified",m.issuer);}
 }
 for(const auto& site:s.economy.settlements)if(site.owner==0&&site.residents>0){
  auto world=const_cast<Universe&>(universe).planet(site.star,site.planet);auto now=conditions(universe,world,site,s.hours),next=conditions(universe,world,site,s.hours+out.hours);
  if(next.storm>.6||std::abs(next.storm-now.storm)>.2)add(next.storm>.8?Notice::Important:Notice::Status,site.name+": storm "+number(now.storm*100)+" -> "+number(next.storm*100)+"% / daylight and production affected",site.id);
  double buffer=std::max(0.,site.stock[Rations])/(site.residents*.035);
  if(buffer<=std::max(6.,out.hours)||site.alerts&1)add(buffer<=out.hours||site.alerts&1?Notice::Important:Notice::Status,site.name+": food buffer "+number(buffer)+" h before production or freight",site.id);
  if(site.health<45||site.alerts&4)add(Notice::Important,site.name+": medical emergency / health "+number(site.health)+"%",site.id);
  if(site.morale<25||site.alerts&8)add(Notice::Important,site.name+": work stoppage / morale "+number(site.morale)+"%",site.id);
  if(site.alerts&16)add(Notice::Important,site.name+": ecological damage threatens harvests and medicine",site.id);
  if(site.alerts&2)add(Notice::Status,site.name+": power deficit throttles production",site.id);
 }
 for(const auto& route:s.economy.routes)if(route.owner==0&&route.outbound&&route.remaining<=out.hours){auto* to=findSettlement(s.economy,route.to);add(Notice::Status,"Freight scheduled for "+(to?to->name:route.to)+" / "+number(std::max(0.,route.remaining))+" h",route.to);}
 std::stable_sort(out.items.begin(),out.items.end(),[](const auto& a,const auto& b){return a.priority>b.priority;});return out;
}
std::vector<TurnEvent> Game::upcoming(double horizon)const {
 std::vector<TurnEvent> events;if(!std::isfinite(horizon))return events;horizon=clamp(horizon,0.,24.);
 auto add=[&](double eta,Notice priority,const std::string& text,const std::string& site=std::string{},bool estimate=false){if(!std::isfinite(eta)||eta>horizon+1e-8)return;events.push_back({std::max(0.,eta),priority,estimate,text,site});};
 for(const auto& [id,d]:s.economy.worldDamage)if(d.recovery.valid&&(d.star==s.currentStar||d.recovery.faction==0)){const auto& r=d.recovery;if(r.crew>0)add(r.supportUntil-s.hours,Notice::Critical,"Wreck life support / "+std::to_string(r.crew)+" survivors",d.owner);if(!r.failed&&!r.stabilized&&r.failureAt>s.hours)add(r.failureAt-s.hours,Notice::Important,"Wreck secondary failure",d.owner);}
 if(s.flight.courier.active)add(s.flight.courier.deadline-s.hours,Notice::Important,"Courier deadline",s.flight.courier.destination);
 for(const auto& c:s.economy.contracts)if(c.accepted&&!c.closed){auto* site=findSettlement(s.economy,c.settlement);add(c.deadline-s.hours,Notice::Important,"Delivery deadline / "+(site?site->name:c.settlement),c.settlement);}
 for(const auto& m:s.economy.missions)if(missionOpen(m))add(m.deadline-s.hours,Notice::Important,"Mission deadline / "+m.title,m.issuer);
 for(const auto& r:s.economy.routes)if(r.outbound&&r.remaining>1e-8){auto* to=findSettlement(s.economy,r.to);if(to&&(r.owner==0||to->owner==0))add(std::max(0.,r.remaining-std::max(0.,s.hours-s.economy.hour)),Notice::Status,"Freight / "+std::string(goods[r.good].name)+" to "+to->name,to->id);}
 const auto& e=s.economy;
 if(e.research>=0&&e.research<techCount){double labs=0;for(const auto& site:e.settlements)if(site.owner==0&&site.residents&&site.stock[Rations]>0)labs+=site.researchRate;
  const double rate=.35+std::min(3.,labs*.4);add(std::max(0.,technologies[e.research].hours-e.researchProgress)/rate,Notice::Status,"Research / "+std::string(technologies[e.research].name),{},true);
 }
 std::stable_sort(events.begin(),events.end(),[](const auto& a,const auto& b){return a.hours==b.hours?a.priority>b.priority:a.hours<b.hours;});return events;
}
void Game::waitPlanned(const std::string& duration){
 if(!turnBased()){notify("Open strategy before planning a wait");return;}
 if(duration=="next"){waitHours(24);return;}
 for(int hours:{1,3,6,12,24})if(duration==std::to_string(hours)){waitHours(hours);return;}
 notify("Choose a wait of 1, 3, 6, 12 or 24 hours");
}
std::string Game::orderHint(const std::string& action,const std::string& arg){
 if(action.rfind("recovery",0)==0){auto q=recoveryQuote(action,arg.empty()?recoveryTarget:arg);return q.available?q.text:q.reason;}
 if(action=="wreckRescue"||action=="wreckSalvage"||action=="wreckCut"||action=="wreckDeliver"){auto quote=wreckQuote(action,arg);return quote.available?quote.text:quote.reason;}
 if(action=="oceanRoute")return "Free route planning / estimates include headwind power and vent exposure; work costs extra";

 if(action=="oceanListen")return "Quiet listening / 2 battery / 1 h / short-range life, vents and signals; no mineral returns or new alarm";
 if(action=="oceanNext")return "Free next resolved contact / keeps samples optional / K returns home";
 if(action=="oceanProceed"){auto q=nextDiveOrder();return q.text;}
 if(action=="oceanHomeStep")return "Return toward the launch buoy / one normal movement step / 1 h";
 if(action=="oceanSelect")return "Free dive objective / plot a real generated site / world stays frozen";
 if(action=="oceanAdvance")return "Advance the plotted dive course / one movement turn / 1 h";
 if(action=="oceanAscend")return "Ascend one movement step / 1 h / physical depth and pressure resolve normally";
 if(action=="oceanReturn")return "Return toward the launch buoy / up to 6 h / stops for events, danger and deadlines";
 if(action=="strategy"||action=="cockpit"||action=="deploy"||action=="dive"||action=="surface"||action=="launch"){auto transfer=transferPlan(action);return transfer.available?transfer.text:transfer.reason;}
 if(action=="system"||action=="galaxy"||action=="expedition")return "Open planning / freeze the world / switching costs no time";
 if(!turnBased())return {};
 if(action.rfind("field",0)==0&&action!="fieldFocus"){auto quote=fieldQuote(action,arg);return quote.available?quote.text:quote.reason;}
 if(action=="waitTurns")return arg=="next"?"Wait up to 24 h / stop after the next event or emergency / full hours only":"Wait up to "+arg+" h / stop for freight, research, docking, threats or crises / planned waypoints stay set";
 if(action=="orders")return "Free next moves / field choices, objectives and forecasts / world stays frozen";
 if(action=="endTurn"||action=="wait"){
  auto forecast=outlook(action=="wait"?6:1);std::string text=action=="wait"?"Wait up to 6 h / stops for crises or resolved missions":"Wait 1 h / outposts, traffic and deadlines advance";
  if(forecast.warnings)text+=" / "+forecast.items.front().text;else if(forecast.battery>s.battery+.05)text+=" / battery "+number(s.battery)+" -> "+number(forecast.battery)+"%";return text;
 }
 if(action=="plotMission")return "Free mission tracking / deadlines and the world remain frozen";
 if(action=="acceptMission")return "Accept: 1 h / issuer funds escrow / objective remains physical";
 if(action=="deliverMission"||action=="debriefMission")return "Successful mission transfer: 1 h / dock at the issuing settlement";
 if(action=="abortMission")return "Abandon: 1 h / return unused escrow / issuer standing -3";
 if(action=="orbitScan")return hasExpedition()?"Return to orbit before orbital spectrometry":"Orbital scan: 1 h / traffic, outposts and deadlines advance";
 if(action=="dive"||action=="surface"||action=="launch")return "Successful expedition transfer: 1 h / the world resolves with the order";
 if(action=="scan"&&s.mode==Mode::Ocean)return s.battery<12?"Sonar requires 12 battery / move normally or recharge":"Sonar: 12 battery / 1 h / battery "+number(s.battery)+" -> "+number(clamp(s.battery-12+4+s.upgrades[3]*1.2,0.,100.))+"% after recovery / reveals signatures; Study and Collect remain separate";
 if(action=="scan")return s.battery<12?"Scanner requires 12 battery / rejected scan is free":"Scan: 1 h / battery "+number(s.battery)+" -> "+number(clamp(s.battery-12+4+s.upgrades[3]*1.2,0.,100.))+"% after recovery";
 if(action=="moveWaypoint"){auto plan=planMove();if(!plan.valid)return "No movement planned / choose a waypoint first";return "One step: 1 h / "+std::to_string(plan.turns)+" estimated steps remain"+(plan.impact?" / seafloor contact":plan.depth>maxDepth()&&s.mode==Mode::Ocean?" / pressure exposure":"");}
 return {};
}
}
