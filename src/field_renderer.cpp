#include "renderer.hpp"
#include <sstream>
#include <iomanip>
namespace hyperion {
namespace {
constexpr Color mint{153,255,218,255},pearl{234,244,231,255},muted{132,163,170,255},amber{255,202,139,255};
std::string number(double n){std::ostringstream o;o<<std::fixed<<std::setprecision(1)<<n;return o.str();}
}
void Renderer::nextMoves(float x,float& y,float width){
 if(game->s.mode==Mode::Ocean){oceanOperations(x,y,width);return;}
 const auto& s=game->s;const bool field=s.mode==Mode::Surface||s.mode==Mode::Ocean;
 if(game->lastTurn.valid&&std::abs(game->lastTurn.endHour-s.hours)<1e-7){wrapped(game->lastTurn.summary(),x,y,width,11,mint,true);y+=12;}
 wreckDeliveries(x,y,width,field);
 if(field){
  if(s.battery<12){auto recharge=game->fieldQuote("fieldRecharge");wrapped(recharge.available?recharge.text:recharge.reason,x,y,width,11,muted);y+=8;button(recharge.available?"Recharge / up to "+std::to_string(recharge.move.turns)+" h":"Recharge",{x,y,width,29},"fieldRecharge","",recharge.available&&!ui->paused);y+=40;}
  if(game->surveyRecall.valid&&game->surveyRecall.dive&&s.mode==Mode::Surface&&distance(s.surface,game->surveyRecall.anchor)<=2){wrapped("Your ocean route is remembered. Dive from this site to continue it; the submersible still descends from the surface.",x,y,width,11,mint);y+=8;button("Resume dive route / 1 h",{x,y,width,29},"dive","",!ui->paused);y+=40;}
  auto sites=game->fieldSites();auto focus=std::find_if(sites.begin(),sites.end(),[&](const auto& c){return c.id==game->focusedContact;});
  if(focus==sites.end())focus=std::find_if(sites.begin(),sites.end(),[&](const auto& c){return !s.scanned.count(c.id);});
  if(focus!=sites.end()){
   const auto& c=*focus;fitted(c.name,x,y,width,18,pearl);y+=27;
   auto study=game->fieldQuote("fieldStudy",c.id),collect=game->fieldQuote("fieldCollect",c.id);
   const double distanceTo=s.mode==Mode::Ocean?std::hypot((c.pos.x-s.oceanX)*5,c.depth-s.depth):distance(c.pos,s.surface);
   fitted(c.kind+" / "+number(distanceTo)+" survey units / "+(s.scanned.count(c.id)?"CATALOGUED":"UNSTUDIED"),x,y,width,10,mint,true);y+=25;
   wrapped(study.available?study.text:collect.available?collect.text:study.reason,x,y,width,12,muted);y+=10;
   if(disclosure("Dismantle object","field-dismantle",x,y,width)){auto clear=game->fieldQuote("fieldDismantle",c.id);wrapped(clear.available?clear.text:clear.reason,x,y,width,11,amber);y+=12;button("Dismantle / 1 h",{x,y,width,30},"fieldDismantle",c.id,clear.available&&!ui->paused);y+=39;}
   button("Study / 1 h",{x,y,(width-6)/2,30},"fieldStudy",c.id,study.available&&!ui->paused,true);
   button("Collect / 1 h",{x+(width+6)/2,y,(width-6)/2,30},"fieldCollect",c.id,collect.available&&!ui->paused);y+=38;
   if(!study.available&&!collect.available&&study.reason.find("closer")!=std::string::npos){auto approach=game->fieldQuote("fieldApproach",c.id);button("Plot / free",{x,y,(width-6)/2,29},"fieldFocus",c.id);button("Approach / up to 6 h",{x+(width+6)/2,y,(width-6)/2,29},"fieldApproach",c.id,approach.available&&!ui->paused);y+=39;wrapped(approach.available?approach.text:approach.reason,x,y,width,11,muted);y+=10;}
   else if(s.scanned.count(c.id)&&!collect.available){wrapped(collect.reason,x,y,width,11,muted);y+=10;}
  }else{wrapped("Explore for a new contact. A focused study gives extra data without taking specimens; collection is a separate choice.",x,y,width,12,muted);y+=12;}
  auto plan=game->planMove();
  if(plan.valid){
   row("Waypoint",std::to_string(plan.turns)+" turns at normal pace",x,y,width);
   auto push=game->fieldQuote("fieldPush"),travel=game->fieldQuote("fieldTravel");
   button("Move / Enter",{x,y,(width-6)/2,29},"moveWaypoint","",!ui->paused);
   button("Push / 1 h",{x+(width+6)/2,y,(width-6)/2,29},"fieldPush","",push.available&&!ui->paused);y+=39;
   wrapped(push.available?push.text:push.reason,x,y,width,11,push.available?amber:muted);y+=9;
   button("Follow route / Shift+Enter",{x,y,width,30},"fieldTravel","",travel.available&&!ui->paused);y+=40;
   wrapped(travel.available?travel.text:travel.reason,x,y,width,11,muted);y+=14;
  }
  if(s.hull<99.95){auto repair=game->fieldQuote("fieldRepair");wrapped(repair.available?repair.text:repair.reason,x,y,width,11,muted);y+=8;button("Patch hull / 1 h",{x,y,width,29},"fieldRepair","",repair.available&&!ui->paused);y+=40;}
  if(!sites.empty()&&disclosure("Nearby discoveries / choose a destination","field-sites",x,y,width)){
   int count=0;for(const auto& c:sites)if(!s.scanned.count(c.id)){button(c.kind+" / "+c.name,{x,y,width,28},"fieldFocus",c.id);y+=35;if(++count==6)break;}
  }
 }else if(game->hasExpedition()){
  wrapped("Your expedition is still deployed. Resume at the same location to study, collect, travel or build.",x,y,width,12,mint);y+=12;button("Resume expedition / L",{x,y,width,32},"expedition");y+=44;
 }else if(s.mode==Mode::System&&s.viewStar==s.currentStar){
  auto p=game->planet();wrapped(p.giant?"Survey this atmosphere from orbit, or choose a solid world for a field expedition.":"Land to explore this world's resources and life. Field studies support research; specimens and ore supply the economy.",x,y,width,12,muted);y+=12;
  if(!p.giant){button("Begin expedition / Enter",{x,y,width,32},"land","",!ui->paused,true);y+=42;}
 }
 if(!field&&s.flight.valid&&!s.flight.deployed&&!s.flight.docked){
  wrapped(s.flight.autopilot?"Navigator controls the ship during committed turns.":"Set the ship's response, then resolve an hour. Nearby threats act in the same turn.",x,y,width,12,muted);y+=10;
  const char* labels[]={"Hold","Engage","Evade"};for(int i=0;i<3;i++)button(labels[i],{x+i*(width+6)/3,y,(width-12)/3,29},"combatOrder",std::to_string(i),!s.flight.autopilot,s.flight.combatOrder==i);y+=42;
 }
 if(s.mode==Mode::Surface&&game->diveReport.valid&&game->diveReport.star==s.activeStar&&game->diveReport.planet==s.activePlanet&&disclosure("Last dive / debrief","dive-report",x,y,width))diveReportDetails(x,y,width);
 const Mission* work=nullptr;for(const auto& m:s.economy.missions)if((missionOpen(m)||m.state==MissionOffered)&&m.deadline>s.hours&&m.star==s.currentStar&&(!work||m.id==game->trackedMission||(work->state==MissionOffered&&missionOpen(m))))work=&m;
 if(work){heading("A REASON TO GO",x,y);y+=32;wrapped(work->title+" / "+std::to_string(work->reward)+" cr",x,y,width,14,mint);y+=8;wrapped(missionObjective(s.economy,*work),x,y,width,12,muted);y+=10;
  button(work->state==MissionOffered?"Review commission":"Track objective",{x,y,width,30},work->state==MissionOffered?"missionBoard":"plotMission",work->id);y+=42;
 }
 const Settlement* need=nullptr;for(const auto& site:s.economy.settlements)if(site.owner==0&&site.residents>0&&(site.alerts||site.health<45)&&(!need||site.health<need->health))need=&site;
 if(need){wrapped(need->name+" / "+settlementStatus(*need),x,y,width,13,amber);y+=9;wrapped(need->bottleneck,x,y,width,12,muted);y+=9;button("Inspect the outpost",{x,y,width,30},"reviewSettlement",need->id);y+=42;}
 else if(field){wrapped("A permanent outpost connects local resources to production, trade and the food web. Review the supplies before founding one.",x,y,width,11,muted);y+=10;button("Outpost options / B",{x,y,width,29},"panel","4");y+=40;}
}
}
