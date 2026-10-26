#include "renderer.hpp"
#include "life_mesh.hpp"
#include "life_shaders.hpp"
#include "rlgl.h"
#include <cstring>
#include <stdexcept>
namespace hyperion::flight {
void Renderer::clearLife(){for(auto& model:lifeModels)if(model.meshCount){UnloadModel(model);model={};}if(lifeShader.id){UnloadShader(lifeShader);lifeShader={};}lifeDrawn=0;}
void Renderer::drawLife(const Scene& sceneView){
 lifeDrawn=0;if(sceneView.organisms.empty())return;
 if(!lifeShader.id){lifeShader=LoadShaderFromMemory(lifeVertex,lifeFragment);if(!IsShaderValid(lifeShader))throw std::runtime_error("Wildlife shader failed");const char* names[]={"clock","phase","gait","activity","rooted","feeding","pigment","sun","eye","shape","genes","bodyPlan","resting"};for(int i=0;i<13;i++)lifeUniforms[i]=GetShaderLocation(lifeShader,names[i]);for(int i=0;i<13;i++)if(lifeUniforms[i]<0)throw std::runtime_error("Wildlife shader interface failed");}
 auto set=[&](int i,float value){if(lifeUniforms[i]>=0)SetShaderValue(lifeShader,lifeUniforms[i],&value,SHADER_UNIFORM_FLOAT);};
 Vector3 light{float(sunDirection.x),float(sunDirection.y),float(sunDirection.z)};SetShaderValue(lifeShader,lifeUniforms[7],&light,SHADER_UNIFORM_VEC3);SetShaderValue(lifeShader,lifeUniforms[8],&camera.position,SHADER_UNIFORM_VEC3);set(0,float(std::fmod(sceneView.hours*55,100000.)));
 V3 eye{camera.position.x,camera.position.y,camera.position.z},look=unit(V3{camera.target.x,camera.target.y,camera.target.z}-eye);int limit=reduced?88:150;
 // One batched, terrain-aligned contact shadow pass anchors nearby land life.
 // Soft outer vertices blend into the existing ground instead of adding a decal texture.
 rlSetTexture(0);rlBegin(RL_TRIANGLES);int shadows=0;
 for(const auto& animal:sceneView.organisms){double d=length(animal.position-eye);if(d>230||dot(animal.position-eye,look)<-8||d<1.2)continue;if(shadows++>=limit)break;if(animal.marine||d>100)continue;
  auto up=unit(animal.up),forward=unit(animal.forward-up*dot(animal.forward,up)),right=unit(cross(forward,up)),at=animal.position-up*.08;double radius=animal.size*(animal.flora?.48:.65);
  for(int k=0;k<12;k++){double a=k*2*pi/12,b=(k+1)*2*pi/12;auto p=at+(right*std::cos(a)+forward*std::sin(a)*.75)*radius,q=at+(right*std::cos(b)+forward*std::sin(b)*.75)*radius;rlColor4ub(3,9,18,45);rlVertex3f(float(at.x),float(at.y),float(at.z));rlColor4ub(3,9,18,0);rlVertex3f(float(p.x),float(p.y),float(p.z));rlVertex3f(float(q.x),float(q.y),float(q.z));}
 }
 rlEnd();rlColor4ub(255,255,255,255);
 for(const auto& animal:sceneView.organisms){double distance=length(animal.position-eye);if(distance>230||dot(animal.position-eye,look)<-8||distance<1.2)continue;if(lifeDrawn>=limit)break;
  bool detailed=!reduced&&distance<42;int key=std::clamp(animal.plan,0,5)+(animal.flora?6:0)+(detailed?12:0);auto& model=lifeModels[key];
  if(!model.meshCount){auto data=lifeMesh(animal.plan,animal.flora,detailed);Mesh mesh{};mesh.vertexCount=int(data.positions.size()/3);mesh.triangleCount=mesh.vertexCount/3;auto copy=[](const auto& source){auto* memory=MemAlloc(unsigned(source.size()*sizeof(source[0])));std::memcpy(memory,source.data(),source.size()*sizeof(source[0]));return memory;};mesh.vertices=static_cast<float*>(copy(data.positions));mesh.normals=static_cast<float*>(copy(data.normals));mesh.colors=static_cast<unsigned char*>(copy(data.colours));UploadMesh(&mesh,false);model=LoadModelFromMesh(mesh);model.materials[0].shader=lifeShader;}
  V3 up=unit(animal.up),forward=unit(animal.forward-up*dot(animal.forward,up)),right=unit(cross(forward,up));up=unit(cross(right,forward));model.transform={float(right.x),float(up.x),float(-forward.x),0,float(right.y),float(up.y),float(-forward.y),0,float(right.z),float(up.z),float(-forward.z),0,0,0,0,1};
  set(1,float(animal.phase));set(2,float(animal.gait));set(3,float(animal.activity));set(4,animal.flora?1.f:0.f);set(5,animal.feeding?1.f:0.f);set(6,float(animal.hue));float shape[]={float(animal.shape.x),float(animal.shape.y),float(animal.shape.z)};SetShaderValue(lifeShader,lifeUniforms[9],shape,SHADER_UNIFORM_VEC3);set(10,float(animal.genes));set(11,float(animal.plan));set(12,animal.resting?1.f:0.f);
  DrawModel(model,{float(animal.position.x),float(animal.position.y),float(animal.position.z)},float(animal.size),WHITE);lifeDrawn++;
 }
}
}
