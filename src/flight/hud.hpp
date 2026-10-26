#pragma once
#include "game.hpp"
#include "../screen_layout.hpp"
namespace hyperion::flight {
struct HUDRect {double x=0,y=0,width=0,height=0;};
struct HUDPoint {double x=0,y=0;};
struct HUDLayout {
 double cx,cy;HUDRect attitude,target,bearing;bool visible;
 HUDLayout(double width,double height,bool panel=false,bool docked=false):cx(width*.5),cy(height*.5),visible(!panel&&!docked){
  double inset=std::clamp(std::min(width,height)*.04,24.,40.),deck=height-inset-104;
  attitude={cx-124,cy-62,248,124};target={cx-96,std::min(cy+78,deck-88),192,40};
  bearing={cx-245,cy-82,490,146};
 }
};
inline bool hudContains(HUDRect r,HUDPoint p){return p.x>=r.x&&p.x<=r.x+r.width&&p.y>=r.y&&p.y<=r.y+r.height;}
// Liang-Barsky clipping keeps rolled ladder segments inside their clear area.
inline bool hudClip(HUDRect r,HUDPoint& a,HUDPoint& b){
 double dx=b.x-a.x,dy=b.y-a.y,lo=0,hi=1;
 const double p[]={-dx,dx,-dy,dy},q[]={a.x-r.x,r.x+r.width-a.x,a.y-r.y,r.y+r.height-a.y};
 for(int i=0;i<4;i++){if(std::abs(p[i])<1e-12){if(q[i]<0)return false;}else{double t=q[i]/p[i];if(p[i]<0)lo=std::max(lo,t);else hi=std::min(hi,t);if(lo>hi)return false;}}
 b={a.x+hi*dx,a.y+hi*dy};a={a.x+lo*dx,a.y+lo*dy};return true;
}
inline HUDPoint hudBearing(HUDRect r,HUDPoint centre,double x,double y){
 if(!std::isfinite(x)||!std::isfinite(y)||std::hypot(x,y)<1e-8){x=0;y=1;}
 double t=1e30;if(x>1e-9)t=std::min(t,(r.x+r.width-centre.x)/x);else if(x<-1e-9)t=std::min(t,(r.x-centre.x)/x);
 if(y>1e-9)t=std::min(t,(r.y+r.height-centre.y)/y);else if(y<-1e-9)t=std::min(t,(r.y-centre.y)/y);
 return {std::clamp(centre.x+x*t,r.x,r.x+r.width),std::clamp(centre.y+y*t,r.y,r.y+r.height)};
}
struct HUDTelemetry {
 bool surface=false,headingAvailable=false,polar=false,brake=false;
 double pitch=0,roll=0,heading=0,speed=0,vertical=0,drift=0,navRange=0,navAlong=0,stop=0,spin=0;
 V3 navigation;
};
inline HUDTelemetry hudTelemetry(const Game& f,const LandingInfo& ground,double stoppingDistance){
 HUDTelemetry m;m.speed=f.speed();m.stop=std::max(0.,stoppingDistance);m.spin=length(f.ship.angularVelocity)*180/pi;
 m.surface=ground.available&&ground.altitude<350;
 auto radial=unit(ground.radial),right=unit(cross(f.ship.forward,f.ship.up));
 m.pitch=std::asin(std::clamp(dot(f.ship.forward,radial),-1.,1.))*180/pi;
 m.roll=std::atan2(dot(radial,right),dot(radial,f.ship.up));m.vertical=-ground.sink;
 m.drift=m.speed>.2?std::acos(std::clamp(dot(unit(f.ship.velocity),f.ship.forward),-1.,1.))*180/pi:0;
 auto north=V3{0,1,0}-radial*radial.y,flat=f.ship.forward-radial*dot(f.ship.forward,radial);
 m.polar=length(north)<1e-7;m.headingAvailable=!m.polar&&length(flat)>1e-7;
 if(m.headingAvailable){north=unit(north);auto east=unit(cross(north,radial));m.heading=std::fmod(std::atan2(dot(flat,east),dot(flat,north))*180/pi+360.,360.);}
 if(f.target>=0&&f.target<int(f.ports.size())){
  const auto& port=f.ports[f.target];m.navigation=port.position;
  if(port.minor<0)m.navigation+=port.normal*(port.planet<0?90.:.25);
  auto delta=m.navigation-f.ship.position;m.navRange=length(delta);m.navAlong=m.navRange>1e-8?dot(f.ship.velocity,delta/m.navRange):0;
  m.brake=m.speed>3&&m.navAlong>m.speed*.35&&m.stop>std::max(0.,m.navRange-2.);
 }
 return m;
}
inline double hudClosing(V3 offset,V3 observerVelocity,V3 targetVelocity){double d=length(offset);return d>1e-8?dot(observerVelocity-targetVelocity,offset/d):0;}
// Contextual cues supplement the lower deck without repeating its readings.
struct HUDCues {bool horizon=false,vector=false,braking=false;};
inline HUDCues hudCues(const HUDTelemetry& m,const LandingInfo& ground,bool assisted,bool navigator,bool launching,bool combat){
 return {m.surface&&(ground.altitude<80||launching)&&!combat,
         m.speed>3&&(m.drift>3||!assisted),m.brake&&!navigator&&!combat};
}
}
