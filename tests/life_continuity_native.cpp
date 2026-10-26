#include "game.hpp"
#include "flight_scene.hpp"
#include "flight/renderer.hpp"
#include "flight/life_visibility.hpp"
#include "economy_storage.hpp"
#include "json.hpp"
#include "raylib.h"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace hyperion;
namespace hf=hyperion::flight;
int main(int argc,char**argv){try{
 std::filesystem::path out=std::filesystem::absolute(argc>1?argv[1]:"captures/life-continuity");std::filesystem::create_directories(out);
 bool low=argc>2&&std::string(argv[2])=="low";
 Game game;game.reset("MERIDIAN-0001");game.ensureEconomy();game.system("HOME");game.s.selectedPlanet=2;game.enterFlight();game.s.hours=2.5;
 auto planet=game.universe.planet("HOME",2);V2 chart{20,20};bool found=false;
 for(int x=-8;x<=8&&!found;x++)for(int y=-8;y<=8;y++){V2 at{(x+.5)*64,(y+.5)*64};auto region=game.universe.region(planet,at);if(region.elevation>game.universe.seaLevel(planet)+.04&&region.biomass>.09){chart=at;found=true;break;}}
 auto& f=game.s.flight;const auto& world=f.planets[2];auto n=game.surfaceNormal(chart);f.ship.position=world.centre+n*(world.radius+hf::terrain(world,n)+12);f.docked=false;f.dockedPort=-1;f.savedDock.clear();f.launchAssist=0;f.cancelNavigation();f.ship.velocity={};f.ship.throttle=0;game.s.economy.docked.clear();
 auto initial=flightScene(game);auto it=std::find_if(initial.organisms.begin(),initial.organisms.end(),[](const auto&a){return a.flora&&a.visibility>.99&&a.size>1;});if(it==initial.organisms.end())throw std::runtime_error("Missing approach cohort");auto target=*it;auto up=hf::unit(target.up),east=hf::unit(hf::cross({0,1,0},up),{1,0,0});auto canonical=encodeEconomy(game.s.economy);double hours=game.s.hours;
 SetConfigFlags(FLAG_WINDOW_HIGHDPI|FLAG_MSAA_4X_HINT);InitWindow(960,600,"Hyperion / life continuity check");SetTargetFPS(0);hf::Renderer renderer;if(!renderer.init())throw std::runtime_error("Renderer initialization failed");renderer.freezeVisualTime(130);
 nlohmann::json frames=nlohmann::json::array();double previous=0,maxJump=0,previousDetail=0,maxDetailJump=0;int visibleFrames=0;hf::LifeVisibilityBudget budget;
 for(int frame=0;frame<480;frame++){
  double range=frame<240?320.-312.*frame/239.:8.+312.*(frame-240)/239.;f.ship.position=target.position+up*(target.size+8)-east*range;f.orient(target.position+up*target.size-f.ship.position,up);
  auto scene=flightScene(game);auto draws=hf::lifeDrawList(scene,f.ship.position,f.ship.forward,false,low,&budget);double coverage=0,detail=0;for(const auto& d:draws)if(d.organism->id==target.id){coverage=d.coverage;detail=d.detail;}
  if(frame){maxJump=std::max(maxJump,std::abs(coverage-previous));if(coverage>.01&&previous>.01)maxDetailJump=std::max(maxDetailJump,std::abs(detail-previousDetail));}previous=coverage;previousDetail=detail;visibleFrames+=coverage>.01;
  BeginDrawing();ClearBackground(BLACK);renderer.prepare(f,scene,false,false,low);renderer.present(float(GetScreenWidth()),float(GetScreenHeight()));EndDrawing();
  frames.push_back({{"frame",frame},{"range",range},{"candidates",scene.organisms.size()},{"draws",renderer.visibleOrganisms()},{"targetCoverage",coverage},{"targetDetail",detail}});
  if(frame%60==0||frame==200||frame==210||frame==220||frame==239||frame==479){Image image=LoadImageFromScreen();bool saved=ExportImage(image,(out/("approach-"+std::to_string(frame)+".jpg")).c_str());UnloadImage(image);if(!saved)throw std::runtime_error("Approach capture failed");}
 }
 renderer.shutdown();CloseWindow();
 if(encodeEconomy(game.s.economy)!=canonical||game.s.hours!=hours)throw std::runtime_error("Presentation changed simulation");
 nlohmann::json result={{"frames",frames},{"maxCoverageStep",maxJump},{"maxDetailStep",maxDetailJump},{"target",target.id},{"visibleFrames",visibleFrames},{"economyAndClockUnchanged",true},{"lowGraphics",low}};std::ofstream(out/"approach.json")<<result.dump(2)<<'\n';if(visibleFrames<100||maxJump>.10||maxDetailJump>.10)throw std::runtime_error("Approach coverage or LOD discontinuity");
 std::cout<<"PASS: 480-frame approach and departure / max coverage step "<<maxJump<<" / max LOD step "<<maxDetailJump<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
