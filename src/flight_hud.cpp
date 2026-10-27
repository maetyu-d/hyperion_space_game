#include "renderer.hpp"
#include "flight/hud.hpp"
#include <cmath>
#include <cstdio>
namespace hyperion {
namespace {
using namespace flight;
constexpr Color mint{153,255,218,210},ice{157,220,255,220},amber{255,202,139,230},pink{255,113,184,235},pearl{234,244,231,235},muted{169,206,214,215};
std::string number(double value,int decimals=0){char b[80];std::snprintf(b,sizeof(b),"%.*f",decimals,value);return b;}
Vector2 point(HUDPoint p){return{float(p.x),float(p.y)};}
void stroke(HUDPoint a,HUDPoint b,Color tint,float width=1){DrawLineEx(point(a),point(b),width+2,Color{3,10,18,120});DrawLineEx(point(a),point(b),width,tint);}
}
void hyperion::Renderer::flightDirector(const flight::LandingInfo& landing,double stopping){
 using namespace flight;const auto& f=game->s.flight;HUDLayout layout(w,h,ui->panel,f.docked);
 if(!ui->enhancedHUD||!layout.visible)return;
 const auto m=hudTelemetry(f,landing,stopping);const double cx=layout.cx,cy=layout.cy;
 const int selected=f.combatTargetIndex();const bool enemy=selected>=0;
 const auto cues=hudCues(m,landing,f.ship.assist,f.autopilot,f.launchAssist>0,enemy);
 auto read=[&](const std::string& s,double x,double y,double width,float size,Color tint){fitted(s,float(x+1),float(y+1),float(width),size,Color{1,8,16,245},true);fitted(s,float(x),float(y),float(width),size,tint,true);};
 // Two true-horizon strokes replace the ladder and compass during close flight.
 if(cues.horizon){
  const double co=std::cos(m.roll),si=std::sin(m.roll),offset=std::tan(m.pitch*flight::pi/180)*(h/1.400415);
  for(int side:{-1,1}){HUDPoint a{cx+si*offset+co*32*side,cy+co*offset-si*32*side},b{cx+si*offset+co*58*side,cy+co*offset-si*58*side};if(hudClip(layout.attitude,a,b))stroke(a,b,Color{153,255,218,130});}
 }
 // Velocity gets a mark only when it adds information beyond the aiming mark.
 if(cues.vector){auto direction=unit(f.ship.velocity);const bool retro=dot(direction,f.ship.forward)<0;direction=retro?-direction:direction;
  if(dot(direction,f.ship.forward)>.05){auto q=flightGraphics.project(f.ship.position+direction*900,w,h);const bool clipped=!hudContains(layout.attitude,{q.x,q.y});
   const auto at=clipped?hudBearing(layout.attitude,{cx,cy},q.x-cx,q.y-cy):HUDPoint{q.x,q.y};const Color tint=retro?amber:mint;
   if(std::hypot(at.x-cx,at.y-cy)>10||retro){DrawCircleLines(int(at.x),int(at.y),4,tint);stroke({at.x-9,at.y},{at.x-5,at.y},tint);stroke({at.x+5,at.y},{at.x+9,at.y},tint);stroke({at.x,at.y-8},{at.x,at.y-5},tint);
    if(retro){stroke({at.x-2,at.y-2},{at.x+2,at.y+2},tint);stroke({at.x+2,at.y-2},{at.x-2,at.y+2},tint);read("RETRO",at.x+12,at.y-4,42,8,tint);}
   }
  }
 }
 const V3 target=enemy?f.apparentPosition(f.ship.position,f.traffic[selected].position):m.navigation,offset=target-f.ship.position;
 const Color targetTint=enemy?(f.traffic[selected].hostile?pink:ice):mint;
 if(length(offset)>1){const bool ahead=flightGraphics.inFront(target);Vector2 q{};if(ahead)q=flightGraphics.project(target,w,h);
  if(ahead&&hudContains(layout.bearing,{q.x,q.y})){if(!enemy&&f.lineOfSight(f.ship.position,target)){stroke({q.x,q.y-5},{q.x+5,q.y},mint);stroke({q.x+5,q.y},{q.x,q.y+5},mint);stroke({q.x,q.y+5},{q.x-5,q.y},mint);stroke({q.x-5,q.y},{q.x,q.y-5},mint);}}
  else{
   const auto rightAxis=unit(cross(f.ship.forward,f.ship.up));auto visual=flightGraphics.visualPosition(target)-f.ship.position;const auto at=hudBearing(layout.bearing,{cx,cy},dot(visual,rightAxis),-dot(visual,f.ship.up));HUDPoint direction{at.x-cx,at.y-cy};const double len=std::max(1.,std::hypot(direction.x,direction.y));direction.x/=len;direction.y/=len;const HUDPoint across{-direction.y*4,direction.x*4};
   stroke({at.x-direction.x*7+across.x,at.y-direction.y*7+across.y},at,targetTint);stroke(at,{at.x-direction.x*7-across.x,at.y-direction.y*7-across.y},targetTint);
   // Navigation already has a name and range on the lower rail.
   if(enemy&&!ahead)read("AFT",at.x<cx-60?at.x-34:at.x+10,at.y-4,30,8,targetTint);
  }
 }
 if(f.damageCue>0)return;
 if(enemy){const auto& t=f.traffic[selected];const auto box=layout.target;
  read(t.name.empty()?enemyDef(t).name:t.name,box.x,box.y,box.width,11,pearl);
  read(t.charge>.05?"CHARGING / "+std::string(weaponDef(std::max(0,t.chargeWeapon)).name):number(length(offset))+" km / "+(f.lock>=1?"LOCK":"TRACK "+number(f.lock*100)+"%"),box.x,box.y+16,box.width,9,t.charge>.05?pink:muted);
  const double sh=std::clamp(t.shield/std::max(1.,t.kind==1?enemyDef(t).shield:100.),0.,1.),hp=std::clamp(t.health/std::max(1.,t.kind==1?enemyDef(t).hull:100.),0.,1.);
  read("S",box.x,box.y+31,9,8,ice);read("H",box.x+104,box.y+31,9,8,targetTint);
  stroke({box.x+14,box.y+35},{box.x+88,box.y+35},Color{43,65,78,190},2);if(sh>0)stroke({box.x+14,box.y+35},{box.x+14+74*sh,box.y+35},ice,2);
  stroke({box.x+118,box.y+35},{box.x+192,box.y+35},Color{43,65,78,190},2);if(hp>0)stroke({box.x+118,box.y+35},{box.x+118+74*hp,box.y+35},targetTint,2);
 }else if(cues.braking)read("BRAKE",cx-22,layout.target.y,44,10,amber);
}
}
