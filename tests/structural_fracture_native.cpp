#include "game.hpp"
#include "flight_scene.hpp"
#include "flight/renderer.hpp"
#include "economy_storage.hpp"
#include "raylib.h"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace hyperion;namespace hf=hyperion::flight;
namespace {void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}int target(Game& g,const std::string& id){g.prepareWorldObjects();for(int i=0;i<int(g.s.flight.worldObjects.size());i++)if(g.s.flight.worldObjects[i].id==id)return i;throw std::runtime_error("Missing structural target "+id);}}
int main(int argc,char** argv){try{
 auto output=std::filesystem::absolute(argc>1?argv[1]:"captures/structural-fracture");std::filesystem::create_directories(output);std::string profile=argc>2?argv[2]:"high";
 SetConfigFlags(FLAG_WINDOW_HIGHDPI|FLAG_MSAA_4X_HINT);InitWindow(1100,700,"Hyperion / architectural destruction");SetTargetFPS(0);hf::Renderer renderer;check(renderer.init(),"Renderer init failed");renderer.freezeVisualTime(130);nlohmann::json report=nlohmann::json::array();
 for(int scene=0;scene<25;scene++){
  Game g;g.enterFlight();auto& e=g.s.economy;auto& f=g.s.flight;auto& site=e.settlements.front();site.owner=scene<14?scene%5:scene<19?scene-14:0;for(auto& count:site.built)count=std::max(1,count);e.geometryRevision++;auto siteId=site.id;g.ensureFlight();bool orbit=scene>=14&&scene<=19;int p=g.flightPort(siteId,orbit);f.docked=false;f.dockedPort=-1;f.savedDock.clear();f.cancelNavigation();f.launchAssist=0;e.docked.clear();f.traffic.clear();f.events.clear();f.ship.velocity={};f.ship.throttle=0;auto port=f.ports[p];f.ship.position=port.position+port.normal*70;std::string name,id;
  if(scene<14){name=buildings[scene].name;id=facilityObjectId(siteId,scene,0);}else if(scene<19){name="station-"+std::to_string(scene-14);id=portObjectId(port.id);}else if(scene==19){name="solar-wing";id=propObjectId(port.id,"panel",0);}else if(scene==20){name="landing-pad";id=portObjectId(port.id);}else if(scene==21){name="loading-gantry";id=propObjectId(port.id,"gantry",0);}else if(scene==22){name="mast";id=propObjectId(port.id,"mast",0);}else if(scene==23){name="terminal";id=propObjectId(port.id,"terminal",0);}else{name="cargo";id=propObjectId(port.id,"cargo",0);}
  int index=target(g,id);auto object=f.worldObjects[index];auto up=object.up,right=object.right,back=object.back;double distance=orbit?340:scene==20?110:scene==21?78:58;auto aim=orbit?object.position+up*15:object.position;f.ship.position=aim+right*(distance*.75)+back*(distance*.9)+up*distance*.3;f.orient(aim-f.ship.position,up);f.ship.previous=f.ship.position;
  hf::Renderer::StructuralStats stats{};double cpu=0;int captured=0;
  for(int frame=0;frame<65;frame++){
   if(frame==5){index=target(g,id);f.damageWorld(index,1e8,-1,object.position,true);g.processFlightEvents();check(e.worldDamage.at(id).structure.valid,"Native damage did not capture architecture");}
   if(frame==35&&scene==11){std::string error;auto file=output/"fractured-campaign.json";check(saveState(g.s,file.string(),error),"Structural save failed");State loaded;check(loadState(loaded,file.string(),error),"Structural reload failed");check(encodeEconomy(loaded.economy)==encodeEconomy(e),"Reload lost structural identity");}
   hf::Input input;f.prepareCombat(.13,input);f.time+=.13;g.syncWorldDamage();auto canonical=frame==5?encodeEconomy(e):nlohmann::json();double hours=g.s.hours;double start=GetTime();
   BeginDrawing();ClearBackground(BLACK);renderer.prepare(f,flightScene(g),profile=="reduced",false,profile=="low");renderer.present(float(GetScreenWidth()),float(GetScreenHeight()));EndDrawing();cpu+=GetTime()-start;
   if(frame>=5){stats=renderer.structuralStats();check(stats.objects>0&&stats.parts>=4&&stats.cached<=48&&stats.caps>0,"No real structural fragments rendered");check(std::abs(stats.sourceArea-stats.partArea)<std::max(.01,stats.sourceArea*.00002),"Native architecture lost exterior area");}
   auto geometry=renderer.architectureStats();check(geometry.invalidVertices==0&&geometry.invalidNormals==0,"Native fragments have invalid geometry");
   check(hours==g.s.hours,"Destruction renderer advanced strategic clock");if(frame==5)check(canonical==encodeEconomy(e),"Renderer mutated canonical destruction state");
   if((scene==0||scene==10||scene==11||scene==16||scene==19||scene==21)&&(frame==4||frame==13||frame==36||frame==64)){auto image=LoadImageFromScreen();check(ExportImage(image,(output/(std::to_string(scene)+"-"+std::to_string(frame)+".jpg")).c_str()),"Capture failed");UnloadImage(image);captured++;}
  }
  if(scene==11){site.owner=0;Stock stock{};stock.fill(1000);e.credits=100000;std::string message;check(construct(e,site,11,stock,message),"Rebuild failed");g.syncWorldDamage();BeginDrawing();renderer.prepare(f,flightScene(g),profile=="reduced",false,profile=="low");EndDrawing();check(!e.worldDamage.count(id)&&renderer.structuralStats().objects==0,"Rebuilt building retains fragments");}
  report.push_back({{"scene",scene},{"name",name},{"frames",65},{"objects",stats.objects},{"parts",stats.parts},{"cached",stats.cached},{"sourceTriangles",stats.sourceTriangles},{"fracturedTriangles",stats.triangles},{"capTriangles",stats.caps},{"sourceArea",stats.sourceArea},{"fracturedArea",stats.partArea},{"meanFrameMs",cpu*1000/65},{"captures",captured},{"profile",profile}});std::cout<<"PASS scene "<<scene<<" / "<<name<<" / "<<stats.parts<<" fragments\n"<<std::flush;
 }
 renderer.shutdown();CloseWindow();std::ofstream(output/"structural-fracture.json")<<report.dump(2)<<'\n';std::cout<<"PASS: 25 native structural scenarios / 1625 frames / "<<profile<<" / no player saves touched\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
