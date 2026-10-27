#pragma once
#include "flight/ring_geometry.hpp"
#include "flight/space_shaders.hpp"
#include "flight/shaders.hpp"
#include "rlgl.h"
#include "raymath.h"
inline nlohmann::json verifyRingSampling(const std::filesystem::path& output){
 namespace hf=hyperion::flight;auto source=hf::spaceVertex(hf::worldVertex,"worldPosition");auto shader=LoadShaderFromMemory(source.c_str(),hf::ringFragment);
 require(IsShaderValid(shader)&&shader.id!=rlGetShaderIdDefault(),"Ring audit shader failed");
 auto vector=[](hf::V3 p){return Vector3{float(p.x),float(p.y),float(p.z)};};auto scalar=[&](const char* name,float v){SetShaderValue(shader,GetShaderLocation(shader,name),&v,SHADER_UNIFORM_FLOAT);};auto value=[&](const char* name,hf::V3 p){auto v=vector(p);SetShaderValue(shader,GetShaderLocation(shader,name),&v,SHADER_UNIFORM_VEC3);};
 std::array<Model,3> models{LoadModelFromMesh(hf::planetRingMesh(256,1)),LoadModelFromMesh(hf::planetRingMesh(256,24)),LoadModelFromMesh(hf::planetRingMesh(512,96))};for(auto& m:models)m.materials[0].shader=shader;
 auto target=LoadRenderTexture(640,400);auto a=hf::V3{1,0,0},b=hf::unit(hf::V3{0,.32,1}),n=hf::planetRingNormal();nlohmann::json report=nlohmann::json::array();
 for(int scene=0;scene<4;scene++){
  auto centre=scene<2?hf::V3{12000,700,-9000}:hf::V3{300000,50000,300000};auto eye=centre+(scene%2?(a*1.52+n*.025):(a*2.4+n*.8))*1000;
  auto aim=centre+(scene%2?(a*1.53+b*.25)*1000:hf::V3{});auto view=hf::spaceView(eye,42);auto direction=hf::unit(hf::spacePoint(view,aim)-eye);
  Camera3D camera{vector(eye),vector(eye+direction*1000),vector(n),70,CAMERA_PERSPECTIVE};
  scalar("hxStrength",1);scalar("hxRadius",float(view.radius));value("hxEye",eye);value("hxX",view.x);value("hxY",view.y);value("hxZ",view.z);
  value("centre",centre);value("eye",eye);value("lightDirection",hf::unit({.6,.7,-.4}));scalar("radius",1000);scalar("seed",10);scalar("time",42);
  std::array<std::vector<Color>,3> images;
  for(int pass=0;pass<3;pass++){
   BeginTextureMode(target);ClearBackground(BLANK);rlSetClipPlanes(.05,60000);BeginMode3D(camera);SetShaderValueMatrix(shader,GetShaderLocation(shader,"hxWorldToClip"),MatrixMultiply(rlGetMatrixModelview(),rlGetMatrixProjection()));rlDisableBackfaceCulling();rlDisableColorBlend();DrawModel(models[pass],vector(centre),1000,WHITE);rlEnableColorBlend();rlEnableBackfaceCulling();EndMode3D();EndTextureMode();
   auto image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);auto* pixels=LoadImageColors(image);images[pass].assign(pixels,pixels+640*400);UnloadImageColors(pixels);require(ExportImage(image,(output/("sampling-"+std::to_string(scene)+"-"+std::to_string(pass)+".png")).string().c_str()),"Ring sampling export failed");UnloadImage(image);
  }
  auto error=[&](int pass){double sum=0;for(size_t i=0;i<images[2].size();i++){auto a=images[pass][i],b=images[2][i];sum+=std::abs(int(a.r)-b.r)+std::abs(int(a.g)-b.g)+std::abs(int(a.b)-b.b)+std::abs(int(a.a)-b.a);}return sum/(images[2].size()*4.);};
  double oldError=error(0),newError=error(1);std::cout<<"ring sampling "<<scene<<" legacy "<<oldError<<" corrected "<<newError<<std::endl;
  report.push_back({{"scene",scene},{"legacyMeanRGBAError",oldError},{"correctedMeanRGBAError",newError}});
  require(newError<1.5,"Curved ring bands disagree with the dense reference");require(oldError<.1||newError<oldError*.55,"Radial subdivision did not improve curved ring accuracy");
 }
 auto opaque=LoadShaderFromMemory(nullptr,"#version 330\nout vec4 finalColor;void main(){finalColor=vec4(0.,1.,0.,1.);}");auto sphere=LoadModelFromMesh(GenMeshSphere(1000,96,192));sphere.materials[0].shader=opaque;
 for(int side:{-1,1}){
  hf::V3 centre{},eye=(a*2.4+n*(side*.6))*1000,forward=hf::unit(-eye),right=hf::unit(hf::cross(forward,n)),up=hf::unit(hf::cross(right,forward));Camera3D camera{vector(eye),{0,0,0},vector(n),70,CAMERA_PERSPECTIVE};
  scalar("hxStrength",0);value("centre",centre);value("eye",eye);std::array<std::vector<Color>,2> images;
  for(int pass=0;pass<2;pass++){
   BeginTextureMode(target);ClearBackground(BLANK);rlSetClipPlanes(1,60000);BeginMode3D(camera);rlDisableColorBlend();if(pass)DrawModel(sphere,{0,0,0},1,WHITE);rlDisableBackfaceCulling();rlDisableDepthMask();DrawModel(models[1],{0,0,0},1000,WHITE);rlEnableDepthMask();rlEnableBackfaceCulling();rlEnableColorBlend();EndMode3D();EndTextureMode();
   auto image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);auto* pixels=LoadImageColors(image);images[pass].assign(pixels,pixels+640*400);UnloadImageColors(pixels);require(ExportImage(image,(output/("occlusion-"+std::to_string(side)+"-"+std::to_string(pass)+".png")).string().c_str()),"Ring occlusion export failed");UnloadImage(image);
  }
  int hidden=0,front=0,leaks=0,missing=0;
  for(int y=0;y<400;y++)for(int x=0;x<640;x++){
   auto ref=images[0][y*640+x],pixel=images[1][y*640+x];if(ref.a<40)continue;
   auto ray=hf::unit(forward+right*((2*(x+.5)/640-1)*1.6*.7002075)+up*((1-2*(y+.5)/400)*.7002075));double plane=-hf::dot(eye,n)/hf::dot(ray,n),along=hf::dot(eye,ray),h=along*along-hf::dot(eye,eye)+1000000;
   if(plane<=0||h<20000)continue;double surface=-along-std::sqrt(h);if(surface<=0)continue;
   if(plane>surface+5){hidden++;leaks+=pixel.g<250||pixel.r>2||pixel.b>2;}else if(plane<surface-5){front++;missing+=pixel.g>250&&pixel.r<2&&pixel.b<2;}
  }
  std::cout<<"ring occlusion "<<side<<" hidden "<<hidden<<" front "<<front<<" leaks "<<leaks<<" missing "<<missing<<std::endl;
  report.push_back({{"side",side},{"farRingPixelsHidden",hidden},{"nearRingPixelsVisible",front},{"ringLeaks",leaks},{"missingNearRingPixels",missing}});
  require(hidden>300&&front>300&&leaks==0&&missing==0,"Planet rings fail front/back depth ordering");
 }
 UnloadModel(sphere);UnloadShader(opaque);for(auto& m:models)UnloadModel(m);UnloadShader(shader);UnloadRenderTexture(target);return report;
}
