#include "renderer.hpp"
#include "flight/weather.hpp"
#include <cstdio>
#include <numeric>
namespace hyperion {
namespace {
constexpr Color pearl{234,244,231,255},mint{153,255,218,255},muted{132,163,170,255},pink{255,113,184,255};
std::string n(double value,int places=0){char out[80];std::snprintf(out,sizeof(out),"%.*f",places,value);return out;}
// Quiet glass with fine spectral inlays; colour never replaces a warning or a reading.
void hudCard(Rectangle box,Color accent){
 DrawRectangleRounded(box,.07f,4,Color{6,16,25,218});
 DrawRectangleRoundedLinesEx(box,.07f,4,1,Color{81,126,143,38});
 const int length=int(std::min(72.f,box.width*.38f)),half=length/2;
 DrawRectangleGradientH(int(box.x+9),int(box.y),half,1,Color{accent.r,accent.g,accent.b,175},Color{138,207,255,110});
 DrawRectangleGradientH(int(box.x+9)+half,int(box.y),length-half,1,Color{138,207,255,110},Color{213,146,255,70});
 DrawRectangleRec({box.x,box.y+8,1,std::min(14.f,box.height-14)},Color{accent.r,accent.g,accent.b,105});
 DrawRectangleRec({box.x+box.width-25,box.y+box.height-1,16,1},Color{197,148,255,45});
}
// The lower instruments share a quiet backing instead of separate framed cards.
void hudRail(Rectangle box,Color accent){
 DrawRectangleGradientV(int(box.x),int(box.y),int(box.width),int(box.height),Color{6,16,25,100},Color{6,16,25,195});
 DrawRectangleGradientH(int(box.x+12),int(box.y),42,1,Color{accent.r,accent.g,accent.b,100},Color{202,166,255,35});
}
void hudBar(Rectangle box,double value,Color tint){
 DrawRectangleRec(box,Color{43,65,78,175});
 DrawRectangleRec({box.x,box.y,box.width*float(clamp(value,0.,1.)),box.height},tint);
 const int segments=box.width>=70?5:3;
 for(int i=1;i<segments;i++)DrawRectangleRec({box.x+box.width*i/segments,box.y,1,box.height},Color{6,16,25,235});
}
void hudDiamond(Vector2 at,float radius,Color tint){
 const Vector2 points[]={{at.x,at.y-radius},{at.x+radius,at.y},{at.x,at.y+radius},{at.x-radius,at.y}};
 for(int i=0;i<4;i++){DrawLineEx(points[i],points[(i+1)%4],3,Color{4,12,20,145});DrawLineEx(points[i],points[(i+1)%4],1,tint);}
}

}
void Renderer::boardingPanel(float x,float& y,float width){
 auto& e=game->s.economy;auto& f=game->s.flight;int target=f.combatTargetIndex();std::string id=target>=0?f.traffic[target].id:"";for(const auto& [key,b]:e.boardings)if(b.status==0)id=key;
 const Boarding* party=nullptr;auto found=e.boardings.find(id);if(found!=e.boardings.end())party=&found->second;
 bool disabled=target>=0&&f.traffic[target].kind==1&&f.traffic[target].modules[0]<=2&&!f.traffic[target].dead;
 bool prisoners=false;for(const auto& [key,b]:e.boardings)prisoners|=b.status==2&&b.defenders>0;
 if(!disabled&&(!party||party->status!=0)&&!prisoners)return;
 if(!disclosure("Boarding and captured crews","boarding",x,y,width))return;
 if(!game->turnBased()){button("Plan boarding / pause",{x,y,width,30},"boardingPlan",id);y+=40;return;}
 auto order=[&](const std::string& label,const std::string& action,const std::string& key){auto q=game->boardingQuote(action,key);button(label+" / 1 h",{x,y,width,29},action,key,q.available);y+=37;wrapped(q.available?q.text:q.reason,x,y,width,11,muted);y+=11;};
 for(const auto& [key,b]:e.boardings)if(b.status==2&&b.defenders>0){wrapped(std::to_string(b.defenders)+" captured crew aboard / shared rescue berths",x,y,width,12,mint);y+=11;order("Disembark captives","boardingRelease",key);}
 if(party&&party->status==0){const auto& b=*party;row("Party / defenders",std::to_string(b.party)+" / "+std::to_string(b.defenders),x,y,width);wrapped(b.surrendered?"Defenders surrendered / all compartments secured":"Isolate engineering, press the bridge, or seize the cargo bay. Every order resolves the world.",x,y,width,11,b.surrendered?mint:muted);y+=12;
  const char* labels[]={"Power","Bridge","Cargo"};const char* actions[]={"boardingPower","boardingBridge","boardingCargo"};for(int i=0;i<3;i++){auto q=game->boardingQuote(actions[i],id);button(std::string(labels[i])+((b.rooms&(1<<i))?" / secure":""),{x+i*(width+6)/3,y,(width-12)/3,28},actions[i],id,q.available);}y+=38;
  std::string next=(b.rooms&3)==3||b.surrendered?"boardingCapture":game->boardingQuote("boardingNegotiate",id).available?"boardingNegotiate":"boardingPower";order(next=="boardingCapture"?"Capture hull":next=="boardingPower"?"Isolate engineering":"Negotiate surrender",next,id);
  if(disclosure("Other boarding orders","boarding-orders",x,y,width)){order("Transfer finite cargo","boardingLoot",id);order("Withdraw surviving party","boardingWithdraw",id);}
 }else if(disabled)order("Board disabled hull / 3 volunteers","boardingBegin",id);
}
void Renderer::recoveryPanel(float x,float& y,float width){
 boardingPanel(x,y,width);
 auto& e=game->s.economy;auto& f=game->s.flight;int responding=0;for(const auto& c:e.recoveryCrews)if(c.star==game->s.currentStar&&c.phase<3)responding++;
 if(responding&&disclosure("Recovery vessels / "+std::to_string(responding),"recovery-vessels",x,y,width))for(const auto& c:e.recoveryCrews)if(c.star==game->s.currentStar&&c.phase<3){wrapped(c.name+" / "+e.factions[c.faction].name,x,y,width,12,mint);y+=9;wrapped(std::string(c.phase==0?"Inbound":c.phase==1?"Working wreck":"Returning")+" / "+n(hyperion::flight::length(c.position-f.ship.position))+" km / "+std::to_string(c.rescued)+" rescued / "+n(stockTotal(c.cargo))+" supplies",x,y,width,11,muted);y+=10;button("Track responder",{x,y,width,27},"inspectContact",c.id);y+=35;}
 std::vector<std::pair<double,std::string>> sites;for(const auto& [id,d]:e.worldDamage)if(d.recovery.valid&&(d.star==game->s.currentStar||d.recovery.aboard>0)){double distance=hyperion::flight::length(d.position-f.ship.position);if(distance<2500||d.recovery.aboard>0||game->recoveryTarget==id)sites.push_back({distance,id});}if(sites.empty())return;std::sort(sites.begin(),sites.end());
 if(!disclosure("Recovery / "+std::to_string(sites.size())+" sites","recovery",x,y,width))return;
 auto name=[&](const WorldDamage& d){auto* site=findSettlement(e,d.owner);return std::string(d.kind==FacilityObject?buildings[d.slot/24].name:d.recovery.shipModel>=0?"Ship wreck":"Orbital equipment")+" / "+(site?site->name:d.star);};
 auto selected=e.worldDamage.find(game->recoveryTarget);if(selected==e.worldDamage.end()||!selected->second.recovery.valid){game->recoveryTarget=sites.front().second;selected=e.worldDamage.find(game->recoveryTarget);}auto& d=selected->second;auto& r=d.recovery;auto id=selected->first;
 if(disclosure(name(d),"recovery-sites",x,y,width)){for(size_t i=0;i<std::min(size_t(6),sites.size());i++){button(name(e.worldDamage.at(sites[i].second)),{x,y,width,28},"recoverySelect",sites[i].second);y+=35;}}
 wrapped(n(hyperion::flight::length(d.position-f.ship.position))+" km / "+(recoveryHazard(r,game->s.hours)>.02?"Live fuel and circuits":"Safe containment")+" / "+std::to_string(r.crew)+" stranded / "+std::to_string(r.aboard)+" aboard",x,y,width,11,recoveryHazard(r,game->s.hours)>.02?pink:mint);y+=12;
 if(r.claimRules){wrapped(r.claim<0?"Unclaimed salvage":r.claim==0?"NOMA salvage claim":e.factions[r.claim].name+std::string(r.permit?" / licensed":r.contested?" / contested title":" / bonded salvage"),x,y,width,11,r.contested?pink:muted);y+=11;}
 if(!game->turnBased()){button("Plan recovery / pause",{x,y,width,30},"recoveryPlan",id);y+=40;return;}
 std::string next=recoveryHazard(r,game->s.hours)>.02?"recoveryStabilize":r.crew>0?"recoveryRescue":r.aboard>0?"recoveryDeliver":r.intact?(f.docked||r.towed?"recoveryPrize":"recoveryTow"):!r.cut?"recoveryCut":r.box&&!r.boxRecovered?"recoveryBox":"salvage";
 if(!game->salvageAuthorized(id)&&next!="recoveryStabilize"&&next!="recoveryRescue"&&next!="recoveryDeliver")next="recoveryPermit";
 auto label=[&](const std::string& a)->std::string{return a=="recoveryPermit"?"Buy salvage licence":a=="recoveryContest"?"Defy faction salvage claim":a=="recoveryClaim"?"Register NOMA claim":a=="recoveryPrize"?"Deliver captured hull": a=="recoveryStabilize"?"Stabilize hazards":a=="recoveryRescue"?"Rescue survivors":a=="recoveryDeliver"?"Deliver evacuees":a=="recoveryCut"?"Cut equipment free":a=="recoveryBox"?"Recover black box":a=="recoveryTow"?"Attach tow":a=="recoveryDetach"?"Release tow":a=="recoveryRebuild"?"Reconstruct original site":"Collect released salvage";};
 if(next=="salvage"){bool available=stockTotal(d.salvage)>0&&f.speed()<=15&&!f.docked&&hyperion::flight::length(d.position-f.ship.position)<55;button(label(next),{x,y,width,30},next,"",available);y+=39;}
 else{auto quote=game->recoveryQuote(next,id);button(label(next)+" / 1 h",{x,y,width,30},next,id,quote.available);y+=38;wrapped(quote.available?quote.text:quote.reason,x,y,width,11,muted);y+=13;}
 button("Plot approach",{x,y,width,28},"recoveryPlot",id,game->recoveryQuote("recoveryPlot",id).available);y+=36;
 if(disclosure("Other recovery orders","recovery-orders",x,y,width))for(const char* a:{"recoveryStabilize","recoveryRescue","recoveryCut","recoveryBox","recoveryDeliver","recoveryTow","recoveryDetach","recoveryRebuild","recoveryPermit","recoveryContest","recoveryClaim"}){if(next==a)continue;auto quote=game->recoveryQuote(a,id);button(label(a)+" / 1 h",{x,y,width,27},a,id,quote.available);y+=34;if(quote.available){wrapped(quote.text,x,y,width,10,muted);y+=8;}}
}
void Renderer::combatPanel(float x,float& y,float width){
 recoveryPanel(x,y,width);
 auto& f=game->s.flight;auto& c=f.ship.combat;int selected=f.combatTargetIndex();
 if(!disclosure("Combat / "+std::string(hyperion::flight::weaponDef(c.weapon).name),"combat",x,y,width))return;
 if(game->turnBased()){wrapped("Orders resolve on Wait 1h / N. K takes the helm in real time.",x,y,width,12,mint);y+=12;}
 const char* orders[]={"Hold","Engage","Evade"};for(int i=0;i<3;i++)button(orders[i],{x+i*(width+6)/3,y,(width-12)/3,29},"combatOrder",std::to_string(i),!f.docked,f.combatOrder==i);y+=39;
 if(game->turnBased()){wrapped(f.autopilot?"Navigator continues on turns; K resumes the same route.":f.combatOrder==0?"Hold damps thrust during turns. Your cockpit helm settings are retained.":"Combat orders act during the committed hour. Planning time stays frozen.",x,y,width,11,muted);y+=14;}
 fitted(selected>=0?(f.traffic[selected].name.empty()?std::string(hyperion::flight::enemyDef(f.traffic[selected].role).name):f.traffic[selected].name)+" / "+n(hyperion::flight::length(f.traffic[selected].position-f.ship.position))+" km":"No hostile target",x,y,width-95,12,selected>=0?(f.traffic[selected].hostile?pink:mint):muted);button("Target / T",{x+width-91,y-6,91,27},"combatTarget");y+=33;
 const char* names[]={"1 Pulse","2 Kinetic","3 Plasma","4 Resonance"};for(int i=0;i<4;i++)button(names[i],{x+(i%2)*(width+6)/2,y+(i/2)*33,(width-6)/2,28},"weapon",std::to_string(hyperion::flight::primaryWeapon(i)),true,c.weapon==hyperion::flight::primaryWeapon(i));y+=76;
 button(std::string("Focus / U / ")+hyperion::flight::focusName(f.aimSystem),{x,y,width,28},"combatFocus");y+=39;
 if(selected>=0){const auto& enemy=f.traffic[selected];if(enemy.managed){wrapped(enemy.allegiance+" / "+enemy.reason,x,y,width,12,enemy.hostile?pink:mint);y+=12;if(enemy.pirate){auto* band=findPirates(game->s.economy,enemy.band);if(band&&!(band->truceUntil>game->s.hours&&band->grievance<10)){button("Settle feud / "+std::to_string(pirateTrucePrice(*band))+" cr",{x,y,width,28},"pirateTruce",band->id);y+=39;}}}row(std::string(enemy.hostile?"Hostile / ":"Contact / ")+hyperion::flight::enemyStateName(enemy),n(enemy.health)+" hull",x,y,width);row("Drive / weapons / shields",n(enemy.modules[0])+" / "+n(enemy.modules[1])+" / "+n(enemy.modules[2]),x,y,width);}

 if(selected>=0){const auto& t=f.traffic[selected];wrapped(t.charge>0?std::string("CHARGING / ")+hyperion::flight::weaponDef(t.chargeWeapon).name:t.state==4?"Withdrawing to a real workshop for supplies":t.state==6?"Venting heat / weapons temporarily quiet":t.state==7?"Servicing from local stores":t.pirate?"Pirate intent / "+t.reason:"Patrol intent / "+t.reason,x,y,width,11,t.charge>0?pink:muted);y+=13;}
 row("Charge / heat",n(c.energy)+"% / "+n(c.heat)+"%",x,y,width);row("Rounds / seekers / decoys",std::to_string(c.rounds)+" / "+std::to_string(c.missiles)+" / "+std::to_string(c.decoys),x,y,width);
 row("Plating",n(c.armour)+"%",x,y,width);for(int i=0;i<hyperion::flight::SystemCount;i++)row(hyperion::flight::systemName(i),n(c.systems[i])+"%",x,y,width);
 if(f.docked&&!game->s.economy.docked.empty()){button("Repair / services",{x,y,(width-6)/2,30},"panel","5");button("Rearm",{x+(width+6)/2,y,(width-6)/2,30},"rearm");y+=41;}
 else{bool salvage=false;for(auto& wreck:f.wrecks)if(hyperion::flight::length(wreck.position-f.ship.position)<55&&(std::accumulate(wreck.cargo.begin(),wreck.cargo.end(),0.)+std::accumulate(wreck.supplies.begin(),wreck.supplies.end(),0.))>0)salvage=true;if(salvage){button("Collect salvage",{x,y,width,30},"salvage","",f.speed()<=15);y+=40;}}
 wrapped("Pulse strips shields. Kinetic pierces plating. Plasma hits hard. Resonance drains charge and chains between hostiles. U focuses a subsystem; penetrating hits with a sustained lock disable it. Z: seeker. Y: decoy.",x,y,width,12,muted);y+=24;
}
void Renderer::flightHUD(){
 auto& f=game->s.flight;
 if(f.ports.empty())return;
 const auto& target=f.ports[f.target];
 const auto delta=target.position-f.ship.position;
 const double d=hyperion::flight::length(delta),speed=f.speed(),stoppingDistance=f.stoppingDistance();
 const float edge=UILayout::worldRight(w,ui->panel),cx=w*.5f,cy=h*.5f;
 const bool compact=ui->panel;std::string recoveryPrompt;double nearestRecovery=55;for(const auto& [id,d]:game->s.economy.worldDamage)if(d.recovery.valid&&d.star==game->s.currentStar&&(!d.recovery.cut||d.recovery.crew||d.recovery.aboard||d.recovery.towed||(d.recovery.box&&!d.recovery.boxRecovered))){double distance=hyperion::flight::length(d.position-f.ship.position);if(distance<nearestRecovery){nearestRecovery=distance;recoveryPrompt=id;}}

 const int boardingIndex=f.combatTargetIndex();const bool boardingPrompt=boardingIndex>=0&&f.traffic[boardingIndex].kind==1&&f.traffic[boardingIndex].modules[0]<=2&&hyperion::flight::length(f.traffic[boardingIndex].position-f.ship.position)<55;
 const auto landing=f.landing();
 const float inset=UILayout::safeInset(w,h),deckY=h-inset-(compact?104:80),deckWidth=edge-2*inset,markerBottom=deckY-80;
 const float systemsWidth=compact?180.f:clamp(deckWidth*.31f,270.f,330.f);
 const float navWidth=compact?deckWidth-systemsWidth-12:clamp(deckWidth*.38f,320.f,440.f);
 const float systemsX=edge-inset-systemsWidth;
 const Color ice{138,207,255,255},violet{202,166,255,255},amber{255,202,139,255};
 auto label=[&](const std::string& value,float x,float y,float width,Color tint=muted){fitted(value,x,y,width,10,tint,true);};
 auto tag=[&](const std::string& value,Rectangle box,Color tint){
  hudCard(box,tint);fitted(value,box.x+10,box.y+8,box.width-20,10,tint,true);
 };
 auto worldLabel=[&](Rectangle box,float anchorX){
  const Rectangle aimClear{cx-34,cy-26,68,52};
  if(CheckCollisionRecs(box,aimClear)){
   const float alternate=2*anchorX-box.x-box.width;
   const Rectangle flipped{alternate,box.y,box.width,box.height};
   if(alternate>=24&&alternate+box.width<=edge-24&&!CheckCollisionRecs(flipped,aimClear))box.x=alternate;
   else box.x=clamp(cx+52,24.f,edge-box.width-24);
  }
  return box;
 };

 // Aim and prograde stay aligned to the camera; opening a panel never shifts them.
 if(!compact&&!f.docked){
  for(int side:{-1,1}){
   DrawLineEx({cx+side*9,cy},{cx+side*21,cy},3,Color{4,12,20,150});
   DrawLineEx({cx+side*21,cy},{cx+side*24,cy+4},3,Color{4,12,20,150});
   DrawLineEx({cx+side*9,cy},{cx+side*21,cy},1,Color{153,255,218,190});
   DrawLineEx({cx+side*21,cy},{cx+side*24,cy+4},1,Color{153,255,218,125});
  }
  DrawCircleV({cx,cy},2.5f,Color{4,12,20,165});DrawCircleV({cx,cy},1.2f,pearl);

 }

 // Short context rails leave the windscreen open; full details remain in the side panels.
 const float contextY=std::max(74.f,ui->noticeBottom+8);
 if(game->toastTime<=0&&compact){
  const float limit=compact?std::min(330.f,edge-48):350.f;
  const auto description=f.planets[f.nearestPlanet()].name+(f.atmosphereDensity()>.001?" / ATMOSPHERE / ":" / SPACE / ")+hyperion::flight::vesselDef(f.ship.vessel.model).name;
  const auto value=elide(description,limit-24,10,true);
  const float railWidth=MeasureTextEx(mono,value.c_str(),10,.1f).x+24;
  hudCard({24,74,railWidth,26},ice);text(value,36,82,10,muted,true);
 }
 if(compact&&f.courier.active){
  const double remaining=std::max(0.,f.courier.deadline-game->s.hours);
  tag("COURIER / "+n(remaining,1)+" h",{edge-190,contextY,166,27},remaining<2?pink:ice);
 }
 const Mission* tracked=nullptr;
 for(const auto& m:game->s.economy.missions)if(missionOpen(m)&&(m.state!=MissionOffered||m.id==game->trackedMission)&&(!tracked||m.id==game->trackedMission))tracked=&m;
 if(!compact){
  const float contextWidth=std::min(340.f,edge-388),contextX=edge-contextWidth-24;
  if(tracked){
   hudCard({contextX,contextY,contextWidth,37},tracked->state==MissionReady?mint:violet);
   fitted(tracked->title,contextX+12,contextY+6,contextWidth-24,12,pearl);
   fitted(missionStateName(tracked->state),contextX+12,contextY+23,contextWidth-24,9,tracked->state==MissionReady?mint:violet,true);
   hyperion::flight::V3 point;bool mark=false;
   if(tracked->star==game->s.currentStar&&tracked->kind==SurveyMission&&tracked->state==MissionActive&&tracked->planet<int(f.planets.size())){
    const auto& body=f.planets[tracked->planet];const auto normal=game->surfaceNormal(tracked->contact.pos);
    point=body.centre+normal*(body.radius+hyperion::flight::terrain(body,normal)+5);mark=true;
   }
   if(tracked->kind==EscortMission)for(const auto& t:f.traffic)if(!t.dead&&t.routeId==tracked->target){point=t.position;mark=true;}
   if(mark&&hyperion::flight::dot(point-f.ship.position,f.ship.forward)>0&&f.lineOfSight(f.ship.position,point)){
    const auto at=flightGraphics.project(point,w,h);
    if(at.x>35&&at.x<edge-220&&at.y>185&&at.y<markerBottom){
     hudDiamond(at,9,violet);
     const auto info=worldLabel({at.x+15,at.y-11,201,33},at.x);hudCard(info,violet);
     fitted(tracked->kind==SurveyMission?tracked->contact.name:"Commissioned convoy",info.x+9,info.y+6,183,11,pearl);
     label(n(hyperion::flight::length(point-f.ship.position))+" km",info.x+9,info.y+21,183,violet);
    }
   }
  }
  if(f.courier.active){
   const double remaining=std::max(0.,f.courier.deadline-game->s.hours);
   tag("COURIER / "+n(remaining,1)+" h remaining",{contextX,contextY+(tracked?45:0),contextWidth,27},remaining<2?pink:ice);
  }
 }
 const bool surfaceApproach=landing.available&&landing.altitude<80&&landing.sink>=-.5;
 const bool landingDisplay=surfaceApproach&&(!f.docked||f.touchdownAge<2);
 if(!compact&&(landingDisplay||f.launchAssist>0)){
  const float y=std::max(108.f,ui->noticeBottom+8),x=24,width=300;
  const std::string phase=f.launchAssist>0?(landing.altitude<100?(f.launchAssist<1.1?"DEPARTURE / FORWARD FLIGHT":"DEPARTURE / VERTICAL LIFT"):"DEPARTURE / BERTH RELEASE"):f.docked?"TOUCHDOWN / SECURE":f.autopilot?"LANDING / FINAL FLARE":"SURFACE APPROACH";
  const Color tint=landing.safe||f.docked||f.launchAssist>0?mint:amber;
  hudCard({x,y,width,57},tint);label(phase,x+12,y+8,width-24,tint);
  std::string advice=!f.ship.gear?"G: lower gear":landing.sink>6?"Reduce descent":landing.drift>10?"Brake drift":landing.tilt>35?"Level ship":hyperion::flight::length(f.ship.angularVelocity)>=.35?"Settle rotation":landing.slope>25?"Find level ground":"Approach stable";
  if(f.docked)advice="Gear secure";else if(f.launchAssist>0)advice="Steer to take control";
  fitted(advice,x+12,y+23,width-24,12,pearl);
  fitted("Drift "+n(landing.drift,1)+" / Tilt "+n(landing.tilt)+" / Slope "+n(landing.slope),x+12,y+42,width-24,9,tint,true);
 }

 if(landing.altitude<160&&!compact){
  const auto& body=f.planets[f.nearestPlanet()];const auto weather=hyperion::flight::weatherAt(body,hyperion::flight::unit(f.ship.position-body.centre),game->s.hours);
  if(weather.storm>.4){
   const float y=contextY+(tracked?45:0)+(f.courier.active?35:0);
   tag("STORM "+n(weather.storm*100)+"% / VARIABLE WINDS",{edge-324,y,300,27},weather.storm>.75?pink:ice);
  }
 }

 auto& c=f.ship.combat;
 const int selected=f.combatTargetIndex();
 if(!f.docked&&selected>=0&&!compact){
  const auto& enemy=f.traffic[selected];const auto offset=enemy.position-f.ship.position;
  const double range=hyperion::flight::length(offset);const auto q=flightGraphics.project(enemy.position,w,h);
  const Color tint=!enemy.hostile?ice:f.lock>=1?mint:pink;
  if(hyperion::flight::dot(offset,f.ship.forward)>0&&f.lineOfSight(f.ship.position,enemy.position)&&q.x>60&&q.x<edge-80&&q.y>180&&q.y<markerBottom){
   const float size=float(clamp(1550/range,14.,46.));
   for(int a:{-1,1})for(int b:{-1,1}){
    DrawLineEx({q.x+a*size,q.y+b*size},{q.x+a*(size-7),q.y+b*size},1.5f,tint);
    DrawLineEx({q.x+a*size,q.y+b*size},{q.x+a*size,q.y+b*(size-7)},1.5f,tint);
   }
   if(enemy.charge>.05)hudBar({q.x-size,q.y+size+6,size*2,3},enemy.charge,pink);
   const double lead=hyperion::flight::interceptTime(offset,enemy.velocity-f.ship.velocity,hyperion::flight::weaponDef(c.weapon).speed);
   const auto pip=flightGraphics.project(enemy.position+(enemy.velocity-f.ship.velocity)*std::min(3.,lead),w,h);
   if(lead>0&&lead<hyperion::flight::weaponDef(c.weapon).life&&pip.x>40&&pip.x<edge-40&&pip.y>170&&pip.y<markerBottom){hudDiamond(pip,4,Color{153,255,218,190});}
  }
 }
 if(!f.docked){
  bool inbound=false;
  for(const auto& bolt:f.bolts)if(bolt.hostile&&bolt.weapon==3&&bolt.target=="NOMA"&&hyperion::flight::length(bolt.position-f.ship.position)<900)inbound=true;
  int worst=0;for(int i=1;i<hyperion::flight::SystemCount;i++)if(c.systems[i]<c.systems[worst])worst=i;
  if(inbound||c.systems[worst]<40||c.overheated){
   const std::string warning=inbound?"SEEKER INBOUND / Y COUNTERMEASURE":c.overheated?"MOUNTS COOLING":std::string(hyperion::flight::systemName(worst))+" DAMAGED / PORT REPAIR";
   tag(warning,{24,deckY-36,std::min(420.f,compact?edge-48:navWidth-12),27},pink);
  }
  if(f.damageCue>0){
   const auto shipRight=hyperion::flight::unit(hyperion::flight::cross(f.ship.forward,f.ship.up));
   Vector2 direction{float(hyperion::flight::dot(f.hitDirection,shipRight)),float(-hyperion::flight::dot(f.hitDirection,f.ship.up))};
   const float length=std::hypot(direction.x,direction.y);
   if(length<.1f)direction={0,hyperion::flight::dot(f.hitDirection,f.ship.forward)<0?1.f:-1.f};else{direction.x/=length;direction.y/=length;}
   const Vector2 at{cx+direction.x*85,cy+direction.y*85},across{-direction.y*7,direction.x*7};
   Color tint=f.impactHull>.01?pink:ice;tint.a=static_cast<unsigned char>(220*std::min(1.,f.damageCue*2));
   DrawLineEx({at.x+across.x-direction.x*7,at.y+across.y-direction.y*7},at,2,tint);
   DrawLineEx(at,{at.x-across.x-direction.x*7,at.y-across.y-direction.y*7},2,tint);
   const std::string impact=f.impactHull>.01?"HULL IMPACT / "+std::string(f.impactSystem>=0?hyperion::flight::systemName(f.impactSystem):"PLATING"):"SHIELD ABSORBED / "+n(f.impactShield,1);
   if(!compact)tag(impact,{cx-145,std::min(cy+110,deckY-69),290,27},f.impactHull>.01?pink:ice);
  }
  if(f.hitConfirm>0){
   Color tint=f.feedback.confirmation==hyperion::flight::ShipExplosion?mint:f.feedback.confirmation==hyperion::flight::ShieldBreak?Color{203,119,255,255}:f.feedback.confirmation==hyperion::flight::HullImpact?Color{255,181,86,255}:ice;
   float pulse=float(std::min(1.,f.hitConfirm*8)),outer=19+4*pulse,inner=15+2*pulse;tint.a=static_cast<unsigned char>(230*pulse);
   for(int side:{-1,1}){DrawLineEx({cx+side*outer,cy-outer},{cx+side*inner,cy-inner},1.7f,tint);DrawLineEx({cx+side*outer,cy+outer},{cx+side*inner,cy+inner},1.7f,tint);}
  }
  if(!compact&&speed<=15&&recoveryPrompt.empty()&&!boardingPrompt)for(const auto& wreck:f.wrecks)if(hyperion::flight::length(wreck.position-f.ship.position)<55&&(std::accumulate(wreck.cargo.begin(),wreck.cargo.end(),0.)+std::accumulate(wreck.supplies.begin(),wreck.supplies.end(),0.))>0){button("Collect salvage",{24,deckY-73,150,30},"salvage");break;}
  if(!compact&&recoveryPrompt.empty()&&boardingPrompt)button("Plan boarding",{24,deckY-73,150,30},"boardingPlan",f.traffic[boardingIndex].id);
  if(!compact&&!recoveryPrompt.empty())button("Plan recovery",{24,deckY-73,150,30},"recoveryPlan",recoveryPrompt);

 }

 // One lower deck carries the flight readings, scope and contextual controls.
 DrawRectangleGradientV(0,int(deckY-12),int(edge),compact?134:110,Color{5,10,18,0},Color{5,10,18,210});
 if(compact){hudCard({inset,deckY,navWidth,104},mint);label("NAVIGATION",inset+12,deckY+9,navWidth-120,mint);}
 else hudRail({inset,deckY,navWidth,80},mint);
 right(n(d)+" km",inset+navWidth-12,deckY+9,11,pearl,true);
 fitted(target.name,inset+12,deckY+(compact?25:7),navWidth-(compact?24:118),compact?19:16,pearl);
 std::string helm=f.docked?"DOCKED":f.autopilot?"NAVIGATOR":f.ship.assist?(f.ship.cruise?"ASSIST / CRUISE":"ASSIST / LOCAL"):"INERTIAL / X ASSIST";
 if(f.precisionActive)helm+=" / PRECISION";else if(f.ship.assistCaptureTime>0)helm+=" / MATCHING";
 const double driftAngle=speed>3?std::acos(clamp(hyperion::flight::dot(hyperion::flight::unit(f.ship.velocity),f.ship.forward),-1.,1.))*180/pi:0;
 if(driftAngle>8)helm+=" / DRIFT "+n(driftAngle)+" deg";
 label(helm,inset+12,deckY+(compact?49:30),navWidth-24,driftAngle>45?amber:f.precisionActive?mint:muted);
 struct Action{std::string title,key,command,arg;};std::vector<Action> actions;
 if(f.docked){
  actions.push_back({"Launch","ENTER","flightDock",""});
  if(!game->s.economy.docked.empty())actions.push_back({"Market","E","panel","5"});
  if(f.dockedPort<0||f.ports[f.dockedPort].planet>=0)actions.push_back({"Deploy","L","deploy",""});
 }else{
  int berth=-1;double closest=1e30;
  for(int i=0;i<int(f.ports.size());i++)if(f.ports[i].minor<0){const double distance=hyperion::flight::length(f.ports[i].position-f.ship.position);if(distance<closest){closest=distance;berth=i;}}
  const bool departing=f.launchAssist>0||landing.sink<-.5;
  const bool nearby=!departing&&berth>=0&&closest<(f.ports[berth].planet>=0?55:95)&&speed<55;
  if(f.autopilot)actions.push_back({"Take helm","J","navigate",""});
  else if(nearby){actions.push_back({f.ports[berth].planet>=0?"Land":"Dock","ENTER","flightDock",""});actions.push_back({"Navigate","J","navigate",""});}
  else actions.push_back({"Navigate","J","navigate",""});
  if(surfaceApproach&&!departing)actions.push_back({f.ship.gear?"Gear down":"Lower gear","G","gear",""});
  else if(speed>3||f.launchAssist>0)actions.push_back({"Brake","BKSP","brake",""});
 }
 const float actionWidth=std::min(140.f,(navWidth-24-6*float(actions.size()-1))/float(actions.size()));
 for(size_t i=0;i<actions.size();i++){
  const auto& action=actions[i];const Rectangle box{inset+12+float(i)*(actionWidth+6),deckY+(compact?68:48),actionWidth,26};
  button("",box,action.command,action.arg,true,i==0);
  const bool hover=CheckCollisionPointRec(uiMouse(),box);
  const Color tint=i==0?mint:ice;const bool enabled=ui->handoff.ready()&&(!ui->paused||Game::planningAction(action.command));
  DrawRectangleRounded(box,.14f,4,hover&&enabled?Color{34,65,67,255}:Color{14,31,40,255});
  DrawRectangleRoundedLinesEx(box,.14f,4,1,Color{tint.r,tint.g,tint.b,static_cast<unsigned char>(enabled?(i==0?125:45):25)});
  const float keyWidth=MeasureTextEx(mono,action.key.c_str(),9,.1f).x+8;
  fitted(action.title,box.x+8,box.y+8,box.width-keyWidth-18,11,enabled?pearl:muted);
  const Rectangle key{box.x+box.width-keyWidth-5,box.y+6,keyWidth,16};
  DrawRectangleRounded(key,.2f,3,Color{tint.r,tint.g,tint.b,25});
  right(action.key,key.x+key.width-4,key.y+4,9,tint,true);
 }
 const bool weaponReadings=!f.docked&&(selected>=0||c.heat>1||c.energy<99||c.overheated);
 const float systemsY=deckY+(!compact&&!f.docked&&!weaponReadings?32:0);
 if(compact)hudCard({systemsX,systemsY,systemsWidth,104},ice);
 else hudRail({systemsX,systemsY,systemsWidth,f.docked||weaponReadings?80.f:48.f},ice);
 const double values[]={f.ship.shield/hyperion::flight::shieldCapacity(f.ship.vessel)*100,game->s.hull,game->s.fuel/game->s.maxFuel*100};
 const char* labels[]={"SHIELD","HULL","FUEL"};
 const float gaugeWidth=(systemsWidth-32)/3;
 for(int i=0;i<3;i++){
  const float x=systemsX+12+i*(gaugeWidth+4);const Color tint=values[i]<25?pink:i==0?ice:i==1?mint:violet;
  fitted(labels[i],x,systemsY+(compact?10:6),gaugeWidth,compact?8:9,muted,true);
  text(n(values[i])+"%",x,systemsY+(compact?22:18),compact?15:16,values[i]<25?pink:pearl,true);
  hudBar({x,systemsY+(compact?44:36),gaugeWidth-6,3},values[i]/100,tint);
 }
 const float weaponX=systemsX+12,weaponY=systemsY+(compact?54:44),weaponWidth=systemsWidth-24;
 if(!f.docked&&(compact||weaponReadings)){
  const Color heatTint=c.overheated||c.heat>80?pink:c.heat>55?amber:muted;
  const bool safe=f.protectedPort(f.ship.position);
  fitted(std::to_string(c.weapon==hyperion::flight::Resonance?4:c.weapon+1)+" / "+hyperion::flight::weaponDef(c.weapon).name,weaponX,weaponY,weaponWidth-36,compact?10:11,pearl,true);
  right(safe?"SAFE":"ARMED",weaponX+weaponWidth,weaponY+1,8,safe?muted:violet,true);
  const float half=(weaponWidth-10)*.5f;
  fitted("CHG "+n(c.energy),weaponX,weaponY+17,half,9,violet,true);
  fitted(std::string(c.overheated?"COOL ":"HEAT ")+n(c.heat),weaponX+half+10,weaponY+17,half,9,heatTint,true);
  if(compact){hudBar({weaponX,weaponY+29,half,2},c.energy/100,violet);hudBar({weaponX+half+10,weaponY+29,half,2},c.heat/100,heatTint);}
  fitted(std::to_string(c.rounds)+" rnd / Z"+std::to_string(c.missiles)+" Y"+std::to_string(c.decoys)+" / U "+hyperion::flight::focusName(f.aimSystem),weaponX,weaponY+(compact?37:28),weaponWidth,9,muted,true);
 }else if(f.docked){
  fitted(n(game->s.economy.credits)+" cr / BERTH SECURE",systemsX+12,deckY+(compact?65:54),systemsWidth-24,compact?14:12,mint,true);
 }
 if(!compact){
  const float radarWidth=std::min(200.f,systemsX-(inset+navWidth)-24),rx=(inset+navWidth+systemsX)*.5f;
  const bool instruments=!f.docked;
  const float scopeTop=deckY-(instruments?48:24),scopeHeight=instruments?128.f:104.f;
  const Vector2 radar{rx,scopeTop+(instruments?85:49)};
  const bool ground=landing.available&&landing.altitude<80&&f.ship.gear;
  const float scopeX=rx-radarWidth*.5f;
  hudRail({scopeX,scopeTop,radarWidth,scopeHeight},ground?mint:violet);
  if(instruments){
   const float left=scopeX+12,right=rx+8,half=radarWidth*.5f-20;
   fitted("VEL / km/s",left,scopeTop+9,half,9,muted,true);
   fitted(n(speed,speed<10?1:0),left,scopeTop+22,half,20,pearl,true);
   fitted(landing.altitude<350?"AGL / km":"THRUST",right,scopeTop+9,half,9,muted,true);
   fitted(landing.altitude<350?n(std::max(0.,landing.altitude),1):n(f.engineLoad*100)+"%",right,scopeTop+22,half,20,pearl,true);
   // A recessed backing keeps the canopy rail from crossing small flight readings.
   DrawRectangleRec({scopeX+1,scopeTop+49,radarWidth-2,17},Color{6,16,25,245});
   if(speed>3)fitted("STOP "+n(stoppingDistance,stoppingDistance<10?1:0)+" km",left,scopeTop+53,half,9,mint,true);
   if(landing.altitude<350)fitted((landing.sink<0?"UP ":"DOWN ")+n(std::abs(landing.sink),1)+" km/s",right,scopeTop+53,half,9,landing.sink>6?pink:ice,true);
   else hudBar({right,scopeTop+57,half,2},f.engineLoad,ice);
  }else label(ground?"LANDING SCOPE":"CONTACTS",scopeX+12,scopeTop+9,radarWidth-24,ground?mint:violet);
  DrawEllipse(int(radar.x),int(radar.y),66,20,Color{10,26,34,170});
  DrawEllipseLines(int(radar.x),int(radar.y),33,10,Color{92,154,168,50});
  // Static spectral orbit: no flashing, bloom or extra render pass.
  for(int i=0;i<48;i++){
   const float a=float(tau*i/48),b=float(tau*(i+1)/48);
   Color tint=ColorFromHSV(145+i*3.8f,.40f,.87f);tint.a=115;
   DrawLineEx({rx+std::cos(a)*66,radar.y+std::sin(a)*20},{rx+std::cos(b)*66,radar.y+std::sin(b)*20},1,tint);
  }
  DrawLineEx({rx-66,radar.y},{rx+66,radar.y},1,Color{92,154,168,35});
  DrawLineEx({rx,radar.y-20},{rx,radar.y+20},1,Color{92,154,168,35});
  const auto shipRight=hyperion::flight::unit(hyperion::flight::cross(f.ship.forward,f.ship.up));
  if(ground){
   const auto rgt=hyperion::flight::unit(hyperion::flight::cross(f.ship.forward,landing.normal)),ahead=hyperion::flight::unit(hyperion::flight::cross(landing.normal,rgt));
   int pad=landing.port;if(pad<0&&target.planet==landing.body&&d<120)pad=f.target;
   const auto offset=pad>=0?f.ports[pad].position-f.ship.position:hyperion::flight::V3{};
   const double u=hyperion::flight::dot(offset,rgt),v=hyperion::flight::dot(offset,ahead);
   const Vector2 point{rx+float(clamp(u,-61.,61.)),radar.y-float(clamp(v,-61.,61.)*.30)};
   DrawEllipseLines(int(point.x),int(point.y),18,8,landing.safe?mint:amber);
   DrawLineEx(radar,{rx+float(clamp(hyperion::flight::dot(f.ship.velocity,rgt)*2,-61.,61.)),radar.y-float(clamp(hyperion::flight::dot(f.ship.velocity,ahead),-22.,22.))},2,amber);
   label(pad>=0?"PAD / "+n(std::hypot(u,v),1)+" km":"GROUND / GEAR DOWN",scopeX+12,scopeTop+scopeHeight-16,radarWidth-24,landing.safe?mint:amber);
  }else{
   for(int i=0;i<int(f.traffic.size());i++){
    const auto& t=f.traffic[i];if(t.dead)continue;
    const auto offset=t.position-f.ship.position;if(hyperion::flight::length(offset)>1400)continue;
    const Vector2 point{rx+float(hyperion::flight::dot(offset,shipRight)/1400*61),radar.y-float(hyperion::flight::dot(offset,f.ship.forward)/1400*18)};
    const float height=float(clamp(hyperion::flight::dot(offset,f.ship.up)/1400*14,-12.,12.));
    const Color tint=t.kind==1?(t.hostile?pink:ice):t.kind==2?amber:mint;
    const Vector2 tip{point.x,clamp(point.y-height,radar.y-20,radar.y+20)};
    DrawLineEx(point,tip,1,Color{tint.r,tint.g,tint.b,100});
    if(i==selected)hudDiamond(tip,4,tint);else DrawCircleV(tip,2,tint);
   }
  }
  DrawTriangle({radar.x,radar.y-4},{radar.x-3,radar.y+3},{radar.x+3,radar.y+3},pearl);
 }
 flightDirector(landing,stoppingDistance);
}
void Renderer::flightPanel(float x,float& y,float width){
 auto& f=game->s.flight;combatPanel(x,y,width);const auto& target=f.ports[f.target];bool debris=target.minor>=0&&f.minorBodies[target.minor].destroyed;if(target.disabled||debris){wrapped(debris?"Fractured body / finite salvage remains":"Berth destroyed / reconstruction required",x,y,width,12,pink);y+=14;}
 fitted(target.name,x,y,width,21,pearl);y+=31;fitted((target.minor>=0?"Fly-by / ":target.planet>=0?"Surface / ":"Orbit / ")+n(f.targetDistance())+" km",x,y,width,11,mint,true);y+=23;
 if(auto* site=flightPresentation.settlement(target.settlement)){fitted(site->factionName+" / "+n(site->residents)+" residents",x,y,width,12,muted);y+=22;if(site->alerts){fitted(site->status,x,y,width,12,pink);y+=24;}}
 button(f.autopilot?"Take control / J":"Navigate / J",{x,y,width,32},"navigate","",!(f.docked&&f.dockedPort==f.target),true);y+=44;
 if(target.minor>=0){const auto& b=f.minorBodies[target.minor];bool surveyed=game->s.scanned.count(b.source.id);wrapped(b.source.composition,x,y,width,12,mint);y+=12;
  button(surveyed?"Survey archived":"Survey / 6 battery",{x,y,width,30},"surveyMinor","",!surveyed&&!b.destroyed&&game->s.battery>=6&&f.speed()<=20&&hyperion::flight::length(b.centre-f.ship.position)-b.source.radius<=600);y+=40;
 }
 const char* filters[]={"All","Surface","Orbit","Bodies"};for(int i=0;i<4;i++)button(filters[i],{x+i*(width+6)/4,y,(width-18)/4,28},"filterDestinations",std::to_string(i),true,ui->destinationFilter==i);y+=40;
 std::vector<int> order(f.ports.size());std::iota(order.begin(),order.end(),0);std::stable_sort(order.begin(),order.end(),[&](int a,int b){return hyperion::flight::length(f.ports[a].position-f.ship.position)<hyperion::flight::length(f.ports[b].position-f.ship.position);});
 for(int index:order){auto& p=f.ports[index];if(ui->destinationFilter==1&&p.planet<0)continue;if(ui->destinationFilter==2&&(p.planet>=0||p.minor>=0))continue;if(ui->destinationFilter==3&&p.minor<0)continue;
  button(p.name,{x,y,width,30},"flightTarget",p.id,true,index==f.target);y+=34;fitted((p.minor>=0?std::string(minorKindName(f.minorBodies[p.minor].source.kind))+" / ":p.planet>=0?"Surface / ":"Orbit / ")+n(hyperion::flight::length(p.position-f.ship.position))+" km",x+7,y,width-14,10,muted,true);y+=23;
 }
 if(disclosure("Selected port details","port-details",x,y,width)){
  wrapped(target.description,x,y,width,13,muted);y+=12;
  if(auto* site=flightPresentation.settlement(target.settlement)){row("Warehouse",n(site->storage)+" / "+n(site->capacity),x,y,width);row("Power / demand",n(site->power,1)+" / "+n(site->demand,1),x,y,width);
   for(auto& packet:flightPresentation.shipments)if(packet.from==site->id||packet.to==site->id){wrapped(std::string(packet.from==site->id?"Out / ":"In / ")+n(packet.amount)+" "+goods[packet.good].name+" / "+(packet.holding?"Holding":packet.loading?"Loading":packet.unloading?"Unloading":"In transit"),x,y,width,12,packet.holding?pink:mint);y+=8;}
  }
  row("Combat victories",std::to_string(f.kills),x,y,width);row("Courier deliveries",std::to_string(f.deliveries),x,y,width);
 }
 if(disclosure("Flight systems","flight-systems",x,y,width)){
  wrapped("W/S sets throttle. R/F lifts, A/D strafes. Hold Option/Alt for precision thrust and steering. X restores assist at your forward speed; Backspace commands a stop. Rotation and drift persist with assist off.",x,y,width,12,muted);y+=18;
  row("Thruster load / cargo mass",n(f.engineLoad*100)+"% / "+n(f.cargoMass),x,y,width);
  for(auto& item:std::vector<std::pair<std::string,std::string>>{{"Brake / Backspace","brake"},{"Flight assist / X","assist"},{"Cruise / C","cruise"},{"Landing gear / G","gear"}}){button(item.first,{x,y,width,30},item.second);y+=38;}
 }
}
}
