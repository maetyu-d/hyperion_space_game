#include "flight/structure_mesh.hpp"
#include <iostream>
#include <map>
#include <stdexcept>
#include <tuple>
using namespace hyperion::flight;
using architecture::Builder;
namespace {
int checks=0;
void require(bool ok,const char* message){checks++;if(!ok)throw std::runtime_error(message);}
using Point=std::array<long long,3>;
Point key(V3 p){return {std::llround(p.x*1e5),std::llround(p.y*1e5),std::llround(p.z*1e5)};}
double inspect(const Builder& b,bool closed){
 require(b.vertices.size()==b.normals.size(),"Position and normal arrays disagree");
 require(b.vertices.size()%9==0&&!b.vertices.empty(),"Triangle stream is incomplete");
 require(b.uv.size()==b.vertices.size()/3*2&&b.colours.size()==b.vertices.size()/3*4,"Vertex attributes disagree");
 std::map<std::pair<Point,Point>,std::pair<int,int>> edges;double volume=0;
 for(size_t t=0;t<b.vertices.size();t+=9){V3 p[3],n[3];
  for(int i=0;i<3;i++){size_t v=t+i*3;p[i]={b.vertices[v],b.vertices[v+1],b.vertices[v+2]};n[i]={b.normals[v],b.normals[v+1],b.normals[v+2]};require(finite(p[i])&&finite(n[i]),"Non-finite mesh attribute");require(std::abs(length(n[i])-1)<1e-5,"Normal lost unit length");}
  auto normal=cross(p[1]-p[0],p[2]-p[0]);require(length(normal)>1e-8,"Degenerate triangle");
  for(auto v:n)require(dot(unit(normal),v)>.01,"Normal disagrees with winding");
  volume+=dot(p[0],cross(p[1],p[2]))/6;
  for(int i=0;i<3;i++){auto a=key(p[i]),c=key(p[(i+1)%3]);bool forward=a<c;auto& edge=edges[forward?std::make_pair(a,c):std::make_pair(c,a)];edge.first++;edge.second+=forward?1:-1;}
 }
 if(closed){for(const auto& [edge,use]:edges)require(use.first==2&&use.second==0,"Mesh has a gap or inconsistent edge winding");require(volume>0,"Closed mesh is inside out");}
 return volume;
}
}
int main(){try{
 for(V3 size:{V3{16,14,18},V3{.8,1.2,4},V3{1,1,1},V3{7,2.4,2.3},V3{3,.05,4}}){
  for(bool transformed:{false,true}){Builder b;if(transformed){double a=.71;b.frame={{32.1,-3.2,17.3},{std::cos(a),0,std::sin(a)},{0,1,0},{-std::sin(a),0,std::cos(a)}};}
   b.bevelBox({},size,WHITE);double volume=inspect(b,true),outer=size.x*size.y*size.z;
   require(volume>outer*.5&&volume<outer*1.001,"Chamfer exceeds housing or collapses it");
  }
 }
 for(int sides:{6,10,16})for(double top:{0.,1.2,3.}){Builder b;b.cylinder({},3,9,WHITE,sides,top);inspect(b,true);}
 for(int slices:{6,12,20}){Builder b;b.sphere({},{3,7,5},WHITE,false,slices,6);double volume=inspect(b,true);require(volume<4*pi*3*7*5/3*1.001,"Ellipsoid exceeds its outer volume");}
 for(int sides:{24,64}){Builder b;b.annularDeck({},20,30,1.35,WHITE,sides);double volume=inspect(b,true);
  require(volume>pi*(30*30-20*20)*1.35*.98&&volume<pi*(30*30-20*20)*1.35,"Mooring collar volume is incorrect");
  for(size_t v=0;v<b.vertices.size();v+=3)require(std::hypot(b.vertices[v],b.vertices[v+2])>=19.99999,"Mooring collar obstructs docking axis");
 }
 for(V3 size:{V3{3.9,3.55,.50},V3{2.85,2.02,.40},V3{11.5,8.3,.38}}){Builder b;b.panelFrame({},size.x,size.y,.15,size.z,WHITE);double volume=inspect(b,true);
  require(std::abs(volume-(size.x*size.y-(size.x-.30)*(size.y-.30))*size.z)<.0001,"Recessed frame volume is incorrect");
 }
 Builder mapped;mapped.mappedQuad({1,-1,0},{-1,-1,0},{-1,1,0},{1,1,0},WHITE,{24,8});inspect(mapped,false);
 for(size_t i=0;i<mapped.vertices.size()/3;i++){
  double x=mapped.vertices[i*3],y=mapped.vertices[i*3+1];
  require(std::abs(mapped.uv[i*2]-(24+(1-x)*.5))<1e-6,"Window U coordinate is discontinuous across the aperture");
  require(std::abs(mapped.uv[i*2+1]-(8+(y+1)*.5))<1e-6,"Window V coordinate is discontinuous across the aperture");
 }
 using architecture::settlementDetail;
 require(settlementDetail(-1,109,30)==0&&settlementDetail(-1,111,30)==2,"Initial detail ignores projected size");
 require(settlementDetail(0,120,30)==0&&settlementDetail(0,136,30)==2,"Far-detail hysteresis is broken");
 int level=2;for(int i=0;i<100;i++){level=settlementDetail(level,400,101+i%39);require(level==2,"Close detail flickers inside the leave threshold");}
 level=settlementDetail(level,400,141);require(level==1,"Close detail never releases");
 for(int i=0;i<100;i++){level=settlementDetail(level,400,100+i%40);require(level==1,"Close detail flickers inside the enter threshold");}
 require(settlementDetail(level,400,99)==2&&settlementDetail(2,89,20)==0,"Near or far transition failed");
 Builder dome;dome.sphere({},{7.5,7,8},WHITE,true,12,6);inspect(dome,false);
 Builder beam;beam.beam({0,0,0},{3,7,4},.15,WHITE,6);inspect(beam,false);
 Builder arc;arc.paintArc({},31,.35,.1,pi,WHITE,32);inspect(arc,false);
 std::cout<<"PASS: "<<checks<<" architecture geometry checks; closed chamfers, cones, tanks, ellipsoids and clear mooring collars; outward unit normals; recessed frames, seam-free window UVs, LOD hysteresis and nondegenerate meshes\n";
 return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
