#include "game.hpp"
#include "flight_scene.hpp"
#include "flight/renderer.hpp"
#include "economy_storage.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace hyperion;namespace hf=hyperion::flight;
namespace {
void require(bool b,const std::string& why){if(!b)throw std::runtime_error(why);}
void free(Game& g){auto& f=g.s.flight;f.docked=f.deployed=false;f.dockedPort=-1;f.savedDock.clear();f.cancelNavigation();f.launchAssist=0;f.ship.velocity={};f.ship.throttle=0;g.s.economy.docked.clear();}
std::string setup(Game& g,bool station){g.enterFlight();auto& e=g.s.economy;auto& site=e.settlements.front();site.owner=1;site.stock.fill(30);site.built[Reactor]=1;g.ensureFlight();e.missions.clear();e.missionClock.clear();e.captains.clear();e.pirates.clear();g.s.flight.traffic.clear();auto port=g.s.flight.ports[g.flightPort(site.id,station)];free(g);g.s.flight.ship.position=port.position+port.normal*100;Stock kit{};kit[Components]=30;kit[Crystal]=10;kit[Medicine]=8;g.storeShipStock(kit);e.credits=100000;return station?portObjectId(port.id):facilityObjectId(site.id,Reactor,0);}
WorldObject object(Game& g,const std::string& id){g.prepareWorldObjects();for(auto o:g.s.flight.worldObjects)if(o.id==id)return o;throw std::runtime_error("Missing graphics fixture");}
void hit(Game& g,const std::string& id,double amount,hf::V3 point){g.prepareWorldObjects();for(int i=0;i<int(g.s.flight.worldObjects.size());i++)if(g.s.flight.worldObjects[i].id==id){g.s.flight.damageWorld(i,amount,-1,point,false,"environment");g.processFlightEvents();return;}}
std::string boarding(Game& g){setup(g,true);auto& e=g.s.economy;Captain c;c.id="native-board";c.name="Vesper / disputed patrol";c.star=g.s.currentStar;c.home=e.settlements.front().id;c.faction=1;c.role=2;c.genome={1,231143,false};c.hull=70;c.modules={0,100,10};c.shield=0;e.captains.push_back(c);g.refreshSecurity();auto& t=g.s.flight.traffic.back();t.position=t.previous=g.s.flight.ship.position+g.s.flight.ship.forward*30;t.positioned=true;t.forward=-g.s.flight.ship.forward;t.velocity={};g.s.flight.combatTarget=t.id;g.openStrategy();return t.id;}
}
int main(int argc,char** argv){try{
 auto output=std::filesystem::absolute(argc>1?argv[1]:"captures/society");std::filesystem::create_directories(output);std::string profile=argc>2?argv[2]:"high";SetTraceLogLevel(LOG_WARNING);SetConfigFlags(FLAG_WINDOW_HIGHDPI|FLAG_MSAA_4X_HINT);InitWindow(1100,700,"Hyperion / living recovery");SetTargetFPS(0);hf::Renderer renderer;require(renderer.init(),"Renderer initialization failed");renderer.freezeVisualTime(130);nlohmann::json report=nlohmann::json::array();const char* names[]={"exposed-machinery","rescue-vessel","boarding-clamps","captured-hull","responder-loss"};
 for(int scene=0;scene<5;scene++){
  Game g;std::string id;hf::V3 focus,up{0,1,0};
  if(scene==0){id=setup(g,false);auto o=object(g,id);auto hitAt=o.position+o.right*o.half.x*.9;hit(g,id,o.capacity*.31,hitAt);focus=o.position;up=o.up;g.s.flight.ship.position=o.position+o.right*28+o.back*8+o.up*12;g.s.flight.orient(focus-g.s.flight.ship.position,up);}
  else if(scene==1||scene==4){id=setup(g,true);auto o=object(g,id);hit(g,id,1e8,o.position);g.s.hours=g.s.economy.hour=2.01;g.updateRecovery(0);require(!g.s.economy.recoveryCrews.empty(),"Native responder was not funded");id=g.s.economy.recoveryCrews.front().id;focus=g.s.economy.recoveryCrews.front().position;up=o.up;
   if(scene==4){int index=-1;for(int i=0;i<int(g.s.flight.traffic.size());i++)if(g.s.flight.traffic[i].id==id)index=i;require(index>=0,"Native responder missing from combat");g.s.flight.damageTraffic(index,10000,-1,focus,false,0,"pirate");g.processFlightEvents();}
  }else{id=boarding(g);g.action("boardingBegin",id);require(g.boardingActive(),"Native boarding did not attach");focus=g.s.flight.traffic.back().position;
   if(scene==3){g.action("boardingPower",id);g.action("boardingNegotiate",id);g.action("boardingCapture",id);require(!g.s.economy.prizes.empty(),"Native capture failed");id=g.s.economy.prizes.front().source;focus=g.s.economy.worldDamage.at(id).position;}
  }
  for(int frame=0;frame<50;frame++){
   if(scene==1){g.s.hours+=.004;g.s.economy.hour=g.s.hours;g.updateRecovery(0);auto c=std::find_if(g.s.economy.recoveryCrews.begin(),g.s.economy.recoveryCrews.end(),[&](const auto& c){return c.id==id;});require(c!=g.s.economy.recoveryCrews.end(),"Responder disappeared");focus=c->position;}
   else{g.s.hours+=.1/55;g.s.economy.hour=g.s.hours;g.updateRecovery(0);}
   auto& f=g.s.flight;f.time+=.1;hf::Input input;f.prepareCombat(.1,input);g.syncWorldDamage();
   if(scene!=0){auto right=hf::unit(hf::cross(up,{0,0,1}),{1,0,0}),back=hf::unit(hf::cross(right,up));f.ship.position=focus+right*(scene==4?38:35)+up*(scene==4?15:9)+back*26;f.orient(focus-f.ship.position,up);}
   auto before=encodeEconomy(g.s.economy);double hour=g.s.hours;BeginDrawing();ClearBackground(BLACK);renderer.prepare(f,flightScene(g),profile=="reduced",false,profile=="low");renderer.present(float(GetScreenWidth()),float(GetScreenHeight()));EndDrawing();if(scene==4)require(renderer.breakupStats().hulls==1,"Responder destruction draws a duplicate hull during its explosion");auto stats=renderer.architectureStats();require(stats.invalidVertices==0&&stats.invalidNormals==0,"Interior geometry invalid");require(g.s.hours==hour&&encodeEconomy(g.s.economy)==before,"Rendering changed recovery society");
   if(frame==10||frame==30||frame==49){auto image=LoadImageFromScreen();require(ExportImage(image,(output/(std::to_string(scene)+"-"+names[scene]+"-"+std::to_string(frame)+".jpg")).c_str()),"Capture export failed");UnloadImage(image);}
  }
  if(scene==4)require(renderer.breakupStats().hulls==1,"Responder destruction draws a duplicate hull");if(scene==0)require(renderer.structuralStats().interiors>0,"Cut faces have no visible machinery");if(scene!=4){g.s.mode=Mode::System;std::string error;auto file=output/(std::string(names[scene])+".json");require(saveState(g.s,file.string(),error),error);State loaded;require(loadState(loaded,file.string(),error),error);require(encodeEconomy(loaded.economy)==encodeEconomy(g.s.economy),"Native society save changed authoritative state");}
  auto stats=renderer.structuralStats();report.push_back({{"scene",scene},{"name",names[scene]},{"profile",profile},{"frames",50},{"interiorParts",stats.interiors},{"sourceArea",stats.sourceArea},{"partArea",stats.partArea}});std::cout<<"PASS "<<names[scene]<<'\n'<<std::flush;
 }
 renderer.shutdown();CloseWindow();std::ofstream(output/"society.json")<<report.dump(2)<<'\n';std::cout<<"PASS 5 native recovery society scenes / 250 frames / "<<profile<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
