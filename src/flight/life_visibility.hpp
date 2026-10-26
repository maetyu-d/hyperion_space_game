#pragma once
#include "scene.hpp"
#include <algorithm>

namespace hyperion::flight {
inline double lifeSmooth(double from,double to,double value){
 double t=std::clamp((value-from)/(to-from),0.,1.);return t*t*(3.-2.*t);
}
// The candidate horizon extends beyond the fully invisible chart boundary.
// This is presentation coverage only; population and behaviour stay canonical.
inline constexpr double lifeCandidateRadius=360;
inline constexpr size_t lifeCandidateLimit=1024;
inline double lifeChartCoverage(double distance){return 1.-lifeSmooth(180.,330.,distance);}
struct LifeDraw {const OrganismView* organism;double distance,coverage,detail,priority;};
struct LifeVisibilityBudget {bool ready=false;double full=0,edge=0,strength=0;};
inline std::vector<LifeDraw> lifeDrawList(const Scene& scene,V3 eye,V3 forward,bool reduced,bool lowGraphics=false,LifeVisibilityBudget* budget=nullptr,double seconds=1./60){
 std::vector<LifeDraw> result;result.reserve(scene.organisms.size());forward=unit(forward);
 for(const auto& animal:scene.organisms){
  V3 offset=animal.position-eye;double distance=length(offset);
  // A body may still fill the view when its feet are behind the pilot.
  double angular=lifeSmooth(-std::max(.1,animal.size*4.),0.,dot(offset,forward));
  double coverage=std::clamp(animal.visibility,0.,1.)*(1.-lifeSmooth(180.,340.,distance))*angular;
  if(coverage<=.001)continue;
  double detail=reduced?0.:1.-lifeSmooth(28.,64.,distance);
  // An emerging or departing body must not consume a full budget slot.
  // Coverage-weighted priority also smooths rear-plane and chart-edge entry.
  result.push_back({&animal,distance,coverage,detail,distance/(std::max(.75,animal.size)*std::max(.001,coverage))});
 }
 std::sort(result.begin(),result.end(),[](const LifeDraw& a,const LifeDraw& b){return a.priority==b.priority?a.organism->id<b.organism->id:a.priority<b.priority;});
 const size_t limit=reduced?128:lowGraphics?150:256;
 if(result.empty()){if(budget)*budget={};return result;}
 double strength=0,edge=budget&&budget->ready?budget->edge:result.back().priority,full=budget&&budget->ready?budget->full:edge*.5;
 if(result.size()>limit*.65){
  strength=lifeSmooth(limit*.65,double(limit),double(result.size()));
  edge=result[std::min(result.size(),limit)-1].priority;full=result[limit/2].priority;
  // Leave an actual fade band even when a dense ring has equal priorities.
  full=std::min(full,edge*((reduced||lowGraphics)?.5:.7));edge=std::max(edge,full+1e-6);
 }
 if(budget){
  if(!budget->ready)*budget={true,full,edge,strength};
  else{double blend=1.-std::exp(-std::clamp(seconds,0.,.05)/.25);budget->full+=(full-budget->full)*blend;budget->edge+=(edge-budget->edge)*blend;budget->strength+=(strength-budget->strength)*blend;}
  full=budget->full;edge=std::max(budget->edge,full+1e-6);strength=budget->strength;
 }
 if(result.size()>limit)result.resize(limit);
 if(strength>.0001)for(auto& draw:result)draw.coverage*=1.-strength*lifeSmooth(full,edge,draw.priority);
 return result;
}
}
