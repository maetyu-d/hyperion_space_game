#pragma once
#include "math.hpp"
#include "raylib.h"
#include <initializer_list>
#include <stdexcept>
namespace hyperion::flight {
inline V3 planetRingPoint(double radius,double angle){return (V3{1,0,0}*std::cos(angle)+unit(V3{0,.32,1})*std::sin(angle))*radius;}
inline V3 planetRingNormal(){return unit(V3{0,-1,.32});}
// Both dimensions need subdivisions: a radial edge is curved by the optical
// chart too. A two-row annulus interpolates entire bands across a straight chord.
inline Mesh planetRingMesh(int segments=384,int radial=32){
 if(segments<3||radial<1||(segments+1)*(radial+1)>65535)throw std::runtime_error("Invalid planet ring tessellation");
 Mesh mesh{};mesh.vertexCount=(segments+1)*(radial+1);mesh.triangleCount=segments*radial*2;
 mesh.vertices=static_cast<float*>(MemAlloc(mesh.vertexCount*3*sizeof(float)));mesh.normals=static_cast<float*>(MemAlloc(mesh.vertexCount*3*sizeof(float)));
 mesh.texcoords=static_cast<float*>(MemAlloc(mesh.vertexCount*2*sizeof(float)));mesh.indices=static_cast<unsigned short*>(MemAlloc(mesh.triangleCount*3*sizeof(unsigned short)));
 auto n=planetRingNormal();for(int i=0;i<=segments;i++)for(int j=0;j<=radial;j++){
  int v=i*(radial+1)+j;auto p=planetRingPoint(1.29+.48*j/radial,i*2*pi/segments);
  mesh.vertices[v*3]=float(p.x);mesh.vertices[v*3+1]=float(p.y);mesh.vertices[v*3+2]=float(p.z);
  mesh.normals[v*3]=float(n.x);mesh.normals[v*3+1]=float(n.y);mesh.normals[v*3+2]=float(n.z);
  mesh.texcoords[v*2]=float(j)/radial;mesh.texcoords[v*2+1]=float(i)/segments;
 }
 int k=0;for(int i=0;i<segments;i++)for(int j=0;j<radial;j++){unsigned short a=static_cast<unsigned short>(i*(radial+1)+j),b=static_cast<unsigned short>(a+radial+1);
  for(auto index:{a,b,static_cast<unsigned short>(a+1),static_cast<unsigned short>(a+1),b,static_cast<unsigned short>(b+1)})mesh.indices[k++]=index;
 }
 UploadMesh(&mesh,false);return mesh;
}
}
