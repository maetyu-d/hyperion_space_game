#include "game.hpp"
#include "flight_scene.hpp"
#include "flight/renderer.hpp"
#include "cga_palette.hpp"
#include "json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace hyperion;namespace hf=hyperion::flight;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
#include "ring_gpu.hpp"
int main(int argc,char** argv){try{
 auto output=std::filesystem::absolute(argc>1?argv[1]:"captures/rings");std::filesystem::create_directories(output);
 SetTraceLogLevel(LOG_WARNING);InitWindow(960,600,"Hyperion / planet ring verification");SetTargetFPS(0);
 auto sampling=verifyRingSampling(output);{std::ofstream out(output/"sampling.json");out<<sampling.dump(2)<<'\n';}
 hf::Renderer renderer;require(renderer.init(GraphicsQuality::Low),"Ring renderer failed");renderer.freezeVisualTime(42);
 Game g;g.enterFlight();auto& f=g.s.flight;f.docked=false;f.dockedPort=-1;f.cancelNavigation();f.launchAssist=0;f.ship.gear=false;f.ship.throttle=0;f.ship.velocity={};
 f.folds.clear();f.nilRegions.clear();f.ports={hf::Port{"audit harbour","",{1000000,0,0},{0,1,0},-1}};f.target=-1;f.traffic.clear();f.minorBodies.clear();f.effects.clear();
 auto body=f.planets[2];body.atmosphere=0;body.life=0;body.giant=true;body.centre={12000,700,-9000};f.planets={body};
 auto a=hf::V3{1,0,0},b=hf::unit(hf::V3{0,.32,1}),n=hf::unit(hf::cross(a,b));
 const char* names[]={"above-flat","below-flat","edge-flat","above-curved","close-curved","below-curved","psychedelic","cga","ega"};
 auto frame=LoadRenderTexture(960,600);auto palette=LoadShaderFromMemory(nullptr,vectorPaletteResolveFragment);nlohmann::json report=nlohmann::json::array();
 for(int scene=0;scene<9;scene++){
  auto delta=(a*2.4+n*(scene==1||scene==5?-1.7:1.7))*body.radius,target=body.centre;
  if(scene==2)delta=(a*2.6+n*.012)*body.radius;
  if(scene==4){delta=(a*1.52+n*.025)*body.radius;target=body.centre+(a*1.53+b*.25)*body.radius;}
  f.ship.position=body.centre+delta;f.orient(target-f.ship.position,n);f.ship.previous=f.ship.position;renderer.curvedSpace(scene>=3);
  bool wire=scene>=7,ega=scene==8;auto quality=scene==6?GraphicsQuality::High:GraphicsQuality::Low;
  renderer.prepare(f,flightScene(g),false,false,quality,scene==6,wire,ega);
  BeginTextureMode(frame);ClearBackground(BLACK);if(wire){float v=ega?1.f:0.f;SetShaderValue(palette,GetShaderLocation(palette,"ega"),&v,SHADER_UNIFORM_FLOAT);BeginShaderMode(palette);}renderer.present(960,600);if(wire)EndShaderMode();EndTextureMode();
  BeginDrawing();ClearBackground(BLACK);DrawTexturePro(frame.texture,{0,0,960,-600},{0,0,960,600},{0,0},0,WHITE);EndDrawing();
  auto image=LoadImageFromTexture(frame.texture);ImageFlipVertical(&image);auto* pixels=LoadImageColors(image);double brightness=0;int coloured=0;
  for(int i=0;i<960*600;i++){auto c=pixels[i];brightness+=c.r+c.g+c.b;coloured+=std::max({c.r,c.g,c.b})-std::min({c.r,c.g,c.b})>20;if(wire)require(wirePaletteIndex(ega?WirePalette::EGA:WirePalette::CGA,c.r,c.g,c.b)>=0,"Ring view escaped wireframe palette");}
  std::cout<<names[scene]<<" brightness "<<brightness<<" coloured "<<coloured<<std::endl;UnloadImageColors(pixels);require(ExportImage(image,(output/(std::string(names[scene])+".png")).string().c_str()),"Ring screenshot export failed");UnloadImage(image);require(brightness>100000&&coloured>100,"Ring scene is blank");
  report.push_back({{"scene",names[scene]},{"curvature",renderer.spaceStats().strength},{"colouredPixels",coloured},{"quality",scene==6?"high":"low"},{"wireframe",wire}});
 }
 {std::ofstream out(output/"results.json");out<<report.dump(2)<<'\n';}UnloadShader(palette);UnloadRenderTexture(frame);renderer.shutdown();CloseWindow();std::cout<<report.dump(2)<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
