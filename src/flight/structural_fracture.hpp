#pragma once
#include "structure_damage.hpp"
#include <vector>
#include <limits>
#include <stdexcept>
namespace hyperion::flight {
struct FractureVertex {V3 position,normal;std::array<float,2> uv{};std::array<unsigned char,4> colour{};};
struct FracturePart {std::vector<FractureVertex> vertices;FragmentBounds bounds;int surfaceTriangles=0,capTriangles=0;};
struct StructureFracture {std::vector<FracturePart> parts;double sourceArea=0,fracturedArea=0;int sourceTriangles=0;};
inline double triangleArea(V3 a,V3 b,V3 c){return length(cross(b-a,c-a))*.5;}
inline double coordinate(V3 p,int axis){return axis==0?p.x:axis==1?p.y:p.z;}
inline FractureVertex fractureMix(const FractureVertex& a,const FractureVertex& b,double t){
 FractureVertex p;p.position=mix(a.position,b.position,t);p.normal=unit(mix(a.normal,b.normal,t),a.normal);
 for(int i=0;i<2;i++)p.uv[i]=float(a.uv[i]+(b.uv[i]-a.uv[i])*t);for(int i=0;i<4;i++)p.colour[i]=a.colour[i];return p;
}
inline std::vector<FractureVertex> clipFracture(const std::vector<FractureVertex>& polygon,int axis,double plane,bool keepAbove){
 std::vector<FractureVertex> out;if(polygon.empty())return out;auto a=polygon.back();double da=(coordinate(a.position,axis)-plane)*(keepAbove?1:-1);
 for(const auto& b:polygon){double db=(coordinate(b.position,axis)-plane)*(keepAbove?1:-1);bool insideA=da>=0,insideB=db>=0;
  if(insideA!=insideB)out.push_back(fractureMix(a,b,std::clamp(da/(da-db),0.,1.)));if(insideB)out.push_back(b);a=b;da=db;
 }return out;
}
inline StructureFracture fractureStructure(const std::vector<FractureVertex>& source,int profile,uint32_t seed){
 if(source.empty()||source.size()%3)throw std::runtime_error("Invalid structural fracture source");StructureFracture result;
 V3 low{1e30,1e30,1e30},high{-1e30,-1e30,-1e30};for(const auto& v:source){if(!finite(v.position)||!finite(v.normal))throw std::runtime_error("Nonfinite structural geometry");low={std::min(low.x,v.position.x),std::min(low.y,v.position.y),std::min(low.z,v.position.z)};high={std::max(high.x,v.position.x),std::max(high.y,v.position.y),std::max(high.z,v.position.z)};}
 int nx=2,ny=3,nz=2;if(profile==StationCore){nx=3;ny=2;nz=3;}else if(profile==StationWing){nx=3;ny=1;nz=2;}else if(profile==LandingPad){nx=3;ny=1;nz=3;}else if(profile==LoadingGantry){nx=3;ny=2;nz=1;}else if(profile==FreightCrate||profile==PortTerminal){nx=2;ny=1;nz=2;}
 const int counts[]={nx,ny,nz};std::array<std::vector<double>,3> planes;
 for(int axis=0;axis<3;axis++){double a=coordinate(low,axis),b=coordinate(high,axis);planes[axis].push_back(a);for(int i=1;i<counts[axis];i++){double jitter=(fractureNoise(seed,axis*7+i)-.5)*.12;planes[axis].push_back(a+(b-a)*(double(i)/counts[axis]+jitter/counts[axis]));}planes[axis].push_back(b);}
 result.parts.resize(nx*ny*nz);result.sourceTriangles=int(source.size()/3);for(size_t i=0;i<source.size();i+=3)result.sourceArea+=triangleArea(source[i].position,source[i+1].position,source[i+2].position);
 struct Segment {V3 a,b;};struct Polygon {std::vector<FractureVertex> vertices;bool cap=false;};
 for(int y=0;y<ny;y++)for(int z=0;z<nz;z++)for(int x=0;x<nx;x++){
  int indices[]={x,y,z};int index=(y*nz+z)*nx+x;auto& part=result.parts[index];
  const V3 cellLow{planes[0][x],planes[1][y],planes[2][z]},cellHigh{planes[0][x+1],planes[1][y+1],planes[2][z+1]};
  std::vector<Polygon> polygons;polygons.reserve(source.size()/3);for(size_t i=0;i<source.size();i+=3)polygons.push_back({{source[i],source[i+1],source[i+2]},false});
  // Close each cut before applying the next plane. Subsequent cuts clip the
  // previous end caps too, so cell corners stay sealed rather than losing faces.
  for(int axis=0;axis<3;axis++)for(int side=0;side<2;side++){
   if(side?indices[axis]+1>=counts[axis]:indices[axis]<=0)continue;double plane=coordinate(side?cellHigh:cellLow,axis);std::vector<Polygon> clipped;std::vector<Segment> segments;
   for(const auto& polygon:polygons){bool coplanar=true;for(const auto& v:polygon.vertices)coplanar&=std::abs(coordinate(v.position,axis)-plane)<1e-9;
    // Half-open ownership prevents source surfaces exactly on an interior
    // plane from being counted in both adjacent cells.
    if(coplanar&&!side&&!polygon.cap)continue;
    auto vertices=clipFracture(polygon.vertices,axis,plane,!side);if(vertices.size()<3)continue;
    if(!coplanar)for(size_t i=0;i<vertices.size();i++){auto a=vertices[i].position,b=vertices[(i+1)%vertices.size()].position;if(std::abs(coordinate(a,axis)-plane)<1e-6&&std::abs(coordinate(b,axis)-plane)<1e-6&&length(a-b)>1e-6)segments.push_back({a,b});}
    clipped.push_back({std::move(vertices),polygon.cap});
   }
   V3 normal{};if(axis==0)normal.x=side?1:-1;else if(axis==1)normal.y=side?1:-1;else normal.z=side?1:-1;
   while(!segments.empty()){
    std::vector<V3> loop{segments.back().a,segments.back().b};segments.pop_back();
    for(size_t guard=0;guard<4096&&!segments.empty();guard++){int next=-1;bool reverse=false;for(int i=0;i<int(segments.size());i++){if(length(segments[i].a-loop.back())<1e-5){next=i;break;}if(length(segments[i].b-loop.back())<1e-5){next=i;reverse=true;break;}}if(next<0)break;loop.push_back(reverse?segments[next].a:segments[next].b);segments.erase(segments.begin()+next);if(length(loop.back()-loop.front())<1e-5)break;}
    if(loop.size()<4||length(loop.back()-loop.front())>1e-4)continue;loop.pop_back();
    // Remove collinear triangle-edge subdivisions before ear clipping.
    bool changed=true;while(changed&&loop.size()>3){changed=false;for(size_t i=0;i<loop.size();i++){auto a=loop[(i+loop.size()-1)%loop.size()],b=loop[i],c=loop[(i+1)%loop.size()];if(length(cross(b-a,c-b))<1e-8){loop.erase(loop.begin()+i);changed=true;break;}}}
    V3 area{};for(size_t i=0;i<loop.size();i++)area+=cross(loop[i],loop[(i+1)%loop.size()]);if(dot(area,normal)<0)std::reverse(loop.begin(),loop.end());
    auto vertex=[&](V3 p){FractureVertex v;v.position=p;v.normal=normal;v.uv={float(coordinate(p,(axis+1)%3)*.14),float(coordinate(p,(axis+2)%3)*.14)};v.colour={53,45,60,16};return v;};
    // Ear clipping keeps concave broken silhouettes; no oversized fan plates.
    for(size_t guard=0;loop.size()>=3&&guard<4096;guard++){bool ear=false;for(size_t i=0;i<loop.size();i++){auto a=loop[(i+loop.size()-1)%loop.size()],b=loop[i],c=loop[(i+1)%loop.size()];if(dot(cross(b-a,c-b),normal)<=1e-9)continue;bool inside=false;for(size_t j=0;j<loop.size();j++){if(j==i||j==(i+1)%loop.size()||j==(i+loop.size()-1)%loop.size())continue;auto p=loop[j];if(dot(cross(b-a,p-a),normal)>1e-8&&dot(cross(c-b,p-b),normal)>1e-8&&dot(cross(a-c,p-c),normal)>1e-8){inside=true;break;}}if(inside)continue;clipped.push_back({{vertex(a),vertex(b),vertex(c)},true});loop.erase(loop.begin()+i);ear=true;break;}if(!ear)break;}
   }
   polygons=std::move(clipped);
  }
  for(const auto& polygon:polygons)for(size_t i=1;i+1<polygon.vertices.size();i++){const auto& a=polygon.vertices[0];const auto& b=polygon.vertices[i];const auto& c=polygon.vertices[i+1];double area=triangleArea(a.position,b.position,c.position);if(area<1e-9)continue;for(auto v:{a,b,c})part.vertices.push_back(v);if(polygon.cap)part.capTriangles++;else{part.surfaceTriangles++;result.fracturedArea+=area;}}
  if(part.vertices.empty()){part.bounds={cellLow,cellLow};continue;}
  part.bounds={{1e30,1e30,1e30},{-1e30,-1e30,-1e30}};for(const auto& v:part.vertices){part.bounds.low={std::min(part.bounds.low.x,v.position.x),std::min(part.bounds.low.y,v.position.y),std::min(part.bounds.low.z,v.position.z)};part.bounds.high={std::max(part.bounds.high.x,v.position.x),std::max(part.bounds.high.y,v.position.y),std::max(part.bounds.high.z,v.position.z)};}
 }
 if(std::abs(result.sourceArea-result.fracturedArea)>std::max(.001,result.sourceArea*.00002))throw std::runtime_error("Structural fracture lost exterior surface area");return result;
}
}
