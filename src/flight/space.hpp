#pragma once
#include "fold.hpp"
#include <cstdint>
namespace hyperion::flight {
// Ordinary flight uses a Nil optical chart. Physical positions and velocities
// remain canonical; the near field blends to a flat chart for precision flight.
struct SpaceView {
 V3 eye{},x{1,0,0},y{0,1,0},z{0,0,1};double radius=14000,strength=1;
 V3 local(V3 v)const{return {dot(v,x),dot(v,y),dot(v,z)};}
 V3 world(V3 v)const{return x*v.x+y*v.y+z*v.z;}
};
inline double spaceSmooth(double a,double b,double v){double t=std::clamp((v-a)/(b-a),0.,1.);return t*t*(3-2*t);}
// Principal inverse of the exact symmetric-Heisenberg exponential map. The
// equation is monotone on (-2 pi,2 pi); bisection stays finite near the cut locus.
inline V3 nilLog(V3 q){
 double r2=q.x*q.x+q.y*q.y,az=std::abs(q.z);if(az<1e-7)return q;
 double lo=0,hi=std::min(az,2*pi-.003);
 for(int i=0;i<18;i++){double a=(lo+hi)*.5,h=a*.5;double term=a<.02?a/12+a*a*a/360:(a-std::sin(a))/(8*std::sin(h)*std::sin(h));if(a+r2*term<az)lo=a;else hi=a;}
 double a=(lo+hi)*.5*(q.z<0?-1:1),h=a*.5,c=std::abs(h)<.001?1-h*h/3:h/std::tan(h);
 return {c*q.x+h*q.y,c*q.y-h*q.x,a};
}
inline V3 spacePoint(const SpaceView& s,V3 point){
 if(s.strength<=0)return point;auto delta=point-s.eye;double blend=s.strength*spaceSmooth(300,2200,length(delta));if(blend<=0)return point;
 auto p=s.local(s.eye)/s.radius,q=s.local(delta)/s.radius;q.z-=.5*(p.x*q.y-p.y*q.x);
 return s.eye+mix(delta,s.world(nilLog(q))*s.radius,blend);
}
inline V3 spaceSky(const SpaceView& s,V3 direction){
 if(s.strength<=0)return direction;auto v=s.local(unit(direction)),p=s.local(s.eye)/s.radius,q=nilGeodesic(v,3.8);q.z+=.5*(p.x*q.y-p.y*q.x);
 return unit(mix(direction,unit(s.world(q)),s.strength),direction);
}
inline SpaceView spaceView(V3 eye,uint32_t seed){
 SpaceView s;s.eye=eye;s.radius=12000+seed%5001;
 s.z=unit({.2+std::sin(seed*.017)*.3,1.,.2+std::cos(seed*.013)*.3});s.x=unit(cross({0,0,1},s.z));s.y=unit(cross(s.z,s.x));return s;
}
}
