#pragma once
#include "math.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
namespace hyperion::flight {
enum StructureProfile {StationCore=14,StationWing,LandingPad,LoadingGantry,PortMast,FreightCrate,PortTerminal,StructureProfileCount};
struct StructureRemains {
 bool valid=false,orbital=false;int profile=0,faction=0,variant=0,ordinal=0;uint32_t seed=0;
 V3 origin,right{1,0,0},up{0,1,0},back{0,0,1},impact;
 double born=-1,localBorn=-1; // localBorn is a transient bridge to the flight clock.
};
inline double fractureNoise(uint32_t seed,int n){uint32_t h=seed^uint32_t(n+1)*0x9e3779b9u;h^=h>>16;h*=0x7feb352du;h^=h>>15;h*=0x846ca68bu;h^=h>>16;return double(h>>8)/16777215.;}
inline V3 structurePoint(const StructureRemains& s,V3 p){return s.origin+s.right*p.x+s.up*p.y+s.back*p.z;}
struct FragmentBounds {V3 low,high;V3 centre()const{return (low+high)*.5;}V3 half()const{return (high-low)*.5;}};
struct StructurePose {V3 centre,right{1,0,0},up{0,1,0},back{0,0,1};double contact=0;};
inline V3 fragmentPoint(const StructurePose& s,V3 p){return s.centre+s.right*p.x+s.up*p.y+s.back*p.z;}
inline StructurePose structurePose(const StructureRemains& s,const FragmentBounds& bounds,int part,double age,double ground=0){
 StructurePose pose;auto pivot=bounds.centre(),half=bounds.half();double size=std::max({half.x,half.y,half.z,.1});
 double delay=.06+.22*fractureNoise(s.seed,part+4);double t=std::max(0.,age-delay);
 auto away=unit(V3{pivot.x-s.impact.x,.12,pivot.z-s.impact.z},{1,0,0});
 if(s.orbital){
  auto direction=unit(pivot-s.impact+V3{fractureNoise(s.seed,part+2)-.5,fractureNoise(s.seed,part+9)-.5,.1},{1,0,0});
  double spread=1-std::exp(-t*.22);pose.centre=pivot+direction*((5+size*.45)*spread);
  auto axis=unit(V3{fractureNoise(s.seed,part+12)-.5,fractureNoise(s.seed,part+31)-.5,fractureNoise(s.seed,part+47)-.5});double angle=std::fmod(t*(.04+.10*fractureNoise(s.seed,part+60)),2*pi);
  pose.right=rotate(pose.right,axis,angle);pose.up=rotate(pose.up,axis,angle);pose.back=rotate(pose.back,axis,angle);return pose;
 }
 double u=std::clamp(t/6.5,0.,1.),ease=u*u*(3-2*u);
 auto axis=unit(V3{-away.z,.18*(fractureNoise(s.seed,part+50)-.5),away.x});
 double angle=(.35+1.15*fractureNoise(s.seed,part+24))*ease;
 if(s.profile==LandingPad)angle*=.25;
 pose.right=rotate(pose.right,axis,angle);pose.up=rotate(pose.up,axis,angle);pose.back=rotate(pose.back,axis,angle);
 double spread=(.8+size*.3)*(1-std::pow(1-u,3));pose.centre=pivot+away*spread;
 double bottom=-std::abs(pose.right.y)*half.x-std::abs(pose.up.y)*half.y-std::abs(pose.back.y)*half.z;
 double floor=ground+std::min(0.,bounds.low.y)-bottom;
 // Fall onto the same terrain plane used by the renderer. The smooth rotation
 // stops at a stable pose, so saved ruins never snap to an unrelated rubble mesh.
 double freeY=pivot.y+(1.2+fractureNoise(s.seed,part+18)*3)*t-6.5*t*t;
 double below=std::max(0.,floor-freeY);pose.contact=below>0?1:0;
 double bounce=below>0&&t<6.5?.18*std::exp(-below*.16)*std::abs(std::sin(below*.6)):0;
 pose.centre.y=std::max(floor,freeY)+bounce;return pose;
}
}
