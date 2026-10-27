#include "renderer.hpp"
#include "fold_shaders.hpp"
#include "rlgl.h"
#include "raymath.h"
#include <stdexcept>

namespace hyperion::flight {
namespace {
Vector3 vector(V3 p){return {float(p.x),float(p.y),float(p.z)};}
void scalar(Shader shader,const char* name,float value){int loc=GetShaderLocation(shader,name);if(loc>=0)SetShaderValue(shader,loc,&value,SHADER_UNIFORM_FLOAT);}
void value(Shader shader,const char* name,V3 v){auto p=vector(v);int loc=GetShaderLocation(shader,name);if(loc>=0)SetShaderValue(shader,loc,&p,SHADER_UNIFORM_VEC3);}
void pair(Shader shader,const char* name,Vector2 v){int loc=GetShaderLocation(shader,name);if(loc>=0)SetShaderValue(shader,loc,&v,SHADER_UNIFORM_VEC2);}
RenderTexture2D target(int w,int h){
 RenderTexture2D t{};t.id=rlLoadFramebuffer();if(!t.id)return t;
 rlEnableFramebuffer(t.id);t.texture={rlLoadTexture(nullptr,w,h,PIXELFORMAT_UNCOMPRESSED_R16G16B16A16,1),w,h,1,PIXELFORMAT_UNCOMPRESSED_R16G16B16A16};
 t.depth={rlLoadTextureDepth(w,h,true),w,h,1,19};rlFramebufferAttach(t.id,t.texture.id,RL_ATTACHMENT_COLOR_CHANNEL0,RL_ATTACHMENT_TEXTURE2D,0);rlFramebufferAttach(t.id,t.depth.id,RL_ATTACHMENT_DEPTH,RL_ATTACHMENT_RENDERBUFFER,0);
 bool complete=rlFramebufferComplete(t.id);rlDisableFramebuffer();if(!complete){UnloadRenderTexture(t);return {}; }SetTextureFilter(t.texture,TEXTURE_FILTER_BILINEAR);return t;
}
}
void Renderer::clearFolds(){
 for(auto& preview:foldPreviews)if(preview.image.id)UnloadRenderTexture(preview.image);foldPreviews.clear();
 if(foldRenderer){foldRenderer->shutdown();foldRenderer.reset();}
 if(foldWindow.id){UnloadShader(foldWindow);foldWindow={};}if(nilOptics.id){UnloadShader(nilOptics);nilOptics={};}
}
void Renderer::clipFoldView(){
 if(!foldViewPass)return;
 auto forward=unit(V3{camera.target.x-camera.position.x,camera.target.y-camera.position.y,camera.target.z-camera.position.z});
 auto right=unit(cross(forward,V3{camera.up.x,camera.up.y,camera.up.z})),up=unit(cross(right,forward));
 V3 eye{camera.position.x,camera.position.y,camera.position.z};
 Vector4 plane{float(dot(foldClipNormal,right)),float(dot(foldClipNormal,up)),float(-dot(foldClipNormal,forward)),float(dot(foldClipNormal,eye-foldClipPosition))};
 Matrix projection=rlGetMatrixProjection();auto sign=[](float v){return v>=0?1.f:-1.f;};
 Vector4 q{sign(plane.x)/projection.m0,sign(plane.y)/projection.m5,-1,(1+projection.m10)/projection.m14};
 float denominator=plane.x*q.x+plane.y*q.y+plane.z*q.z+plane.w*q.w;if(std::abs(denominator)<1e-5)return;
 float scale=2/denominator;projection.m2=plane.x*scale;projection.m6=plane.y*scale;projection.m10=plane.z*scale+1;projection.m14=plane.w*scale;
 rlSetMatrixProjection(projection);
}
void Renderer::prepareFoldViews(const Game& g,const Scene& sceneView,bool reducedEffects,bool cockpitVisible,bool psychedelicLook,bool wireframeLook,bool egaPalette){
 foldDrawn=0;if(foldViewPass)return;
 std::vector<std::pair<double,int>> visible;auto eye=g.ship.position+(cockpitVisible?g.ship.up*3:V3{});
 for(int i=0;i<int(g.folds.size());i++){const auto& gate=g.folds[i];auto offset=gate.position-eye;double d=length(offset);
  if(gate.peer<0||gate.peer>=int(g.folds.size())||d>3800||dot(eye-gate.position,gate.normal)<.03||dot(offset,g.ship.forward)<-gate.radius)continue;
  visible.push_back({gate.radius/std::max(1.,d),i});
 }
 std::sort(visible.rbegin(),visible.rend());if(visible.size()>2)visible.resize(2);
 if(visible.empty()){for(auto& p:foldPreviews)p.gate=-1;return;}
 const auto previewQuality=quality==GraphicsQuality::Low?GraphicsQuality::Low:GraphicsQuality::High;
 if(!foldRenderer){foldRenderer=std::make_unique<Renderer>();foldRenderer->foldViewPass=true;if(!foldRenderer->init(previewQuality))throw std::runtime_error("Fold destination renderer initialization failed");}
 while(foldPreviews.size()<visible.size())foldPreviews.push_back({});
 for(size_t slot=0;slot<visible.size();slot++){
  auto& preview=foldPreviews[slot];int index=visible[slot].second;const auto& entry=g.folds[index];const auto& exit=g.folds[entry.peer];
  Game remote=g;remote.folds.clear();remote.ship.position=foldPoint(entry,exit,eye);remote.ship.forward=unit(foldVector(entry,exit,g.ship.forward));remote.ship.up=unit(foldVector(entry,exit,g.ship.up));remote.ship.velocity=foldVector(entry,exit,g.ship.velocity);remote.touchdownAge=10;
  foldRenderer->foldClipPosition=exit.position;foldRenderer->foldClipNormal=exit.normal;foldRenderer->freezeVisualTime(fixedVisualTime);
  foldRenderer->prepare(remote,sceneView,reducedEffects,false,previewQuality,psychedelicLook,wireframeLook,egaPalette);
  auto image=foldRenderer->scene.texture;
  if(preview.image.texture.width!=image.width||preview.image.texture.height!=image.height){if(preview.image.id)UnloadRenderTexture(preview.image);preview.image=target(image.width,image.height);if(!preview.image.id)throw std::runtime_error("Fold window allocation failed");}
  BeginTextureMode(preview.image);ClearBackground(BLACK);DrawTexturePro(image,{0,0,float(image.width),-float(image.height)},{0,0,float(image.width),float(image.height)},{0,0},0,WHITE);EndTextureMode();preview.gate=index;
 }
 for(size_t slot=visible.size();slot<foldPreviews.size();slot++)foldPreviews[slot].gate=-1;
}
void Renderer::drawFolds(const Game& g){
 if(foldViewPass)return;
 if(!foldWindow.id){foldWindow=LoadShaderFromMemory(nullptr,foldWindowFragment);if(!IsShaderValid(foldWindow)||foldWindow.id==rlGetShaderIdDefault())throw std::runtime_error("Fold aperture shader failed");}
 for(int index=0;index<int(g.folds.size());index++){
  const auto& gate=g.folds[index];if(length(gate.position-g.ship.position)>9000)continue;
  ++foldDrawn;
  auto right=foldRight(gate);const FoldPreview* preview=nullptr;for(const auto& p:foldPreviews)if(p.gate==index)preview=&p;
  if(preview){
   BeginShaderMode(foldWindow);pair(foldWindow,"resolution",{float(scene.texture.width),float(scene.texture.height)});
   rlDrawRenderBatchActive();rlDisableBackfaceCulling();rlBegin(RL_TRIANGLES);rlSetTexture(preview->image.texture.id);rlColor4ub(255,255,255,255);
   for(int k=0;k<96;k++)for(auto p:{gate.position,gate.position+(right*std::cos(k*2*pi/96)+gate.up*std::sin(k*2*pi/96))*gate.radius,gate.position+(right*std::cos((k+1)*2*pi/96)+gate.up*std::sin((k+1)*2*pi/96))*gate.radius})rlVertex3f(float(p.x),float(p.y),float(p.z));
   rlEnd();rlDrawRenderBatchActive();rlSetTexture(0);rlEnableBackfaceCulling();EndShaderMode();
  }
  Color tint=wireframe?(gate.nil?Color{255,85,255,255}:Color{85,255,255,255}):gate.nil?Color{232,96,255,255}:Color{102,246,218,255};
  for(int layer=0;layer<3;layer++)for(int k=0;k<128;k++){
   auto at=[&](int n){double a=n*2*pi/128,r=gate.radius*(1+layer*.025);return gate.position+(right*std::cos(a)+gate.up*std::sin(a))*r;};
   DrawLine3D(vector(at(k)),vector(at(k+1)),layer==1?WHITE:tint);
  }
  for(int k=0;k<12;k++){double a=k*2*pi/12;auto n=right*std::cos(a)+gate.up*std::sin(a);DrawLine3D(vector(gate.position+n*(gate.radius*1.04)),vector(gate.position+n*(gate.radius*1.12)),tint);}
 }
}
int Renderer::nilBody(const Game& g)const{
 for(const auto& region:g.nilRegions)if(region.body>=0&&region.body<int(g.planets.size())&&nilWeight(region,g.ship.position)>.995)return region.body;
 return -1;
}
void Renderer::drawNil(const Game& g,V3 eye){
 nilDrawn=-1;
 const NilRegion* active=nullptr;double weight=0;for(const auto& region:g.nilRegions){double w=nilWeight(region,g.ship.position);if(w>weight){weight=w;active=&region;}}
 if(!active||weight<.001||active->body<0||active->body>=int(g.planets.size()))return;
 if(!nilOptics.id){nilOptics=LoadShaderFromMemory(nullptr,nilFlightFragment);if(!IsShaderValid(nilOptics)||nilOptics.id==rlGetShaderIdDefault())throw std::runtime_error("Nil geodesic shader failed");}nilDrawn=active->body;
 const auto& body=g.planets[active->body];auto axis=unit(active->axis),up=unit(active->up),right=unit(cross(up,axis));
 auto local=[&](V3 p){p=(p-active->position)/active->scale;return V3{dot(p,right),dot(p,up),dot(p,axis)};};
 auto direction=[&](V3 p){return V3{dot(p,right),dot(p,up),dot(p,axis)};};
 value(nilOptics,"observer",local(eye));value(nilOptics,"centre",local(body.centre));value(nilOptics,"forward",direction(g.ship.forward));value(nilOptics,"up",direction(g.ship.up));value(nilOptics,"right",direction(unit(cross(g.ship.forward,g.ship.up))));
 value(nilOptics,"worldRight",right);value(nilOptics,"worldUp",up);value(nilOptics,"worldAxis",axis);value(nilOptics,"sun",sunDirection);
 pair(nilOptics,"resolution",{float(scene.texture.width),float(scene.texture.height)});scalar(nilOptics,"radius",float(body.radius/active->scale));scalar(nilOptics,"weight",float(weight));scalar(nilOptics,"wireframe",wireframe?1.f:0.f);scalar(nilOptics,"ega",egaGraphics?1.f:0.f);scalar(nilOptics,"water",float(body.water));scalar(nilOptics,"seed",float(body.seed));scalar(nilOptics,"psychedelic",psychedelic?1.f:0.f);
 BeginShaderMode(nilOptics);SetShaderValueTexture(nilOptics,GetShaderLocation(nilOptics,"surfaceMap"),surfaceMaps[active->body].texture);SetShaderValueTexture(nilOptics,GetShaderLocation(nilOptics,"geologyMap"),geologyMaps[active->body].texture);DrawRectangle(0,0,scene.texture.width,scene.texture.height,WHITE);EndShaderMode();
}
}
