#include "renderer.hpp"
#include "wreck_interior.hpp"
#include "destruction.hpp"
#include "rlgl.h"
#include "settlement_detail.hpp"
#include "structural_fracture.hpp"
#include "world_objects.hpp"
#include <cstring>
#include <stdexcept>

namespace hyperion::flight {
namespace {
using architecture::Frame;using architecture::Builder;
Vector3 vec(V3 v){return {float(v.x),float(v.y),float(v.z)};}
Color colour(const std::array<unsigned char,3>& c,int code=0){return {c[0],c[1],c[2],static_cast<unsigned char>(code)};}
Color coded(Color c,int code){c.a=static_cast<unsigned char>(code);return c;}
constexpr std::array<Color,9> cargoColours{{{129,107,85,0},{125,164,191,0},{188,121,255,0},{89,179,105,0},{240,197,105,0},{108,238,220,0},{119,166,246,0},{245,137,83,0},{201,174,255,0}}};
void uniform(Shader shader,const char* name,float value){int loc=GetShaderLocation(shader,name);if(loc>=0)SetShaderValue(shader,loc,&value,SHADER_UNIFORM_FLOAT);}
void uniform(Shader shader,const char* name,V3 value){auto v=vec(value);int loc=GetShaderLocation(shader,name);if(loc>=0)SetShaderValue(shader,loc,&v,SHADER_UNIFORM_VEC3);}
void structureState(Shader shader,const FactionStyle& style,V3 eye,V3 sun,double seconds,const SettlementView* view){
 uniform(shader,"fracturePivot",V3{});uniform(shader,"fractureHeat",0.f);uniform(shader,"eye",eye);uniform(shader,"lightDirection",sun);uniform(shader,"time",float(seconds));
 uniform(shader,"accent",V3{style.accent[0]/255.,style.accent[1]/255.,style.accent[2]/255.});
 uniform(shader,"materialStyle",float(style.silhouette));
 // Shared shader state must be reset before every port or craft draw.
 uniform(shader,"craftScale",0.f);uniform(shader,"craftIdentity",0.f);uniform(shader,"serviceWear",0.f);
 uniform(shader,"condition",float(view?view->condition:1));uniform(shader,"pollution",float(view?view->pollution:0));uniform(shader,"ecology",float(view?view->ecology:1));
 uniform(shader,"powerOn",float(!view||view->power>0?1.:.12));
 std::array<float,14> activity{};for(int i=0;i<14;i++)activity[i]=float(view?view->activity[i]:1);
 int loc=GetShaderLocation(shader,"activity[0]");if(loc>=0)SetShaderValueV(shader,loc,activity.data(),SHADER_UNIFORM_FLOAT,14);
}
void emblem(Builder& m,int faction,V3 at,double s,Color tint){
 if(faction==0){m.beam(at+V3{-s,0,0},at+V3{0,s*1.5,0},.18,tint);m.beam(at+V3{0,s*1.5,0},at+V3{s,0,0},.18,tint);}
 else if(faction==1){for(int i=0;i<3;i++)m.box(at+V3{(i-1)*s*.65,s*.5,0},{s*.32,s, .3},tint);}
 else if(faction==2){m.beam(at+V3{-s,0,0},at+V3{0,s,0},.18,tint);m.beam(at+V3{0,s,0},at+V3{s,0,0},.18,tint);m.beam(at+V3{s,0,0},at+V3{0,-s*.5,0},.18,tint);m.beam(at+V3{0,-s*.5,0},at+V3{-s,0,0},.18,tint);}
 else if(faction==3){m.box(at,{s*1.9,.25,.35},tint);m.box(at+V3{0,s*.65,0},{.25,s*1.5,.35},tint);for(int side:{-1,1})m.beam(at+V3{side*s*.8,0,0},at+V3{side*s*.6,-s*.7,0},.15,tint);}
 else{for(int i=0;i<3;i++)m.box(at+V3{0,i*s*.48,0},{s*(1.5-i*.35),.20,.30},tint);}
}
void roofUnit(Builder& m,V3 p,V3 size,Color metal,int code){
 m.bevelBox(p,size,metal,.12);m.box(p+V3{0,size.y*.5+.025,0},{size.x*.78,.06,size.z*.72},coded({28,38,45,0},224+code));
}
void domeFrame(Builder& m,V3 c,V3 scale,Color metal){
 for(int rib=0;rib<6;rib++){double phi=rib*2*pi/6;for(int j=0;j<6;j++){double a=j*pi/12,b=(j+1)*pi/12;
  auto p=[&](double t){return c+V3{std::sin(t)*std::cos(phi)*scale.x,std::cos(t)*scale.y,std::sin(t)*std::sin(phi)*scale.z};};m.beam(p(a),p(b),.065,metal,4);
 }}
 for(int row=1;row<3;row++){double t=row*pi/6;for(int i=0;i<24;i++){double a=i*2*pi/24,b=(i+1)*2*pi/24;m.beam(c+V3{std::cos(a)*scale.x*std::sin(t),scale.y*std::cos(t),std::sin(a)*scale.z*std::sin(t)},c+V3{std::cos(b)*scale.x*std::sin(t),scale.y*std::cos(t),std::sin(b)*scale.z*std::sin(t)},.055,metal,4);}}
}
void dish(Builder& m,V3 c,double r,double depth,Color tint){
 auto p=[&](double u,double a){return c+V3{std::cos(a)*r*u,depth*u*u,std::sin(a)*r*u};};
 auto n=[&](double u,double a){return unit(V3{-2*depth*u*std::cos(a)/r,1,-2*depth*u*std::sin(a)/r});};
 for(int j=0;j<4;j++)for(int i=0;i<24;i++){double a=i*2*pi/24,b=(i+1)*2*pi/24,u=j/4.,v=(j+1)/4.;
  auto A=p(u,a),B=p(u,b),C=p(v,b),D=p(v,a),na=n(u,a),nb=n(u,b),nc=n(v,b),nd=n(v,a);
  m.triangleNormals(A,B,C,na,nb,nc,tint);m.triangleNormals(A,C,D,na,nc,nd,tint);
  m.triangleNormals(A,C,B,-na,-nc,-nb,tint);m.triangleNormals(A,D,C,-na,-nd,-nc,tint);
 }
}
void detailFacility(Builder& m,const FacilitySite& site,const FactionStyle& style,int faction,bool close){
 int k=site.kind,code=k+1;Color metal=colour(style.hull,code),trim=coded({105,122,132,0},code),glass=colour(style.secondary,32+code),dark=coded({25,34,41,0},224+code),paint=coded({192,199,184,0},192+code);
 double h=style.height,w=style.width;
 if(k==Habitat||k==Medical||k==Fabricator||k==Warehouse||k==Observatory)architecture::entrance(m,k,style,site.ordinal,close);
 if(k==Habitat){double height=(8+(site.occupants/4)*4)*h;
  if(faction!=2&&faction!=4){
   for(int side:{-1,1}){m.box({side*(7*w+.10),height*.5,0},{.35,height-1,13.4},trim);roofUnit(m,{side*3.,height+.7,1},{3,1.4,4.5},metal,code);}
   m.box({0,height-.6,-7.25},{14*w,.38,.5},trim);
  }else if(faction==2)domeFrame(m,{}, {8*w+.04,height+.04,8.04},trim);
 }
 if(k==Garden){domeFrame(m,{0,2,0},{7.55,7.05,8.05},trim);m.bevelBox({0,3,-9.3},{4.8,5.6,4.4},metal,.28);m.box({0,3,-11.55},{3.2,3.9,.12},glass);}
 if(k==Smelter){
  for(int side:{-1,1}){m.box({side*5.3,5,-7.4},{.8,6.5,.8},trim);m.beam({side*4.,9,4},{side*7.,9,7},.45,metal,8);}
  m.box({0,8.35,-7.4},{11.4,.75,.8},trim);roofUnit(m,{-4,12.6,-3},{4,1.2,4},metal,code);
 }
 if(k==Extractor){for(int j=0;j<3;j++){double y=7+j*5*h,x=6*(1.-(y-5)/(25*h-5));for(int side:{-1,1})m.beam({-x,y,side*x},{x*.65,y+4*h,side*x*.65},.15,trim,4);}m.bevelBox({7,3,0},{3,5,6},metal);}
 if(k==Bore||k==Harvester){roofUnit(m,{7,2,2},{3,4,5},metal,code);m.beam({7,1,2},{2,1,2},.25,trim,6);}
 if(k==Fabricator){
  m.box({0,5,-8.2},{12,8,.40},dark);m.bevelBox({0,10,-8.9},{15,1.1,3.3},metal,.13);
  for(int side:{-1,1})m.bevelBox({side*6.8,4.5,-9.1},{.8,9,.8},trim,.10);
  roofUnit(m,{-4,14*h+1.2,1},{3.8,2.4,6},metal,code);
 }
 if(k==Fuelworks){for(int side:{-1,1}){m.beam({side*6.,2,-4},{side*6.,6,-4},.32,trim,8);m.cylinder({side*6.,5.5,-4},.65,.5,paint,8);}}
 if(k==Reactor){for(int i=0;i<6;i++){double a=i*2*pi/6;m.bevelBox({std::cos(a)*6.4,10,std::sin(a)*6.4},{.65,12,.8},trim,.08);}roofUnit(m,{8,2,0},{3,4,6},metal,code);}
 if(k==Observatory){m.ring({0,12*h+2.4,0},8,.09,trim,32);for(int side:{-1,1})m.beam({side*6.,12*h+1,0},{0,12*h+5,0},.09,trim,4);}
 if(k==Warehouse){
  m.facingQuad({-8.8,10,-10},{0,12,-10},{0,12,10},{-8.8,10,10},metal,{-1,1,0});m.facingQuad({0,12,-10},{8.8,10,-10},{8.8,10,10},{0,12,10},metal,{1,1,0});
  m.box({0,5,-9.23},{11,8,.15},dark);m.bevelBox({0,9.7,-10},{13,.8,2},trim,.10);roofUnit(m,{4,11.4,3},{3,1.4,4},metal,code);
 }
 if(k==Medical){for(int side:{-1,1})m.beam({side*5.,1,3},{side*5.,8*h,3},.22,trim,6);}
}
template<class Surface> void pavedRoad(Builder& m,const Surface& surface,V3 direction,double reach,bool distant){
 V3 across{-direction.z,0,direction.x};double step=distant?12:4;
 auto point=[&](double r,double x,double lift){auto f=surface(direction*r+across*x);return f.origin+f.y*lift;};
 for(double r=44;r<reach;r+=step){double end=std::min(reach,r+step);
  m.facingQuad(point(r,-3,.20),point(end,-3,.20),point(end,3,.20),point(r,3,.20),coded({49,62,69,0},128),{0,1,0});
  if(!distant){
   for(int side:{-1,1})m.facingQuad(point(r,side*2.65,.23),point(end,side*2.65,.23),point(end,side*2.95,.23),point(r,side*2.95,.23),coded({129,143,141,0},192),{0,1,0});
   if(int((r-44)/step)%3==0)m.facingQuad(point(r,-.11,.235),point(end,-.11,.235),point(end,.11,.235),point(r,.11,.235),coded({197,181,111,0},192),{0,1,0});
  }
 }
}
void freightContainer(Builder& m,V3 p,Color body,bool distant){
 m.bevelBox(p,{7,2.4,2.3},body,.12);if(distant)return;
 Color frame=coded({72,88,101,0},0),label=coded({208,215,190,0},192);
 for(int side:{-1,1}){m.box(p+V3{side*2.65,0,0},{.16,2.44,2.34},frame);m.box(p+V3{side*3.38,0,-1.18},{.19,2.16,.12},frame);}
 m.box(p+V3{0,0,-1.19},{1.25,.55,.08},label);
}
void facility(Builder& m,const FacilitySite& site,const FactionStyle& style,int faction,bool distant,bool close){
 int k=site.kind,code=k+1;Color metal=colour(style.hull,code),light=colour(style.accent,64+code),glass=colour(style.secondary,32+code);
 double h=style.height,w=style.width;
 m.cylinder({0,-.95,0},11,1.05,coded({73,85,69,0},128),distant?8:16);
 m.bevelBox({0,.35,0},{18,.7,20},coded({56,65,69,0},code));
 if(distant){double height=(k==Habitat?12+(site.occupants/4)*3.2:k==Solar?4:k==Bore||k==Observatory?22:13)*h;
  if(faction==2||k==Garden)m.sphere({0,0,0},{8*w,height,8},glass,true,6,3);else m.box({0,height*.5,0},{12*w,height,14},metal);m.box({0,height+.1,0},{10*w,.4,6},light);return;}
 switch(k){
 case Extractor:
  m.bevelBox({0,3,0},{14,6,14},metal);for(int side:{-1,1}){m.beam({double(side*6),5,-5},{0,25*h,0},.6,metal);m.beam({double(side*6),5,5},{0,25*h,0},.6,metal);}m.beam({0,25*h,0},{0,6,0},.5,light);m.ring({0,6,0},4,.4,light,16);break;
 case Smelter:
  m.bevelBox({0,6,0},{15,12,14},metal);for(int side:{-1,1}){m.cylinder({double(side*4),10,3},2.2,17*h,metal,8);m.cylinder({double(side*4),10+17*h,3},2.3,.8,light,8);}m.bevelBox({0,5,-7.1},{9,5,.3},coded({255,115,62,0},64+code));break;
 case Bore:
  m.cylinder({0,0,0},6,4,metal,8);m.cylinder({0,4,0},4,24*h,glass,6,.15);for(int i=0;i<3;i++)m.ring({0,7+i*5.,0},5-i*.8,.25,light,16);break;
 case Harvester:
  m.bevelBox({0,4,0},{13,8,15},metal);for(int side:{-1,1}){m.beam({0,7,0},{double(side*9),3,-6},.5,metal);m.sphere({double(side*9),3,-6},{2.5,1,2.5},glass,false,8,4);}break;
 case Garden:
  m.bevelBox({0,1,0},{16,2,17},metal);m.sphere({0,2,0},{7.5,7,8},glass,true,close?32:12,close?12:6);for(int i=-2;i<=2;i++)m.bevelBox({i*2.8,2.4,0},{.8,.6,12},coded({101,203,128,0},160));break;
 case Medical:
  m.cylinder({0,0,0},7,12*h,metal,6);m.sphere({0,12*h,0},{7,3,7},glass,true,close?24:12,close?10:6);m.bevelBox({0,8,-6.2},{1.3,5,.4},light);m.bevelBox({0,8,-6.4},{4.5,1.3,.4},light);break;
 case Fabricator:
  m.bevelBox({0,7,0},{16,14*h,16},metal);for(int i=-1;i<=1;i++)m.bevelBox({i*4.5,14*h,0},{2,2,13},glass);m.bevelBox({0,5,-8.1},{10,7,.25},light);m.beam({-7,17,0},{7,17,0},.5,metal);break;
 case Fuelworks:
  for(int i=-1;i<=1;i++){m.cylinder({i*5.,0,0},2.3,12*h,metal,close?24:16);m.sphere({i*5.,12*h,0},{2.3,2.3,2.3},glass,true,close?24:16,close?10:6);m.ring({i*5.,8*h,0},2.35,.24,light,16);}m.beam({-6,2,-4},{6,2,-4},.5,metal);break;
 case Observatory:
  m.cylinder({0,0,0},4,12*h,metal,8);dish(m,{0,12*h,0},8,2.4,glass);m.beam({0,13,0},{0,25*h,0},.35,metal);m.ring({0,20*h,0},7,.26,light,24);break;
 case Solar:
  for(int side:{-1,1}){
   m.beam({double(side*4),0,0},{double(side*4),5,0},.35,metal);auto original=m.frame;double tilt=.22;
   m.frame.origin=original.point({side*4.5,5,0});m.frame.y=original.y*std::cos(tilt)+original.z*std::sin(tilt);m.frame.z=original.z*std::cos(tilt)-original.y*std::sin(tilt);
   m.box({},{7,.55,17},coded({27,63,118,0},32+code));
   for(int i=-3;i<=3;i++)m.box({0,.30,i*2.3},{6.8,.06,.08},metal);m.box({0,.31,0},{.10,.06,16.7},metal);m.frame=original;
  }break;
 case Reactor:
  m.cylinder({0,0,0},6,17*h,metal,10);m.cylinder({0,17*h,0},5,3,glass,10,2);for(int i=0;i<3;i++)m.ring({0,5+i*4.,0},6.3,.28,light,24);break;
 case Habitat:{
  double height=(8+(site.occupants/4)*4)*h;
  if(faction==2){m.sphere({0,0,0},{8*w,height,8},glass,true,close?32:12,close?12:6);m.ring({0,height*.35,0},8*w,.4,metal,24);}
  else if(faction==4){m.cylinder({0,0,0},7*w,height,metal,4,3);m.cylinder({0,height,0},3,6,glass,4,0);}
  else{m.bevelBox({0,height*.5,0},{14*w,height,14},metal);if(faction==1)m.bevelBox({0,height*.8,0},{9*w,height*.6,9},metal);if(faction==3)for(int side:{-1,1})m.bevelBox({side*5.,height*.65,0},{3,height,12},glass);}
  architecture::habitatWindows(m,site,style,faction,close);break;}
 case Warehouse:
  m.bevelBox({0,5,0},{16,10,18},metal);m.bevelBox({0,5,-9.12},{11,8,.2},glass);for(int i=-1;i<=1;i++)m.bevelBox({i*4.5,10.4,0},{.5,.8,18},light);break;
 case Restoration:
  m.cylinder({0,0,0},6,8,glass,10);for(int side:{-1,1}){m.beam({side*6.,0,0},{side*6.,18*h,0},.6,metal);m.sphere({side*6.,18*h,0},{3,3,3},coded({94,211,143,0},160),false,8,4);}m.ring({0,8,0},6.4,.3,light,24);break;
 }
 detailFacility(m,site,style,faction,close);
 if(close)architecture::closeFacility(m,site,style,faction);
 m.box({0,2,-10.96},{2.4,.24,.06},light);
}
void orbitalCore(Builder& m,int faction,int variant,bool far){
 const auto& style=factionStyle(faction);Color metal=colour(style.hull),glow=colour(style.accent,96),secondary=colour(style.secondary,96);
   // Faction silhouettes extend to their orbital bonded warehouses.
   if(faction==1){for(int side:{-1,1})m.box({side*55.,0,0},{28,34,74},metal);m.box({0,0,0},{90,16,16},metal);}
   else if(faction==2){m.sphere({0,0,0},{67,18,67},metal,false,20,8);m.ring({0,12,0},54,2,glow,48);}
   else if(faction==3){for(int side:{-1,1}){m.cylinder({side*55.,-22,0},18,55,metal,6);m.cylinder({side*55.,33,0},18,16,colour(style.secondary,32),6,3);}m.box({0,0,0},{100,12,18},metal);}
   else if(faction==4){m.cylinder({0,-30,0},38,72,metal,4,13);m.cylinder({0,42,0},13,30,colour(style.secondary,32),4,0);m.ring({0,0,0},74,2,glow,40);}
   else{m.box({0,0,0},{58,24,55},metal);m.ring({0,0,0},80,4,metal,48);}
   m.ring({0,-18,0},62,3.5,metal,far?24:48);m.ring({0,-12,0},64,.6,glow,far?24:48);
   m.beam({0,-70,0},{0,70,0},2,metal);m.ring({0,70,0},18,.6,glow,32);m.ring({0,80,0},22,.3,secondary,32);
   if(variant>>4){int design=(variant>>4)-1;
    if(design==0||design==3){for(int side:{-1,1}){m.beam({side*70.,0,0},{side*105.,45,45},1.3,metal);m.ring({side*105.,45,45},23,1,secondary,far?12:32);}}
    if(design==1||design==2){for(int k=0;k<3;k++){double a=k*2*pi/3;m.sphere({std::cos(a)*48,-40,std::sin(a)*48},{12,18,12},metal,false,far?6:12,6);}}
    if(design==4)m.cylinder({0,-80,0},14,24,secondary,6,4);
   }
   int warehouses=variant&15;for(int i=0;i<std::min(warehouses,8);i++){double angle=i*2*pi/std::max(1,warehouses);m.box({std::cos(angle)*78,-22,std::sin(angle)*78},{17,16,22},metal);}
   m.bevelBox({0,26,-26},{22,13,1},metal);emblem(m,faction,{0,25,-26.7},4,glow);
   // Keep the actual docking axis open, with the deck below the existing
   // ninety-metre berth and a separate loading gantry off to its side.
   m.annularDeck({0,88.4,0},20,30,1.35,metal,far?24:64);m.paintArc({0,89.78,0},25,.35,0,2*pi,glow,far?24:64);
   for(int side:{-1,1}){m.bevelBox({side*33.,96,0},{2,15,3},metal,.18);m.box({side*33.,102,0},{2.2,1.2,3.2},secondary);}
   m.bevelBox({0,75,55},{46,2,26},metal,.5);
   for(int side:{-1,1}){m.bevelBox({side*18.,84,55},{1.6,18,1.6},metal,.12);m.beam({side*18.,75,55},{side*18.,22,12},1.1,metal,6);}
   m.bevelBox({0,93,55},{38,1.6,2.6},metal,.18);
   if(!far){for(int side:{-1,1})m.box({side*25.,89.79,0},{.45,.035,14},coded({195,206,196,0},192));
    for(int i=-2;i<=2;i++)roofUnit(m,{i*12.,13,-15},{8,3,9},metal,0);}
}
void stationWing(Builder& m,int faction,int side){m.box({},{68,2,36},coded({27,58,103,0},32));m.beam({-side*81.,0,0},{side*29.,0,0},1.3,colour(factionStyle(faction).hull));}
void landingPad(Builder& m,int faction,bool far){const auto& style=factionStyle(faction);Color metal=colour(style.hull),glow=colour(style.accent,96),secondary=colour(style.secondary,96);
   m.cylinder({0,-1.5,0},38,1.7,coded({52,65,70,0},0),far?24:64,37.4);
   m.paintArc({0,.245,0},34.7,.22,0,2*pi,glow,far?32:80);
   for(int i=0;i<8;i++){double begin=i*2*pi/8+.045,end=(i+1)*2*pi/8-.045;m.paintArc({0,.24,0},31.3,.30,begin,end,coded({177,190,182,0},192),far?3:10);}
   for(int side:{-1,1})m.box({side*7.,.24,0},{.48,.035,18},coded({210,218,202,0},192));m.box({0,.24,0},{14,.035,.48},coded({210,218,202,0},192));
   if(!far){for(int i=0;i<12;i++){double angle=i*2*pi/12;m.paintArc({0,.24,0},36.2,.72,angle,angle+.13,coded({210,169,84,0},192),3);}for(int side:{-1,1})m.bevelBox({side*39.,.55,5},{1.8,1.1,12},metal,.18);}
}
void loadingGantry(Builder& m,int faction,bool far){const auto& style=factionStyle(faction);Color metal=colour(style.hull),glow=colour(style.accent,96),secondary=colour(style.secondary,96);
   m.bevelBox({0,1.2,56},{32,2,20},metal,.25);m.bevelBox({-18,13,53},{1.5,26,1.5},metal,.12);m.bevelBox({18,13,53},{1.5,26,1.5},metal,.12);m.bevelBox({0,26,53},{38,1.4,2.4},metal,.16);
   if(!far){for(int side:{-1,1}){m.bevelBox({side*18.,1.3,53},{5,2.6,5},metal,.20);m.beam({side*18.,4,49},{side*18.,21,54},.22,metal,4);m.box({side*15.2,2.23,56},{.28,.06,17},coded({224,181,82,0},192));}
    m.box({0,27,53},{35,.20,.32},secondary);m.box({0,4.5,45.85},{12,2.2,.12},coded({38,50,59,0},224));}
 if(!far)for(int side:{-1,1})for(int j=0;j<5;j++)m.beam({side*18.,3.+j*4,52},{side*18.,7.+j*4,54},.10,metal,4);
}
void portMast(Builder& m,int faction,bool far){const auto& style=factionStyle(faction);Color metal=colour(style.hull),glow=colour(style.accent,96),secondary=colour(style.secondary,96);
   m.box({-43,17,14},{.8,34,.8},metal);m.box({-43,25,14},{16,10,1.2},metal);emblem(m,faction,{-43,24,13.3},3.1,glow);
}
void portTerminal(Builder& m,int faction,bool far){const auto& style=factionStyle(faction);Color metal=colour(style.hull),glow=colour(style.accent,96),secondary=colour(style.secondary,96);
    m.bevelBox({-17,2.8,67},{5,4.6,3.4},metal,.18);m.mappedQuad({-18.85,1.95,68.76},{-15.15,1.95,68.76},{-15.15,4.05,68.76},{-18.85,4.05,68.76},colour(style.secondary,48+13));
}
void structuralSource(Builder& m,const StructureRemains& f,bool low){
 if(f.profile<14){FacilitySite site;site.kind=f.profile;site.ordinal=f.ordinal;site.occupants=f.variant;facility(m,site,factionStyle(f.faction),f.faction,false,!low);}
 else if(f.profile==StationCore)orbitalCore(m,f.faction,f.variant,false);
 else if(f.profile==StationWing)stationWing(m,f.faction,f.variant);
 else if(f.profile==LandingPad)landingPad(m,f.faction,false);
 else if(f.profile==LoadingGantry)loadingGantry(m,f.faction,false);
 else if(f.profile==PortMast)portMast(m,f.faction,false);
 else if(f.profile==PortTerminal)portTerminal(m,f.faction,false);
 else if(f.profile==FreightCrate)freightContainer(m,{},coded(cargoColours[std::clamp(f.variant,0,8)],17),false);
}

}

GeometryStats Renderer::architectureStats()const{
 GeometryStats result;
 auto inspect=[&](const Model& model,bool distant){result.models++;
  for(int m=0;m<model.meshCount;m++){const auto& mesh=model.meshes[m];(distant?result.distantTriangles:result.detailedTriangles)+=mesh.triangleCount;
   for(int v=0;v<mesh.vertexCount;v++){V3 p{mesh.vertices[v*3],mesh.vertices[v*3+1],mesh.vertices[v*3+2]},n{mesh.normals[v*3],mesh.normals[v*3+1],mesh.normals[v*3+2]};if(!finite(p))result.invalidVertices++;if(!finite(n)||std::abs(length(n)-1)>.01)result.invalidNormals++;}
   for(int t=0;t<mesh.triangleCount;t++){V3 p[3];for(int k=0;k<3;k++){int v=mesh.indices?mesh.indices[t*3+k]:t*3+k;p[k]={mesh.vertices[v*3],mesh.vertices[v*3+1],mesh.vertices[v*3+2]};}if(length(cross(p[1]-p[0],p[2]-p[0]))<1e-8)result.degenerateTriangles++;}
  }
 };
 for(const auto& [id,asset]:portAssets){inspect(asset.close,false);result.closeTriangles+=asset.close.meshes[0].triangleCount;inspect(asset.detailed,false);inspect(asset.distant,true);if(asset.detail==2)result.closePorts++;}for(const auto& [id,model]:craftAssets)inspect(model,false);for(const auto& [id,a]:brokenStructures){for(const auto& m:a.parts)inspect(m,false);for(const auto& m:a.interiors)if(m.meshCount)inspect(m,false);}for(const auto& [key,h]:fracturedHulls)for(const auto& m:h.interiors)if(m.meshCount)inspect(m,false);return result;
}

Renderer::FracturedHull Renderer::buildFracturedHull(const HullRemains& hull){
 auto source=buildCraft(hull.faction,hull.model,0,0);const auto& mesh=source.meshes[0];FracturedHull result;
 std::array<Builder,5> builders;std::array<int,5> counts{};
 auto vertex=[&](int i){return V3{mesh.vertices[i*3],mesh.vertices[i*3+1],mesh.vertices[i*3+2]};};
 for(int triangle=0;triangle<mesh.triangleCount;triangle++){
  int indices[3];for(int k=0;k<3;k++)indices[k]=mesh.indices?mesh.indices[triangle*3+k]:triangle*3+k;
  auto centre=(vertex(indices[0])+vertex(indices[1])+vertex(indices[2]))/3.;int part=hullFragment(centre);auto& b=builders[part];
  for(int index:indices){auto p=vertex(index);result.pivots[part]+=p;counts[part]++;for(double v:{p.x,p.y,p.z})b.vertices.push_back(float(v));
   for(int k=0;k<3;k++)b.normals.push_back(mesh.normals[index*3+k]);for(int k=0;k<2;k++)b.uv.push_back(mesh.texcoords[index*2+k]);for(int k=0;k<4;k++)b.colours.push_back(mesh.colors[index*4+k]);}
 }
 for(int part=0;part<5;part++)if(counts[part]){
  auto& b=builders[part];result.pivots[part]=result.pivots[part]/counts[part];
  for(size_t i=0;i<b.vertices.size();i+=3){b.vertices[i]-=float(result.pivots[part].x);b.vertices[i+1]-=float(result.pivots[part].y);b.vertices[i+2]-=float(result.pivots[part].z);}
  auto model=b.model(structure);model.materials[0].maps[MATERIAL_MAP_NORMAL].texture=buildingTexture;model.materials[0].maps[MATERIAL_MAP_METALNESS].texture=buildingDetailTexture;model.materials[0].maps[MATERIAL_MAP_OCCLUSION].texture=sunScene.texture;result.triangles+=model.meshes[0].triangleCount;result.parts[part]=model;
  V3 low{1e30,1e30,1e30},high{-1e30,-1e30,-1e30};for(size_t i=0;i<b.vertices.size();i+=3){V3 p{b.vertices[i],b.vertices[i+1],b.vertices[i+2]};low={std::min(low.x,p.x),std::min(low.y,p.y),std::min(low.z,p.z)};high={std::max(high.x,p.x),std::max(high.y,p.y),std::max(high.z,p.z)};}auto inside=architecture::hullInterior(low,high,hull.seed,part);if(!inside.vertices.empty()){auto m=inside.model(structure);m.materials[0].maps[MATERIAL_MAP_NORMAL].texture=buildingTexture;m.materials[0].maps[MATERIAL_MAP_METALNESS].texture=buildingDetailTexture;m.materials[0].maps[MATERIAL_MAP_OCCLUSION].texture=sunScene.texture;result.interiors[part]=m;}
 }
 if(result.triangles!=mesh.triangleCount)throw std::runtime_error("Fractured hull lost geometry");UnloadModel(source);return result;
}
void Renderer::drawRemains(const HullRemains& hull,V3 at,double age){
 auto key=std::make_pair(hull.faction,hull.model);auto it=fracturedHulls.find(key);
 if(it==fracturedHulls.end()){
  // At most twelve nearby wrecks are drawn, with a bounded cache across battles.
  if(fracturedHulls.size()>=24){auto first=fracturedHulls.begin();for(auto& part:first->second.parts)if(part.meshCount)UnloadModel(part);for(auto& part:first->second.interiors)if(part.meshCount)UnloadModel(part);fracturedHulls.erase(first);}
  it=fracturedHulls.emplace(key,buildFracturedHull(hull)).first;
 }
 structureState(structure,factionStyle(hull.faction),{camera.position.x,camera.position.y,camera.position.z},sunDirection,visualTime,nullptr);
 uniform(structure,"condition",.08f);uniform(structure,"powerOn",float(std::exp(-age*3)*.3));uniform(structure,"craftScale",float(hull.size));uniform(structure,"craftIdentity",float(hull.seed%65521)/65521.f);uniform(structure,"serviceWear",.9f);uniform(structure,"useSunShadow",0.f);uniform(structure,"structureAir",0.f);uniform(structure,"skyRadial",V3{0,1,0});
 std::array<float,14> disabled{};int activityLoc=GetShaderLocation(structure,"activity[0]");if(activityLoc>=0)SetShaderValueV(structure,activityLoc,disabled.data(),SHADER_UNIFORM_FLOAT,14);
 auto base=debrisPose(hull,0,0);debrisHulls++;
 for(int part=0;part<5;part++){auto& model=it->second.parts[part];if(!model.meshCount)continue;auto pose=debrisPose(hull,part,age);
  auto r=pose.right,u=pose.up,b=pose.back;model.transform={float(r.x),float(u.x),float(b.x),0,float(r.y),float(u.y),float(b.y),0,float(r.z),float(u.z),float(b.z),0,0,0,0,1};
  auto centre=at+debrisPoint(base,it->second.pivots[part])*hull.size+pose.offset;uniform(structure,"fracturePivot",it->second.pivots[part]);uniform(structure,"fractureHeat",reduced?0.f:float(std::exp(-age*.32)));drawVectorModel(model,vec(centre),float(hull.size));if(age>.01&&it->second.interiors[part].meshCount){auto& inside=it->second.interiors[part];inside.transform=model.transform;drawVectorModel(inside,vec(centre),float(hull.size));}debrisParts++;
 }
}
void Renderer::drawShipRemains(const Game& game){
 debrisHulls=debrisParts=0;for(const auto& [id,d]:game.worldDamage)if(d.recovery.valid&&d.recovery.shipModel>=0&&objectIntegrity(game.worldDamage,"wreck:world:"+id)>0&&length(d.position-game.ship.position)<1700&&dot(d.position-game.ship.position,game.ship.forward)>-50){const auto& r=d.recovery;HullRemains hull{true,r.faction,r.shipModel,r.shipSeed,d.scale,r.shipForward};drawRemains(hull,d.position,r.intact?0:std::max(0.,(presentation->hours-r.born)*55.));}
 std::vector<const Wreck*> near;
 for(const auto& wreck:game.wrecks)if(std::none_of(game.worldDamage.begin(),game.worldDamage.end(),[&](const auto& pair){return pair.second.recovery.valid&&pair.second.recovery.shipId==wreck.id;})&&wreck.debrisKind==0&&wreck.id.rfind("world:",0)!=0&&objectIntegrity(game.worldDamage,"wreck:"+wreck.id)>0&&length(wreck.position-game.ship.position)<1700&&dot(wreck.position-game.ship.position,game.ship.forward)>-50)near.push_back(&wreck);
 std::stable_sort(near.begin(),near.end(),[&](auto a,auto b){return length(a->position-game.ship.position)<length(b->position-game.ship.position);});
 for(size_t i=0;i<std::min(size_t(12),near.size());i++){
  const auto& wreck=*near[i];auto hull=wreck.hull;
  if(!hull.valid){auto actor=std::find_if(game.traffic.begin(),game.traffic.end(),[&](const auto&t){return t.id==wreck.id;});if(actor!=game.traffic.end())hull=hullRemains(*actor);else{uint32_t seed=2166136261u;for(unsigned char c:wreck.id)seed=(seed^c)*16777619u;hull={true,int(seed%5),3+int(seed%4),seed,8,{0,0,-1}};}}
  drawRemains(hull,wreck.position,std::max(0.,180-wreck.life));
 }
 // Civilian losses may have no salvage entry, but still retain their own hull.
 for(const auto& e:game.effects)if(e.kind==ShipExplosion&&e.hull.valid&&debrisHulls<12&&length(e.position-game.ship.position)<1700&&dot(e.position-game.ship.position,game.ship.forward)>-50){
  bool hasWreck=std::any_of(game.wrecks.begin(),game.wrecks.end(),[&](const auto&w){return w.id==e.actor;})||std::any_of(game.worldDamage.begin(),game.worldDamage.end(),[&](const auto& p){return p.second.recovery.valid&&p.second.recovery.shipId==e.actor;});if(!hasWreck)drawRemains(e.hull,e.position,e.age);
 }
}
double Renderer::structureAge(const Game& game,const StructureRemains& shape)const{
 if(shape.born<0)return 20;return std::max({0.,(presentation->hours-shape.born)*55.,shape.localBorn>=0?game.time-shape.localBorn:0.});
}
Renderer::BrokenStructure Renderer::buildBrokenStructure(const StructureRemains& shape){
 Builder source;structuralSource(source,shape,false);std::vector<FractureVertex> vertices;vertices.reserve(source.vertices.size()/3);
 for(size_t i=0;i<source.vertices.size()/3;i++){FractureVertex v;v.position={source.vertices[i*3],source.vertices[i*3+1],source.vertices[i*3+2]};v.normal={source.normals[i*3],source.normals[i*3+1],source.normals[i*3+2]};v.uv={source.uv[i*2],source.uv[i*2+1]};for(int k=0;k<4;k++)v.colour[k]=source.colours[i*4+k];vertices.push_back(v);}
 auto fracture=fractureStructure(vertices,shape.profile,shape.seed,shape.localized?&shape:nullptr);BrokenStructure asset;asset.sourceTriangles=fracture.sourceTriangles;asset.sourceArea=fracture.sourceArea;asset.partArea=fracture.fracturedArea;
 for(int cell=0;cell<int(fracture.parts.size());cell++){const auto& part=fracture.parts[cell];if(part.vertices.empty())continue;asset.indices.push_back(cell);Builder m;auto pivot=part.bounds.centre();for(const auto& v:part.vertices){auto p=v.position-pivot;for(double n:{p.x,p.y,p.z})m.vertices.push_back(float(n));for(double n:{v.normal.x,v.normal.y,v.normal.z})m.normals.push_back(float(n));for(float n:v.uv)m.uv.push_back(n);for(auto n:v.colour)m.colours.push_back(n);}
  auto model=m.model(structure);model.materials[0].maps[MATERIAL_MAP_NORMAL].texture=buildingTexture;model.materials[0].maps[MATERIAL_MAP_METALNESS].texture=buildingDetailTexture;model.materials[0].maps[MATERIAL_MAP_OCCLUSION].texture=sunScene.texture;
  asset.parts.push_back(model);asset.bounds.push_back(part.bounds);asset.triangles+=int(part.vertices.size()/3);asset.caps+=part.capTriangles;
  auto inside=architecture::fractureInterior(part,shape.seed^uint32_t(cell*997),lowGraphics?2:extremeGraphics?12:6);Model detail{};if(!inside.vertices.empty()){for(size_t i=0;i<inside.vertices.size();i+=3){inside.vertices[i]-=float(pivot.x);inside.vertices[i+1]-=float(pivot.y);inside.vertices[i+2]-=float(pivot.z);}detail=inside.model(structure);detail.materials[0].maps[MATERIAL_MAP_NORMAL].texture=buildingTexture;detail.materials[0].maps[MATERIAL_MAP_METALNESS].texture=buildingDetailTexture;detail.materials[0].maps[MATERIAL_MAP_OCCLUSION].texture=sunScene.texture;}asset.interiors.push_back(detail);
 }return asset;
}
void Renderer::prepareStructures(const Game& game){
 // Geometry is rebuilt only for changed damage, location or mesh quality. The
 // per-frame path evaluates bounded analytic fragment poses without allocation.
 for(auto& instance:structuralInstances){auto it=game.worldDamage.find(instance.id);if(it!=game.worldDamage.end()&&it->second.structure.valid)instance.shape=it->second.structure;}
 if(structureRevision==game.geometryRevision&&length(structureAnchor-game.ship.position)<200&&structureLow==lowGraphics)return;
 structureRevision=game.geometryRevision;structureAnchor=game.ship.position;structureLow=lowGraphics;structuralInstances.clear();
 for(const auto& [id,d]:game.worldDamage)if((d.integrity<=0||hasDetachedSections(d.structure))&&d.structure.valid&&length(d.structure.origin-game.ship.position)<3500)structuralInstances.push_back({id,d.structure,d.body});
 for(const auto& port:game.ports)if(length(port.position-game.ship.position)<3000)for(const auto& object:portObjects(game,*presentation,port)){
  if(object.integrity>0||object.structureProfile<0)continue;auto record=game.worldDamage.find(object.id);if(record!=game.worldDamage.end()&&record->second.structure.valid)continue;
  StructureRemains shape;shape.valid=true;shape.profile=object.structureProfile;shape.variant=object.structureVariant;shape.ordinal=object.kind==FacilityObject?object.slot%24:0;shape.faction=object.structureFaction;shape.orbital=shape.profile==StationCore||shape.profile==StationWing;shape.origin=object.structureOrigin;shape.right=object.right;shape.up=object.up;shape.back=object.back;shape.seed=sceneHash(object.id);structuralInstances.push_back({object.id,shape,object.body});
 }
 std::stable_sort(structuralInstances.begin(),structuralInstances.end(),[&](const auto& a,const auto& b){return length(a.shape.origin-game.ship.position)<length(b.shape.origin-game.ship.position);});
 // A whole nearby district can remain visibly ruined. Cap resident GPU assets;
 // returning to a culled district reconstructs exactly the same saved pieces.
 if(structuralInstances.size()>48)structuralInstances.resize(48);
 for(auto it=brokenStructures.begin();it!=brokenStructures.end();){bool keep=std::any_of(structuralInstances.begin(),structuralInstances.end(),[&](const auto& s){return s.id==it->first;});if(!keep){for(auto& m:it->second.parts)UnloadModel(m);for(auto& m:it->second.interiors)if(m.meshCount)UnloadModel(m);it=brokenStructures.erase(it);}else ++it;}
 for(const auto& instance:structuralInstances){const auto& f=instance.shape;uint64_t key=f.seed;for(auto n:{f.profile,f.variant,f.ordinal,f.faction,int(lowGraphics),int(f.localized)})key=key*1099511628211ull+uint64_t(n+1);auto it=brokenStructures.find(instance.id);
  if(it!=brokenStructures.end()&&it->second.key!=key){for(auto& m:it->second.parts)UnloadModel(m);for(auto& m:it->second.interiors)if(m.meshCount)UnloadModel(m);brokenStructures.erase(it);it=brokenStructures.end();}
  if(it==brokenStructures.end()){auto asset=buildBrokenStructure(f);asset.key=key;brokenStructures.emplace(instance.id,std::move(asset));}
 }
}
void Renderer::drawStructuralRemains(const Game& game,bool shadow){
 if(!shadow){structuralDraws=structuralParts=interiorDraws=0;}
 for(const auto& instance:structuralInstances){const auto& f=instance.shape;auto found=brokenStructures.find(instance.id);if(found==brokenStructures.end())continue;auto& asset=found->second;double age=structureAge(game,f);
  if(shadow&&(f.orbital||instance.body!=groundBody||length(f.origin-game.ship.position)>480))continue;
  if(!shadow&&dot(f.origin-game.ship.position,game.ship.forward)<-220)continue;
  if(!shadow){structureState(structure,factionStyle(f.faction),{camera.position.x,camera.position.y,camera.position.z},sunDirection,visualTime,nullptr);uniform(structure,"fractureHeat",reduced?0.f:float(std::exp(-age*.8)*std::clamp(age/.12,0.,1.)));uniform(structure,"condition",float(.15+.7*std::exp(-age*.65)));uniform(structure,"powerOn",float(std::exp(-age*1.3)));uniform(structure,"pollution",.7f);uniform(structure,"useSunShadow",shadowActive&&shadowBody==instance.body?groundBlend:0.f);uniform(structure,"shadowCentre",instance.body>=0?game.planets[instance.body].centre:V3{});uniform(structure,"skyRadial",f.up);uniform(structure,"structureAir",!f.orbital&&instance.body>=0?float(game.planets[instance.body].atmosphere):0.f);int loc=GetShaderLocation(structure,"sunShadowMatrix");if(loc>=0)SetShaderValueMatrix(structure,loc,sunMatrix);structuralDraws++;}
  for(int part=0;part<int(asset.parts.size());part++){int cell=asset.indices[part],section=f.localized?structureCellSection(cell,structureRows(f)):0;if(f.localized&&(f.cleared&(1u<<cell)))continue;bool intact=f.localized&&f.sections[section]>0;double partAge=f.localized?sectionAge(f,section,age):age;auto motionBounds=f.localized?structureCell(f,cell):asset.bounds[part];auto pose=structurePose(f,motionBounds,cell,partAge);auto modelPivot=pose.centre+pose.right*(asset.bounds[part].centre().x-motionBounds.centre().x)+pose.up*(asset.bounds[part].centre().y-motionBounds.centre().y)+pose.back*(asset.bounds[part].centre().z-motionBounds.centre().z);auto at=structurePoint(f,modelPivot);
   if(!shadow){uniform(structure,"fractureHeat",reduced||intact?0.f:float(std::exp(-partAge*.8)));uniform(structure,"condition",intact?.8f:float(.15+.7*std::exp(-partAge*.65)));uniform(structure,"powerOn",intact?1.f:float(std::exp(-partAge*1.3)));}
   // Follow local terrain instead of letting wide toppled sections sink through
   // curved ground. Preserve the exact source pose at the moment of release.
   if(!f.orbital&&instance.body>=0&&!intact&&partAge>.3){const auto& planet=game.planets[instance.body];double lift=0;auto half=asset.bounds[part].half();for(int x:{-1,1})for(int z:{-1,1}){auto foot=structurePoint(f,fragmentPoint(pose,{x*half.x,-half.y,z*half.z}));auto radial=unit(foot-planet.centre);auto floor=planet.centre+radial*(planet.radius+terrain(planet,radial)+2.02);lift=std::max(lift,dot(floor-foot,f.up));}at+=f.up*(lift*std::clamp((partAge-.3)/.5,0.,1.));}
   auto rotateBasis=[&](V3 n){return f.right*n.x+f.up*n.y+f.back*n.z;};auto r=rotateBasis(pose.right),u=rotateBasis(pose.up),b=rotateBasis(pose.back);auto& model=asset.parts[part];model.transform={float(r.x),float(u.x),float(b.x),0,float(r.y),float(u.y),float(b.y),0,float(r.z),float(u.z),float(b.z),0,0,0,0,1};
   if(shadow){auto original=model.materials[0].shader;model.materials[0].shader=sunDepth;uniform(sunDepth,"groundPass",0.f);DrawModel(model,vec(at-game.planets[groundBody].centre),1,WHITE);model.materials[0].shader=original;}
   else{uniform(structure,"fracturePivot",asset.bounds[part].centre());drawVectorModel(model,vec(at),1);structuralParts++;if(part<int(asset.interiors.size())&&asset.interiors[part].meshCount){auto& interior=asset.interiors[part];interior.transform=model.transform;drawVectorModel(interior,vec(at),1);interiorDraws++;}}
   if(shadow||intact||partAge<.08||partAge>7)continue;
   if(!reduced&&partAge<10&&part%3==0){double pressure=std::exp(-partAge*.45);auto vent=at+u*asset.bounds[part].half().y*.4;BeginBlendMode(BLEND_ADDITIVE);rlDisableDepthMask();for(int jet=0;jet<(lowGraphics?2:4);jet++){double t=std::fmod(partAge*.7+jet*.23,1.);auto plume=vent+u*(t*(2+pressure*5))+r*(std::sin(t*pi+part)*pressure*.6);if(combatGlow.id)DrawBillboard(camera,combatGlow,vec(plume),float(.4+t*1.8),Color{137,169,184,static_cast<unsigned char>(pressure*(1-t)*30)});}rlEnableDepthMask();EndBlendMode();}
   // Cold mineral dust and spectral arcs originate on the moving architecture,
   // including delayed secondary failures after the main fireball has faded.
   double pulse=std::exp(-std::pow((partAge-(.6+fractureNoise(f.seed,part+80)*2.4))/.35,2));auto tint=colour(factionStyle(f.faction).accent,255);
   if(!reduced&&combatGlow.id&&part%(lowGraphics?3:2)==0){rlDisableDepthMask();auto dustAt=f.orbital?at:at-f.up*asset.bounds[part].half().y+f.up*(age*.35);double fade=std::sin(std::clamp(partAge/7.,0.,1.)*pi);DrawBillboard(camera,combatGlow,vec(dustAt),float((2+age)*std::min(5.,asset.bounds[part].half().x*.5)),Color{44,33,60,static_cast<unsigned char>(fade*62)});rlEnableDepthMask();}
   if(pulse>.02&&part%(reduced?4:lowGraphics?2:1)==0){BeginBlendMode(BLEND_ADDITIVE);rlDisableDepthMask();if(combatGlow.id)DrawBillboard(camera,combatGlow,vec(at),float(1.5+asset.bounds[part].half().x*.22),Color{tint.r,tint.g,tint.b,static_cast<unsigned char>(pulse*180)});
    int sparks=reduced?2:lowGraphics?4:7;for(int j=0;j<sparks;j++){double phase=age*3+j*2.399963,reach=(.4+pulse*2)*(1+fractureNoise(f.seed,part*9+j));auto direction=f.right*std::cos(phase)+f.back*std::sin(phase)+f.up*(f.orbital?std::sin(phase*1.7):.6);DrawLine3D(vec(at+direction*reach),vec(at+direction*(reach+.6)),Color{tint.r,tint.g,tint.b,static_cast<unsigned char>(pulse*205)});}rlEnableDepthMask();EndBlendMode();}
  }
 }
}
void Renderer::clearPortAssets(){for(auto& [id,a]:brokenStructures){for(auto& m:a.parts)UnloadModel(m);for(auto& m:a.interiors)if(m.meshCount)UnloadModel(m);}brokenStructures.clear();structuralInstances.clear();structureRevision=~uint64_t(0);for(auto& [key,hull]:fracturedHulls){for(auto& part:hull.parts)if(part.meshCount)UnloadModel(part);for(auto& part:hull.interiors)if(part.meshCount)UnloadModel(part);}fracturedHulls.clear();for(auto& [id,a]:portAssets)for(auto* m:{&a.close,&a.detailed,&a.distant})if(m->meshCount)UnloadModel(*m);portAssets.clear();for(auto& [key,m]:craftAssets)if(m.meshCount)UnloadModel(m);craftAssets.clear();}
Renderer::PortAsset Renderer::buildPort(const Port& port,const SettlementView* view,const Game& game){
 SettlementView empty;empty.id=port.id;const auto& s=view?*view:empty;const auto& style=factionStyle(s.faction);
 auto layout=layoutSettlement(s);PortAsset asset;asset.key=portGeometryKey(port,view,game);asset.at=port.position;asset.up=port.normal;asset.radius=port.planet<0?190:layout.radius;
 V3 n=port.normal,a=unit(cross({1,0,0},n),{0,0,1}),b=unit(cross(a,n));
 Matrix rotation{float(a.x),float(n.x),float(b.x),0,float(a.y),float(n.y),float(b.y),0,float(a.z),float(n.z),float(b.z),0,0,0,0,1};
 auto surface=[&](V3 p){Frame f;f.origin=p;
  if(port.planet>=0){const auto& planet=game.planets[port.planet];auto radial=unit(port.position+a*p.x+b*p.z-planet.centre);auto floor=planet.centre+radial*(planet.radius+terrain(planet,radial)+2.12);auto d=floor-port.position;f.origin={dot(d,a),dot(d,n),dot(d,b)};f.y={dot(radial,a),dot(radial,n),dot(radial,b)};f.x=unit(V3{1,0,0}-f.y*f.y.x);f.z=unit(cross(f.x,f.y));}
  return f;};
 asset.detailCentres.push_back(port.position+n*(port.planet<0?90:6));
 if(port.planet>=0){asset.detailCentres.push_back(port.position+b*56+n*10);for(const auto& site:layout.facilities){auto f=surface(site.position);asset.detailCentres.push_back(port.position+a*f.origin.x+n*(f.origin.y+8)+b*f.origin.z);}}
 for(int lod=0;lod<3;lod++){bool close=lod==0,far=lod==2;Builder m;Color metal=colour(style.hull),glow=colour(style.accent,96),secondary=colour(style.secondary,96);
  if(port.planet>=0){
   if(!port.disabled&&!modularStructure(game.worldDamage,portObjectId(port.id))){
   landingPad(m,s.faction,far);
   }else{for(int i=0;i<8;i++)m.bevelBox({std::sin(i*3.1)*24,.5,std::cos(i*2.7)*24},{6,1.4,4},coded({37,38,44,0},0),.2);}
   // A real loading yard lies outside the player's landing circle.
   if(objectIntegrity(game.worldDamage,propObjectId(port.id,"gantry",0))>0&&!modularStructure(game.worldDamage,propObjectId(port.id,"gantry",0))){
   loadingGantry(m,s.faction,far);
   }
   if(objectIntegrity(game.worldDamage,propObjectId(port.id,"mast",0))>0&&!modularStructure(game.worldDamage,propObjectId(port.id,"mast",0))){
   portMast(m,s.faction,far);
   }
   for(int i=0;i<9;i++)for(int c=0;c<std::min(far?2:8,cargoSlots(game.worldDamage,port.id,i,s.stock[i]));c++){
    if(objectIntegrity(game.worldDamage,propObjectId(port.id,"cargo",i*8+c))<=0||modularStructure(game.worldDamage,propObjectId(port.id,"cargo",i*8+c)))continue;
    V3 p{-11+(i%3)*11.,3.42+(c/2)*2.44,48+(i/3)*6.+(c%2)*2.6};freightContainer(m,p,coded(cargoColours[i],17),far!=0);
   }
   if(close){
    if(objectIntegrity(game.worldDamage,propObjectId(port.id,"terminal",0))>0&&!modularStructure(game.worldDamage,propObjectId(port.id,"terminal",0))){
    portTerminal(m,s.faction,far);
    }
   }
   std::array<bool,14> roads{};
   for(auto& site:layout.facilities){if(far&&site.ordinal>2)continue;
    if(!roads[site.kind]){roads[site.kind]=true;int count=0;for(auto& f:layout.facilities)if(f.kind==site.kind)count++;double reach=84+((count-1)/2)*27.;
     m.frame={};pavedRoad(m,surface,unit(site.position),reach,far!=0);}
    m.frame=surface(site.position);double facing=site.angle+(site.ordinal%2?pi:0);m.frame.x=unit(m.frame.x*std::cos(facing)+m.frame.z*std::sin(facing));m.frame.z=unit(cross(m.frame.x,m.frame.y));
    auto damaged=s.facilityIntegrity.find(site.kind*24+site.ordinal);double hp=damaged==s.facilityIntegrity.end()?1:damaged->second;
    if(hp<=0){m.cylinder({0,-.65,0},10,1.,coded({38,37,44,0},128),far?8:16);}
    else if(!modularStructure(game.worldDamage,facilityObjectId(s.id,site.kind,site.ordinal))){size_t first=m.colours.size();facility(m,site,style,s.faction,far,close);if(hp<1)for(size_t c=first;c<m.colours.size();c+=4)for(int k=0;k<3;k++)m.colours[c+k]=static_cast<unsigned char>(m.colours[c+k]*(.5+.5*hp));}
   }
   m.frame={};
   if(!far){for(int i=0;i<14;i++){double angle=i*2*pi/14+.1,rad=62+(i%3)*6;V3 at{std::cos(angle)*rad,0,std::sin(angle)*rad};
     if(!architecture::plantingClear(layout,at))continue;if(objectIntegrity(game.worldDamage,propObjectId(port.id,"tree",i))<=0)continue;
     auto f=surface(at);m.frame=f;
     m.cylinder({},4.5,.25,coded({60,76,61,0},128),12);m.beam({0,0,0},{0,6.+(i%4),0},.23,coded({89,126,88,0},160));m.sphere({0,6.+(i%4),0},{2.2,3.5,2.2},coded({91,169,123,0},160),false,10,5);
     for(int side:{-1,1}){m.beam({0,4,0},{side*1.4,6.+(i%3),.5},.12,coded({89,126,88,0},160),4);m.sphere({side*1.4,6.+(i%3),.5},{1.4,2,1.5},coded({102,177,133,0},160),false,8,4);}}
    m.frame={};for(int i=0;i<10;i++)for(int side:{-1,1}){auto f=surface({side*13.,0,-45-i*7.});m.frame=f;if(objectIntegrity(game.worldDamage,propObjectId(port.id,"beacon",i*2+(side>0)))>0)m.cylinder({},.28,.65,glow,5);}m.frame={};}
  }else{
   if(port.disabled||modularStructure(game.worldDamage,portObjectId(port.id)))m.box({0,-18,0},{.05,.05,.05},coded({30,30,36,0},0));
   if(!port.disabled&&!modularStructure(game.worldDamage,portObjectId(port.id)))orbitalCore(m,s.faction,view?std::min(8,view->facilities[Warehouse]):((int(game.planets.at(port.body).worldSeed%5)+1)<<4),far);
   for(int side:{-1,1})if(objectIntegrity(game.worldDamage,propObjectId(port.id,"panel",side+1))>0&&!modularStructure(game.worldDamage,propObjectId(port.id,"panel",side+1))){m.frame.origin={side*131.,0,0};stationWing(m,s.faction,side);m.frame={};}
  }

  auto model=m.model(structure);model.materials[0].maps[MATERIAL_MAP_NORMAL].texture=buildingTexture;model.materials[0].maps[MATERIAL_MAP_METALNESS].texture=buildingDetailTexture;model.materials[0].maps[MATERIAL_MAP_OCCLUSION].texture=sunScene.texture;model.transform=rotation;if(close)asset.close=model;else if(far)asset.distant=model;else asset.detailed=model;
 }return asset;
}
void Renderer::drawSettlement(const Port& port,const SettlementView* view,const Game& game){
 auto key=portGeometryKey(port,view,game);auto it=portAssets.find(port.id);
 if(it==portAssets.end())it=portAssets.emplace(port.id,buildPort(port,view,game)).first;
 else if(it->second.key!=key||length(it->second.at-port.position)>1e-5||length(it->second.up-port.normal)>1e-8){UnloadModel(it->second.close);UnloadModel(it->second.detailed);UnloadModel(it->second.distant);it->second=buildPort(port,view,game);}
 auto& asset=it->second;double distance=length(port.position-game.ship.position),pixels=asset.radius/std::max(1.,distance)*scene.texture.height;
 structureState(structure,factionStyle(view?view->faction:0),{camera.position.x,camera.position.y,camera.position.z},sunDirection,presentation->seconds,view);
 double nearest=distance;for(auto centre:asset.detailCentres)nearest=std::min(nearest,length(centre-game.ship.position));
 asset.detail=architecture::settlementDetail(asset.detail,extremeGraphics?pixels*1.7:pixels,extremeGraphics?nearest*.55:nearest);
 uniform(structure,"useSunShadow",shadowActive&&shadowBody==port.planet?groundBlend:0.f);
 uniform(structure,"shadowCentre",port.planet>=0?game.planets[port.planet].centre:V3{});uniform(structure,"skyRadial",port.normal);
 uniform(structure,"structureAir",port.planet>=0?float(game.planets[port.planet].atmosphere):0.f);
 int matrixLocation=GetShaderLocation(structure,"sunShadowMatrix");if(matrixLocation>=0)SetShaderValueMatrix(structure,matrixLocation,sunMatrix);
 drawVectorModel(asset.detail==2?asset.close:asset.detail==1?asset.detailed:asset.distant,vec(port.position),1,4);
}
void Renderer::drawPortActivity(const Port& port,const Scene& sceneView){
 bool loading=false,receiving=false,holding=false;int good=0;for(auto& s:sceneView.shipments){if(s.from==port.settlement&&s.loading){loading=true;good=s.good;}if(s.to==port.settlement&&(s.unloading||s.holding)){receiving=true;holding|=s.holding;good=s.good;}}
 if(!loading&&!receiving)return;
 auto* settlement=sceneView.settlement(port.settlement);auto tint=colour(factionStyle(settlement?settlement->faction:0).accent);tint.a=255;if(holding)tint={255,113,141,255};
 V3 n=port.normal,a=unit(cross({1,0,0},n),{0,0,1}),b=unit(cross(a,n));V3 centre=port.position+n*(port.planet>=0?22:91)+b*(port.planet>=0?53:55);
 double travel=holding?0:.5+.5*std::sin(sceneView.seconds*.55);V3 hook=centre+a*((travel-.5)*24)-n*(4+travel*8);
 DrawLine3D(vec(centre+a*((travel-.5)*24)),vec(hook),tint);
 if(!holding){DrawCubeV(vec(hook-n*1.2),{3,2.3f,2.3f},wireframe?Color{0,1,2,255}:coded(cargoColours[std::clamp(good,0,8)],255));if(wireframe)DrawCubeWiresV(vec(hook-n*1.2),{3,2.3f,2.3f},Color{103,222,239,255});}
 for(int i=0;i<6;i++){V3 p=centre+a*((i-2.5)*4)-n*18;Color c=tint;c.a=static_cast<unsigned char>(holding?190:((int(sceneView.seconds*2)+i)%6==0?240:50));DrawSphereEx(vec(p),.45f,3,4,c);}
}
Model Renderer::buildCraft(int faction,int kind,int pods,int good){
 int variant=kind/32;kind%=32;bool patrol=kind>=13;if(patrol)kind-=10;
 Builder m;const auto& style=factionStyle(faction);Color hull=colour(style.hull),glass=colour(style.secondary,32),glow=colour(style.accent,96);
 if(!patrol&&(kind==1||kind>=3))glow={255,83,153,96};
 if(faction==0){m.cylinder({0,-.18,0},.6,.4,hull,4,.4);for(int side:{-1,1}){m.triangle({0,.16,-1.7},{side*1.2,0,.7},{0,-.16,.9},hull);m.triangle({0,.16,-1.7},{0,.28,.8},{side*1.2,0,.7},hull);}m.box({0,.28,-.15},{.3,.16,.8},glass);}
 else if(faction==1){m.box({0,0,0},{1.4,.6,2.2},hull);for(int side:{-1,1}){m.box({side*.9,0,.1},{.35,.4,2},hull);m.box({side*.72,.33,.15},{.12,.12,1.8},glow);}m.cylinder({0,.3,-.4},.28,.22,glass,4);}
 else if(faction==2){m.sphere({0,0,0},{1.3,.29,1.6},hull,false,14,6);m.sphere({0,.23,-.3},{.48,.24,.75},glass,true,10,4);m.ring({0,.02,0},1.1,.025,glow,24);}
 else if(faction==3){for(int side:{-1,1}){m.box({side*.65,0,0},{.35,.42,2.6},hull);m.box({side*.65,.24,-.6},{.15,.05,1.2},glow);}m.box({0,0,0},{1.2,.25,.9},hull);m.box({0,.22,-.2},{.48,.22,.6},glass);}
 else{m.cylinder({0,-.85,0},.7,1.7,hull,4,.15);m.triangle({0,0,-1.8},{1.25,0,.6},{-1.25,0,.6},hull);m.triangle({0,-.2,-1.8},{-1.25,-.2,.6},{1.25,-.2,.6},hull);m.beam({0,-.8,.2},{0,.9,.2},.07,glow);m.box({0,.1,-.65},{.28,.2,.65},glass);}
 // Armed wings share faction materials, with equipment changing the silhouette.
 if(kind==3){for(int side:{-1,1}){m.beam({side*.55,0,-1.3},{side*1.45,-.16,.5},.13,hull);m.bevelBox({side*1.2,-.08,-.25},{.20,.20,1.4},hull,.045);m.box({side*1.2,-.08,-.96},{.115,.115,.025},glow);}}
 if(kind==4){for(int side:{-1,1}){m.triangle({side*.4,0,-1.9},{side*1.9,-.12,.95},{side*.5,.12,.8},hull);m.bevelBox({side*1.5,-.1,.7},{.24,.28,.65},hull,.06);m.box({side*1.5,-.1,1.04},{.16,.18,.025},glow);}m.box({0,.4,.4},{.1,.8,.75},hull);}
 if(kind==5){for(int side:{-1,1}){m.box({side*.9,-.05,0},{.8,.85,2.25},hull);m.box({side*.9,.42,-.3},{.14,.08,1.6},glow);m.cylinder({side*.9,.5,-.7},.2,.25,hull,8);}m.box({0,.55,.3},{.7,.55,1.15},hull);}
 if(kind==6){m.sphere({0,0,.1},{.65,.4,1.05},hull,false,12,6);for(int side:{-1,1}){m.beam({side*.45,0,.65},{side*1.4,.18,-.5},.16,hull);m.beam({side*1.4,.18,-.5},{side*.8,.16,-1.5},.13,hull);m.box({side*.82,.18,-1.45},{.16,.2,.45},glow);}m.ring({0,.08,.1},1.25,.06,glow,32);m.box({0,.4,-.25},{.28,.16,.65},glass);}

 if(variant){
  double span=1.15+(variant%3)*.24;
  for(int side:{-1,1}){
   if(variant&1)m.triangle({side*.4,.12,-.8},{side*span,.28,.4},{side*.5,.48,.8},hull);
   if(variant&2){m.cylinder({side*span,-.12,.2},.22,.9,hull,6,.13);m.box({side*span,.35,.2},{.12,.06,.55},glow);}
   if(variant&4)m.beam({side*.48,.15,-.3},{side*span,.6,-.9},.065,colour(style.secondary,96));
  }
  m.box({0,.35,.4},{.12,.3+(variant%3)*.14,.8},hull);
 }
 if(kind==2){for(int i=0;i<pods;i++){double side=i%2?1:-1,z=.85-(i/2)*.55;m.box({side*.91,-.38,z},{.5,.5,.48},coded(cargoColours[std::clamp(good,0,8)],0));m.box({side*.91,-.66,z},{.38,.035,.35},glow);}}
 for(int side:{-1,1})m.box({side*.44,0,1.16},{.25,.22,.14},glow);
 auto model=m.model(structure);model.materials[0].maps[MATERIAL_MAP_NORMAL].texture=buildingTexture;model.materials[0].maps[MATERIAL_MAP_METALNESS].texture=buildingDetailTexture;model.materials[0].maps[MATERIAL_MAP_OCCLUSION].texture=sunScene.texture;return model;
}
void Renderer::drawFactionCraft(const Traffic& traffic,const Scene& sceneView){
 auto* shipment=sceneView.shipment(traffic.routeId);int pods=shipment?std::clamp(int(std::ceil(shipment->amount/5.)),1,8):0,good=shipment?shipment->good:0;
 int modelKind=craftModel(traffic);auto key=std::make_tuple(traffic.faction,modelKind,pods,good);auto it=craftAssets.find(key);if(it==craftAssets.end())it=craftAssets.emplace(key,buildCraft(traffic.faction,modelKind,pods,good)).first;
 auto f=unit(traffic.forward,{0,0,-1}),right=unit(cross(f,{0,1,0}),{1,0,0}),up=unit(cross(right,f));double size=(traffic.kind==2||traffic.kind==3)?11:traffic.kind==1?enemyDef(traffic).radius:5;
 Matrix rotation{float(right.x),float(up.x),float(-f.x),0,float(right.y),float(up.y),float(-f.y),0,float(right.z),float(up.z),float(-f.z),0,0,0,0,1};it->second.transform=rotation;
 structureState(structure,factionStyle(traffic.faction),{camera.position.x,camera.position.y,camera.position.z},sunDirection,sceneView.seconds,nullptr);uniform(structure,"condition",float(std::clamp(traffic.health/(traffic.kind==1?enemyDef(traffic).hull:100.),.08,1.)));
 uniform(structure,"craftScale",float(size));uniform(structure,"craftIdentity",float(traffic.genome.seed%65521)/65521.f);
 uniform(structure,"serviceWear",traffic.pirate?.22f:traffic.kind==1?.10f:.04f);
 uniform(structure,"useSunShadow",0.f);uniform(structure,"structureAir",0.f);uniform(structure,"skyRadial",up);
 drawVectorModel(it->second,vec(traffic.position),float(size),5,wireframe?colour(factionStyle(traffic.faction).accent):WHITE);
 if(traffic.charge>.025&&length(traffic.position-V3{camera.position.x,camera.position.y,camera.position.z})<1500){
  Color c=traffic.chargeWeapon==Resonance?Color{180,125,255,220}:traffic.chargeWeapon==Seeker?Color{255,173,110,220}:Color{255,110,197,220};
  BeginBlendMode(BLEND_ADDITIVE);rlDisableDepthMask();
  for(int side:{-1,1}){auto muzzle=traffic.position+f*size*.95+right*(side*size*.48);DrawSphereEx(vec(muzzle),float(.3+traffic.charge*1.4),6,8,c);DrawLine3D(vec(muzzle),vec(muzzle-f*(2+traffic.charge*size)),c);}
  rlEnableDepthMask();EndBlendMode();
 }


 if(length(traffic.position-V3{camera.position.x,camera.position.y,camera.position.z})<2200){Color c=colour(factionStyle(traffic.faction).accent);c.a=180;
  for(int side:{-1,1}){auto engine=traffic.position+right*(side*size*.44)-f*size*1.16;DrawLine3D(vec(engine),vec(engine-f*(size*(.65+.08*std::sin(sceneView.seconds*7+traffic.phase)))),c);}}
}
}
