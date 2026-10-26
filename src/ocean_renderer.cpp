#include "renderer.hpp"
#include "ecosystem.hpp"
#include <sstream>
#include <iomanip>
namespace hyperion {
namespace {
constexpr Color mint{153,255,218,255},pearl{234,244,231,255},muted{132,163,170,255},amber{255,202,139,255};
std::string n(double v){return std::to_string(int(std::round(v)));}
std::string amount(double v){std::ostringstream out;out<<(v>0?"+":"")<<std::fixed<<std::setprecision(std::abs(v-std::round(v))<.05?0:1)<<v;return out.str();}
}
void Renderer::diveReportDetails(float x,float& y,float width){
 const auto& r=game->diveReport;wrapped(r.complete?"SURVEY RECOVERED":r.emergency?"EMERGENCY RECOVERY":"TRANSECT ENDED",x,y,width,12,r.complete?mint:amber,true);y+=9;
 wrapped(r.goal,x,y,width,16,pearl);y+=10;
 row("Studies / collections",std::to_string(r.studies)+" / "+std::to_string(r.collections),x,y,width);
 row("Deployment to recovery",amount(r.hours).substr(r.hours>0?1:0)+" h",x,y,width);row("Hull change",amount(r.hull)+"%",x,y,width);
 bool changed=false;for(int g=0;g<goodCount;g++)if(std::abs(r.delta[g])>=.05){row(goods[g].name,amount(r.delta[g]),x,y,width);changed=true;}
 if(!changed){wrapped("The cargo hold is unchanged.",x,y,width,11,muted);y+=12;}
 wrapped("Net cargo above is already in the shared hold. Research supports science and trade; biomass feeds existing supply chains, and mineral caches supply industry. A new dive clears this session debrief.",x,y,width,11,muted);y+=15;
}

void Renderer::wreckDeliveries(float x,float& y,float width,bool guide){
 const OceanWreck* pending=nullptr;for(const auto& w:game->s.economy.oceanWrecks)if(w.aboard||stockTotal(w.recovered)>0){if(!pending)pending=&w;auto quote=game->wreckQuote("wreckDeliver",w.contact.id);if(quote.available){button(quote.text,{x,y,width,30},"wreckDeliver",w.contact.id,!ui->paused,true);y+=40;return;}}
 if(guide&&pending){auto quote=game->wreckQuote("wreckDeliver",pending->contact.id);wrapped(quote.reason,x,y,width,11,mint);y+=10;button("Plot receiving settlement",{x,y,width,29},"plotWreckHome",pending->contact.id);y+=39;}
}
void Renderer::oceanOperations(float x,float& y,float width){
 auto b=game->diveBrief();const auto& s=game->s;
 if(disclosure("Time / forecast","turn-planner",x,y,width))turnPlanner(x,y,width);
 if(b.goal.kind=="wreck")if(auto* w=findOceanWreck(s.economy,b.goal.id)){
  wrapped(w->contact.name,x,y,width,19,pearl);y+=10;auto* home=findSettlement(s.economy,w->home);
  wrapped((home?home->name:w->home)+" / "+s.economy.factions[w->faction].name,x,y,width,11,mint);y+=10;
  row("Crew inside / aboard",std::to_string(w->crew)+" / "+std::to_string(w->aboard),x,y,width);
  if(w->lost)row("Life support losses",std::to_string(w->lost)+" crew",x,y,width);
  if(w->crew)row("Life support",n(std::max(0.,w->supportUntil-s.hours))+" h",x,y,width);
  if(!w->surveyed){wrapped("Study the hull, then approach within 80 survey units to enter or recover supplies.",x,y,width,12,muted);y+=12;}
  else{
   int selected=game->wreckCargoGood(*w);for(int g=0;g<goodCount;g++)if(w->remaining[g]>=1){button(std::string(goods[g].name)+" / "+n(w->remaining[g]),{x,y,width,28},"wreckCargo",std::to_string(g),true,g==selected);y+=34;}
   auto rescue=game->wreckQuote("wreckRescue",w->contact.id),take=game->wreckQuote("wreckSalvage",w->contact.id),cut=game->wreckQuote("wreckCut",w->contact.id);
   if(w->crew){button("Rescue / 1h",{x,y,width,30},"wreckRescue",w->contact.id,rescue.available&&!ui->paused);y+=36;wrapped(rescue.available?rescue.text:rescue.reason,x,y,width,11,muted);y+=12;}
   button("Recover / C",{x,y,(width-6)/2,30},"wreckSalvage",w->contact.id,take.available&&!ui->paused);button("Cut / X",{x+(width+6)/2,y,(width-6)/2,30},"wreckCut",w->contact.id,cut.available&&!ui->paused);y+=38;
   wrapped(take.available?take.text:take.reason,x,y,width,11,muted);y+=9;wrapped(cut.available?cut.text:cut.reason,x,y,width,11,amber);y+=12;
  }
  for(const auto& m:s.economy.missions)if(m.kind==WreckMission&&m.target==w->contact.id&&m.state<=MissionReady){wrapped(missionObjective(s.economy,m),x,y,width,11,mint);y+=8;if(m.state==MissionOffered){button("Accept / "+std::to_string(m.reward)+" cr",{x,y,width,29},"acceptMission",m.id,!ui->paused);y+=38;}break;}
  if(w->aboard||stockTotal(w->recovered)>0){button("Plot receiving settlement",{x,y,width,29},"plotWreckHome",w->contact.id);y+=38;wrapped("Crew and supplies stay aboard until physically unloaded at the owning settlement. Payment follows debrief.",x,y,width,11,muted);y+=12;}
  if(disclosure("Routes / current and vents","wreck-routes",x,y,width)){
   auto direct=game->oceanRoute(false),shelter=game->oceanRoute(true);
   button("Direct / free",{x,y,(width-6)/2,29},"oceanRoute","direct");button("Shelter / free",{x+(width+6)/2,y,(width-6)/2,29},"oceanRoute","shelter");y+=38;
   wrapped("Direct ~"+std::to_string(direct.turns)+" h / ~"+n(direct.power)+" power / ~"+amount(-direct.wear)+" hull. "+(shelter.detour?"Sheltered route ~"+std::to_string(shelter.turns)+" h / ~"+n(shelter.power)+" power / ~"+amount(-shelter.wear)+" hull.":"Direct course is already clearest.")+" Estimates exclude work, recharge and interruptions.",x,y,width,11,muted);y+=12;
  }
  if(disclosure("Wreck history","wreck-history",x,y,width)){wrapped(w->contact.desc,x,y,width,11,muted);y+=12;}
 }
 if(b.goal.kind!="wreck"){wrapped(b.hasGoal?b.goal.name:"Uncharted water",x,y,width,20,pearl);y+=12;
 auto study=b.hasGoal?game->fieldQuote("fieldStudy",b.goal.id):FieldQuote{},take=b.hasGoal?game->fieldQuote("fieldCollect",b.goal.id):FieldQuote{};
 wrapped(b.studied?(take.available?take.text:"Recorded / "+take.reason):(study.available?study.text:study.reason),x,y,width,11,muted);y+=16;
 }
 row("Dive work",std::to_string(game->diveState.studies)+" studied / "+std::to_string(game->diveState.collections)+" sampled",x,y,width);
 row("Hold",n(stockTotal(game->shipStock()))+" / "+n(game->cargoCapacity()),x,y,width);
 row("Home",std::to_string(b.homeTurns)+" moves",x,y,width);
 button("Listen / 1h",{x,y,(width-6)/2,30},"oceanListen","",s.battery>=2&&!ui->paused);
 button("Ping / 1h",{x+(width+6)/2,y,(width-6)/2,30},"scan","",s.battery>=12&&!ui->paused);y+=40;
 fitted("2 power / range "+n(140+s.upgrades[1]*100)+"     12 power / range "+n(b.range),x,y,width,9,muted,true);y+=25;
 if(disclosure("Sonar & ecology","dive-sonar",x,y,width)){
  wrapped("Listening resolves life, vents and signals at shorter range. Active pings also find mineral deposits, but nearby fauna evade the sub for a few turns. Neither method takes samples. Study leaves populations intact; C takes actual biomass or finite minerals.",x,y,width,12,muted);y+=12;
  if(b.hasGoal&&(b.goal.kind=="life"||b.goal.kind=="flora")){auto p=game->activePlanet();if(auto* habitat=contactHabitat(s.economy,game->universe,p,b.goal)){wrapped(populationReport(*habitat,b.goal),x,y,width,11,mint);y+=12;}}
 }
 if(s.hull<99.95){auto repair=game->fieldQuote("fieldRepair");button("Patch / 1h",{x,y,width,30},"fieldRepair","",repair.available&&!ui->paused);y+=40;wrapped(repair.available?repair.text:repair.reason,x,y,width,11,muted);y+=12;}
 if(turnOutlook.warnings&&disclosure("Risks / "+std::to_string(turnOutlook.warnings),"dive-risks",x,y,width)){int shown=0;for(const auto& item:turnOutlook.items)if(item.priority!=Notice::Status){wrapped(item.text,x,y,width,11,amber);y+=10;if(++shown==3)break;}}
 if(game->lastTurn.valid&&disclosure("Recent resolutions","turn-report",x,y,width))turnReport(x,y,width);
 if(disclosure("Keys","dive-help",x,y,width)){wrapped("Enter: labelled next action. C: optional sample or wreck cargo. X: faster wreck cutting. Space: active sonar. Shift+Space: quiet listen. K: up to six return steps. E: ascend or recover. WASD: one move. R: sites. I: details. O: charts. N: wait. All work resolves turns; planning is free.",x,y,width,11,muted);y+=12;}
}
void Renderer::oceanDrawer(){
 panelDrawing=true;float left=UILayout::panelLeft(w),x=left+20,cw=UILayout::panelWidth-40;
 DrawRectangleRec({left,60,UILayout::panelWidth,h-60},Color{6,14,23,248});DrawLineEx({left,60},{left,h},1,Color{83,139,150,80});
 fitted("DIVE",x,77,cw-50,13,mint,true);button("X",{w-45,70,28,28},"oceanClose");
 const char* pages[]={"Sites","Contacts","Details"};for(int i=0;i<3;i++)button(pages[i],{x+i*(cw+6)/3,101,(cw-12)/3,28},"divePage",std::to_string(i),true,ui->divePage==i);
 BeginScissorMode(int((left+1)*width/w),int(132*height/h),int((UILayout::panelWidth-2)*width/w),int((h-184)*height/h));panelContent=true;float y=141-ui->scroll,top=y;
 if(ui->divePage==2)oceanOperations(x,y,cw);
 else{
  auto sites=ui->divePage==0?game->diveSites():game->oceanContacts();if(ui->divePage==0){auto wrecks=game->wreckSites();sites.insert(sites.begin(),wrecks.begin(),wrecks.end());}int shown=0;
  for(const auto& c:sites){auto info=game->diveSiteInfo(c);bool recorded=game->s.scanned.count(c.id);button(c.name,{x,y,cw,30},"oceanSelect",c.id,true,c.id==game->diveState.goal.id);y+=36;
   fitted(info.role+" / "+n(c.depth)+"m / "+(recorded?"recorded":ui->divePage==0?"~"+std::to_string(info.approach)+" out / ~"+std::to_string(info.home)+" home":"unstudied"),x+7,y,cw-14,10,info.shell>80?amber:muted,true);y+=30;
   if(++shown==(ui->divePage==0?4:6))break;
  }
  if(sites.empty()){wrapped(ui->divePage==0?"No suitable sites in this transect.":"No resolved contacts. Listen quietly or ping active sonar.",x,y,cw,12,muted);y+=15;}
  if(ui->divePage==0){wrapped("Choose a destination. Movement estimates exclude work, recharge and interruptions.",x,y,cw,11,muted);y+=12;}
  else if(sites.size()>6){fitted("Six nearest returns / select a map marker for others",x,y,cw,9,muted,true);y+=25;}
 }
 ui->panelHeight=y-top;panelContent=false;EndScissorMode();ui->scroll=clamp(ui->scroll,0.f,std::max(0.f,ui->panelHeight-(h-184)));
 fitted("Planning is free / choosing a site closes this drawer",x,h-33,cw,9,muted,true);panelDrawing=false;
}
void Renderer::oceanOverlay(){
 auto b=game->diveBrief();auto order=game->nextDiveOrder();const auto& s=game->s;float edge=UILayout::worldRight(w,ui->panel);
 DrawRectangleRec({0,0,w,48},Color{4,10,18,220});fitted(ui->paused?"PAUSED / P":"HYPERION",20,14,116,13,ui->paused?pearl:muted,true);
 fitted(b.hasGoal?b.goal.name:"Uncharted water",147,14,std::max(60.f,edge-324),14,pearl);
 std::string status=n(s.depth)+" / "+n(game->maxDepth())+"m    power "+n(s.battery)+"%"+(game->rescuedCrew()?"    crew "+std::to_string(game->rescuedCrew())+"/6":"")+(s.hull<99.95?"    hull "+n(s.hull)+"%":"");
 fitted(status,20,35,std::max(80.f,edge-170),10,s.depth>game->maxDepth()||s.hull<45?amber:mint,true);
 right("T"+std::to_string(s.turns)+" / home ~"+std::to_string(b.homeTurns)+"h"+(turnOutlook.warnings?" !":""),edge-17,35,9,muted,true);
 button("Map / O",{w-150,8,82,28},"system");button("Menu",{w-62,8,50,28},"menu","",true,ui->menu);
 if(game->toastTime>0){float nw=std::min(560.f,edge-40);DrawRectangleRec({16,54,nw+8,24},Color{3,10,18,205});fitted(game->toast,24,61,nw-8,10,game->toastPriority==Notice::Critical?amber:mint,true);}
 auto sample=b.hasGoal?(b.goal.kind=="wreck"?game->wreckQuote("wreckSalvage",b.goal.id):game->fieldQuote("fieldCollect",b.goal.id)):FieldQuote{};bool canSample=sample.available&&b.phase!=4;
 float pw=170,rw=130,sw=canSample?112:0,bw=84,gap=6,total=pw+rw+bw+sw+(canSample?3:2)*gap,x=(edge-total)/2,y=h-61;
 DrawRectangleGradientV(0,int(h-91),int(edge),91,Color{4,10,19,0},Color{4,10,19,235});
 std::string label=order.action=="oceanAdvance"?"Travel 1h":order.action=="scan"?"Ping 1h":order.action=="fieldStudy"?"Study 1h":order.action=="oceanHomeStep"?"Return 1h":order.action=="surface"?"Recover 1h":order.action=="oceanAscend"?"Ascend 1h":order.action=="fieldRecharge"?"Charge <="+std::to_string(order.hours)+"h":order.action=="oceanNext"?"Next / free":order.action=="wreckRescue"?"Rescue 1h":order.action=="wreckSalvage"?"Salvage 1h":"Choose a site";
 button(label+" / Enter",{x,y,pw,32},order.action,order.arg,order.available&&(!ui->paused||Game::planningAction(order.action)),true);x+=pw+gap;
 if(canSample){button(b.goal.kind=="wreck"?"Take 1h / C":"Sample 1h / C",{x,y,sw,32},b.goal.kind=="wreck"?"wreckSalvage":"fieldCollect",b.goal.id,!ui->paused);x+=sw+gap;}
 bool home=b.atBuoy&&b.nearSurface;button(home?"Recover 1h / E":"Return <=6h / K",{x,y,rw,32},home?"surface":"oceanReturn","",!ui->paused&&(home||s.depth<=game->maxDepth()));x+=rw+gap;
 button("Sites / R",{x,y,bw,32},"orders","",true,ui->panel&&ui->tab==0);
 fitted("WASD steer / Space ping / Shift+Space listen / E ascend / N wait",std::max(20.f,(edge-460)/2),h-16,std::min(460.f,edge-40),9,muted,true);
 if(!order.available||b.phase==4){fitted(order.text,20,h-89,edge-40,10,b.phase==4?amber:muted,true);}
 if(ui->panel){if(ui->tab==0)oceanDrawer();else panel();}menus();
 if(!ui->orderHint.empty()&&!ui->menu&&!ui->sections&&!ui->help&&!ui->settings){float ty=h-125;DrawRectangleRec({16,ty-7,edge-32,31},Color{3,10,18,234});fitted(ui->orderHint,24,ty,edge-48,10,mint,true);}
 if(ui->help||ui->settings)modal();
 if(!ui->saveWarning.empty())fitted("SAVE / "+ui->saveWarning,20,88,edge-40,11,amber,true);
}
}
