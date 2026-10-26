#pragma once
#include <cmath>
namespace hyperion::flight {
constexpr double pi=3.14159265358979323846;
struct V3 {
  double x=0,y=0,z=0;
  V3 operator+(V3 b)const{return{x+b.x,y+b.y,z+b.z};}
  V3 operator-(V3 b)const{return{x-b.x,y-b.y,z-b.z};}
  V3 operator-()const{return{-x,-y,-z};}
  V3 operator*(double s)const{return{x*s,y*s,z*s};}
  V3 operator/(double s)const{return *this*(1/s);}
  V3& operator+=(V3 b){return *this=*this+b;}
};
inline double dot(V3 a,V3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline V3 cross(V3 a,V3 b){return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline double length(V3 a){return std::sqrt(dot(a,a));}
inline V3 unit(V3 a,V3 fallback={0,1,0}){double l=length(a);return l>1e-9?a/l:fallback;}
inline V3 mix(V3 a,V3 b,double t){return a*(1-t)+b*t;}
inline V3 rotate(V3 v,V3 axis,double a){axis=unit(axis);return v*std::cos(a)+cross(axis,v)*std::sin(a)+axis*dot(axis,v)*(1-std::cos(a));}
inline bool finite(V3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
}
