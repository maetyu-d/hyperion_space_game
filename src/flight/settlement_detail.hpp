#pragma once
#include "structure_mesh.hpp"
#include "scene.hpp"

namespace hyperion::flight::architecture {
inline Color material(std::array<unsigned char,3> c,int code){return {c[0],c[1],c[2],static_cast<unsigned char>(code)};}
inline Color material(Color c,int code){c.a=static_cast<unsigned char>(code);return c;}
inline void turnFrame(Builder& m,const Frame& parent,double angle){
 m.frame=parent;m.frame.x=parent.x*std::cos(angle)+parent.z*std::sin(angle);m.frame.z=parent.z*std::cos(angle)-parent.x*std::sin(angle);
}
inline void windowBay(Builder& m,V3 c,double width,double height,Color glass,Color trim,bool close,int identity){
 double x=width*.5,y=height*.5;
 m.mappedQuad(c+V3{x,-y,0},c+V3{-x,-y,0},c+V3{-x,y,0},c+V3{x,y,0},glass,{float((identity%16)*2),float(((identity/16)%8)*2)});
 if(!close)return;
 m.panelFrame(c+V3{0,0,-.12},width+.30,height+.30,.15,.40,trim);
 m.box(c+V3{0,-y-.18,-.25},{width+.5,.16,.68},trim);
 m.box(c+V3{0,0,-.24},{.055,height,.055},trim);
}
inline void habitatWindows(Builder& m,const FacilitySite& site,const FactionStyle& style,int faction,bool close){
 if(faction==2||faction==4)return;
 int code=site.kind+1,rows=std::max(1,int((8+(site.occupants/4)*4)*style.height/3.5)-1);
 Color glass=material(style.secondary,48+code),trim=material(style.hull,code);auto parent=m.frame;
 for(int face=0;face<4;face++){
  turnFrame(m,parent,face*pi*.5);double half=face%2?7*style.width:7;
  for(int row=0;row<rows;row++)for(int col=-1;col<=1;col++){
   double spacing=face%2?3.7:3.7*style.width;
   windowBay(m,{col*spacing,3+row*3.5,-half-.045},2.55*(face%2?1:style.width),1.72,glass,trim,close,site.ordinal*96+face*21+row*3+col+1);
  }
 }
 m.frame=parent;
}
inline void entrance(Builder& m,int kind,const FactionStyle& style,int ordinal,bool close){
 int code=kind+1;auto metal=material(style.hull,code),dark=material(Color{32,40,51,0},224+code),paint=material(Color{157,167,172,0},192+code),glass=material(style.secondary,48+code),inlay=material(style.accent,208+code);
 // Threshold, treads and vestibule connect to the existing foundation.
 double back=kind==Observatory||(kind==Habitat&&style.silhouette==4)?-4.:-7.;
 m.bevelBox({0,2.65,(-11.6+back)*.5},{5.6,3.7,back+11.6},metal,.20);
 m.box({0,.4,-12.1},{4.8,.8,1.2},paint);
 for(int i=0;i<3;i++)m.box({0,.1*(i+1),-14+i*.6},{4.2,.2*(i+1),.62},paint);
 m.mappedQuad({1.7,.82,-11.64},{-1.7,.82,-11.64},{-1.7,3.85,-11.64},{1.7,3.85,-11.64},glass,{float((ordinal+1)*2),2});
 if(!close)return;
 m.panelFrame({0,2.34,-11.77},3.9,3.55,.24,.50,metal);
 m.box({0,2.34,-12.04},{.075,3.07,.07},dark);
 m.bevelBox({0,4.65,-11.2},{6.5,.40,3.9},metal,.08);
 m.box({0,4.41,-12.7},{5.6,.045,.13},inlay);
 for(int side:{-1,1}){
  m.beam({side*2.5,.70,-13.8},{side*2.5,1.7,-12.0},.06,metal,6);
  m.beam({side*2.5,.15,-13.8},{side*2.5,.70,-13.8},.06,metal,6);
 }
 m.bevelBox({3.0,1.7,-12.3},{.65,1.5,.45},metal,.08);m.box({3.,2.,-12.55},{.42,.55,.06},inlay);
}
inline void closeFacility(Builder& m,const FacilitySite& site,const FactionStyle& style,int faction){
 int k=site.kind,code=k+1;auto metal=material(style.hull,code),trim=material(Color{124,139,145,0},code),dark=material(Color{27,36,45,0},224+code),inlay=material(style.accent,208+code),paint=material(Color{153,162,158,0},192+code);
 if(k==Garden){
  for(int i=0;i<4;i++)m.box({0,.125*(i+1),-14+i*.50},{4.2,.25*(i+1),.52},paint);
  m.box({0,.50,-11.95},{4.4,1.,1.2},paint);
  m.panelFrame({0,3,-11.69},3.7,4.4,.23,.42,metal);m.box({0,3,-11.94},{.075,3.95,.07},trim);
  for(int side:{-1,1}){m.beam({side*2.9,.8,-12.7},{side*2.9,1.8,-10.9},.07,metal,6);m.bevelBox({side*5.,1.8,-7.7},{1.5,3.4,2.2},metal,.15);}
 }
 if(k==Habitat||k==Garden||k==Medical){
  m.bevelBox({0,0,-15.1},{8,.16,6.8},material(Color{73,88,92,0},128),.06);
  for(int side:{-1,1}){
   m.box({side*3.1,.39,-15.1},{.08,.04,5.7},inlay);
   m.bevelBox({side*4.2,.60,-15},{1.2,1.15,3.7},metal,.13);m.box({side*4.2,1.2,-15},{1.25,.12,3.8},paint);
   m.beam({side*4.8,.2,-17},{side*4.8,3.4,-17},.09,metal,6);m.box({side*4.8,3.4,-17},{.9,.16,.5},inlay);
  }
 }
 if(k==Smelter||k==Fabricator||k==Reactor||k==Fuelworks){
  double y=k==Smelter?12.3:k==Fabricator?14*style.height+1.4:6.4;
  m.box({0,y,-7.8},{14,.22,2.0},metal);
  for(int side:{-1,1}){m.beam({side*6.8,y,-8.7},{side*6.8,y+1.25,-8.7},.055,trim,6);}
  m.beam({-6.8,y+1.25,-8.7},{6.8,y+1.25,-8.7},.055,trim,6);
  for(int rung=0;rung<int(y/.65);rung++)m.beam({7.2,rung*.65,-7.8},{8.2,rung*.65,-7.8},.045,trim,6);
  for(double x:{7.2,8.2})m.beam({x,.1,-7.8},{x,y+1.2,-7.8},.055,trim,6);
  m.bevelBox({-8.0,1.6,-7.9},{2.4,3.2,2.7},metal,.17);m.box({-8,2.0,-9.3},{1.6,1.0,.07},dark);m.box({-8,2.7,-9.34},{1.5,.10,.06},inlay);
 }
 if(k==Fuelworks){
  for(int i=-1;i<=1;i++){
   for(double y:{2.,10.*style.height})m.ring({i*5.,y,0},2.37,.09,trim,24);
   m.beam({i*5.,1,-3.1},{i*5.,7,-3.1},.18,metal,10);m.ring({i*5.,4,-3.1},.52,.065,inlay,12);
  }
 }
 if(k==Warehouse||k==Fabricator){
  double z=k==Warehouse?-9.4:-8.5;
  m.panelFrame({0,5,z},11.5,8.3,.28,.38,trim);
  for(int row=0;row<9;row++)m.box({0,1.4+row*.82,z-.23},{10.8,.07,.065},metal);
  for(int side:{-1,1})m.bevelBox({side*6.7,.9,z-1},{.65,1.8,.65},paint,.12);
 }
 if(k==Habitat&&faction==4){double height=(8+(site.occupants/4)*4)*style.height;for(int i=0;i<4;i++){double a=i*pi*.5;m.beam({std::cos(a)*7*style.width,.4,std::sin(a)*7*style.width},{std::cos(a)*3,height,std::sin(a)*3},.09,inlay,6);}}
}
inline bool plantingClear(const SettlementLayout& layout,V3 p){return hyperion::flight::plantingClear(layout,p);}
}
