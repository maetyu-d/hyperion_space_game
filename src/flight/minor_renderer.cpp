#include "renderer.hpp"
#include "rlgl.h"
#include "minor_material_layout.hpp"
#include <filesystem>
#include <stdexcept>
namespace hyperion::flight {
namespace {
Vector3 vec(V3 v){return{float(v.x),float(v.y),float(v.z)};}
void uniform(Shader shader,const char* name,float x){int l=GetShaderLocation(shader,name);if(l>=0)SetShaderValue(shader,l,&x,SHADER_UNIFORM_FLOAT);}
void uniform(Shader shader,const char* name,V3 x){auto v=vec(x);int l=GetShaderLocation(shader,name);if(l>=0)SetShaderValue(shader,l,&v,SHADER_UNIFORM_VEC3);}
Mesh bodyMesh(const MinorBody& b,bool extreme=false){
 auto mesh=GenMeshSphere(1,extreme?(b.kind==MinorKind::Moon?128:80):(b.kind==MinorKind::Moon?48:28),extreme?(b.kind==MinorKind::Moon?192:120):(b.kind==MinorKind::Moon?72:40));
 auto point=[&](V3 n){n=unit(n);return n*minorRadius(b,{n.x,n.y,n.z});};
 for(int i=0;i<mesh.vertexCount;i++){
  auto n=unit(V3{mesh.vertices[i*3],mesh.vertices[i*3+1],mesh.vertices[i*3+2]});auto east=unit(cross({0,1,0},n),{1,0,0}),north=unit(cross(n,east));
  auto p=point(n),normal=unit(cross(point(n+east*.002)-point(n-east*.002),point(n+north*.002)-point(n-north*.002)),n);if(dot(normal,n)<0)normal=-normal;
  mesh.vertices[i*3]=float(p.x);mesh.vertices[i*3+1]=float(p.y);mesh.vertices[i*3+2]=float(p.z);
  mesh.normals[i*3]=float(normal.x);mesh.normals[i*3+1]=float(normal.y);mesh.normals[i*3+2]=float(normal.z);
 }
 UpdateMeshBuffer(mesh,0,mesh.vertices,mesh.vertexCount*3*int(sizeof(float)),0);UpdateMeshBuffer(mesh,2,mesh.normals,mesh.vertexCount*3*int(sizeof(float)),0);return mesh;
}
}
RenderTexture2D Renderer::bakeMinorField(const MinorBody& body,int width){
 int height=width/2;RenderTexture2D target{};target.id=rlLoadFramebuffer();
 if(target.id){rlEnableFramebuffer(target.id);target.texture={rlLoadTexture(nullptr,width,height,PIXELFORMAT_UNCOMPRESSED_R16G16B16A16,1),width,height,1,PIXELFORMAT_UNCOMPRESSED_R16G16B16A16};rlFramebufferAttach(target.id,target.texture.id,RL_ATTACHMENT_COLOR_CHANNEL0,RL_ATTACHMENT_TEXTURE2D,0);bool ready=target.texture.id&&rlFramebufferComplete(target.id);rlDisableFramebuffer();if(!ready){UnloadRenderTexture(target);target={};}}
 if(!target.id)target=LoadRenderTexture(width,height);
 if(!target.id)throw std::runtime_error("Minor-body material allocation failed");
 Vector2 resolution{float(width),float(height)};SetShaderValue(minorBake,GetShaderLocation(minorBake,"resolution"),&resolution,SHADER_UNIFORM_VEC2);
 uniform(minorBake,"seed",float(body.seed%65536)/131.);uniform(minorBake,"hue",float(body.hue));uniform(minorBake,"ice",float(body.ice));uniform(minorBake,"metal",float(body.metal));uniform(minorBake,"kind",float(body.kind));
 std::array<Vector4,12> basins;std::array<float,12> depths;
 for(int i=0;i<12;i++){const auto& c=body.basins[i];basins[i]={float(c.axis.x),float(c.axis.y),float(c.axis.z),float(c.width)};depths[i]=float(c.depth);}
 SetShaderValueV(minorBake,GetShaderLocation(minorBake,"basins[0]"),basins.data(),SHADER_UNIFORM_VEC4,12);SetShaderValueV(minorBake,GetShaderLocation(minorBake,"basinDepth[0]"),depths.data(),SHADER_UNIFORM_FLOAT,12);
 BeginTextureMode(target);ClearBackground(BLANK);rlDrawRenderBatchActive();rlDisableColorBlend();BeginShaderMode(minorBake);DrawRectangle(0,0,width,height,WHITE);EndShaderMode();rlDrawRenderBatchActive();rlEnableColorBlend();EndTextureMode();
 GenTextureMipmaps(&target.texture);SetTextureWrap(target.texture,TEXTURE_WRAP_REPEAT);SetTextureFilter(target.texture,TEXTURE_FILTER_TRILINEAR);if(extremeGraphics)SetTextureFilter(target.texture,TEXTURE_FILTER_ANISOTROPIC_16X);return target;
}
void Renderer::prepareMinorBodies(const Game& g){
 V3 eye{camera.position.x,camera.position.y,camera.position.z};std::vector<MinorDetailCandidate> candidates;
 for(int i=0;i<int(g.minorBodies.size());i++){const auto& b=g.minorBodies[i];if(b.destroyed)continue;auto offset=b.centre-eye;double distance=length(offset),pixels=b.source.radius/std::max(1.,distance)*scene.texture.height/.7002;
  if(!inFront(b.centre,b.source.radius*1.3)||distance>58000||pixels<.65)continue;
  if(!minorModels.count(b.source.id)){auto model=LoadModelFromMesh(bodyMesh(b.source,extremeGraphics));model.materials[0].shader=minorShader;minorModels.emplace(b.source.id,model);minorFields[b.source.id].base=bakeMinorField(b.source,256);}
  candidates.push_back({i,pixels,minorFields.at(b.source.id).detail.id!=0});
 }
 auto selected=minorDetailSelection(std::move(candidates));auto wanted=[&](const std::string& id){for(int index:selected)if(index>=0&&g.minorBodies[index].source.id==id)return true;return false;};
 float step=std::clamp(GetFrameTime(),0.f,.05f)*3.5f;int resident=0;
 for(auto& entry:minorFields)if(entry.second.detail.id){auto& field=entry.second;if(!wanted(entry.first))field.blend=std::max(0.f,field.blend-step);if(!wanted(entry.first)&&field.blend<=0){UnloadRenderTexture(field.detail);field.detail={};}else ++resident;}
 for(int index:selected)if(index>=0){const auto& b=g.minorBodies[index];auto& field=minorFields.at(b.source.id);if(!field.detail.id&&resident<2){field.detail=bakeMinorField(b.source,flightQualityProfile(quality).minorWidth);++resident;}if(field.detail.id)field.blend=std::min(1.f,field.blend+step);}
}
int Renderer::minorDetailFields()const{int count=0;for(const auto& field:minorFields)count+=field.second.detail.id!=0;return count;}
int Renderer::minorMaterialWidth(const Game& g,int index)const{if(index<0||index>=int(g.minorBodies.size()))return 0;auto it=minorFields.find(g.minorBodies[index].source.id);return it==minorFields.end()?0:it->second.detail.id?it->second.detail.texture.width:it->second.base.texture.width;}
bool Renderer::exportMinorMaterial(const Game& g,int index,const std::string& path)const{
 if(index<0||index>=int(g.minorBodies.size()))return false;auto it=minorFields.find(g.minorBodies[index].source.id);if(it==minorFields.end())return false;
 auto image=LoadImageFromTexture(it->second.detail.id?it->second.detail.texture:it->second.base.texture);if(!image.data)return false;ImageFlipVertical(&image);ImageFormat(&image,PIXELFORMAT_UNCOMPRESSED_R8G8B8);auto folder=std::filesystem::path(path).parent_path();if(!folder.empty())std::filesystem::create_directories(folder);bool ok=ExportImage(image,path.c_str());UnloadImage(image);return ok;
}
void Renderer::clearMinorBodies(){for(auto& entry:minorModels)UnloadModel(entry.second);minorModels.clear();for(auto& field:minorFields){if(field.second.base.id)UnloadRenderTexture(field.second.base);if(field.second.detail.id)UnloadRenderTexture(field.second.detail);}minorFields.clear();if(cometGlow.id){UnloadTexture(cometGlow);cometGlow={};}}
void Renderer::drawMinorBodies(const Game& g,bool depth){
 V3 eye{camera.position.x,camera.position.y,camera.position.z};
 if(depth){uniform(planetDepth,"patchCos",0.f);uniform(planetDepth,"groundPass",0.f);}
 else{minorDrawn=0;uniform(minorShader,"wireframe",wireframe?1.f:0.f);uniform(minorShader,"wireEGA",egaGraphics?1.f:0.f);uniform(minorShader,"wirePixelScale",std::max(1.f,float(scene.texture.width)/std::max(1,GetRenderWidth())));uniform(minorShader,"lightDirection",sunDirection);uniform(minorShader,"eye",eye);}
 for(const auto& b:g.minorBodies){if(b.destroyed)continue;auto offset=b.centre-eye;double distance=length(offset),pixels=b.source.radius/std::max(1.,distance)*scene.texture.height/.7002;
  if(!inFront(b.centre,b.source.radius*1.3)||distance>58000||pixels<.65)continue;
  auto it=minorModels.find(b.source.id);if(it==minorModels.end())continue;
  auto& model=it->second;model.materials[0].shader=depth?planetDepth:minorShader;
  if(!depth){++minorDrawn;auto& field=minorFields.at(b.source.id);model.materials[0].maps[MATERIAL_MAP_NORMAL].texture=detailMap.texture;model.materials[0].maps[MATERIAL_MAP_ALBEDO].texture=field.base.texture;model.materials[0].maps[MATERIAL_MAP_ROUGHNESS].texture=field.detail.id?field.detail.texture:field.base.texture;uniform(minorShader,"detailBlend",field.blend);uniform(minorShader,"radius",float(b.source.radius));uniform(minorShader,"hue",float(b.source.hue));uniform(minorShader,"ice",float(b.source.ice));uniform(minorShader,"metal",float(b.source.metal));uniform(minorShader,"visibility",float(std::clamp((pixels-.65)/1.5,0.,1.)));}
  DrawModel(model,vec(b.centre),1,WHITE);model.materials[0].shader=minorShader;
 }
}
void Renderer::drawCometTails(const Game& g){
 V3 eye{camera.position.x,camera.position.y,camera.position.z};
 if(!cometGlow.id){auto image=GenImageColor(128,128,BLANK);auto* pixels=static_cast<Color*>(image.data);for(int y=0;y<128;y++)for(int x=0;x<128;x++){double r=std::hypot((x-63.5)/63.5,(y-63.5)/63.5);pixels[y*128+x]={255,255,255,static_cast<unsigned char>(255*std::exp(-r*r*7)*std::clamp((1-r)*8,0.,1.))};}cometGlow=LoadTextureFromImage(image);UnloadImage(image);GenTextureMipmaps(&cometGlow);SetTextureFilter(cometGlow,TEXTURE_FILTER_TRILINEAR);}
 BeginBlendMode(BLEND_ADDITIVE);rlDrawRenderBatchActive();rlDisableDepthMask();rlDisableBackfaceCulling();
 for(const auto& b:g.minorBodies)if(!b.destroyed&&b.source.kind==MinorKind::Comet&&length(b.centre-eye)<50000){
  double activity=std::clamp(12000./std::max(4000.,length(b.centre)),.3,1.4);auto tail=-sunDirection;auto side=unit(cross(tail,eye-b.centre),{1,0,0});
  auto orbit=minorOrbit(b.source,(presentation?presentation->hours:g.time/55)+1);auto tangent=unit(V3{orbit.x,orbit.y,orbit.z}-b.centre,{0,0,1});
  if(wireframe){for(int strand=-1;strand<=1;strand++)for(int i=0;i<40;i++){
   auto point=[&](int k){double t=k/40.,size=b.source.radius*115*activity;return b.centre+tail*(size*t)+tangent*(size*t*t*.08)+side*(strand*b.source.radius*(.25+t*2.4));};
   DrawLine3D(vec(point(i)),vec(point(i+1)),Color{static_cast<unsigned char>(strand?155:105),static_cast<unsigned char>(strand?139:225),255,static_cast<unsigned char>(200*(1.-i/40.))});
  }continue;}
  for(int plume=0;plume<2;plume++){
   double size=b.source.radius*(plume?80:115)*activity;auto point=[&](double t){return b.centre+tail*(size*t)+tangent*(plume?size*t*t*.18:0);};
   auto width=[&](double t){return b.source.radius*(.7+std::pow(t,.65)*(plume?8:2.8))*activity;};
   auto vertex=[&](double t,double across){double a=(1-t)*(1-t)*(plume?.07:.15)*std::exp(-across*across*5)*(1-across*across);rlColor4ub(plume?214:113,plume?139:232,255,static_cast<unsigned char>(a*255));auto p=point(t)+side*(width(t)*across);rlVertex3f(float(p.x),float(p.y),float(p.z));};
   rlBegin(RL_TRIANGLES);for(int i=0;i<40;i++)for(int j=0;j<8;j++){double a=i/40.,c=(i+1)/40.,left=-1+j*.25,right=left+.25;vertex(a,left);vertex(c,left);vertex(a,right);vertex(a,right);vertex(c,left);vertex(c,right);}rlEnd();
  }
  DrawBillboard(camera,cometGlow,vec(b.centre),float(b.source.radius*6),Color{118,230,235,105});
 }
 rlDrawRenderBatchActive();rlEnableDepthMask();rlEnableBackfaceCulling();EndBlendMode();
}
}
