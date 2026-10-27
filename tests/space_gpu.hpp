#pragma once
#include "flight/space.hpp"
#include "flight/space_shaders.hpp"
#include "raylib.h"
#include "rlgl.h"
#include <array>
#include <stdexcept>
// Evaluate the production GLSL map into a float framebuffer and compare with
// the CPU map used by target markers. This catches drift between both paths.
inline double verifySpaceGPU(){
 namespace hf=hyperion::flight;auto view=hf::spaceView({12500,700,-9000},42);constexpr int count=24;std::array<Vector3,count> samples{};
 for(int i=0;i<count;i++){auto d=i<2?hf::V3{double(i)*200,0,0}:hf::V3{std::cos(i*.7)*21000,std::sin(i*.4)*13000,std::sin(i*.7)*21000};auto p=view.eye+d;samples[i]={float(p.x),float(p.y),float(p.z)};}
 std::string source="#version 330\n";source+=hf::spaceGLSL;source+="\nuniform vec3 hxSamples[24];out vec4 finalColor;void main(){finalColor=vec4(hxPoint(hxSamples[int(gl_FragCoord.x)])-hxEye,1.);}";
 auto shader=LoadShaderFromMemory(nullptr,source.c_str());if(!IsShaderValid(shader)||shader.id==rlGetShaderIdDefault())throw std::runtime_error("Curvature audit shader failed");
 auto scalar=[&](const char* name,float value){SetShaderValue(shader,GetShaderLocation(shader,name),&value,SHADER_UNIFORM_FLOAT);};
 auto vector=[&](const char* name,hf::V3 v){Vector3 value{float(v.x),float(v.y),float(v.z)};SetShaderValue(shader,GetShaderLocation(shader,name),&value,SHADER_UNIFORM_VEC3);};
 scalar("hxStrength",1);scalar("hxRadius",float(view.radius));vector("hxEye",view.eye);vector("hxX",view.x);vector("hxY",view.y);vector("hxZ",view.z);SetShaderValueV(shader,GetShaderLocation(shader,"hxSamples[0]"),samples.data(),SHADER_UNIFORM_VEC3,count);
 auto target=LoadRenderTexture(count,1);UnloadTexture(target.texture);target.texture={rlLoadTexture(nullptr,count,1,PIXELFORMAT_UNCOMPRESSED_R32G32B32A32,1),count,1,1,PIXELFORMAT_UNCOMPRESSED_R32G32B32A32};rlEnableFramebuffer(target.id);rlFramebufferAttach(target.id,target.texture.id,RL_ATTACHMENT_COLOR_CHANNEL0,RL_ATTACHMENT_TEXTURE2D,0);rlDisableFramebuffer();
 BeginTextureMode(target);ClearBackground(BLANK);BeginShaderMode(shader);DrawRectangle(0,0,count,1,WHITE);EndShaderMode();EndTextureMode();auto image=LoadImageFromTexture(target.texture);
 if(!image.data||image.format!=PIXELFORMAT_UNCOMPRESSED_R32G32B32A32)throw std::runtime_error("Curvature float readback failed");auto* values=static_cast<float*>(image.data);double error=0;
 for(int i=0;i<count;i++){auto s=samples[i];auto expected=hf::spacePoint(view,{s.x,s.y,s.z})-view.eye;hf::V3 actual{values[i*4],values[i*4+1],values[i*4+2]};if(!hf::finite(actual))throw std::runtime_error("Curvature GPU returned a non-finite position");error=std::max(error,hf::length(actual-expected));}
 UnloadImage(image);UnloadRenderTexture(target);UnloadShader(shader);if(error>.8)throw std::runtime_error("GPU curvature and CPU target projection disagree: "+std::to_string(error));return error;
}

#include "raymath.h"
inline double verifySpaceRaster(){
 namespace hf=hyperion::flight;auto view=hf::spaceView({12500,700,-9000},42);auto point=view.eye+hf::V3{14000,4000,-16000};auto image=hf::spacePoint(view,point);auto forward=hf::unit(image-view.eye);auto side=hf::unit(hf::cross(forward,{0,1,0}));forward=hf::unit(forward+side*.16);
 auto vector=[](hf::V3 p){return Vector3{float(p.x),float(p.y),float(p.z)};};Camera3D camera{vector(view.eye),vector(view.eye+forward*1000),{0,1,0},70,CAMERA_PERSPECTIVE};
 auto source=hf::spaceVertex(hf::spacePrimitiveVertex,"vertexPosition");auto shader=LoadShaderFromMemory(source.c_str(),"#version 330\nout vec4 finalColor;void main(){finalColor=vec4(1.,0.,0.,1.);}");if(!IsShaderValid(shader)||shader.id==rlGetShaderIdDefault())throw std::runtime_error("Curved primitive audit failed");
 auto scalar=[&](const char* name,float value){SetShaderValue(shader,GetShaderLocation(shader,name),&value,SHADER_UNIFORM_FLOAT);};auto value=[&](const char* name,hf::V3 v){auto p=vector(v);SetShaderValue(shader,GetShaderLocation(shader,name),&p,SHADER_UNIFORM_VEC3);};scalar("hxStrength",1);scalar("hxRadius",float(view.radius));value("hxEye",view.eye);value("hxX",view.x);value("hxY",view.y);value("hxZ",view.z);
 auto target=LoadRenderTexture(640,400);BeginTextureMode(target);ClearBackground(BLACK);rlSetClipPlanes(1,60000);BeginMode3D(camera);SetShaderValueMatrix(shader,GetShaderLocation(shader,"hxWorldToClip"),MatrixMultiply(rlGetMatrixModelview(),rlGetMatrixProjection()));BeginShaderMode(shader);DrawSphere(vector(point),200,RED);EndShaderMode();EndMode3D();EndTextureMode();
 auto pixels=LoadImageFromTexture(target.texture);ImageFlipVertical(&pixels);auto* colours=LoadImageColors(pixels);double x=0,y=0;int count=0;for(int j=0;j<pixels.height;j++)for(int i=0;i<pixels.width;i++){auto c=colours[j*pixels.width+i];if(c.r>240&&c.g<10&&c.b<10){x+=i+.5;y+=j+.5;count++;}}
 auto expected=GetWorldToScreenEx(vector(image),camera,640,400);double error=count?std::hypot(x/count-expected.x,y/count-expected.y):1e9;UnloadImageColors(colours);UnloadImage(pixels);UnloadRenderTexture(target);UnloadShader(shader);if(error>2)throw std::runtime_error("Curved world raster misses its CPU marker: "+std::to_string(error));return error;
}

#include "flight/shaders.hpp"
inline int verifyNearMeshPrecision(){
 namespace hf=hyperion::flight;auto view=hf::spaceView({12500,700,-9000},42);auto point=view.eye+hf::V3{.4,.2,-2.};auto vector=[](hf::V3 p){return Vector3{float(p.x),float(p.y),float(p.z)};};Camera3D camera{vector(view.eye),vector(point+hf::V3{.2,0,0}),{0,1,0},70,CAMERA_PERSPECTIVE};
 auto source=hf::spaceVertex(hf::worldVertex,"worldPosition");auto shader=LoadShaderFromMemory(source.c_str(),"#version 330\nout vec4 finalColor;void main(){finalColor=vec4(1.,0.,0.,1.);}");if(!IsShaderValid(shader)||shader.id==rlGetShaderIdDefault())throw std::runtime_error("Near mesh audit shader failed");
 auto scalar=[&](const char* name,float v){SetShaderValue(shader,GetShaderLocation(shader,name),&v,SHADER_UNIFORM_FLOAT);};auto value=[&](const char* name,hf::V3 p){auto v=vector(p);SetShaderValue(shader,GetShaderLocation(shader,name),&v,SHADER_UNIFORM_VEC3);};scalar("hxRadius",float(view.radius));value("hxEye",view.eye);value("hxX",view.x);value("hxY",view.y);value("hxZ",view.z);
 auto model=LoadModelFromMesh(GenMeshCube(.02,.02,.02));model.materials[0].shader=shader;auto target=LoadRenderTexture(640,400);std::vector<Color> baseline;int changed=0,visible=0;
 for(int pass=0;pass<2;pass++){scalar("hxStrength",float(pass));BeginTextureMode(target);ClearBackground(BLACK);rlSetClipPlanes(.01,60000);BeginMode3D(camera);SetShaderValueMatrix(shader,GetShaderLocation(shader,"hxWorldToClip"),MatrixMultiply(rlGetMatrixModelview(),rlGetMatrixProjection()));DrawModel(model,vector(point),1,RED);EndMode3D();EndTextureMode();auto image=LoadImageFromTexture(target.texture);auto* pixels=LoadImageColors(image);if(!pass)baseline.assign(pixels,pixels+640*400);else for(int i=0;i<640*400;i++){visible+=pixels[i].r>240&&pixels[i].g<10;changed+=pixels[i].r!=baseline[i].r||pixels[i].g!=baseline[i].g||pixels[i].b!=baseline[i].b;}UnloadImageColors(pixels);UnloadImage(image);}
 UnloadModel(model);UnloadRenderTexture(target);UnloadShader(shader);if(changed||!visible)throw std::runtime_error("Curvature changed a locally flat close mesh");return changed;
}
