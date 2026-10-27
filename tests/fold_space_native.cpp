#include "game.hpp"
#include "flight_scene.hpp"
#include "flight/renderer.hpp"
#include "flight/space_shaders.hpp"
#include "rlgl.h"
#include "space_gpu.hpp"
#include "cga_palette.hpp"
#include "json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <array>
#include <cmath>
using namespace hyperion;namespace hf=hyperion::flight;
void require(bool b,const char* why){if(!b)throw std::runtime_error(why);}
int main(int argc,char** argv){try{
 auto output=std::filesystem::absolute(argc>1?argv[1]:"captures/folds");std::filesystem::create_directories(output);
 SetTraceLogLevel(LOG_WARNING);InitWindow(960,600,"Hyperion / folded space verification");SetTargetFPS(0);double gpuError=verifySpaceGPU(),rasterError=verifySpaceRaster();int nearMeshPixels=verifyNearMeshPrecision();
 hf::Renderer renderer;require(renderer.init(GraphicsQuality::High),"Renderer initialization failed");renderer.freezeVisualTime(42);
 auto frame=LoadRenderTexture(GetRenderWidth(),GetRenderHeight());auto capture=LoadRenderTexture(GetRenderWidth(),GetRenderHeight());auto palette=LoadShaderFromMemory(nullptr,vectorPaletteResolveFragment);
 std::vector<Color> crossingPixels,ordinaryPixels,closePixels;
 nlohmann::json results=nlohmann::json::array();Game g;g.enterFlight();auto& f=g.s.flight;f.docked=false;f.dockedPort=-1;f.savedDock.clear();f.cancelNavigation();f.launchAssist=0;f.ship.gear=false;f.ship.velocity={};f.ship.throttle=0;g.s.economy.docked.clear();
 g.updateFlight(.01);auto a=f.folds[0];auto region=f.nilRegions[0];
 const char* names[]={"trade-fold-high","trade-fold-cga","nil-pocket-high","nil-pocket-cga","crossing-before","crossing-after","ordinary-flat","ordinary-curved","ordinary-cga","ordinary-ega","ordinary-psychedelic","close-flat","close-curved","ordinary-low"};
 for(int scene=0;scene<14;scene++){
  bool wire=scene==1||scene==3||scene==8||scene==9;bool crossing=scene==4||scene==5;bool ordinary=scene>=6;bool near=scene==11||scene==12;bool ega=scene==9;bool flat=scene==6||scene==11;auto quality=crossing||scene==13?GraphicsQuality::Low:GraphicsQuality::High;renderer.curvedSpace(!flat);
  if(scene<2||scene==4){f.ship.position=a.position+a.normal*(crossing?1.:210.);f.orient(-a.normal,a.up);}
  if(scene==2||scene==3){f.ship.position=region.position;f.orient(hf::rotate(region.axis,region.up,.68),region.up);}
  if(scene==5){f.ship.position=a.position-a.normal;require(f.crossFold(0),"Native crossing failed");g.processFlightEvents();}
  if(ordinary){const auto& body=f.planets[2];auto n=hf::unit(hf::V3{.6,.7,-.4}),side=hf::unit(hf::cross(n,{0,1,0}));f.ship.position=body.centre+n*(body.radius+(near?20:1800));f.orient(-n+side*.2,{0,1,0});}
  auto sceneView=flightScene(g);renderer.prepare(f,sceneView,false,!crossing,quality,scene==2||scene==10,wire,ega);
  auto stats=renderer.foldStats();require(scene>=2||stats.windows>0,"No destination window rendered");require(scene!=2&&scene!=3||stats.nilBody==region.body,"Nil region did not activate its real planet");
  BeginTextureMode(frame);ClearBackground(BLACK);renderer.present(float(GetRenderWidth()),float(GetRenderHeight()));EndTextureMode();
  BeginTextureMode(capture);ClearBackground(BLACK);if(wire){float v=ega?1.f:0.f;SetShaderValue(palette,GetShaderLocation(palette,"ega"),&v,SHADER_UNIFORM_FLOAT);BeginShaderMode(palette);}DrawTexturePro(frame.texture,{0,0,float(frame.texture.width),-float(frame.texture.height)},{0,0,float(capture.texture.width),float(capture.texture.height)},{0,0},0,WHITE);if(wire)EndShaderMode();EndTextureMode();
  BeginDrawing();ClearBackground(BLACK);DrawTexturePro(capture.texture,{0,0,float(capture.texture.width),-float(capture.texture.height)},{0,0,float(GetScreenWidth()),float(GetScreenHeight())},{0,0},0,WHITE);EndDrawing();
  auto image=LoadImageFromTexture(capture.texture);ImageFlipVertical(&image);auto* pixels=LoadImageColors(image);require(pixels,"Native pixel readback failed");
  double sum=0,sum2=0;size_t count=0;for(int y=image.height*3/10;y<image.height*7/10;y++)for(int x=image.width*4/10;x<image.width*6/10;x++){auto c=pixels[y*image.width+x];double l=(c.r+c.g+c.b)/3.;sum+=l;sum2+=l*l;count++;}
  double variance=sum2/count-std::pow(sum/count,2);require(variance>(ordinary?5:60),"The destination or Nil image is blank or flat");
  if(wire)for(int i=0;i<image.width*image.height;i++){auto c=pixels[i];require(wirePaletteIndex(ega?WirePalette::EGA:WirePalette::CGA,c.r,c.g,c.b)>=0,"A curved-space pixel escaped its selected palette");}
  double seamError=-1;if(scene==4)crossingPixels.assign(pixels,pixels+image.width*image.height);
  if(scene==5){require(crossingPixels.size()==size_t(image.width*image.height),"Crossing images disagree in size");seamError=0;for(size_t i=0;i<crossingPixels.size();i++)seamError+=std::abs(int(pixels[i].r)-crossingPixels[i].r)+std::abs(int(pixels[i].g)-crossingPixels[i].g)+std::abs(int(pixels[i].b)-crossingPixels[i].b);seamError/=crossingPixels.size()*3.;require(seamError<12,"The visible destination does not match the real post-crossing view");}
  double viewError=-1;auto compare=[&](const std::vector<Color>& before){require(before.size()==size_t(image.width*image.height),"View comparison size disagrees");double difference=0;for(size_t i=0;i<before.size();i++)difference+=std::abs(int(before[i].r)-pixels[i].r)+std::abs(int(before[i].g)-pixels[i].g)+std::abs(int(before[i].b)-pixels[i].b);return difference/(before.size()*3.);};
  if(scene==6)ordinaryPixels.assign(pixels,pixels+image.width*image.height);
  if(scene==7){viewError=compare(ordinaryPixels);require(renderer.spaceStats().strength>.95&&viewError>5,"Normal flight is still flat or visually unchanged");}
  if(scene==11)closePixels.assign(pixels,pixels+image.width*image.height);
  if(scene==12){viewError=compare(closePixels);require(renderer.spaceStats().strength==0&&viewError<.1,"Close landing view is not stable and readable");}
  UnloadImageColors(pixels);require(ExportImage(image,(output/(std::string(names[scene])+".png")).string().c_str()),"Native capture export failed");UnloadImage(image);
  results.push_back({{"scene",names[scene]},{"destinationViews",stats.windows},{"mouths",stats.mouths},{"nilBody",stats.nilBody},{"quality",quality==GraphicsQuality::Low?"low":"high"},{"wireframe",wire},{"centreVariance",variance},{"crossingMeanAbsoluteError",seamError},{"curvature",renderer.spaceStats().strength},{"comparisonError",viewError},{"ega",ega},{"gpuProjectionErrorKm",gpuError},{"gpuRasterErrorPixels",rasterError},{"nearMeshDifferencePixels",nearMeshPixels}});
 }
 {std::ofstream out(output/"results.json");out<<results.dump(2)<<'\n';}
 UnloadShader(palette);UnloadRenderTexture(capture);UnloadRenderTexture(frame);renderer.shutdown();CloseWindow();std::cout<<results.dump(2)<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"Fold native check failed: "<<e.what()<<'\n';return 1;}}
