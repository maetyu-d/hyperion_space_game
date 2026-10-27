#pragma once
#include "math.hpp"
#include <array>
#include <map>
#include <tuple>
#include <vector>
namespace hyperion::flight {
// Weld by position: architectural builders split vertices at material and
// normal seams. Coplanar triangle diagonals should never become visible lines.
inline std::vector<std::array<float,3>> wireFeatures(const float* positions,int vertices,const unsigned short* indices,int triangles){
 using Point=std::array<long long,3>;using Edge=std::pair<Point,Point>;
 struct Face {int triangle,opposite;V3 normal;};std::map<Edge,std::vector<Face>> edges;
 std::vector<std::array<float,3>> result(std::max(0,triangles),{0,0,0});
 if(!positions||vertices<3||triangles<=0)return result;
 double extent=1;for(int i=0;i<vertices*3;i++)extent=std::max(extent,std::abs(double(positions[i])));const double epsilon=extent*1e-6;
 auto point=[&](int i){return V3{positions[i*3],positions[i*3+1],positions[i*3+2]};};
 auto weld=[&](V3 p){return Point{std::llround(p.x/epsilon),std::llround(p.y/epsilon),std::llround(p.z/epsilon)};};
 for(int t=0;t<triangles;t++){
  int ids[3];bool valid=true;for(int k=0;k<3;k++){ids[k]=indices?indices[t*3+k]:t*3+k;valid=valid&&ids[k]>=0&&ids[k]<vertices;}if(!valid)continue;
  V3 p[3]={point(ids[0]),point(ids[1]),point(ids[2])};auto crossNormal=cross(p[1]-p[0],p[2]-p[0]);if(length(crossNormal)<epsilon*epsilon)continue;auto normal=unit(crossNormal);
  for(int k=0;k<3;k++){auto a=weld(p[(k+1)%3]),b=weld(p[(k+2)%3]);if(a==b)continue;if(b<a)std::swap(a,b);edges[{a,b}].push_back({t,k,normal});}
 }
 for(const auto& [edge,faces]:edges){bool feature=faces.size()==1;
  for(size_t i=0;i<faces.size()&&!feature;i++)for(size_t j=i+1;j<faces.size();j++)if(std::abs(dot(faces[i].normal,faces[j].normal))<.94)feature=true;
  if(feature)for(const auto& f:faces)result[f.triangle][f.opposite]=1;
 }return result;
}
}
