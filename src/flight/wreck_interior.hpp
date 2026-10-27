#pragma once
#include "structural_fracture.hpp"
#include "structure_mesh.hpp"
#include <set>
#include <tuple>

namespace hyperion::flight::architecture {
// Recessed housings, layered metal and severed service bundles belong to each
// cut face. They use its local frame, so they move with the original fragment.
inline Builder fractureInterior(const FracturePart& part,uint32_t seed,int limit){
 Builder out;std::set<std::tuple<int,int,int,int>> used;int placed=0;
 for(size_t i=0;i+2<part.vertices.size()&&placed<limit;i+=3){const auto& a=part.vertices[i];const auto& b=part.vertices[i+1];const auto& c=part.vertices[i+2];if(a.colour[3]!=16||b.colour[3]!=16||c.colour[3]!=16)continue;
  auto n=unit(a.normal),x=unit(b.position-a.position),y=unit(cross(n,x));double area=triangleArea(a.position,b.position,c.position);if(area<.45)continue;auto centre=(a.position+b.position+c.position)/3.;int axis=std::abs(n.x)>.9?0:std::abs(n.y)>.9?1:2;auto key=std::make_tuple(axis,int(std::round(coordinate(centre,axis)*100)),int(std::floor(coordinate(centre,(axis+1)%3)/3)),int(std::floor(coordinate(centre,(axis+2)%3)/3)));if(!used.insert(key).second)continue;
  double width=std::min({5.,length(b.position-a.position)*.28,length(c.position-a.position)*.28});if(width<.35)continue;placed++;out.frame={centre+n*.025,x,y,n};
  Color metal{66,61,73,16},graphite{31,37,45,7},copper{105,72,56,0},ceramic{107,118,112,5};
  out.bevelBox({0,0,.02},{width*1.6,width*.9,.09},metal,.06);out.bevelBox({-width*.22,0,.11},{width*.8,width*.52,.15},graphite,.04);
  for(int rib=0;rib<4;rib++)out.box({-width*.51+rib*width*.19,0,.20},{width*.065,width*.43,.05},ceramic);
  for(int wire=0;wire<3;wire++){double offset=width*(.20+wire*.10);auto previous=V3{-width*.72,offset,.12};for(int k=1;k<=5;k++){double t=k/5.;V3 at{-width*.72+t*width*1.25,offset+std::sin(t*pi)*width*.11,.12+std::sin(t*pi)*width*.14};out.beam(previous,at,std::max(.018,width*.018),wire==1?ceramic:copper,5);previous=at;}}
  // Torn ends curl away from the bulkhead; thin spectral insulation is a
  // material accent, while the cooling seam supplies the transient light.
  for(int wire=0;wire<2;wire++){double phase=fractureNoise(seed,int(i)+wire)*pi;V3 a{width*.53,width*(.18+wire*.14),.13},b{width*.72,width*(.18+wire*.14),.18+width*.12},c{width*(.78+std::sin(phase)*.06),width*(.08+wire*.19),.12+width*.25};out.beam(a,b,width*.018,copper,5);out.beam(b,c,width*.015,Color{100,151,143,16},5);}
  out.box({width*.50,-width*.19,.12},{width*.26,width*.24,.14},graphite);out.box({width*.50,-width*.19,.20},{width*.19,width*.06,.025},Color{105,159,157,16});
 }
 out.frame={};return out;
}
inline Builder hullInterior(V3 low,V3 high,uint32_t seed,int part){
 Builder out;auto size=high-low,centre=(high+low)*.5;double width=std::min(size.x,size.z)*.45,depth=std::min(size.y*.45,.15);if(width<.025||depth<.008)return out;
 out.bevelBox(centre,{std::min(size.x*.62,.45),std::max(.025,depth),std::min(size.z*.7,.6)},Color{46,51,57,7},.02);
 for(int i=0;i<(part==4?4:2);i++){double t=(i+1.)/(part==4?5:3);auto a=V3{low.x+size.x*.20,centre.y+depth*.6,low.z+size.z*t},b=V3{high.x-size.x*.12,centre.y+depth*(1+fractureNoise(seed,part*7+i)*.5),low.z+size.z*t};out.beam(a,b,std::max(.004,width*.035),i%2?Color{115,82,60,0}:Color{91,141,137,16},5);}
 return out;
}
}
