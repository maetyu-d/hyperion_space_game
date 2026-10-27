#pragma once
#include "math.hpp"
#include <array>
#include <map>
#include <vector>
namespace hyperion::flight {
struct WireFace {std::array<float,3> features{0,0,0};std::array<V3,3> adjacent{};};
inline std::array<float,2> wireOctEncode(V3 n){n=unit(n);n=n/(std::abs(n.x)+std::abs(n.y)+std::abs(n.z));if(n.z<0){double x=n.x;n.x=(1.-std::abs(n.y))*(x<0?-1:1);n.y=(1.-std::abs(x))*(n.y<0?-1:1);}return {float(n.x),float(n.y)};}
inline V3 wireOctDecode(std::array<float,2> p){V3 n{p[0],p[1],1.-std::abs(p[0])-std::abs(p[1])};if(n.z<0){double x=n.x;n.x=(1.-std::abs(n.y))*(x<0?-1:1);n.y=(1.-std::abs(x))*(n.y<0?-1:1);}return unit(n);}
// Weld normal/material seams, retain true creases, and remember the adjacent
// face for dynamic silhouettes. Coplanar triangulation stays invisible.
inline std::vector<WireFace> wireTopology(const float* positions,int vertices,const unsigned short* indices,int triangles){
 using Point=std::array<long long,3>;using Edge=std::pair<Point,Point>;
 struct Face {int triangle,opposite;V3 normal;};std::map<Edge,std::vector<Face>> edges;
 std::vector<WireFace> result(std::max(0,triangles));if(!positions||vertices<3||triangles<=0)return result;
 double extent=1;for(int i=0;i<vertices*3;i++)extent=std::max(extent,std::abs(double(positions[i])));const double epsilon=extent*1e-6;
 auto point=[&](int i){return V3{positions[i*3],positions[i*3+1],positions[i*3+2]};};
 auto weld=[&](V3 p){return Point{std::llround(p.x/epsilon),std::llround(p.y/epsilon),std::llround(p.z/epsilon)};};
 for(int t=0;t<triangles;t++){
  int ids[3];bool valid=true;for(int k=0;k<3;k++){ids[k]=indices?indices[t*3+k]:t*3+k;valid=valid&&ids[k]>=0&&ids[k]<vertices;}if(!valid)continue;
  V3 p[3]={point(ids[0]),point(ids[1]),point(ids[2])};auto crossNormal=cross(p[1]-p[0],p[2]-p[0]);if(length(crossNormal)<epsilon*epsilon)continue;auto normal=unit(crossNormal);
  for(int k=0;k<3;k++){result[t].adjacent[k]=normal;auto a=weld(p[(k+1)%3]),b=weld(p[(k+2)%3]);if(a==b)continue;if(b<a)std::swap(a,b);edges[{a,b}].push_back({t,k,normal});}
 }
 for(const auto& [edge,faces]:edges){bool feature=faces.size()==1;
  for(size_t i=0;i<faces.size()&&!feature;i++)for(size_t j=i+1;j<faces.size();j++)if(std::abs(dot(faces[i].normal,faces[j].normal))<.94)feature=true;
  for(const auto& f:faces){auto& out=result[f.triangle];out.features[f.opposite]=feature?1.f:0.f;double best=-2;
   for(const auto& other:faces)if(other.triangle!=f.triangle){double facing=dot(f.normal,other.normal);if(facing>best){best=facing;out.adjacent[f.opposite]=other.normal;}}
  }
 }return result;
}
inline std::vector<std::array<float,3>> wireFeatures(const float* positions,int vertices,const unsigned short* indices,int triangles){auto topology=wireTopology(positions,vertices,indices,triangles);std::vector<std::array<float,3>> result;result.reserve(topology.size());for(const auto& face:topology)result.push_back(face.features);return result;}
}
