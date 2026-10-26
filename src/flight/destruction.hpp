#pragma once
#include "game.hpp"
namespace hyperion::flight {
inline int craftModel(const Traffic& t){return (t.kind==1?3+std::clamp(t.role,0,3):t.kind)+(t.managed&&!t.pirate&&t.kind==1?10:0)+hullVariant(t.genome)*32;}
inline HullRemains hullRemains(const Traffic& t){return {true,t.faction,craftModel(t),t.genome.seed,t.kind==2?11:t.kind==1?enemyDef(t).radius:5,unit(t.forward,{0,0,-1})};}
inline double debrisNoise(uint32_t seed,int index){uint32_t h=seed^uint32_t(index+1)*0x9e3779b9u;h^=h>>16;h*=0x7feb352du;h^=h>>15;h*=0x846ca68bu;h^=h>>16;return double(h>>8)/16777215.;}
// Whole triangles belong to exactly one fragment, retaining all hull geometry.
inline int hullFragment(V3 centre){return centre.x<-.5?0:centre.x>.5?1:centre.z<-.35?2:centre.z>.35?3:4;}
struct DebrisPose {V3 offset,right,up,back;};
inline DebrisPose debrisPose(const HullRemains& hull,int part,double age){
 auto forward=unit(hull.forward,{0,0,-1}),right=unit(cross(forward,{0,1,0}),{1,0,0}),up=unit(cross(right,forward)),back=-forward;
 double t=std::max(0.,age-.045*part),spread=1-std::exp(-t*1.15);
 const V3 directions[]={{-1,.1,.15},{1,-.08,.2},{.15,.12,-1},{-.12,-.1,1},{0,-.35,.1}};
 auto local=directions[std::clamp(part,0,4)];
 auto offset=(right*local.x+up*local.y+back*local.z)*(hull.size*spread*(part==4?.3:1.2+.5*debrisNoise(hull.seed,part)));
 auto axis=unit(right*(debrisNoise(hull.seed,part+10)*2-1)+up*(debrisNoise(hull.seed,part+20)*2-1)+back*.4);
 double angle=t*(.24+.55*debrisNoise(hull.seed,part+30));
 return {offset,rotate(right,axis,angle),rotate(up,axis,angle),rotate(back,axis,angle)};
}
inline V3 debrisPoint(const DebrisPose& pose,V3 local){return pose.right*local.x+pose.up*local.y+pose.back*local.z;}
}
