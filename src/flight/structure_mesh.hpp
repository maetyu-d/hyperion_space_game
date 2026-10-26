#pragma once
#include "game.hpp"
#include "raylib.h"
#include <cstring>

namespace hyperion::flight::architecture {
// Separate enter/leave thresholds keep camera drift from toggling meshes.
inline int settlementDetail(int current,double pixels,double nearest){
 if(current<0)return pixels<110?0:nearest<100?2:1;
 if(pixels<90)return 0;
 if(current==0&&pixels<=135)return 0;
 return current==2?(nearest<=140?2:1):(nearest<100?2:1);
}
struct Frame {V3 origin{},x{1,0,0},y{0,1,0},z{0,0,1};V3 point(V3 p)const{return origin+x*p.x+y*p.y+z*p.z;}};
struct Builder {
 Frame frame;std::vector<float> vertices,normals,uv;std::vector<unsigned char> colours;
 void triangleNormals(V3 a,V3 b,V3 c,V3 na,V3 nb,V3 nc,Color tint){
  if(length(cross(b-a,c-a))<1e-10)return;
  std::array<V3,3> points{a,b,c},ns{na,nb,nc};
  for(int i=0;i<3;i++){auto p=frame.point(points[i]),n=unit(frame.x*ns[i].x+frame.y*ns[i].y+frame.z*ns[i].z);
   for(double v:{p.x,p.y,p.z})vertices.push_back(float(v));for(double v:{n.x,n.y,n.z})normals.push_back(float(v));uv.push_back(i==1?1.f:0.f);uv.push_back(i==2?1.f:0.f);for(auto v:{tint.r,tint.g,tint.b,tint.a})colours.push_back(v);
  }
 }
 void triangle(V3 a,V3 b,V3 c,Color tint){auto n=unit(cross(b-a,c-a));triangleNormals(a,b,c,n,n,n,tint);}
 void facingQuad(V3 a,V3 b,V3 c,V3 d,Color tint,V3 outward){if(dot(cross(b-a,c-a),outward)<0)quad(a,d,c,b,tint);else quad(a,b,c,d,tint);}
 void facingTriangle(V3 a,V3 b,V3 c,Color tint,V3 outward){if(dot(cross(b-a,c-a),outward)<0)triangle(a,c,b,tint);else triangle(a,b,c,tint);}
 void quad(V3 a,V3 b,V3 c,V3 d,Color tint){triangle(a,b,c,tint);triangle(a,c,d,tint);}
 void mappedQuad(V3 a,V3 b,V3 c,V3 d,Color tint,Vector2 offset={}){
  size_t start=uv.size();quad(a,b,c,d,tint);
  if(uv.size()!=start+12)return;
  constexpr float coordinates[]={0,0,1,0,1,1,0,0,1,1,0,1};
  for(int i=0;i<12;i++)uv[start+i]=coordinates[i]+(i%2?offset.y:offset.x);
 }
 // A single watertight frame, including the inner reveal of the aperture.
 void panelFrame(V3 c,double width,double height,double border,double depth,Color tint){
  std::array<V3,4> outer{{{-width*.5,-height*.5,0},{width*.5,-height*.5,0},{width*.5,height*.5,0},{-width*.5,height*.5,0}}};
  auto inner=outer;for(auto& p:inner){p.x-=std::copysign(border,p.x);p.y-=std::copysign(border,p.y);}
  V3 front{0,0,-depth*.5},back{0,0,depth*.5};
  for(int i=0;i<4;i++){int j=(i+1)%4;auto a=c+outer[i],b=c+outer[j],u=c+inner[i],v=c+inner[j];
   facingQuad(a+front,b+front,v+front,u+front,tint,{0,0,-1});facingQuad(a+back,u+back,v+back,b+back,tint,{0,0,1});
   facingQuad(a+front,a+back,b+back,b+front,tint,outer[i]+outer[j]);facingQuad(u+front,v+front,v+back,u+back,tint,-inner[i]-inner[j]);
  }
 }
 void box(V3 c,V3 size,Color tint){auto h=size*.5;
  quad(c+V3{h.x,-h.y,-h.z},c+V3{h.x,h.y,-h.z},c+V3{h.x,h.y,h.z},c+V3{h.x,-h.y,h.z},tint);
  quad(c+V3{-h.x,-h.y,h.z},c+V3{-h.x,h.y,h.z},c+V3{-h.x,h.y,-h.z},c+V3{-h.x,-h.y,-h.z},tint);
  quad(c+V3{-h.x,h.y,-h.z},c+V3{-h.x,h.y,h.z},c+V3{h.x,h.y,h.z},c+V3{h.x,h.y,-h.z},tint);
  quad(c+V3{-h.x,-h.y,h.z},c+V3{-h.x,-h.y,-h.z},c+V3{h.x,-h.y,-h.z},c+V3{h.x,-h.y,h.z},tint);
  quad(c+V3{h.x,-h.y,h.z},c+V3{h.x,h.y,h.z},c+V3{-h.x,h.y,h.z},c+V3{-h.x,-h.y,h.z},tint);
  quad(c+V3{-h.x,-h.y,-h.z},c+V3{-h.x,h.y,-h.z},c+V3{h.x,h.y,-h.z},c+V3{h.x,-h.y,-h.z},tint);
 }
 // Thin decals keep simple boxes; substantial housings get real chamfers.
 void bevelBox(V3 centre,V3 size,Color tint,double bevel=.22){
  double minimum=std::min({size.x,size.y,size.z});if(minimum<.75){box(centre,size,tint);return;}
  std::array<V3,3> axis{{{1,0,0},{0,1,0},{0,0,1}}};std::array<double,3> h{size.x*.5,size.y*.5,size.z*.5};double b=std::min(bevel,minimum*.24);
  for(int i=0;i<3;i++){int j=(i+1)%3,k=(i+2)%3;for(int sign:{-1,1}){auto c=centre+axis[i]*(sign*h[i]),u=axis[j]*(h[j]-b),v=axis[k]*(h[k]-b);facingQuad(c-u-v,c+u-v,c+u+v,c-u+v,tint,axis[i]*sign);}}
  for(int i=0;i<3;i++)for(int j=i+1;j<3;j++){int k=3-i-j;for(int si:{-1,1})for(int sj:{-1,1}){
   auto a=centre+axis[i]*(si*h[i])+axis[j]*(sj*(h[j]-b)),c=centre+axis[i]*(si*(h[i]-b))+axis[j]*(sj*h[j]),v=axis[k]*(h[k]-b);
   facingQuad(a-v,a+v,c+v,c-v,tint,axis[i]*si+axis[j]*sj);
  }}
  for(int x:{-1,1})for(int y:{-1,1})for(int z:{-1,1}){
   V3 a=centre+V3{x*h[0],y*(h[1]-b),z*(h[2]-b)},c=centre+V3{x*(h[0]-b),y*h[1],z*(h[2]-b)},d=centre+V3{x*(h[0]-b),y*(h[1]-b),z*h[2]};facingTriangle(a,c,d,tint,V3{double(x),double(y),double(z)});
  }
 }
 void cylinder(V3 c,double radius,double height,Color tint,int sides=10,double top=-1){if(top<0)top=radius;
  for(int i=0;i<sides;i++){double a=i*2*pi/sides,b=(i+1)*2*pi/sides;V3 p{std::cos(a),0,std::sin(a)},q{std::cos(b),0,std::sin(b)};
   auto A=c+p*radius,B=c+p*top+V3{0,height,0},C=c+q*top+V3{0,height,0},D=c+q*radius;
   if(sides>=10){auto np=unit(p+V3{0,(radius-top)/height,0}),nq=unit(q+V3{0,(radius-top)/height,0});if(top>0)triangleNormals(A,B,C,np,np,nq,tint);triangleNormals(A,C,D,np,nq,nq,tint);}
   else{if(top>0)triangle(A,B,C,tint);triangle(A,C,D,tint);}
   if(top>0)triangle(c+V3{0,height,0},C,B,tint);triangle(c,A,D,tint);
  }
 }
 // A closed service collar leaves an open cylinder around the docking axis.
 void annularDeck(V3 c,double inner,double outer,double height,Color tint,int sides=48){
  for(int i=0;i<sides;i++){double a=i*2*pi/sides,b=(i+1)*2*pi/sides;V3 p{std::cos(a),0,std::sin(a)},q{std::cos(b),0,std::sin(b)},up{0,height,0};
   auto A=c+p*inner,B=c+q*inner,C=c+q*outer,D=c+p*outer;
   facingQuad(A+up,B+up,C+up,D+up,tint,{0,1,0});facingQuad(A,D,C,B,tint,{0,-1,0});
   facingQuad(D,C,C+up,D+up,tint,p+q);facingQuad(A,A+up,B+up,B,tint,-p-q);
  }
 }
 void paintArc(V3 c,double radius,double width,double begin,double end,Color tint,int segments=40){
  for(int i=0;i<segments;i++){double a=begin+(end-begin)*i/segments,b=begin+(end-begin)*(i+1)/segments;V3 p{std::cos(a),0,std::sin(a)},q{std::cos(b),0,std::sin(b)};
   facingQuad(c+p*(radius-width*.5),c+q*(radius-width*.5),c+q*(radius+width*.5),c+p*(radius+width*.5),tint,{0,1,0});}
 }
 void beam(V3 a,V3 b,double radius,Color tint,int sides=5){V3 d=unit(b-a),x=unit(cross(d,{0,1,0}),{1,0,0}),z=unit(cross(x,d));
  for(int i=0;i<sides;i++){double t=i*2*pi/sides,u=(i+1)*2*pi/sides;V3 p=(x*std::cos(t)+z*std::sin(t))*radius,q=(x*std::cos(u)+z*std::sin(u))*radius;quad(a+p,b+p,b+q,a+q,tint);}}
 void ring(V3 c,double radius,double thick,Color tint,int segments=32){for(int i=0;i<segments;i++){double a=i*2*pi/segments,b=(i+1)*2*pi/segments;beam(c+V3{std::cos(a)*radius,0,std::sin(a)*radius},c+V3{std::cos(b)*radius,0,std::sin(b)*radius},thick,tint,4);}}
 void sphere(V3 c,V3 scale,Color tint,bool dome=false,int slices=12,int stacks=6){
  auto point=[&](double a,double b){return c+V3{std::sin(a)*std::cos(b)*scale.x,std::cos(a)*scale.y,std::sin(a)*std::sin(b)*scale.z};};
  auto normal=[&](double a,double b){return unit(V3{std::sin(a)*std::cos(b)/scale.x,std::cos(a)/scale.y,std::sin(a)*std::sin(b)/scale.z});};
  for(int y=0;y<stacks;y++)for(int x=0;x<slices;x++){double a=y*(dome?pi*.5:pi)/stacks,b=(y+1)*(dome?pi*.5:pi)/stacks,u=x*2*pi/slices,v=(x+1)*2*pi/slices;
   auto A=point(a,u),B=point(a,v),C=point(b,v),D=point(b,u),na=normal(a,u),nb=normal(a,v),nc=normal(b,v),nd=normal(b,u);
   if(y>0)triangleNormals(A,B,C,na,nb,nc,tint);if(dome||y+1<stacks)triangleNormals(A,C,D,na,nc,nd,tint);
  }
 }
 Model model(Shader shader){Mesh m{};m.vertexCount=int(vertices.size()/3);m.triangleCount=m.vertexCount/3;
  auto copy=[](const auto& v){auto* p=MemAlloc(unsigned(v.size()*sizeof(v[0])));std::memcpy(p,v.data(),v.size()*sizeof(v[0]));return p;};
  m.vertices=static_cast<float*>(copy(vertices));m.normals=static_cast<float*>(copy(normals));m.texcoords=static_cast<float*>(copy(uv));m.colors=static_cast<unsigned char*>(copy(colours));UploadMesh(&m,false);auto result=LoadModelFromMesh(m);result.materials[0].shader=shader;return result;
 }
};
}
