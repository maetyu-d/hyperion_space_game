#include "renderer.hpp"
#include <sstream>
#include <iomanip>
namespace hyperion {
namespace {
constexpr Color mint{153,255,218,255},pearl{234,244,231,255},muted{132,163,170,255},amber{255,202,139,255},ice{138,207,255,255};
std::string number(double value){std::ostringstream out;out<<std::fixed<<std::setprecision(1)<<value;return out.str();}
}
void Renderer::turnPlanner(float x,float& y,float width){
 const auto& s=game->s;auto forecast=game->outlook(ui->plannedHours);
 wrapped("The world waits while you plan. Each resolved turn is one hour.",x,y,width,12,muted);y+=12;
 const int durations[]={1,3,6,12,24};const float cell=(width-20)/5;
 for(int i=0;i<5;i++)button(std::to_string(durations[i])+" h",{x+i*(cell+5),y,cell,28},"planHours",std::to_string(durations[i]),true,ui->plannedHours==durations[i]);y+=38;
 const bool pressure=game->hasExpedition()&&game->expeditionLayer()==Mode::Ocean&&s.depth>game->maxDepth();
 button("Wait "+std::to_string(ui->plannedHours)+" h",{x,y,(width-6)/2,32},"waitTurns",std::to_string(ui->plannedHours),!ui->paused&&!pressure,true);
 button("Next event",{x+(width+6)/2,y,(width-6)/2,32},"waitTurns","next",!ui->paused&&!pressure);y+=44;
 wrapped("Waits stop on events and emergencies at the next full hour. Next event waits at most 24 h. Your waypoint stays set.",x,y,width,11,muted);y+=10;
 if(pressure){wrapped("Ascend to certified depth before a long wait. One-hour orders still resolve pressure exposure.",x,y,width,12,amber);y+=12;}
 row("Battery / full wait",number(s.battery)+" -> "+number(forecast.battery)+"%",x,y,width);
 if(forecast.pressureLoss>0)row("Pressure / full wait",number(forecast.pressureLoss)+" hull",x,y,width);
 if(s.flight.valid&&!s.flight.deployed&&!s.flight.docked){const char* orders[]={"Hold / damp drift","Engage hostiles","Evade hostiles"};row("Ship on committed turns",s.flight.autopilot?"Navigator":orders[std::clamp(s.flight.combatOrder,0,2)],x,y,width);}
 const auto plan=game->planMove();
 if(plan.valid){wrapped("Waypoint / "+std::to_string(plan.turns)+" one-hour steps remain",x,y,width,12,ice);y+=7;button("Move one step / Enter",{x,y,width,29},"moveWaypoint","",!ui->paused);y+=40;}
 if(!forecast.items.empty()){heading("ATTENTION BEFORE WAITING",x,y);y+=32;int count=0;for(const auto& item:forecast.items){if(item.priority==Notice::Status)continue;wrapped(item.text,x,y,width,12,amber);y+=10;if(++count==3)break;}if(forecast.warnings>3){fitted("+ "+std::to_string(forecast.warnings-3)+" more / expand Before next turn",x,y,width,10,muted,true);y+=24;}}
 heading("UPCOMING / NEXT 24 HOURS",x,y);y+=32;
 const auto events=game->upcoming();
 if(events.empty()){wrapped("No scheduled events in the next day. New threats and emergencies can still interrupt a wait.",x,y,width,12,muted);y+=12;}
 int count=0;
 for(const auto& event:events){if(count++==8)break;const Color tint=event.hours<=ui->plannedHours?(event.priority==Notice::Status?ice:amber):muted;
  const bool linked=!event.settlement.empty()&&findSettlement(s.economy,event.settlement);
  if(linked)button("",{x,y,width,32},"reviewSettlement",event.settlement);
  fitted((event.estimate?"~":"")+number(event.hours)+" h",x+7,y+10,59,10,tint,true);
  fitted(event.text,x+72,y+9,width-80,12,pearl);y+=38;
 }
 if(events.size()>8){fitted("+ "+std::to_string(events.size()-8)+" later events",x,y,width,10,muted,true);y+=24;}
 wrapped("~ research times use current lab output; food, power and weather can change them. Select a settlement event to inspect it.",x,y,width,10,muted);y+=14;
}
void Renderer::turnReport(float x,float& y,float width){
 const auto& reports=game->recentTurns;ui->reportIndex=std::clamp(ui->reportIndex,0,std::max(0,int(reports.size())-1));const auto& r=reports.empty()?game->lastTurn:reports[size_t(ui->reportIndex)];
 if(reports.size()>1){button("< Older",{x,y,98,27},"reportOlder","",ui->reportIndex+1<int(reports.size()));button("Newer >",{x+104,y,98,27},"reportNewer","",ui->reportIndex>0);right(std::to_string(ui->reportIndex+1)+" / "+std::to_string(reports.size()),x+width,y+8,10,muted,true);y+=38;}
 wrapped(r.summary(),x,y,width,12,mint,true);y+=10;
 row("Campaign hour",number(r.startHour)+" -> "+number(r.endHour),x,y,width);
 if(!r.interruption.empty()){wrapped("Stopped after "+number(r.hours)+" of "+number(r.requested)+" h / "+r.interruption,x,y,width,12,amber);y+=12;}
 row("Outpost rations",number(r.rations),x,y,width);row("Outpost alloy",number(r.alloy),x,y,width);
 for(const auto& colony:r.colonies){wrapped(colony,x,y,width,12,mint);y+=12;}
 for(const auto& event:r.events){if(event==r.interruption||event=="Wait interrupted / "+r.interruption)continue;wrapped(event,x,y,width,12,muted);y+=12;}y+=12;
 fitted("Recent 12 resolutions / this session",x,y,width,10,muted,true);y+=28;
}
}
