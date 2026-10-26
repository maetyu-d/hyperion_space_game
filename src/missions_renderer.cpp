#include "renderer.hpp"
namespace hyperion {
void Renderer::missionPanel(float x,float& y,float width){
 auto& e=game->s.economy;int active=0,offers=0;
 for(const auto& m:e.missions){active+=missionOpen(m);offers+=m.state==MissionOffered&&m.star==game->s.currentStar;}
 if(!disclosure("Missions / "+std::to_string(active)+" active / "+std::to_string(offers)+(offers==1?" offer":" offers"),"missions",x,y,width))return;
 constexpr Color mint{153,255,218,255},muted{132,163,170,255},pearl{234,244,231,255},warning{255,178,125,255};
 auto para=[&](const std::string& t,Color c){wrapped(t,x,y,width,12,c);y+=9;};
 para("Jobs follow settlement needs, freight, pirates and field discoveries. Accept to reserve payment; planning pauses deadlines.",muted);
 int count=0;
 for(int pass=0;pass<2;pass++)for(const auto& m:e.missions)if((pass==0?missionOpen(m):m.state==MissionOffered&&m.star==game->s.currentStar)){
  auto* site=findSettlement(e,m.issuer);para(m.title,pearl);para((site?site->name:m.issuer)+" / "+missionStateName(m.state)+" / "+std::to_string(int(std::min(1e8,std::ceil(std::max(0.,m.deadline-game->s.hours)))))+" h / "+std::to_string(m.reward)+" cr",mint);
  if(disclosure("Cause / risk "+std::to_string(int(m.risk*100))+"/100", "cause:"+m.id,x,y,width))para(m.cause,muted);
  para(missionObjective(e,m),m.state==MissionReady?mint:muted);
  if(m.state==MissionOffered){button("Accept commission",{x,y,width,30},"acceptMission",m.id);y+=38;}
  else{
   button("Plot / track",{x,y,(width-6)/2,30},"plotMission",m.id);button(m.state==MissionReady?"Debrief":"Deliver cargo",{x+(width+6)/2,y,(width-6)/2,30},m.state==MissionReady?"debriefMission":"deliverMission",m.id,(m.state==MissionReady||(m.kind==ReliefMission||m.kind==WreckMission))&&site&&game->canDock(*site));y+=38;
   if(disclosure("Commission details","job:"+m.id,x,y,width)){para(m.cause,muted);if(m.kind==EscortMission)para("Actual freight must arrive with at least 65% of its commissioned cargo. Escort coverage requires an undocked ship, clear line of sight and proximity to the named trip.",warning);button("Abandon / return escrow",{x,y,width,28},"abortMission",m.id);y+=38;}
  }
  y+=12;++count;
 }
 if(!count)para("No commissions available here. Offers refresh with six-hour simulation shifts as conditions change.",muted);
 if(disclosure("Resolved missions","resolved-missions",x,y,width)){int shown=0;for(auto it=e.missions.rbegin();it!=e.missions.rend()&&shown<8;++it)if(it->state>=MissionComplete){para(it->title+" / "+missionStateName(it->state),muted);para(it->result,muted);++shown;}}
}
}
