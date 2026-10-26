#pragma once
#include <algorithm>
#include <cmath>
namespace hyperion {
// Presentation time only. A mode change must settle and release held controls
// before either simulation time or another gameplay command can resume.
struct ModeHandoff {
 int mode=-1;double remaining=0;bool release=false;
 bool ready()const{return remaining<=0&&!release;}
 bool observe(int next,double dt,bool held){
  bool changed=mode>=0&&mode!=next;
  if(changed){remaining=.32;release=true;}
  else if(std::isfinite(dt))remaining=std::max(0.,remaining-std::clamp(dt,0.,.1));
  mode=next;if(!held)release=false;return changed;
 }
 float opacity()const{double t=std::clamp(remaining/.32,0.,1.);return float(t*t*(3-2*t));}
};
}
