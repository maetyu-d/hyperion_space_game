#include "game.hpp"
#include "flight_scene.hpp"
#include "flight/renderer.hpp"
#include "economy_storage.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace hyperion;namespace hf=hyperion::flight;
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
int target(Game& g,const std::string& id){g.prepareWorldObjects();for(int i=0;i<int(g.s.flight.worldObjects.size());i++)if(g.s.flight.worldObjects[i].id==id)return i;throw std::runtime_error("Missing native target");}
void free(Game& g){auto& f=g.s.flight;f.docked=false;f.dockedPort=-1;f.savedDock.clear();f.deployed=false;f.launchAssist=0;f.cancelNavigation();f.ship.velocity={};f.ship.throttle=0;g.s.economy.docked.clear();}
int main(int argc,char** argv){try{
 auto output=std::filesystem::absolute(argc>1?argv[1]:"captures/recovery");std::filesystem::create_directories(output);std::string profile=argc>2?argv[2]:"high";SetTraceLogLevel(LOG_WARNING);SetConfigFlags(FLAG_WINDOW_HIGHDPI|FLAG_MSAA_4X_HINT);InitWindow(1100,700,"Hyperion / tactical recovery");SetTargetFPS(0);hf::Renderer renderer;check(renderer.init(),"Renderer failed");renderer.freezeVisualTime(130);nlohmann::json report=nlohmann::json::array();const char* names[]={"side-breach","live-station","shoot-debris","cut-equipment","ship-recorder","tow-wing","reconstruction"};
 for(int scene=0;scene<7;scene++){
  Game g;g.enterFlight();auto& e=g.s.economy;auto& f=g.s.flight;auto& site=e.settlements.front();site.owner=scene==1?1:0;site.stock.fill(1000);site.built[Reactor]=site.built[Warehouse]=1;g.ensureFlight();e.missions.clear();e.missionClock.clear();e.captains.clear();e.pirates.clear();f.traffic.clear();free(g);bool orbit=scene==1||scene==4||scene==5;auto port=f.ports[g.flightPort(site.id,orbit)];f.ship.position=port.position+port.normal*65;std::string id=scene==1?portObjectId(port.id):scene==5?propObjectId(port.id,"panel",0):facilityObjectId(site.id,scene==3||scene==6?Warehouse:Reactor,0);int index=target(g,id);auto object=f.worldObjects[index];double distance=orbit?300:scene==3?50:60;f.ship.position=object.position+object.right*distance+object.back*distance*.10+object.up*distance*.35;f.orient(object.position-f.ship.position,object.up);f.ship.previous=f.ship.position;
  Stock kit{};kit[Alloy]=100;kit[Components]=40;kit[Crystal]=20;g.storeShipStock(kit);e.credits=100000;
  for(int frame=0;frame<70;frame++){
   if(frame==5){index=target(g,id);object=f.worldObjects[index];if(scene==4){hf::Traffic t;t.id="native-recorder";t.kind=1;t.health=1;t.armour=0;t.faction=2;t.home=g.flightPort(site.id,true);t.position=object.position;t.forward=hf::unit(hf::V3{.2,.1,-1});t.genome={1,712345,false};f.traffic.push_back(t);f.damageTraffic(int(f.traffic.size()-1),100,-1,t.position,true);g.processFlightEvents();for(const auto& [key,d]:e.worldDamage)if(d.recovery.shipId==t.id)id=key;}
    else{auto point=scene==0||scene==2?object.position+object.right*object.half.x*.9:object.position;f.damageWorld(index,scene==0||scene==2?object.capacity*.31:1e8,-1,point,scene!=1,"environment");g.processFlightEvents();}
   }
   if(frame==20&&(scene==3||scene==5||scene==6)){g.openStrategy();free(g);auto& d=e.worldDamage.at(id);f.ship.position=d.position+d.normal*45;if(scene==6)g.recoveryAction("recoveryRebuild",id);else{g.recoveryAction("recoveryCut",id);if(scene==5){free(g);auto& wreck=e.worldDamage.at(id);f.ship.position=wreck.position+wreck.normal*45;e.vessel.fitted[hf::FlightDrive]=1;f.ship.vessel=e.vessel;g.recoveryAction("recoveryTow",id);check(e.worldDamage.at(id).recovery.towed,"Native tow attachment failed");}}g.s.mode=Mode::Flight;}
   if(frame==25&&scene==2){g.prepareWorldObjects();for(int i=0;i<int(f.worldObjects.size());i++)if(f.worldObjects[i].kind==DebrisObject){auto part=f.worldObjects[i];f.damageWorld(i,1000,-1,part.position,true);g.processFlightEvents();break;}}
   if(frame>5){g.s.hours+=.1/55;g.prepareWorldObjects();g.updateRecovery(.1);g.processFlightEvents();}hf::Input input;f.prepareCombat(.1,input);f.time+=.1;g.syncWorldDamage();
   auto canonical=encodeEconomy(e);auto hours=g.s.hours;BeginDrawing();ClearBackground(BLACK);renderer.prepare(f,flightScene(g),profile=="reduced",false,profile=="low");renderer.present(float(GetScreenWidth()),float(GetScreenHeight()));EndDrawing();check(g.s.hours==hours&&canonical==encodeEconomy(e),"Rendering mutated recovery state");auto geometry=renderer.architectureStats();check(geometry.invalidVertices==0&&geometry.invalidNormals==0,"Recovery geometry invalid");
   if(frame==4||frame==15||frame==35||frame==69){auto image=LoadImageFromScreen();check(ExportImage(image,(output/(std::to_string(scene)+"-"+names[scene]+"-"+std::to_string(frame)+".jpg")).c_str()),"Capture failed");UnloadImage(image);}
  }
  if(scene==0||scene==2)check(e.worldDamage.at(id).integrity>0&&hf::hasDetachedSections(e.worldDamage.at(id).structure),"Partial section damage became whole destruction");if(scene==1)check(e.worldDamage.at(id).recovery.failed,"Delayed failure never resolved");if(scene==2)check(e.worldDamage.at(id).structure.cleared>0,"Destroyed piece remains active");if(scene==6)check(!e.worldDamage.count(id)&&facilityIntegrity(site,Warehouse,0)==1,"Native reconstruction failed");
  if(scene==0||scene==1||scene==4||scene==5){std::string error;auto file=output/(std::string(names[scene])+".json");check(saveState(g.s,file.string(),error),"Native recovery save failed");State loaded;check(loadState(loaded,file.string(),error),error.c_str());check(encodeEconomy(loaded.economy)==encodeEconomy(e),"Native recovery save changed canonical state");}
  auto stats=renderer.structuralStats();report.push_back({{"scene",scene},{"name",names[scene]},{"frames",70},{"profile",profile},{"objects",stats.objects},{"parts",stats.parts},{"sourceTriangles",stats.sourceTriangles},{"triangles",stats.triangles},{"caps",stats.caps},{"sourceArea",stats.sourceArea},{"partArea",stats.partArea}});std::cout<<"PASS "<<names[scene]<<'\n'<<std::flush;
 }
 renderer.shutdown();CloseWindow();std::ofstream(output/"recovery.json")<<report.dump(2)<<'\n';std::cout<<"PASS 7 native tactical recovery scenarios / 490 frames / "<<profile<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
