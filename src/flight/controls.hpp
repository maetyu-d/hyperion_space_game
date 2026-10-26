#pragma once
#include <algorithm>
#include <cmath>
namespace hyperion::flight {
struct LookRates {double yaw=0,pitch=0;};
// Mouse deltas are distances per frame; helm commands are angular rates.
inline LookRates pointerLook(double dx,double dy,double seconds){
 if(!std::isfinite(seconds)||seconds<=0||!std::isfinite(dx)||!std::isfinite(dy))return {};
 double scale=.15/(60*seconds);
 return {std::clamp(dx*scale,-1.,1.),std::clamp(dy*scale,-1.,1.)};
}
}
