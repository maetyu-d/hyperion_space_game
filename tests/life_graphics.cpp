#include "flight/life_mesh.hpp"
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
using namespace hyperion::flight;
namespace {int checks=0;void require(bool value,const char* why){checks++;if(!value)throw std::runtime_error(why);}}
int main(){try{
 int counts[2]{};std::set<size_t> silhouettes;
 for(int flora=0;flora<2;flora++)for(int plan=0;plan<6;plan++){
  V3 bounds[2][2];size_t vertices[2]{};
  for(int detailed=0;detailed<2;detailed++){
   auto m=lifeMesh(plan,flora,detailed);vertices[detailed]=m.positions.size()/3;counts[detailed]+=int(vertices[detailed]/3);silhouettes.insert(vertices[detailed]);
   require(m.positions.size()==m.normals.size()&&m.positions.size()%9==0&&m.colours.size()==vertices[detailed]*4,"complete position, normal and tissue attributes");
   bounds[detailed][0]={1e9,1e9,1e9};bounds[detailed][1]={-1e9,-1e9,-1e9};std::set<int> tissues,flex;
   for(size_t i=0;i<vertices[detailed];i++){
    V3 p{m.positions[i*3],m.positions[i*3+1],m.positions[i*3+2]},n{m.normals[i*3],m.normals[i*3+1],m.normals[i*3+2]};
    require(finite(p)&&finite(n)&&std::abs(length(n)-1)<1e-5,"finite anatomy and unit smooth normals");
    require(p.y>=-.015&&std::abs(p.x)<3.2&&p.y<3.2&&std::abs(p.z)<3.2,"appendages remain above the attachment plane and inside the footprint");
    for(int axis=0;axis<3;axis++){double v=axis==0?p.x:axis==1?p.y:p.z;double* lo=axis==0?&bounds[detailed][0].x:axis==1?&bounds[detailed][0].y:&bounds[detailed][0].z;double* hi=axis==0?&bounds[detailed][1].x:axis==1?&bounds[detailed][1].y:&bounds[detailed][1].z;*lo=std::min(*lo,v);*hi=std::max(*hi,v);}
    int role=m.colours[i*4];require(role==LifeFlesh||role==LifeShell||role==LifeMembrane||role==LifeVein||role==LifeEye||role==LifePupil,"every tissue uses the shader's material contract");tissues.insert(role);flex.insert(m.colours[i*4+1]);require(m.colours[i*4+3]==255,"opaque tissue geometry retains stable depth occlusion");
   }
   for(size_t i=0;i<vertices[detailed];i+=3){V3 p[3],n{};for(int k=0;k<3;k++){p[k]={m.positions[(i+k)*3],m.positions[(i+k)*3+1],m.positions[(i+k)*3+2]};n+=V3{m.normals[(i+k)*3],m.normals[(i+k)*3+1],m.normals[(i+k)*3+2]};}auto face=cross(p[1]-p[0],p[2]-p[0]);require(length(face)>1e-10,"no degenerate anatomy triangles");if(dot(unit(face),unit(n))<=.01)std::cerr<<"Mesh "<<flora<<"/"<<plan<<"/"<<detailed<<" triangle "<<i/3<<" normal agreement "<<dot(unit(face),unit(n))<<"\n";require(dot(unit(face),unit(n))>.01,"surface winding agrees with outward tissue normals");}
   require(tissues.size()>=2&&flex.size()>3,"anatomy contains distinct tissue and graded appendage movement");
  }
  require(vertices[1]>vertices[0],"near anatomy has more geometric detail than distant anatomy");
  require(length(bounds[0][0]-bounds[1][0])<.18&&length(bounds[0][1]-bounds[1][1])<.18,"LOD transitions preserve the same body footprint and silhouette");
 }
 require(silhouettes.size()>=18&&counts[0]<22000&&counts[1]<48000,"all body forms remain varied and bounded for flight");
 std::cout<<"Life graphics: "<<checks<<" checks / 24 anatomical LODs / "<<counts[0]<<" distant and "<<counts[1]<<" close triangles / tissue roles, smooth normals, winding, appendages and footprints passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" life graphics checks: "<<e.what()<<'\n';return 1;}}
