#include "game.hpp"
#include "flight_scene.hpp"
#include "flight/renderer.hpp"
#include "flight/ground_objects.hpp"
#include "economy_storage.hpp"
#include "json.hpp"
#include "raylib.h"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace hyperion;namespace hf=hyperion::flight;
namespace {
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
void undock(Game& game){auto& f=game.s.flight;f.docked=false;f.dockedPort=-1;f.savedDock.clear();f.cancelNavigation();f.launchAssist=0;f.ship.velocity={};f.ship.throttle=0;game.s.economy.docked.clear();f.traffic.clear();f.events.clear();}
int target(Game& g,const std::string& id){g.prepareWorldObjects();for(int i=0;i<int(g.s.flight.worldObjects.size());i++)if(g.s.flight.worldObjects[i].id==id)return i;throw std::runtime_error("Native fixture lacks "+id);}
}
int main(int argc,char** argv){try{
 auto output=std::filesystem::absolute(argc>1?argv[1]:"captures/world-destruction");std::filesystem::create_directories(output);std::string profile=argc>2?argv[2]:"high";bool low=profile=="low",reduced=profile=="reduced";
 SetConfigFlags(FLAG_WINDOW_HIGHDPI|FLAG_MSAA_4X_HINT);InitWindow(960,600,"Hyperion / destructible universe");SetTargetFPS(0);hf::Renderer renderer;check(renderer.init(),"Native renderer init failed");renderer.freezeVisualTime(130);nlohmann::json scenes=nlohmann::json::array();
 const char* names[]={"reactor","habitat","warehouse","solar","gantry","cargo","tree","orbital-panel","orbital-core","asteroid","comet","planet-crater","moon-scar","wildlife","ground-rock","pirate-den","persistent-ruins","reconstruction"};
 for(int scene=0;scene<18;scene++){
  Game g;g.enterFlight();auto& e=g.s.economy;auto& f=g.s.flight;auto& site=e.settlements.front();site.owner=0;std::string siteId=site.id;g.ensureFlight();undock(g);int port=g.flightPort(siteId,scene==7||scene==8);auto up=f.ports[port].normal;auto right=hf::unit(hf::cross({1,0,0},up),{0,0,1});f.ship.position=f.ports[port].position+up*70;std::string id;int index=-1;hf::Event scar{"groundHit",2};double initialMass=0;
  if(scene<=3||scene==16||scene==17){int kind=scene==0||scene>=16?Reactor:scene==1?Habitat:scene==2?Warehouse:Solar;id=facilityObjectId(siteId,kind,0);index=target(g,id);}
  else if(scene<=6){if(scene==4)id=propObjectId("surface:"+siteId,"gantry",0);if(scene==5)id=propObjectId("surface:"+siteId,"cargo",0);if(scene==6){g.prepareWorldObjects();auto t=std::find_if(f.worldObjects.begin(),f.worldObjects.end(),[](const auto& o){return o.kind==PlantObject;});check(t!=f.worldObjects.end(),"No planted tree");id=t->id;}index=target(g,id);}
  else if(scene==7||scene==8){id=scene==7?propObjectId("orbit:"+siteId,"panel",0):portObjectId("orbit:"+siteId);index=target(g,id);}
  else if(scene==9||scene==10||scene==12){auto kind=scene==9?MinorKind::Asteroid:scene==10?MinorKind::Comet:MinorKind::Moon;auto body=std::find_if(f.minorBodies.begin(),f.minorBodies.end(),[&](const auto& b){return b.source.kind==kind;});check(body!=f.minorBodies.end(),"Missing generated body");up=hf::unit(hf::V3{.6,.7,-.4});right=hf::unit(hf::cross(up,{0,1,0}));f.ship.position=body->centre+up*(body->source.radius*2.8+30);if(scene==12){scar.kind="minorGroundHit";scar.port=int(body-f.minorBodies.begin());scar.position=body->centre+up*(minorRadius(body->source,{up.x,up.y,up.z}));scar.weapon=hf::Seeker;f.ship.position=scar.position+up*9+right*12;id=body->source.id;}else{id="body:"+body->source.id;index=target(g,id);}}
  else if(scene==11){auto& p=f.planets[2];up=hf::unit(hf::V3{.6,.7,-.4});right=hf::unit(hf::cross(up,{0,1,0}));scar.kind="groundHit";scar.port=2;scar.position=p.centre+up*(p.radius+hf::terrain(p,up)+2);scar.weapon=hf::Seeker;f.ship.position=scar.position+up*19+right*29;id="planet-scar";}
  else if(scene==13||scene==14){auto p=g.universe.planet("HOME",2);V2 chart{20,20};bool found=false;for(int x=-8;x<=8&&!found;x++)for(int y=-8;y<=8;y++){V2 q{(x+.5)*64,(y+.5)*64};auto r=g.universe.region(p,q);if(r.elevation>g.universe.seaLevel(p)+.05&&r.biomass>.09){chart=q;found=true;break;}}up=g.surfaceNormal(chart);right=hf::unit(hf::cross(up,{0,1,0}));f.ship.position=f.planets[2].centre+up*(f.planets[2].radius+hf::terrain(f.planets[2],up)+18);g.prepareWorldObjects();auto t=std::find_if(f.worldObjects.begin(),f.worldObjects.end(),[&](const auto&o){return o.kind==(scene==13?LifeObject:RockObject);});check(t!=f.worldObjects.end(),"Missing native terrain target");id=t->id;index=int(t-f.worldObjects.begin());}
  else if(scene==15){auto band=e.pirates.front().id;for(const auto& p:f.ports)if(p.id=="den:"+band){up=p.normal;right=hf::unit(hf::cross(up,{0,1,0}));f.ship.position=p.position+up*280;id=portObjectId(p.id);break;}index=target(g,id);}
  WorldObject object;if(index>=0){object=f.worldObjects[index];up=object.up;right=object.right;double distance=std::clamp(object.scale*2.5+28,18.,550.);f.ship.position=object.position+up*(distance*.55)+right*distance;f.orient(object.position-f.ship.position,up);}else f.orient(scar.position-f.ship.position,up);f.ship.previous=f.ship.position;
  auto damage=[&](){if(index>=0){index=target(g,id);f.damageWorld(index,1e8,-1,object.position,true);g.processFlightEvents();}else g.processWorldHit(scar);};
  if(scene==16||scene==17){damage();if(scene==17){Stock stock{};stock.fill(1000);e.credits=100000;std::string message;check(construct(e,site,Reactor,stock,message),"Reconstruction failed");g.syncWorldDamage();}else{auto file=output/"damaged.json";std::string error;check(saveState(g.s,file.string(),error),"Native destroyed save failed");State restored;check(loadState(restored,file.string(),error),"Native destroyed save failed to load");g.s=std::move(restored);g.universe.reset(g.s.seed,g.s.generation,g.s.legacySystems);g.ensureFlight();undock(g);index=target(g,id);check(g.s.flight.worldObjects[index].integrity==0,"Loaded ruins regained health");}}
  auto before=encodeEconomy(g.s.economy);double hours=g.s.hours;
  for(int frame=0;frame<54;frame++){
   if(frame==8&&scene<16)damage();auto& flight=g.s.flight;hf::Input input;flight.prepareCombat(1./60,input);flight.time+=1./60;g.syncWorldDamage();
   BeginDrawing();ClearBackground(BLACK);renderer.prepare(flight,flightScene(g),reduced,false,low);renderer.present(float(GetScreenWidth()),float(GetScreenHeight()));EndDrawing();
   if(frame==7||frame==18||frame==53){auto stats=renderer.architectureStats();check(stats.invalidVertices==0&&stats.invalidNormals==0,"Destroyed architecture has invalid geometry");Image image=LoadImageFromScreen();check(ExportImage(image,(output/(std::to_string(scene)+"-"+names[scene]+"-"+std::to_string(frame)+".jpg")).c_str()),"Native screenshot failed");UnloadImage(image);}
  }
  check(g.s.hours==hours,"Rendering destruction advanced strategy clock");if(scene<16&&index>=0)check(g.s.flight.worldObjects[index].integrity==0,"Native hit did not destroy target");
  if(scene==17)check(facilityIntegrity(g.s.economy.settlements.front(),Reactor,0)==1,"Restored facility not operational");if(scene>=16)check(before==encodeEconomy(g.s.economy),"Rendering changed canonical restored state");
  scenes.push_back({{"scene",scene},{"name",names[scene]},{"frames",54},{"target",id},{"persistentDamage",g.s.economy.worldDamage.size()},{"profile",profile},{"frozenClock",true},{"geometryValid",true}});
 }
 renderer.shutdown();CloseWindow();std::ofstream(output/"world-destruction.json")<<scenes.dump(2)<<'\n';std::cout<<"PASS: 18 native world destruction scenarios / 972 frames / "<<profile<<" graphics / saves untouched\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
