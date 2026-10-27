#include "renderer.hpp"
#include "wire_shaders.hpp"
#include "world_objects.hpp"
#include "../screen_layout.hpp"
#include "shaders.hpp"
#include "minor_shaders.hpp"
#include "geology.hpp"
#include "building_material.hpp"
#include "rlgl.h"
#include "raymath.h"
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <unordered_map>
#if defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#endif

namespace hyperion::flight {
namespace {
constexpr Color ink{7,12,22,255},mint{164,255,206,255},cyan{91,221,255,255};
void texture(Shader s,const char* name,Texture2D value){int l=GetShaderLocation(s,name);if(l>=0)SetShaderValueTexture(s,l,value);}
Color alpha(Color c,float a){c.a=static_cast<unsigned char>(std::clamp(a,0.f,1.f)*255);return c;}
Vector3 vec(V3 v){return{float(v.x),float(v.y),float(v.z)};}
std::unordered_map<unsigned int,std::unordered_map<std::string,int>> uniforms;
int loc(Shader s,const char*n){auto&u=uniforms[s.id];auto it=u.find(n);return it==u.end()?u.emplace(n,GetShaderLocation(s,n)).first->second:it->second;}
void set(Shader s,const char*n,float x){int l=loc(s,n);if(l>=0)SetShaderValue(s,l,&x,SHADER_UNIFORM_FLOAT);}
void set(Shader s,const char*n,V3 v){Vector3 x=vec(v);int l=loc(s,n);if(l>=0)SetShaderValue(s,l,&x,SHADER_UNIFORM_VEC3);}
void set(Shader s,const char*n,Vector4 x){int l=loc(s,n);if(l>=0)SetShaderValue(s,l,&x,SHADER_UNIFORM_VEC4);}
void set(Shader s,const char*n,Vector2 x){int l=loc(s,n);if(l>=0)SetShaderValue(s,l,&x,SHADER_UNIFORM_VEC2);}
void set(Shader s,const char*n,Matrix x){int l=loc(s,n);if(l>=0)SetShaderValueMatrix(s,l,x);}
void line(V3 a,V3 b,Color c){DrawLine3D(vec(a),vec(b),c);}
void tube(V3 a,V3 b,float radius,Color c){if(length(b-a)>.001)DrawCylinderEx(vec(a),vec(b),radius,radius,6,c);}
void ring(V3 centre,V3 a,V3 b,double radius,Color c,int segments=160,float thick=0){
  if(thick>0){
    rlBegin(RL_TRIANGLES);rlColor4ub(c.r,c.g,c.b,c.a);
    for(int i=0;i<segments;i++){
      double t=i*2*pi/segments,s=(i+1)*2*pi/segments;V3 u=a*std::cos(t)+b*std::sin(t),v=a*std::cos(s)+b*std::sin(s);
      V3 A=centre+u*(radius-thick),B=centre+v*(radius-thick),C=centre+u*(radius+thick),D=centre+v*(radius+thick);
      // Explicit back faces keep the ring visible from either side without
      // depending on a culling state change inside raylib's deferred batch.
      for(V3 p:{A,B,C,B,D,C,A,C,B,B,C,D})rlVertex3f(float(p.x),float(p.y),float(p.z));
    }
    rlEnd();return;
  }
  for(int i=0;i<segments;i++){double t=i*2*pi/segments,s=(i+1)*2*pi/segments;
    V3 p=centre+(a*std::cos(t)+b*std::sin(t))*radius,q=centre+(a*std::cos(s)+b*std::sin(s))*radius;
    line(p,q,c);
  }
}
RenderTexture2D linearTarget(int width,int height,int format=PIXELFORMAT_UNCOMPRESSED_R16G16B16A16){
 RenderTexture2D target{};target.id=rlLoadFramebuffer();
 if(target.id){
  rlEnableFramebuffer(target.id);target.texture={rlLoadTexture(nullptr,width,height,format,1),width,height,1,format};
  target.depth={rlLoadTextureDepth(width,height,true),width,height,1,19};
  rlFramebufferAttach(target.id,target.texture.id,RL_ATTACHMENT_COLOR_CHANNEL0,RL_ATTACHMENT_TEXTURE2D,0);
  rlFramebufferAttach(target.id,target.depth.id,RL_ATTACHMENT_DEPTH,RL_ATTACHMENT_RENDERBUFFER,0);
  bool complete=target.texture.id&&rlFramebufferComplete(target.id);rlDisableFramebuffer();
  if(complete)return target;UnloadRenderTexture(target);
 }
 return LoadRenderTexture(width,height);
}
Texture2D lookupNoise(){
 auto value=[](int x,int y){uint32_t h=uint32_t(x&255)*1973u+uint32_t(y&255)*9277u+89173u;h=(h^(h>>16))*2246822519u;h=(h^(h>>13))*3266489917u;return static_cast<unsigned char>((h^(h>>16))>>24);};
 std::vector<Color> pixels(256*256);for(int y=0;y<256;y++)for(int x=0;x<256;x++)pixels[y*256+x]={value(x,y),value(x+37,y+17),value(x+91,y+53),255};
 Image source{pixels.data(),256,256,1,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};Texture2D result=LoadTextureFromImage(source);SetTextureFilter(result,TEXTURE_FILTER_BILINEAR);SetTextureWrap(result,TEXTURE_WRAP_REPEAT);return result;
}
void material(Model& model,Shader shader,Texture2D noise){model.materials[0].shader=shader;model.materials[0].maps[MATERIAL_MAP_NORMAL].texture=noise;}
struct CabinMesh {
 std::vector<float> positions,normals,uv;std::vector<unsigned char> colours;double aspect;
 V3 point(Vector2 p,double depth)const{return{(p.x*2-1)*aspect*.7002075*depth,(1-p.y*2)*.7002075*depth,-depth};}
 void triangle(V3 a,V3 b,V3 c,Color colour){
  auto n=unit(cross(b-a,c-a));if(dot(n,-(a+b+c)/3.)<0){std::swap(b,c);n=-n;}
  int index=0;for(auto p:{a,b,c}){for(double v:{p.x,p.y,p.z})positions.push_back(float(v));for(double v:{n.x,n.y,n.z})normals.push_back(float(v));uv.push_back(index==1?1.f:0.f);uv.push_back(index==2?1.f:0.f);for(auto v:{colour.r,colour.g,colour.b,colour.a})colours.push_back(v);++index;}
 }
 void panel(std::initializer_list<Vector2> source,Color face,float front=.76f,float bevel=.10f){
  std::vector<Vector2> outer(source),inner;Vector2 centre{};for(auto p:outer){centre.x+=p.x;centre.y+=p.y;}centre.x/=outer.size();centre.y/=outer.size();
  for(auto p:outer)inner.push_back({p.x+(centre.x-p.x)*bevel,p.y+(centre.y-p.y)*bevel});
  auto depth=[&](Vector2 p){return front/(1.+(p.y-.74)*.28);};
  for(size_t i=1;i+1<inner.size();i++)triangle(point(inner[0],depth(inner[0])),point(inner[i],depth(inner[i])),point(inner[i+1],depth(inner[i+1])),face);
  Color rim{static_cast<unsigned char>(std::min(255,int(face.r)*3/2+12)),static_cast<unsigned char>(std::min(255,int(face.g)*3/2+12)),static_cast<unsigned char>(std::min(255,int(face.b)*3/2+12)),face.a};
  for(size_t i=0;i<outer.size();i++){size_t j=(i+1)%outer.size();auto a=point(inner[i],depth(inner[i])),b=point(inner[j],depth(inner[j])),c=point(outer[i],depth(outer[i])+.035),d=point(outer[j],depth(outer[j])+.035);triangle(a,c,b,rim);triangle(b,c,d,rim);
   auto backA=point(outer[i],depth(outer[i])+.18),backB=point(outer[j],depth(outer[j])+.18);triangle(c,backA,d,{13,22,30,0});triangle(d,backA,backB,{13,22,30,0});}
 }
 Model model(){
  Mesh mesh{};mesh.vertexCount=int(positions.size()/3);mesh.triangleCount=mesh.vertexCount/3;
  auto floats=[](const std::vector<float>& values){auto* result=static_cast<float*>(MemAlloc(unsigned(values.size()*sizeof(float))));std::copy(values.begin(),values.end(),result);return result;};
  mesh.vertices=floats(positions);mesh.normals=floats(normals);mesh.texcoords=floats(uv);mesh.colors=static_cast<unsigned char*>(MemAlloc(unsigned(colours.size())));std::copy(colours.begin(),colours.end(),mesh.colors);UploadMesh(&mesh,false);return LoadModelFromMesh(mesh);
 }
};
Mesh annulus(){
 constexpr int segments=256;Mesh mesh{};mesh.vertexCount=(segments+1)*2;mesh.triangleCount=segments*2;
 mesh.vertices=static_cast<float*>(MemAlloc(mesh.vertexCount*3*sizeof(float)));mesh.normals=static_cast<float*>(MemAlloc(mesh.vertexCount*3*sizeof(float)));
 mesh.texcoords=static_cast<float*>(MemAlloc(mesh.vertexCount*2*sizeof(float)));mesh.indices=static_cast<unsigned short*>(MemAlloc(mesh.triangleCount*3*sizeof(unsigned short)));
 V3 a{1,0,0},b=unit({0,.32,1}),normal=unit(cross(a,b));
 for(int i=0;i<=segments;i++)for(int edge=0;edge<2;edge++){
  int v=i*2+edge;double angle=i*2*pi/segments;auto p=(a*std::cos(angle)+b*std::sin(angle))*(edge?1.77:1.29);
  mesh.vertices[v*3]=float(p.x);mesh.vertices[v*3+1]=float(p.y);mesh.vertices[v*3+2]=float(p.z);
  mesh.normals[v*3]=float(normal.x);mesh.normals[v*3+1]=float(normal.y);mesh.normals[v*3+2]=float(normal.z);
  mesh.texcoords[v*2]=float(edge);mesh.texcoords[v*2+1]=float(i)/segments;
 }
 for(int i=0;i<segments;i++){int k=i*6;unsigned short a=static_cast<unsigned short>(i*2);for(auto index:{a,static_cast<unsigned short>(a+2),static_cast<unsigned short>(a+1),static_cast<unsigned short>(a+1),static_cast<unsigned short>(a+2),static_cast<unsigned short>(a+3)})mesh.indices[k++]=index;}
 UploadMesh(&mesh,false);return mesh;
}
Mesh globe(const Planet&p,int lon=192,int lat=96){Mesh mesh{};mesh.vertexCount=(lon+1)*(lat+1);mesh.triangleCount=lon*lat*2;
  mesh.vertices=static_cast<float*>(MemAlloc(mesh.vertexCount*3*sizeof(float)));
  mesh.normals=static_cast<float*>(MemAlloc(mesh.vertexCount*3*sizeof(float)));
  mesh.texcoords=static_cast<float*>(MemAlloc(mesh.vertexCount*2*sizeof(float)));
  mesh.indices=static_cast<unsigned short*>(MemAlloc(mesh.triangleCount*3*sizeof(unsigned short)));
  for(int y=0;y<=lat;y++)for(int x=0;x<=lon;x++){
    double theta=pi*y/lat,phi=2*pi*x/lon;V3 n{std::sin(theta)*std::cos(phi),std::cos(theta),std::sin(theta)*std::sin(phi)};
    V3 point=n*(p.radius+terrain(p,n));int i=y*(lon+1)+x;
    V3 east=unit(cross({0,1,0},n),{1,0,0}),north=unit(cross(n,east));double step=.0004;
    V3 nx=unit(n+east*step),ny=unit(n+north*step);
    V3 dx=nx*(p.radius+terrain(p,nx))-point,dy=ny*(p.radius+terrain(p,ny))-point;
    V3 normal=unit(cross(dx,dy),n);if(dot(normal,n)<0)normal=-normal;
    mesh.vertices[i*3]=float(point.x);mesh.vertices[i*3+1]=float(point.y);mesh.vertices[i*3+2]=float(point.z);
    mesh.normals[i*3]=float(normal.x);mesh.normals[i*3+1]=float(normal.y);mesh.normals[i*3+2]=float(normal.z);
    mesh.texcoords[i*2]=float(x)/lon;mesh.texcoords[i*2+1]=float(y)/lat;
  }
  int k=0;for(int y=0;y<lat;y++)for(int x=0;x<lon;x++){
    unsigned short a=static_cast<unsigned short>(y*(lon+1)+x),b=static_cast<unsigned short>(a+lon+1);
    mesh.indices[k++]=a;mesh.indices[k++]=a+1;mesh.indices[k++]=b;
    mesh.indices[k++]=a+1;mesh.indices[k++]=b+1;mesh.indices[k++]=b;
  }
  UploadMesh(&mesh,false);return mesh;
}
}
bool Renderer::init(GraphicsQuality initialQuality){
 wireShader=LoadShaderFromMemory(wireVertex,wireFragment);if(!IsShaderValid(wireShader)||wireShader.id==rlGetShaderIdDefault())return false;
 quality=initialQuality;lowGraphics=quality==GraphicsQuality::Low;extremeGraphics=quality==GraphicsQuality::Extreme;
 uniforms.clear();noiseTexture=lookupNoise();sky=LoadShaderFromMemory(nullptr,skyFragment);world=LoadShaderFromMemory(worldVertex,worldFragment);
 // Distant globe fragments do not carry close material registers or work.
 std::string groundVertex=worldVertex,groundFragment=worldFragment;
 for(auto* source:{&groundVertex,&groundFragment})source->insert(source->find('\n')+1,"#define TERRAIN_DETAIL\n");
 groundShader=LoadShaderFromMemory(groundVertex.c_str(),groundFragment.c_str());planetDepth=LoadShaderFromMemory(groundVertex.c_str(),planetDepthFragment);sunDepth=LoadShaderFromMemory(groundVertex.c_str(),sunDepthFragment);
 std::string volumeVertex=worldVertex;volumeVertex.insert(volumeVertex.find('\n')+1,"#define CLAMP_NEAR\n");
 atmo=LoadShaderFromMemory(volumeVertex.c_str(),atmoFragment);cloud=LoadShaderFromMemory(volumeVertex.c_str(),cloudFragment);ringShader=LoadShaderFromMemory(worldVertex,ringFragment);aurora=LoadShaderFromMemory(volumeVertex.c_str(),auroraFragment);
 cloudResolve=LoadShaderFromMemory(nullptr,cloudResolveFragment);
 blur=LoadShaderFromMemory(nullptr,blurFragment);post=LoadShaderFromMemory(nullptr,postFragment);supersampleResolve=LoadShaderFromMemory(nullptr,supersampleResolveFragment);cockpitShader=LoadShaderFromMemory(worldVertex,cockpitFragment);
 structure=LoadShaderFromMemory(worldVertex,structureFragment);minorShader=LoadShaderFromMemory(worldVertex,minorFragment);minorBake=LoadShaderFromMemory(nullptr,minorBakeFragment);
 minorShader.locs[SHADER_LOC_MAP_NORMAL]=GetShaderLocation(minorShader,"grainTexture");
 minorShader.locs[SHADER_LOC_MAP_ALBEDO]=GetShaderLocation(minorShader,"materialTexture");minorShader.locs[SHADER_LOC_MAP_ROUGHNESS]=GetShaderLocation(minorShader,"detailTexture");
 surfaceBake=LoadShaderFromMemory(nullptr,surfaceBakeFragment);geologyBake=LoadShaderFromMemory(nullptr,geologyBakeFragment);detailBake=LoadShaderFromMemory(nullptr,detailBakeFragment);
 dust=LoadShaderFromMemory(nullptr,dustFragment);
 for(auto shader:{world,groundShader,cloud,cockpitShader})shader.locs[SHADER_LOC_MAP_NORMAL]=GetShaderLocation(shader,"noiseTexture");
 groundShader.locs[SHADER_LOC_MAP_METALNESS]=GetShaderLocation(groundShader,"detailTexture");
 for(auto shader:{cloud,atmo,aurora})shader.locs[SHADER_LOC_MAP_METALNESS]=GetShaderLocation(shader,"opaqueDepth");
 structure.locs[SHADER_LOC_MAP_NORMAL]=GetShaderLocation(structure,"buildingTexture");
 structure.locs[SHADER_LOC_MAP_METALNESS]=GetShaderLocation(structure,"buildingDetailTexture");
 structure.locs[SHADER_LOC_MAP_OCCLUSION]=GetShaderLocation(structure,"sunShadowTexture");
 groundShader.locs[SHADER_LOC_MAP_OCCLUSION]=GetShaderLocation(groundShader,"sunShadowTexture");
 for(auto shader:{world,groundShader,cloud})shader.locs[SHADER_LOC_MAP_ALBEDO]=GetShaderLocation(shader,"surfaceTexture");
 for(auto shader:{world,groundShader})shader.locs[SHADER_LOC_MAP_ROUGHNESS]=GetShaderLocation(shader,"geologyTexture");
 ringModel=LoadModelFromMesh(annulus());ringModel.materials[0].shader=ringShader;
 for(auto shader:{sky,world,groundShader,atmo,cloud,cloudResolve,ringShader,aurora,blur,post,supersampleResolve,cockpitShader,structure,surfaceBake,geologyBake,detailBake,planetDepth,sunDepth,dust,minorShader,minorBake})if(shader.id==0||shader.id==rlGetShaderIdDefault())return false;
 // Set wrapping before filtering: raylib resets anisotropy on parameter
 // changes. Trilinear min/mag comes first, then 16x anisotropy last.
 // Bake/upload one field at a time so the higher resolution only needs one
 // temporary CPU image. Both fields keep a complete, repeating mip chain.
 const auto profile=flightQualityProfile(quality);const int buildingSize=profile.materialWidth;
 auto loadBuildingField=[&](const std::vector<unsigned char>& pixels){
  Image image{const_cast<unsigned char*>(pixels.data()),buildingSize,buildingSize,1,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
  auto result=LoadTextureFromImage(image);
  if(result.id){GenTextureMipmaps(&result);SetTextureWrap(result,TEXTURE_WRAP_REPEAT);SetTextureFilter(result,TEXTURE_FILTER_TRILINEAR);if(extremeGraphics)SetTextureFilter(result,TEXTURE_FILTER_ANISOTROPIC_16X);}
  return result;
 };
 buildingTexture=loadBuildingField(architecture::bakeBuildingMaterial(buildingSize));if(!buildingTexture.id)return false;
 std::printf("Building material storage: RGBA8 / %d / %d mip levels\n",buildingTexture.width,buildingTexture.mipmaps);
 std::array<float,4> detailMean{};
 {
  auto pixels=architecture::bakeBuildingDetail(buildingSize);detailMean=architecture::bakedMaterialMean(pixels);
  buildingDetailTexture=loadBuildingField(pixels);
 }
 if(!buildingDetailTexture.id)return false;
 std::printf("Building detail storage: RGBA8 / %d / %d mip levels\n",buildingDetailTexture.width,buildingDetailTexture.mipmaps);
 int detailMeanLoc=GetShaderLocation(structure,"buildingDetailMean");if(detailMeanLoc<0)return false;
 SetShaderValue(structure,detailMeanLoc,detailMean.data(),SHADER_UNIFORM_VEC4);
 std::printf("Building detail mean: %.5f / %.5f / %.5f / %.5f\n",detailMean[0],detailMean[1],detailMean[2],detailMean[3]);
 // These channels are material data, including alpha, so bake without blending.
 int detailSize=profile.detailWidth;detailMap=LoadRenderTexture(detailSize,detailSize);if(!detailMap.id)return false;
 BeginTextureMode(detailMap);ClearBackground(BLANK);set(detailBake,"resolution",Vector2{float(detailMap.texture.width),float(detailMap.texture.height)});rlDrawRenderBatchActive();rlDisableColorBlend();
 BeginShaderMode(detailBake);texture(detailBake,"noiseTexture",noiseTexture);DrawRectangle(0,0,detailMap.texture.width,detailMap.texture.height,WHITE);EndShaderMode();rlDrawRenderBatchActive();rlEnableColorBlend();EndTextureMode();
 GenTextureMipmaps(&detailMap.texture);SetTextureWrap(detailMap.texture,TEXTURE_WRAP_REPEAT);SetTextureFilter(detailMap.texture,TEXTURE_FILTER_TRILINEAR);if(extremeGraphics)SetTextureFilter(detailMap.texture,TEXTURE_FILTER_ANISOTROPIC_16X);
 int shadowSize=profile.shadowWidth;sunScene=linearTarget(shadowSize,shadowSize,PIXELFORMAT_UNCOMPRESSED_R32);
 SetTextureFilter(sunScene.texture,TEXTURE_FILTER_POINT);SetTextureWrap(sunScene.texture,TEXTURE_WRAP_CLAMP);
 std::printf("Sun shadow storage: %s / %d\n",sunScene.texture.format==PIXELFORMAT_UNCOMPRESSED_R32?"R32F":"disabled fallback",shadowSize);
 return noiseTexture.id!=0;
}
void Renderer::bakePlanetFields(const Game& g,int body,bool close){
 const auto& p=g.planets[body];const auto profile=flightQualityProfile(quality);
 for(auto* maps:{&surfaceMaps,&geologyMaps}){auto& previous=(*maps)[body];if(previous.id){UnloadRenderTexture(previous);previous={};}}
  int mapWidth=close?profile.nearSurfaceWidth:p.giant?profile.giantSurfaceWidth:profile.surfaceWidth,mapHeight=mapWidth/2;auto data=linearTarget(mapWidth,mapHeight);if(!data.id)throw std::runtime_error("Planet field allocation failed");BeginTextureMode(data);ClearBackground(BLANK);set(surfaceBake,"resolution",Vector2{float(mapWidth),float(mapHeight)});set(surfaceBake,"seed",float(p.seed));set(surfaceBake,"kind",float(p.kind));set(surfaceBake,"water",float(p.water));set(surfaceBake,"generation",float(p.generation));set(surfaceBake,"worldParams",Vector4{float(p.worldSeed&65535),float(p.worldSeed>>16),float(p.landform),float(p.tectonic)});
  rlDrawRenderBatchActive();rlDisableColorBlend();BeginShaderMode(surfaceBake);texture(surfaceBake,"noiseTexture",noiseTexture);DrawRectangle(0,0,mapWidth,mapHeight,WHITE);EndShaderMode();rlDrawRenderBatchActive();rlEnableColorBlend();EndTextureMode();
  GenTextureMipmaps(&data.texture);SetTextureWrap(data.texture,TEXTURE_WRAP_REPEAT);SetTextureFilter(data.texture,TEXTURE_FILTER_TRILINEAR);if(extremeGraphics)SetTextureFilter(data.texture,TEXTURE_FILTER_ANISOTROPIC_16X);surfaceMaps[body]=data;
  // The extra material field is small and mipmapped. Geological loops run
  // once here rather than in every flight fragment, including on distant worlds.
  int geologyWidth=close?profile.nearGeologyWidth:p.giant?profile.giantGeologyWidth:profile.geologyWidth,geologyHeight=geologyWidth/2;auto geology=linearTarget(geologyWidth,geologyHeight);
  if(geology.id){
   BeginTextureMode(geology);ClearBackground(BLANK);set(geologyBake,"resolution",Vector2{float(geologyWidth),float(geologyHeight)});
   set(geologyBake,"seed",float(p.seed));set(geologyBake,"kind",float(p.kind));set(geologyBake,"water",float(p.water));set(geologyBake,"life",float(p.life));set(geologyBake,"atmosphere",float(p.atmosphere));set(geologyBake,"temperature",float(p.temperature));
   rlDrawRenderBatchActive();rlDisableColorBlend();BeginShaderMode(geologyBake);texture(geologyBake,"noiseTexture",noiseTexture);DrawRectangle(0,0,geologyWidth,geologyHeight,WHITE);EndShaderMode();rlDrawRenderBatchActive();rlEnableColorBlend();EndTextureMode();
   GenTextureMipmaps(&geology.texture);SetTextureWrap(geology.texture,TEXTURE_WRAP_REPEAT);SetTextureFilter(geology.texture,TEXTURE_FILTER_TRILINEAR);if(extremeGraphics)SetTextureFilter(geology.texture,TEXTURE_FILTER_ANISOTROPIC_16X);
  }
  geologyMaps[body]=geology;
 if(body<int(planets.size()))for(auto* models:{&planets,&distantPlanets}){auto& material=(*models)[body].materials[0];material.maps[MATERIAL_MAP_ALBEDO].texture=data.texture;material.maps[MATERIAL_MAP_ROUGHNESS].texture=geology.texture;}
 if(body<int(clouds.size()))clouds[body].materials[0].maps[MATERIAL_MAP_ALBEDO].texture=data.texture;
 if(groundBody==body)groundBody=-1;
}
void Renderer::preparePlanetDetail(const Game& g){
 if(!extremeGraphics||surfaceMaps.empty())return;const int next=g.nearestPlanet();if(next==extremeDetailBody)return;
 if(extremeDetailBody>=0)bakePlanetFields(g,extremeDetailBody,false);
 bakePlanetFields(g,next,true);extremeDetailBody=next;
}
bool Renderer::materialFilteringValid()const{
#if defined(__APPLE__)
 GLint previous=0;glGetIntegerv(GL_TEXTURE_BINDING_2D,&previous);bool valid=true;
 auto inspect=[&](Texture2D t){
  if(!t.id){valid=false;return;}glBindTexture(GL_TEXTURE_2D,t.id);GLint minFilter=0,magFilter=0;GLfloat anisotropy=1;
  glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,&minFilter);glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,&magFilter);
  if(extremeGraphics)glGetTexParameterfv(GL_TEXTURE_2D,0x84FE /* GL_TEXTURE_MAX_ANISOTROPY_EXT */,&anisotropy);
  bool filtered=minFilter==GL_LINEAR_MIPMAP_LINEAR&&magFilter==GL_LINEAR&&(!extremeGraphics||anisotropy>=15.9f);
  if(!filtered)std::printf("Material filter audit: texture %u / %dx%d / min %d / mag %d / anisotropy %.1f\n",t.id,t.width,t.height,minFilter,magFilter,anisotropy);
  valid=valid&&filtered;
 };
 inspect(buildingTexture);inspect(buildingDetailTexture);inspect(detailMap.texture);for(const auto& map:surfaceMaps)inspect(map.texture);for(const auto& map:geologyMaps)inspect(map.texture);
 glBindTexture(GL_TEXTURE_2D,GLuint(previous));return valid;
#else
 return true;
#endif
}
void Renderer::rebuild(const Game& g){
 clearPortAssets();clearMinorBodies();
 for(auto* model:{&groundModel,&rockModel})if(model->meshCount){UnloadModel(*model);*model={};}groundBody=-1;
 for(auto* list:{&planets,&distantPlanets,&atmospheres,&clouds}){for(auto& model:*list)if(model.meshCount)UnloadModel(model);list->clear();}
 for(auto* maps:{&surfaceMaps,&geologyMaps}){for(auto& map:*maps)if(map.id)UnloadRenderTexture(map);maps->clear();}
 surfaceMaps.resize(g.planets.size());geologyMaps.resize(g.planets.size());extremeDetailBody=extremeGraphics?g.nearestPlanet():-1;
 const auto profile=flightQualityProfile(quality);
 for(int body=0;body<int(g.planets.size());body++){const auto& p=g.planets[body];
  // Static geographic fields are baked once per world. Moving silk, waves,
  // cloud advection and the lighting remain live in their original shaders.
  bakePlanetFields(g,body,body==extremeDetailBody);auto data=surfaceMaps[body],geology=geologyMaps[body];
  auto model=LoadModelFromMesh(globe(p,profile.globeLongitude,profile.globeLatitude));material(model,world,noiseTexture);model.materials[0].maps[MATERIAL_MAP_ALBEDO].texture=data.texture;model.materials[0].maps[MATERIAL_MAP_ROUGHNESS].texture=geology.texture;planets.push_back(model);
  auto distant=LoadModelFromMesh(GenMeshSphere(float(p.radius+6),24,48));material(distant,world,noiseTexture);distant.materials[0].maps[MATERIAL_MAP_ALBEDO].texture=data.texture;distant.materials[0].maps[MATERIAL_MAP_ROUGHNESS].texture=geology.texture;distantPlanets.push_back(distant);
  auto air=LoadModelFromMesh(GenMeshSphere(float(p.radius+40),extremeGraphics?128:64,extremeGraphics?192:96));material(air,atmo,noiseTexture);atmospheres.push_back(air);
  auto weather=LoadModelFromMesh(GenMeshSphere(float(p.radius+48),extremeGraphics?128:64,extremeGraphics?192:96));material(weather,cloud,noiseTexture);weather.materials[0].maps[MATERIAL_MAP_ALBEDO].texture=data.texture;clouds.push_back(weather);
 }
 detailedBodies.assign(g.planets.size(),-1);worldKey=g.worldKey;shadowBody=-1;
}
Renderer::GenerationAudit Renderer::auditGeneration(const Game& g)const{
 GenerationAudit result;
 for(size_t i=0;i<g.planets.size()&&i<surfaceMaps.size();i++){const auto& p=g.planets[i];if(p.generation<2||p.giant)continue;auto im=LoadImageFromTexture(surfaceMaps[i].texture);if(!im.data)throw std::runtime_error("Missing generated surface field");ImageFormat(&im,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
  for(int row=0;row<=8;row++)for(int col=0;col<16;col++){int x=col*(im.width-1)/15,y=row*(im.height-1)/8;double phi=(x+.5)/im.width*2*pi,theta=double(y)/(im.height-1)*pi;V3 n{std::sin(theta)*std::cos(phi),std::cos(theta),std::sin(theta)*std::sin(phi)};double expected=materialElevation(p,n),actual=GetImageColor(im,x,y).r/255.;result.maximumError=std::max(result.maximumError,std::abs(expected-actual));result.samples++;}UnloadImage(im);
 }
 return result;
}
bool Renderer::exportMaterial(int body,const std::string& path)const{
 if(body<0||body>=int(geologyMaps.size())||!geologyMaps[body].texture.id)return false;
 auto image=LoadImageFromTexture(geologyMaps[body].texture);if(!image.data)return false;
 ImageFlipVertical(&image);ImageFormat(&image,PIXELFORMAT_UNCOMPRESSED_R8G8B8);auto parent=std::filesystem::path(path).parent_path();if(!parent.empty())std::filesystem::create_directories(parent);
 bool result=ExportImage(image,path.c_str());UnloadImage(image);return result;
}
bool Renderer::exportDetail(const std::string& path)const{
 if(!detailMap.id)return false;auto image=LoadImageFromTexture(detailMap.texture);if(!image.data)return false;
 int covered=0;for(int y=0;y<16;y++)for(int x=0;x<16;x++)covered+=GetImageColor(image,(x*2+1)*image.width/32,(y*2+1)*image.height/32).r>=12;
 std::printf("Ground material coverage: %d/256 samples / %dx%d\n",covered,image.width,image.height);
 if(covered!=256){UnloadImage(image);return false;}
 ImageFlipVertical(&image);bool result=ExportImage(image,path.c_str());UnloadImage(image);return result;
}
bool Renderer::exportShadow(const std::string& path)const{
 if(!shadowActive)return false;auto image=LoadImageFromTexture(sunScene.texture);if(!image.data)return false;
 // Convert explicitly: raylib 5.5's normalized R32 image conversion repeats
 // its first pixel. Keep the diagnostic faithful to the sampled GPU data.
 auto* values=static_cast<float*>(image.data);float lo=1,hi=0;
 auto* pixels=static_cast<unsigned char*>(MemAlloc(unsigned(image.width*image.height)));
 for(int i=0;i<image.width*image.height;i++){float depth=std::clamp(values[i],0.f,1.f);lo=std::min(lo,depth);hi=std::max(hi,depth);pixels[i]=static_cast<unsigned char>(depth*255);}
 Image preview{pixels,image.width,image.height,1,PIXELFORMAT_UNCOMPRESSED_GRAYSCALE};ImageFlipVertical(&preview);
 std::printf("Sun shadow depth: %.6f .. %.6f\n",lo,hi);bool result=ExportImage(preview,path.c_str());UnloadImage(preview);UnloadImage(image);return result;
}
void Renderer::shutdown(){
 clearFolds();
 clearWireModels();if(wireShader.id){UnloadShader(wireShader);wireShader={};}
 clearLife();clearMinorBodies();if(combatGlow.id){UnloadTexture(combatGlow);combatGlow={};}
 std::printf("Sun shadow cache: %u updates / %u active frames\n",shadowUpdates,shadowFrames);
 if(sunScene.id){UnloadRenderTexture(sunScene);sunScene={};}
 if(detailMap.id){UnloadRenderTexture(detailMap);detailMap={};}
 clearPortAssets();if(buildingTexture.id){UnloadTexture(buildingTexture);buildingTexture={};}if(buildingDetailTexture.id){UnloadTexture(buildingDetailTexture);buildingDetailTexture={};}
 for(auto* maps:{&surfaceMaps,&geologyMaps}){for(auto& map:*maps)if(map.id)UnloadRenderTexture(map);maps->clear();}
 for(auto* target:{&scene,&composite,&opaqueScene,&auroraScene,&cloudScene,&bloom[0],&bloom[1]})if(target->id){UnloadRenderTexture(*target);*target={};}
 for(auto* list:{&planets,&distantPlanets,&atmospheres,&clouds}){for(auto& model:*list)if(model.meshCount)UnloadModel(model);list->clear();}
 for(auto* model:{&ringModel,&cockpitModel,&groundModel,&rockModel})if(model->meshCount){UnloadModel(*model);*model={};}
 for(auto shader:{sky,world,groundShader,atmo,cloud,cloudResolve,ringShader,aurora,blur,post,supersampleResolve,cockpitShader,structure,surfaceBake,geologyBake,detailBake,planetDepth,sunDepth,dust,minorShader,minorBake})UnloadShader(shader);if(noiseTexture.id)UnloadTexture(noiseTexture);opaqueDepth={};uniforms.clear();
}
void Renderer::drawRings(const Planet&p,double now){
 set(ringShader,"centre",p.centre);set(ringShader,"eye",V3{camera.position.x,camera.position.y,camera.position.z});set(ringShader,"lightDirection",sunDirection);
 set(ringShader,"radius",float(p.radius));set(ringShader,"seed",float(p.seed));set(ringShader,"time",float(now));
 rlDrawRenderBatchActive();rlDisableBackfaceCulling();rlDisableDepthMask();BeginBlendMode(BLEND_ALPHA);
 DrawModel(ringModel,vec(p.centre),float(p.radius),WHITE);
 EndBlendMode();rlEnableDepthMask();rlEnableBackfaceCulling();
}
void Renderer::drawPlanetDepth(const Game& g,const std::vector<int>& visible,V3 eye){
 set(planetDepth,"eye",eye);set(planetDepth,"groundBlend",groundBlend);set(planetDepth,"patchNormal",groundAnchor);
 for(int i:visible){const auto& body=g.planets[i];set(planetDepth,"centre",body.centre);set(planetDepth,"patchCos",groundBody==i?float(1/std::sqrt(1+std::pow(170/body.radius,2))):0.f);set(planetDepth,"groundPass",0.f);
  auto& model=detailedBodies[i]?planets[i]:distantPlanets[i];auto original=model.materials[0].shader;model.materials[0].shader=planetDepth;DrawModel(model,vec(body.centre),1,BLANK);model.materials[0].shader=original;
  if(groundBody==i&&groundModel.meshCount){set(planetDepth,"groundPass",1.f);auto shader=groundModel.materials[0].shader;groundModel.materials[0].shader=planetDepth;DrawModel(groundModel,vec(body.centre),1,BLANK);groundModel.materials[0].shader=shader;
   if(rockModel.meshCount){set(planetDepth,"groundPass",2.f);auto rockShader=rockModel.materials[0].shader;rockModel.materials[0].shader=planetDepth;DrawModel(rockModel,vec(body.centre),1,BLANK);rockModel.materials[0].shader=rockShader;}
  }
 }
}
void Renderer::drawAuroras(const Game& g,const std::vector<int>& visible,V3 eye){
 // Aurora rays use the same opaque distance field as clouds. Resolve their
 // half-resolution emission at full resolution before adding it to the scene.
 rlSetClipPlanes(worldNearPlane,60000.);BeginTextureMode(auroraScene);ClearBackground(BLANK);BeginMode3D(camera);clipFoldView();
 set(aurora,"viewForward",g.ship.forward);set(aurora,"resolution",Vector2{float(auroraScene.texture.width),float(auroraScene.texture.height)});
 BeginBlendMode(BLEND_ADDITIVE);rlDisableDepthMask();
 for(int i:visible){const auto& body=g.planets[i];double distance=length(eye-body.centre);if(body.atmosphere<.12||distance<body.radius+70)continue;
  set(aurora,"centre",body.centre);set(aurora,"eye",eye);set(aurora,"lightDirection",sunDirection);
  double fade=std::clamp((distance-body.radius-70)/70.,0.,1.);fade=fade*fade*(3-2*fade);
  set(aurora,"seed",float(body.seed));set(aurora,"time",float(visualTime));set(aurora,"atmosphere",float(body.atmosphere));set(aurora,"radius",float(body.radius));set(aurora,"strength",float(fade)*(reduced?.55f:1.f));
  auto& shell=atmospheres[i];shell.materials[0].maps[MATERIAL_MAP_METALNESS].texture=opaqueDepth;shell.materials[0].shader=aurora;DrawModel(shell,vec(body.centre),float((body.radius+70)/(body.radius+40)),WHITE);shell.materials[0].shader=atmo;
 }
 rlEnableDepthMask();EndBlendMode();EndMode3D();EndTextureMode();rlSetClipPlanes(worldNearPlane,60000.);
 resolveWeather(g,eye,auroraScene.texture,true);if(auditClouds)measureCloudOcclusion(g,g.nearestPlanet(),eye,true);
}
void Renderer::drawClouds(const Game& g,const std::vector<int>& visible,V3 eye){
 // The main terrain depth is now complete and detached from this framebuffer.
 // Cloud integration reads it directly, avoiding a second near-clamped mesh.
 rlSetClipPlanes(worldNearPlane,60000.);BeginTextureMode(cloudScene);ClearBackground(BLANK);BeginMode3D(camera);clipFoldView();
 auto sorted=visible;std::stable_sort(sorted.begin(),sorted.end(),[&](int a,int b){return length(g.planets[a].centre-eye)>length(g.planets[b].centre-eye);});
 BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);rlDrawRenderBatchActive();rlDisableDepthMask();rlDisableBackfaceCulling();
 for(auto shader:{cloud,atmo}){set(shader,"viewForward",g.ship.forward);set(shader,"resolution",Vector2{float(cloudScene.texture.width),float(cloudScene.texture.height)});}
 for(int i:sorted){const auto& body=g.planets[i];if(!body.giant&&body.atmosphere>=.12){
  set(cloud,"centre",body.centre);set(cloud,"eye",eye);set(cloud,"lightDirection",sunDirection);
  set(cloud,"weatherHours",float(presentation?presentation->hours:g.time/55));set(cloud,"weatherSeed",float(body.worldSeed%8191)*.013f);
  set(cloud,"time",float(visualTime));set(cloud,"seed",float(body.seed));set(cloud,"water",float(body.water));set(cloud,"kind",float(body.kind));
  set(cloud,"ringed",i==2||body.giant?1.f:0.f);set(cloud,"radius",float(body.radius));set(cloud,"atmosphere",float(body.atmosphere));set(cloud,"sampleCount",float(reduced?10:flightQualityProfile(quality).cloudSamples));
  set(cloud,"resolution",Vector2{float(cloudScene.texture.width),float(cloudScene.texture.height)});
  set(cloud,"useSurface",surfaceMaps[i].texture.format==PIXELFORMAT_UNCOMPRESSED_R16G16B16A16?1.f:0.f);
  clouds[i].materials[0].maps[MATERIAL_MAP_METALNESS].texture=opaqueDepth;DrawModel(clouds[i],vec(body.centre),1,WHITE);
  }
  if(body.atmosphere>=.08&&length(g.ship.position-body.centre)>=body.radius+42){set(atmo,"centre",body.centre);set(atmo,"eye",eye);set(atmo,"lightDirection",sunDirection);set(atmo,"seed",float(body.seed));set(atmo,"kind",float(body.kind));set(atmo,"atmosphere",float(body.atmosphere));set(atmo,"radius",float(body.radius));atmospheres[i].materials[0].maps[MATERIAL_MAP_METALNESS].texture=opaqueDepth;DrawModel(atmospheres[i],vec(body.centre),1,WHITE);}
 }
 rlDrawRenderBatchActive();rlEnableBackfaceCulling();rlEnableDepthMask();EndBlendMode();EndMode3D();EndTextureMode();rlSetClipPlanes(worldNearPlane,60000.);
 resolveWeather(g,eye,cloudScene.texture);if(auditClouds)measureCloudOcclusion(g,g.nearestPlanet(),eye);
}
void Renderer::resolveWeather(const Game& g,V3 eye,Texture2D source,bool emission){
 set(cloudResolve,"emissionPass",emission?1.f:0.f);
 // Resolve while the main framebuffer is unbound; copy back after reads finish.
 const auto& local=g.planets[g.nearestPlanet()];auto right=unit(cross(g.ship.forward,g.ship.up));
 set(cloudResolve,"eye",eye);set(cloudResolve,"localCentre",local.centre);set(cloudResolve,"localRadius",float(local.radius));
 set(cloudResolve,"viewForward",g.ship.forward);set(cloudResolve,"viewRight",right);set(cloudResolve,"viewUp",unit(cross(right,g.ship.forward)));
 BeginTextureMode(composite);ClearBackground(BLANK);rlDrawRenderBatchActive();rlDisableColorBlend();
 BeginShaderMode(cloudResolve);texture(cloudResolve,"opaqueDepth",opaqueDepth);
 DrawTexturePro(source,{0,0,float(source.width),-float(source.height)},{0,0,float(scene.texture.width),float(scene.texture.height)},{0,0},0,WHITE);
 rlDrawRenderBatchActive();rlEnableColorBlend();EndShaderMode();EndTextureMode();
}
void Renderer::measureCloudOcclusion(const Game& g,int body,V3 eye,bool emission){
 auto& stats=emission?auroraStats:cloudStats;stats={};
#if defined(__APPLE__)
 // Explicit no-save GPU regression only: compare resolved weather alpha with
 // independently reconstructed opaque distances and the empty inner shell.
 int width=scene.texture.width,height=scene.texture.height;std::vector<float> depth(size_t(width)*height),weather(size_t(width)*height*4);
 glBindTexture(GL_TEXTURE_2D,opaqueDepth.id);glGetTexImage(GL_TEXTURE_2D,0,GL_RED,GL_FLOAT,depth.data());
 glBindTexture(GL_TEXTURE_2D,composite.texture.id);glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_FLOAT,weather.data());glBindTexture(GL_TEXTURE_2D,0);
 std::printf("Weather depth audit: %.5f .. %.5f / centre %.5f\n",*std::min_element(depth.begin(),depth.end()),*std::max_element(depth.begin(),depth.end()),depth[size_t(height/2)*width+width/2]);
 const auto& p=g.planets[body];auto origin=eye-p.centre,right=unit(cross(g.ship.forward,g.ship.up)),up=unit(cross(right,g.ship.forward));double inner=p.radius+20;
 for(int y=0;y<height;y+=2)for(int x=0;x<width;x+=2){
  size_t pixel=size_t(y)*width+x;float alpha=emission?std::max({weather[pixel*4],weather[pixel*4+1],weather[pixel*4+2]}):weather[pixel*4+3];if(!std::isfinite(alpha))throw std::runtime_error("Non-finite cloud resolve");stats.maxCloudAlpha=std::max(stats.maxCloudAlpha,alpha);
  if(length(origin)>=inner-.1||depth[pixel]<=0)continue;
  auto direction=unit(g.ship.forward+right*((2*(x+.5)/width-1)*double(width)/height*.7002075)+up*((2*(y+.5)/height-1)*.7002075));
  double b=dot(origin,direction),exit=-b+std::sqrt(std::max(0.,b*b+inner*inner-dot(origin,origin)));
  double opaque=depth[pixel];
  if(opaque>=exit*dot(direction,g.ship.forward)-.1)continue;
  stats.testedPixels++;stats.maxLeakAlpha=std::max(stats.maxLeakAlpha,alpha);if(alpha>1.f/1024)stats.leakingPixels++;
 }
#endif
}
void Renderer::drawSunShadows(const Game& g,V3 eye){
 shadowActive=false;if(reduced||groundBody<0||!groundModel.meshCount||sunScene.texture.format!=PIXELFORMAT_UNCOMPRESSED_R32||dot(groundAnchor,sunDirection)<.035)return;
 const auto& body=g.planets[groundBody];V3 localEye=eye-body.centre;uint64_t casterKey=0;std::vector<int> ports;
 for(int i=0;i<int(g.ports.size());i++){const auto& port=g.ports[i];if(port.planet!=groundBody||length(port.position-g.ship.position)>480)continue;
  const auto* view=presentation->settlement(port.settlement);auto key=portGeometryKey(port,view,g);auto it=portAssets.find(port.id);
  if(it==portAssets.end())it=portAssets.emplace(port.id,buildPort(port,view,g)).first;
  else if(it->second.key!=key||length(it->second.at-port.position)>1e-5||length(it->second.up-port.normal)>1e-8){UnloadModel(it->second.close);UnloadModel(it->second.detailed);UnloadModel(it->second.distant);it->second=buildPort(port,view,g);}
  casterKey^=it->second.key+0x9e3779b97f4a7c15ULL+(casterKey<<6)+(casterKey>>2);ports.push_back(i);
 }
 for(const auto& instance:structuralInstances)if(!instance.shape.orbital&&instance.body==groundBody){double age=structureAge(g,instance.shape);if(instance.shape.localized){casterKey^=instance.shape.cleared;for(int sector=0;sector<6;sector++)if(instance.shape.sections[sector]<=0)casterKey^=uint64_t(std::min(7.,sectionAge(instance.shape,sector,age))*30)*(0x9e3779b9u+sector*17u); }casterKey^=sceneHash(instance.id)+uint64_t(std::min(age,7.)*30)*0x9e3779b9u;}
 shadowActive=true;++shadowFrames;
 if(shadowBody==groundBody&&length(shadowAnchor-groundAnchor)<1e-8&&length(shadowEye-localEye)<4&&std::abs(shadowBlend-groundBlend)<.002&&shadowCasters==casterKey)return;
 V3 forward=-sunDirection,right=unit(cross(forward,{0,1,0})),up=unit(cross(right,forward));
 V3 target=groundAnchor*(body.radius+terrain(body,groundAnchor)+2);constexpr double span=340;double texel=span/std::max(1,sunScene.texture.width);
 // Snap in the light's fixed axes, so recentering the ground patch does not
 // slide the shadow texels over the world.
 target+=right*(std::round(dot(target,right)/texel)*texel-dot(target,right));target+=up*(std::round(dot(target,up)/texel)*texel-dot(target,up));
 Camera3D lightCamera{vec(target+sunDirection*360),vec(target),vec(up),float(span),CAMERA_ORTHOGRAPHIC};
 rlSetClipPlanes(.5,1000.);BeginTextureMode(sunScene);ClearBackground(WHITE);BeginMode3D(lightCamera);
 sunMatrix=MatrixMultiply(GetCameraMatrix(lightCamera),rlGetMatrixProjection());set(sunDepth,"eye",localEye);set(sunDepth,"groundBlend",groundBlend);
 rlDrawRenderBatchActive();rlDisableColorBlend();
 auto draw=[&](Model& model,V3 at,float pass){set(sunDepth,"groundPass",pass);auto original=model.materials[0].shader;model.materials[0].shader=sunDepth;DrawModel(model,vec(at),1,WHITE);model.materials[0].shader=original;};
 draw(groundModel,{},1);if(rockModel.meshCount)draw(rockModel,{},2);
 drawStructuralRemains(g,true);
 for(int i:ports)draw(portAssets.at(g.ports[i].id).close,g.ports[i].position-body.centre,0);
 rlDrawRenderBatchActive();rlEnableColorBlend();EndMode3D();EndTextureMode();rlSetClipPlanes(worldNearPlane,60000.);
 shadowBody=groundBody;shadowAnchor=groundAnchor;shadowEye=localEye;shadowBlend=groundBlend;shadowCasters=casterKey;++shadowUpdates;
}
void Renderer::drawMotion(const Game& g){
 if(reduced||g.docked||g.speed()<24)return;
 V3 direction=unit(g.ship.velocity);double strength=std::clamp(g.speed()/450.,0.,1.);
 int gx=int(std::floor(g.ship.position.x/180)),gy=int(std::floor(g.ship.position.y/180)),gz=int(std::floor(g.ship.position.z/180));
 BeginBlendMode(BLEND_ADDITIVE);rlDisableDepthMask();
 for(int x=-2;x<=2;x++)for(int y=-2;y<=2;y++)for(int z=-2;z<=2;z++){
  double h=std::sin((gx+x)*127.1+(gy+y)*311.7+(gz+z)*74.7)*43758.5453;h-=std::floor(h);
  V3 p{(gx+x+h)*180,(gy+y+std::fmod(h*17,1.))*180,(gz+z+std::fmod(h*31,1.))*180};
  double d=length(p-g.ship.position);if(d<30||d>360||dot(p-g.ship.position,g.ship.forward)<20)continue;
  const auto& world=g.planets[g.nearestPlanet()];if(length(p-world.centre)<world.radius+25)continue;
  line(p,p-direction*(1+g.speed()*.028),alpha(cyan,float((1-d/360)*strength*.36)));
 }
 rlEnableDepthMask();EndBlendMode();
}
void Renderer::prepare(const Game&g,const Scene& sceneView,bool reducedEffects,bool cockpitVisible,GraphicsQuality graphicsQuality,bool psychedelicLook,bool wireframeLook,bool egaPalette){
 if(g.planets.empty()||g.ports.empty())return;
 if(quality!=graphicsQuality){shutdown();quality=graphicsQuality;lowGraphics=quality==GraphicsQuality::Low;extremeGraphics=quality==GraphicsQuality::Extreme;worldKey.clear();bodiesKey.clear();cockpitWidth=cockpitHeight=0;groundBody=shadowBody=-1;if(!init(quality))throw std::runtime_error("Graphics quality initialization failed");}
 prepareFoldViews(g,sceneView,reducedEffects,cockpitVisible,psychedelicLook,wireframeLook,egaPalette);
 for(auto shader:{sky,world,groundShader,cloud,post})set(shader,"lowGraphics",lowGraphics?1.f:0.f);
 const auto profile=flightQualityProfile(quality);
 egaGraphics=egaPalette;wireframe=wireframeLook;psychedelic=psychedelicLook&&!wireframe;wireDraws=wireTriangles=wireEdges=0;++wireSerial;
 for(auto it=wireModels.begin();it!=wireModels.end();)if(!wireframe||wireSerial-it->second.used>2){UnloadModel(it->second.model);it=wireModels.erase(it);}else ++it;
 for(auto shader:{sky,world,groundShader,post,ringShader}){set(shader,"wireEGA",egaGraphics?1.f:0.f);set(shader,"wireframe",wireframe?1.f:0.f);set(shader,"wirePixelScale",std::max(1.f,float(flightRenderSize(quality,reducedEffects,GetRenderWidth(),GetRenderHeight()).width)/std::max(1,GetRenderWidth())));}
 psychedelic=psychedelicLook&&!wireframe;
 for(auto shader:{sky,world,groundShader,cloud,atmo,ringShader,aurora,structure,cockpitShader,post})set(shader,"psychedelic",psychedelic?1.f:0.f);
 for(auto shader:{world,groundShader,cloud,atmo,aurora,structure})set(shader,"extremeQuality",extremeGraphics?1.f:0.f);
 set(atmo,"integrationSamples",float(reducedEffects?8:profile.atmosphereSamples));set(atmo,"sunSamples",float(reducedEffects?3:profile.atmosphereSunSamples));set(aurora,"integrationSamples",float(reducedEffects?6:profile.auroraSamples));
 presentation=&sceneView;reduced=reducedEffects;showCockpit=cockpitVisible;speed=g.speed();visualTime=fixedVisualTime>=0?fixedVisualTime:GetTime();
 if(worldKey!=g.worldKey){std::string key=g.star;for(const auto& p:g.planets)key+='|'+p.name+std::to_string(p.seed)+std::to_string(p.radius);
  if(key!=bodiesKey){rebuild(g);bodiesKey=key;}else{worldKey=g.worldKey;groundBody=-1;}
 }
 preparePlanetDetail(g);
 const auto renderSize=flightRenderSize(quality,reduced,GetRenderWidth(),GetRenderHeight());const int renderH=renderSize.height,renderW=renderSize.width;
 const int cloudDivisor=reduced?3:profile.cloudDivisor,auroraDivisor=reduced?2:profile.auroraDivisor;
 if(scene.texture.width!=renderW||scene.texture.height!=renderH||cloudScene.texture.width!=std::max(1,renderW/cloudDivisor)){
 for(auto* target:{&scene,&composite,&opaqueScene,&auroraScene,&cloudScene,&bloom[0],&bloom[1]})if(target->id)UnloadRenderTexture(*target);
  scene=linearTarget(renderW,renderH);composite=linearTarget(renderW,renderH);opaqueScene=linearTarget(renderW,renderH,PIXELFORMAT_UNCOMPRESSED_R32);
  if(!scene.id||!composite.id)throw std::runtime_error("Flight weather depth target could not be allocated");
  if(!opaqueScene.id||opaqueScene.texture.format!=PIXELFORMAT_UNCOMPRESSED_R32)throw std::runtime_error("Flight weather distance target could not be allocated");
  opaqueDepth=opaqueScene.texture;SetTextureFilter(opaqueDepth,TEXTURE_FILTER_POINT);SetTextureWrap(opaqueDepth,TEXTURE_WRAP_CLAMP);
  std::printf("Weather depth: R32F / matched terrain / %dx%d\n",renderW,renderH);
  auroraScene=linearTarget(std::max(1,renderW/auroraDivisor),std::max(1,renderH/auroraDivisor));
  cloudScene=linearTarget(std::max(1,renderW/cloudDivisor),std::max(1,renderH/cloudDivisor));
  for(auto& target:bloom)target=linearTarget(std::max(1,renderW/3),std::max(1,renderH/3));
  hdr=scene.texture.format==PIXELFORMAT_UNCOMPRESSED_R16G16B16A16;
#if defined(__APPLE__)
  GLint depthBits=0;glBindRenderbuffer(GL_RENDERBUFFER,scene.depth.id);glGetRenderbufferParameteriv(GL_RENDERBUFFER,GL_RENDERBUFFER_DEPTH_SIZE,&depthBits);glBindRenderbuffer(GL_RENDERBUFFER,0);std::printf("Flight depth buffer: %d bits\n",int(depthBits));
#endif
  std::printf("Flight renderer: %s / %s graphics / spectral silk / auroras / 3D cockpit / %dx%d\n",hdr?"HDR16F":"LDR fallback",graphicsQualityName(quality),renderW,renderH);
  for(auto* target:{&scene,&composite,&auroraScene,&cloudScene,&bloom[0],&bloom[1]})SetTextureFilter(target->texture,TEXTURE_FILTER_BILINEAR);
 }
 // The ship position is its landing-gear origin; the pilot sits above the pad.
 V3 eye=g.ship.position+(showCockpit?g.ship.up*3.:V3{});
 if(showCockpit&&g.touchdownAge<1.5)eye+=g.ship.up*(std::sin(g.touchdownAge*22)*std::exp(-g.touchdownAge*4)*.12);
 // A long sight vector retains direction precision after float conversion.
 // A unit-long target jitters by whole pixels far from the system origin.
 camera={vec(eye),vec(eye+g.ship.forward*1024.),vec(g.ship.up),70,CAMERA_PERSPECTIVE};
 // Keep a near aperture drawable until the physical crossing. Colour and
 // weather depth always use the same projection.
 worldNearPlane=1;
 for(const auto& gate:g.folds){auto offset=eye-gate.position;double distance=dot(offset,gate.normal);
  if(distance>0&&length(offset-gate.normal*distance)<gate.radius+10)worldNearPlane=std::min(worldNearPlane,std::max(.002,distance*.2));
 }
 int nearest=g.nearestPlanet();const auto&p=g.planets[nearest];double altitude=g.altitude();float now=float(visualTime);
 updateGround(g,nearest);
 sunDirection=unit({.6,.7,-.4});
 auto stamp=[&](){if(profiling){rlDrawRenderBatchActive();
#if defined(__APPLE__)
  glFinish();
#endif
 }return GetTime();};double tick=profiling?stamp():0.;
 auto elapsed=[&](int stage){if(profiling){double next=stamp();timings[stage]=(next-tick)*1000;tick=next;}};
 std::vector<int> visible;for(int i=0;i<int(g.planets.size());i++){const auto& body=g.planets[i];if(dot(body.centre-g.ship.position,g.ship.forward)>=-body.radius*1.85)visible.push_back(i);}
 int lensed=nilBody(g);if(lensed>=0)visible.erase(std::remove(visible.begin(),visible.end(),lensed),visible.end());
 // Hysteresis prevents a hovering body from alternating between meshes.
 // Main colour, cloud occlusion and aurora occlusion share this one decision.
 for(int i:visible){const auto& body=g.planets[i];double pixels=body.radius/std::max(1.,length(body.centre-eye))*renderH/.7002;auto& detail=detailedBodies[i];if(detail<0)detail=pixels>130;else if(detail&&pixels<112)detail=0;else if(!detail&&pixels>152)detail=1;}
 // Opaque terrain is drawn first. Weather reads that exact depth only while
 // the main framebuffer is unbound, and resolves before ships and the cockpit.
 // The cockpit has its own close projection. A one-kilometre world near
 // plane preserves substantially more precision in distant layered planets.
 prepareMinorBodies(g);prepareStructures(g);if(!wireframe)drawSunShadows(g,eye);else shadowActive=false;rlSetClipPlanes(worldNearPlane,60000);
 BeginTextureMode(scene);ClearBackground(ink);
 set(sky,"resolution",Vector2{float(renderW),float(renderH)});set(sky,"forward",g.ship.forward);set(sky,"up",g.ship.up);
 set(sky,"right",unit(cross(g.ship.forward,g.ship.up)));set(sky,"radial",unit(g.ship.position-p.centre));set(sky,"lightDirection",sunDirection);
 auto localWeather=weatherAt(p,unit(g.ship.position-p.centre),presentation?presentation->hours:g.time/55);
 set(sky,"storm",float(localWeather.storm));set(sky,"lightning",reduced?0.f:float(localWeather.lightning));
 set(sky,"time",now);set(sky,"altitude",float(altitude));set(sky,"air",float(std::clamp(1-altitude/240,0.,1.))*.88f*float(std::clamp(p.atmosphere,.0,1.2)));set(sky,"seed",float(g.planets.front().seed));
 BeginShaderMode(sky);texture(sky,"noiseTexture",noiseTexture);DrawRectangle(0,0,renderW,renderH,WHITE);EndShaderMode();
 elapsed(0);
 rlSetClipPlanes(worldNearPlane,60000);BeginMode3D(camera);clipFoldView();
 for(auto shader:{world,groundShader}){set(shader,"time",now);set(shader,"eye",eye);set(shader,"lightDirection",sunDirection);set(shader,"metal",0.f);set(shader,"groundBlend",groundBlend);set(shader,"gearPosition",g.ship.position);set(shader,"groundClearance",float(altitude));}
 for(int i:visible){
  const auto& body=g.planets[i];
  // Distant bodies use a lighter mesh. Never cull a body intersecting the camera.
  set(world,"centre",body.centre);set(world,"seed",float(body.seed));set(world,"ringed",i==2||body.giant?1.f:0.f);
  set(world,"useSurface",surfaceMaps[i].texture.format==PIXELFORMAT_UNCOMPRESSED_R16G16B16A16?1.f:0.f);set(world,"useGeology",geologyMaps[i].id?1.f:0.f);
  set(world,"water",float(body.water));set(world,"life",float(body.life));set(world,"kind",float(body.kind));set(world,"atmosphere",float(body.atmosphere));set(world,"radius",float(body.radius));
  set(world,"temperature",float(body.temperature));set(world,"patchNormal",groundAnchor);set(world,"patchCos",groundBody==i?float(1/std::sqrt(1+std::pow(170/body.radius,2))):0.f);set(world,"groundPass",0.f);
  DrawModel(detailedBodies[i]?planets[i]:distantPlanets[i],vec(body.centre),1,WHITE);
  if(groundBody==i&&groundModel.meshCount){
   set(groundShader,"centre",body.centre);set(groundShader,"seed",float(body.seed));set(groundShader,"ringed",i==2||body.giant?1.f:0.f);set(groundShader,"sunShadowMatrix",sunMatrix);set(groundShader,"useSunShadow",shadowActive&&shadowBody==i?groundBlend:0.f);set(groundShader,"useSurface",surfaceMaps[i].texture.format==PIXELFORMAT_UNCOMPRESSED_R16G16B16A16?1.f:0.f);set(groundShader,"useGeology",geologyMaps[i].id?1.f:0.f);
   set(groundShader,"water",float(body.water));set(groundShader,"life",float(body.life));set(groundShader,"kind",float(body.kind));set(groundShader,"atmosphere",float(body.atmosphere));set(groundShader,"radius",float(body.radius));set(groundShader,"temperature",float(body.temperature));
   set(groundShader,"patchNormal",groundAnchor);set(groundShader,"patchCos",float(1/std::sqrt(1+std::pow(170/body.radius,2))));
   set(groundShader,"groundPass",1.f);DrawModel(groundModel,vec(body.centre),1,WHITE);if(rockModel.meshCount){set(groundShader,"groundPass",2.f);if(wireframe)drawVectorModel(rockModel,vec(body.centre),1,6);else DrawModel(rockModel,vec(body.centre),1,WHITE);}
  }
 }
 drawMinorBodies(g);
 for(int i:visible)if(g.planets[i].giant||i==2)drawRings(g.planets[i],now);
 EndMode3D();drawNil(g,eye);EndTextureMode();
 // Full-resolution distance capture uses the identical mesh, morph and 1 km
 // near plane as colour, keeping close occluders precise without shading twice.
 if(!wireframe){BeginTextureMode(opaqueScene);ClearBackground(BLANK);BeginMode3D(camera);clipFoldView();rlDrawRenderBatchActive();rlDisableColorBlend();
 drawPlanetDepth(g,visible,eye);drawMinorBodies(g,true);rlDrawRenderBatchActive();rlEnableColorBlend();EndMode3D();EndTextureMode();drawClouds(g,visible,eye);
 BeginTextureMode(scene);BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
 DrawTexturePro(composite.texture,{0,0,float(renderW),-float(renderH)},{0,0,float(renderW),float(renderH)},{0,0},0,WHITE);
 EndBlendMode();EndTextureMode();elapsed(1);drawAuroras(g,visible,eye);elapsed(2);
 BeginTextureMode(scene);BeginBlendMode(BLEND_ADDITIVE);
 DrawTexturePro(composite.texture,{0,0,float(renderW),-float(renderH)},{0,0,float(renderW),float(renderH)},{0,0},0,WHITE);
 EndBlendMode();}else{elapsed(1);elapsed(2);BeginTextureMode(scene);}BeginMode3D(camera);clipFoldView();
 drawCometTails(g);
 for(int i=0;i<int(g.ports.size());i++){
  const auto&port=g.ports[i];if(port.minor>=0||port.recovery||port.fold>=0)continue;double d=length(port.position-g.ship.position);
  if(d<8500&&(dot(port.position-g.ship.position,g.ship.forward)>-380)&&(port.planet<0||dot(port.normal,g.ship.position-port.position)>-160)){
   drawSettlement(port,sceneView.settlement(port.settlement),g);if(d<1500)drawPortActivity(port,sceneView);
  }
 }
 // A restrained approach corridor is attached to the actual selected berth.
 if(g.target>=0&&g.target<int(g.ports.size())){const auto& port=g.ports[g.target];double d=length(port.position-g.ship.position);
  if(port.minor<0&&port.fold<0&&!g.docked&&d<2400&&dot(port.normal,g.ship.position-port.position)>0){
   V3 a=unit(cross({1,0,0},port.normal),{0,0,1}),b=unit(cross(port.normal,a));BeginBlendMode(BLEND_ADDITIVE);rlDisableDepthMask();
   for(int i=1;i<=5;i++)ring(port.position+port.normal*(i*85+25),a,b,port.planet>=0?24:32,alpha(mint,.12f+i*.025f),48,.15f);
   rlEnableDepthMask();EndBlendMode();
  }
 }
 drawLife(sceneView);
 for(const auto&t:g.traffic)if(!t.dead&&length(t.position-g.ship.position)<6500&&dot(t.position-g.ship.position,g.ship.forward)>-80)drawFactionCraft(t,sceneView);
 drawWorldDamage(g);drawShipRemains(g);drawCombat(g);drawFolds(g);
 auto facing=unit(cross(g.ship.forward,g.ship.up)),vertical=g.ship.up;
 for(const auto& wreck:g.wrecks)if(length(wreck.position-g.ship.position)<1700)ring(wreck.position,facing,vertical,6,alpha(mint,.25f),24);
 // A bounded, seeded discharge follows the same storm front as the helm.
 if(!reduced&&altitude<160&&localWeather.lightning>.025){
  auto n=unit(g.ship.position-p.centre),east=unit(cross({0,1,0},n),{1,0,0}),side=unit(cross(n,east));
  auto base=p.centre+unit(n*p.radius+east*65+side*40)*(p.radius+8);auto top=base+n*26;
  if(g.lineOfSight(g.ship.position,top)){
   BeginBlendMode(BLEND_ADDITIVE);for(int i=0;i<9;i++){auto point=[&](int k){return mix(base,top,k/9.)+east*(std::sin(k*2.7+p.seed)*1.4)+side*(std::sin(k*4.1+p.seed)*.8);};tube(point(i),point(i+1),.12f,alpha(Color{187,155,255,255},float(localWeather.lightning*.7)));}EndBlendMode();
  }
 }
 drawGroundEffects(g);drawMotion(g);EndMode3D();elapsed(3);
 if(showCockpit){
  auto viewport=hyperion::UIViewport(float(GetScreenWidth()),float(GetScreenHeight()));int cabinW=int(std::lround(viewport.width)),cabinH=int(std::lround(viewport.height));
  if(cockpitWidth!=cabinW||cockpitHeight!=cabinH||cockpitDocked!=g.docked||cockpitKind!=g.ship.vessel.model)rebuildCockpit(cabinW,cabinH,g.docked,g.ship.vessel.model);
  drawCockpit(g);
 }
 elapsed(4);
 EndTextureMode();
 for(int i=0;i<2;i++){
  Texture2D source=i==0?scene.texture:bloom[0].texture;
  BeginTextureMode(bloom[i]);ClearBackground(BLACK);set(blur,"direction",i==0?Vector2{2.f/source.width,0}:Vector2{0,2.f/source.height});set(blur,"extractLight",i==0?1.f:0.f);
  BeginShaderMode(blur);DrawTexturePro(source,{0,0,float(source.width),-float(source.height)},{0,0,float(bloom[i].texture.width),float(bloom[i].texture.height)},{0,0},0,WHITE);EndShaderMode();EndTextureMode();
 }
 auto sunScreen=GetWorldToScreenEx(vec(g.ship.position+sunDirection*40000),camera,renderW,renderH);float sunVisibility=dot(g.ship.forward,sunDirection)>0?1.f:0.f;
 for(const auto& body:g.planets){auto offset=body.centre-g.ship.position;double along=dot(offset,sunDirection);if(along>0&&length(offset-sunDirection*along)<body.radius+8)sunVisibility=0;}
 for(const auto& body:g.minorBodies){if(body.destroyed)continue;auto offset=body.centre-g.ship.position;double along=dot(offset,sunDirection);if(along>0&&length(offset-sunDirection*along)<body.source.radius)sunVisibility=0;}
 set(post,"sunPosition",Vector2{sunScreen.x/renderW,1-sunScreen.y/renderH});set(post,"sunVisibility",sunVisibility);
 set(post,"texel",Vector2{1.f/renderW,1.f/renderH});set(post,"hit",float(g.hitFlash));set(post,"effects",reduced?.12f:1.f);
 auto shipRight=unit(cross(g.ship.forward,g.ship.up));
 set(post,"impactColour",g.impactHull>.01?V3{.55,.075,.12}:V3{.075,.38,.60});
 set(post,"impactDirection",Vector2{float(dot(g.hitDirection,shipRight)),float(dot(g.hitDirection,g.ship.up))});
 set(post,"shieldBreak",float(g.feedback.shieldBreak));
 BeginTextureMode(composite);ClearBackground(ink);BeginShaderMode(post);texture(post,"bloomTexture",bloom[1].texture);
 DrawTexturePro(scene.texture,{0,0,float(renderW),-float(renderH)},{0,0,float(renderW),float(renderH)},{0,0},0,WHITE);EndShaderMode();EndTextureMode();
 elapsed(5);
}
void Renderer::rebuildCockpit(int width,int height,bool docked,int kind){
 if(cockpitModel.meshCount)UnloadModel(cockpitModel);cockpitWidth=width;cockpitHeight=height;cockpitDocked=docked;cockpitKind=kind;CabinMesh mesh;mesh.aspect=double(width)/height;
 float top=60.f/height,deck=1-112.f/height;Color titanium{45,65,78,0},dark{17,29,38,0},copper{91,65,44,0},glow{25,115,119,255};
 if(kind==1){titanium={35,60,82,0};copper={48,87,99,0};glow={35,134,169,255};}
 if(kind==2){titanium={64,69,60,0};copper={105,79,46,0};glow={125,108,38,255};}
 if(kind==3){titanium={52,42,77,0};copper={76,48,109,0};glow={111,46,157,255};}
 mesh.panel({{0,top-.05f},{.24f,top-.025f},{.21f,top+.009f},{0,top+.035f}},titanium,.86f,.10f);
 mesh.panel({{1,top-.05f},{.76f,top-.025f},{.79f,top+.009f},{1,top+.035f}},titanium,.86f,.10f);
 mesh.panel({{0,top},{.032f,top+.032f},{.085f,deck-.045f},{.054f,deck+.03f},{0,deck-.005f}},titanium,.70f,.16f);
 mesh.panel({{1,top},{.968f,top+.032f},{.915f,deck-.045f},{.946f,deck+.03f},{1,deck-.005f}},titanium,.70f,.16f);
 mesh.panel({{0,deck-.005f},{.13f,deck-.055f},{.28f,deck+.025f},{0,1}},titanium,.65f,.10f);
 mesh.panel({{1,deck-.005f},{.87f,deck-.055f},{.72f,deck+.025f},{1,1}},titanium,.65f,.10f);
 mesh.panel({{.13f,deck},{.87f,deck},{1,1},{0,1}},dark,.68f,.035f);
 // Copper rails and recessed instrument housings have actual beveled side faces.
 mesh.panel({{.11f,deck-.041f},{.14f,deck-.043f},{.232f,deck+.005f},{.216f,deck+.011f}},copper,.62f,.22f);
 mesh.panel({{.89f,deck-.041f},{.86f,deck-.043f},{.768f,deck+.005f},{.784f,deck+.011f}},copper,.62f,.22f);
 // Instrument glass is laid out by the responsive HUD above this physical deck.
 for(int side:{0,1})for(int i=0;i<6;i++){
  float x=side?1-.018f-i*.012f:.018f+i*.012f;
  mesh.panel({{x,deck-.016f},{x+.006f,deck-.016f},{x+.006f,deck-.012f},{x,deck-.012f}},i==5?Color{122,50,24,255}:glow,.58f,0);
 }
 for(float y:{.25f,.43f,.61f})for(int side:{0,1}){float x=.023f+(y-top)*.077f;if(side)x=1-x;mesh.panel({{x-.003f,y-.004f},{x+.003f,y-.004f},{x+.003f,y+.004f},{x-.003f,y+.004f}},{100,111,114,0},.66f,.35f);}
 for(int i=0;i<14;i++){float x=.35f+i*.022f;mesh.panel({{x,.982f},{x+.012f,.982f},{x+.009f,.995f},{x-.003f,.995f}},{6,11,15,0},.56f,.15f);}
 if(kind==1)for(int side:{0,1}){float a=side?.96f:.04f,b=side?.89f:.11f;mesh.panel({{a,.24f},{a+(side?-.01f:.01f),.24f},{b,.44f},{b+(side?.006f:-.006f),.44f}},titanium,.76f,.045f);}
 if(kind==2)for(int side:{0,1}){float a=side?.965f:.035f,b=side?.86f:.14f;mesh.panel({{a,.27f},{b,.30f},{b,.318f},{a,.29f}},titanium,.7f,.09f);mesh.panel({{a,deck-.09f},{b,deck-.05f},{b,deck-.027f},{a,deck-.065f}},copper,.7f,.08f);}
 if(kind==3)for(int side:{0,1}){float a=side?.97f:.03f,b=side?.79f:.21f;mesh.panel({{a,top+.026f},{b,top+.004f},{b,top+.009f},{a,top+.032f}},glow,.76f,.01f);}
 cockpitModel=mesh.model();material(cockpitModel,cockpitShader,noiseTexture);
}
void Renderer::drawCockpit(const Game& g){
 Camera3D cabin{{0,0,0},{0,0,-1},{0,1,0},70,CAMERA_PERSPECTIVE};rlSetClipPlanes(.025,6.);BeginMode3D(cabin);
 auto right=unit(cross(g.ship.forward,g.ship.up));V3 localLight{dot(sunDirection,right),dot(sunDirection,g.ship.up),-dot(sunDirection,g.ship.forward)};
 set(cockpitShader,"lightDirection",localLight);set(cockpitShader,"time",float(visualTime));
 const V3 colours[]={{.16,.67,1.},{1.,.48,.10},{.65,.20,1.},{1.,.24,.13},{.22,1.,.62}};
 double strength=reduced?.12:1.;auto combatLight=colours[std::clamp(g.feedback.weapon,0,4)]*(g.feedback.recoil*strength);
 combatLight+=(g.impactHull>.01?V3{1.,.10,.18}:V3{.10,.55,1.})*(g.feedback.impact*strength);
 set(cockpitShader,"combatLight",combatLight);
 // Only the cabin moves: the world camera, reticle and projectile aim remain aligned.
 Vector3 kick{};if(!reduced)kick={float(std::sin(g.time*71)*g.feedback.impact*.003),float(-g.feedback.recoil*.004+std::sin(g.time*59)*g.feedback.impact*.004),float(g.feedback.recoil*.025)};
 drawVectorModel(cockpitModel,kick,1,2);EndMode3D();rlSetClipPlanes(worldNearPlane,60000.);
}
void Renderer::present(float width,float height){
 if(!composite.id)return;
 if(extremeGraphics&&!reduced){
  set(supersampleResolve,"texel",Vector2{1.f/composite.texture.width,1.f/composite.texture.height});BeginShaderMode(supersampleResolve);
  DrawTexturePro(composite.texture,{0,0,float(composite.texture.width),-float(composite.texture.height)},{0,0,width,height},{0,0},0,WHITE);EndShaderMode();
 }else DrawTexturePro(composite.texture,{0,0,float(composite.texture.width),-float(composite.texture.height)},{0,0,width,height},{0,0},0,WHITE);

}
Vector2 Renderer::project(V3 p,float width,float height)const{return GetWorldToScreenEx(vec(p),camera,int(width),int(height));}
}
