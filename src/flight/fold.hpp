#pragma once
#include "math.hpp"
#include <algorithm>
#include <limits>
#include <string>
#include <vector>

namespace hyperion::flight {
struct FoldGate {
 std::string id,link,name;V3 position,normal{0,0,1},up{0,1,0};
 double radius=90;int peer=-1,body=-1,authority=3;bool surveyed=false,nil=false;
};
struct NilRegion {std::string id;V3 position,axis{0,0,1},up{0,1,0};double radius=1000,scale=1000;int body=-1;};
inline V3 foldRight(const FoldGate& g){return unit(cross(g.up,g.normal),{1,0,0});}
inline V3 foldVector(const FoldGate& a,const FoldGate& b,V3 v){
 return foldRight(b)*-dot(v,foldRight(a))+b.up*dot(v,a.up)-b.normal*dot(v,a.normal);
}
inline V3 foldPoint(const FoldGate& a,const FoldGate& b,V3 p){return b.position+foldVector(a,b,p-a.position);}
inline double foldIntersection(const FoldGate& gate,V3 a,V3 b,double clearance=0){
 double before=dot(a-gate.position,gate.normal),after=dot(b-gate.position,gate.normal);
 if(before<=1e-8||after>0||before-after<1e-9||clearance>=gate.radius)return std::numeric_limits<double>::infinity();
 double t=before/(before-after);V3 offset=mix(a,b,t)-gate.position;
 return length(offset-gate.normal*dot(offset,gate.normal))<=gate.radius-clearance?t:std::numeric_limits<double>::infinity();
}
inline int firstFold(const std::vector<FoldGate>& gates,V3 a,V3 b,double& fraction,double clearance=0){
 int result=-1;fraction=std::numeric_limits<double>::infinity();
 for(int i=0;i<int(gates.size());i++)if(gates[i].peer>=0&&gates[i].peer<int(gates.size())){
  double t=foldIntersection(gates[i],a,b,clearance);if(t<fraction){fraction=t;result=i;}
 }
 return result;
}
inline std::vector<V3> avoidFoldRoute(const std::vector<FoldGate>& gates,V3 start,const std::vector<V3>& path,int intended=-1){
 std::vector<V3> result;auto current=start;
 for(auto end:path){
  for(int pass=0;pass<8;pass++){
   int blocker=-1;double first=std::numeric_limits<double>::infinity();
   for(int i=0;i<int(gates.size());i++)if(i!=intended){double t=foldIntersection(gates[i],current,end,-80);if(t<first){first=t;blocker=i;}}
   if(blocker<0)break;const auto& gate=gates[blocker];auto side=foldRight(gate)*(gate.radius+180);
   V3 before=gate.position+side+gate.normal*140,after=gate.position+side-gate.normal*140;
   result.push_back(before);result.push_back(after);current=after;
  }
  result.push_back(end);current=end;
 }
 return result;
}
inline double nilWeight(const NilRegion& n,V3 p){double t=std::clamp((n.radius-length(p-n.position))/(n.radius*.3),0.,1.);return t*t*(3-2*t);}
// Symmetric Heisenberg coordinates. The orthonormal initial velocity has unit
// metric length; t is intrinsic path length, not Euclidean coordinate distance.
inline V3 nilGeodesic(V3 v,double t){
 if(std::abs(v.z)<1e-6)return {v.x*t,v.y*t,v.z*t};
 double s=std::sin(v.z*t),c=std::cos(v.z*t),w=v.z;
 return {(v.x*s+v.y*(c-1))/w,(v.x*(1-c)+v.y*s)/w,w*t+(v.x*v.x+v.y*v.y)*(w*t-s)/(2*w*w)};
}
}
