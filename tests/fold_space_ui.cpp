#include "renderer.hpp"
#include "json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace hyperion;
void require(bool b,const std::string& why){if(!b)throw std::runtime_error(why);}
int main(int argc,char** argv){try{
 auto output=std::filesystem::absolute(argc>1?argv[1]:"captures/fold-ui");std::filesystem::create_directories(output);
 SetTraceLogLevel(LOG_WARNING);InitWindow(720,450,"Hyperion / folded space interface");SetTargetFPS(0);
 Renderer renderer;require(renderer.init(GraphicsQuality::Low),"UI renderer initialization failed");renderer.freezeFlightVisualTime(42);
 Game g;g.enterFlight();auto& f=g.s.flight;auto gate=f.folds[0];f.docked=false;f.dockedPort=-1;f.savedDock.clear();f.cancelNavigation();f.launchAssist=0;f.ship.gear=false;f.ship.throttle=0;f.ship.velocity={};f.ship.position=gate.position+gate.normal*210;f.orient(-gate.normal,gate.up);g.s.economy.docked.clear();
 f.selectTarget(int(std::find_if(f.ports.begin(),f.ports.end(),[&](const auto& p){return p.id==gate.id;})-f.ports.begin()));
 UI ui;ui.graphicsQuality=GraphicsQuality::Low;ui.auditInterface=true;ui.openPanel(0);ui.expanded={"folds"};nlohmann::json results=nlohmann::json::array();
 auto draw=[&](){ui.observeMode(int(g.s.mode),.1,false);BeginDrawing();ClearBackground(BLACK);renderer.draw(g,ui,.1);EndDrawing();};
 auto find=[&](const std::string& key){ui.scroll=0;for(int i=0;i<60;i++){draw();if(ui.auditButtons.count(key))return ui.auditButtons.at(key);if(ui.handoff.ready())ui.scroll+=20;}throw std::runtime_error("Fold control is unreachable at 720x450: "+key);};
 auto click=[&](const std::string& key){auto r=find(key);auto viewport=uiViewport();require(r.x>=0&&r.y>=0&&r.x+r.width<=viewport.width&&r.y+r.height<=viewport.height,"Fold control exceeds viewport");ui.pointerPressed=true;ui.pressPosition={r.x+r.width*.5f,r.y+r.height*.5f};draw();ui.pointerPressed=false;require(ui.action+"|"+ui.arg==key,"Native pointer missed fold action");auto action=ui.action,arg=ui.arg;ui.action.clear();ui.arg.clear();g.issueOrder(action,arg,ui.paused);};
 double hour=g.s.hours;find("foldSurvey|"+gate.id);require(g.s.hours==hour,"Cockpit UI rendering advanced time");renderer.screenshot((output/"cockpit-destinations.png").string());results.push_back("Cockpit survey fits small window");
 g.system("HOME");ui.openPanel(0);ui.expanded={"folds"};for(int i=0;i<5;i++)draw();
 ui.paused=true;draw();require(!ui.auditButtons.count("foldSurvey|"+gate.id),"Paused survey remained enabled");ui.paused=false;
 click("foldSurvey|"+gate.id);require(g.s.hours==hour+1&&g.s.economy.folds.at(gate.link).surveyed,"Pointer survey did not commit one turn");results.push_back("Strategy pointer survey resolves one hour; pause blocks it");
 // Survey resolves a turn; return to an approach before committing transit.
 f.ship.position=gate.position+gate.normal*30;f.ship.velocity={};hour=g.s.hours;auto mode=g.s.mode;click("foldTraverse|"+gate.id);require(g.s.mode==mode&&g.s.hours==hour+1&&g.s.economy.folds.at(gate.link).passages==1,"Strategy pointer crossing did not preserve its chart mode");results.push_back("Strategy pointer transit preserves mode and commits one hour");
 ui.scroll=0;for(int i=0;i<5;i++)draw();renderer.screenshot((output/"charted-connections.png").string());
 auto position=f.ship.position;hour=g.s.hours;for(int i=0;i<30;i++){g.simulate(.1);draw();}require(g.s.hours==hour&&flight::length(position-f.ship.position)<1e-8,"Planning advances the clock or ship");
 g.enterFlight();for(int i=0;i<5;i++)draw();require(flight::length(position-f.ship.position)<1e-8&&g.s.hours==hour,"Returning to the helm altered physical state");results.push_back("Planning and cockpit handoff preserve the same ship and time");
 renderer.close();CloseWindow();std::ofstream(output/"results.json")<<results.dump(2)<<'\n';std::cout<<results.dump(2)<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"Fold UI check failed: "<<e.what()<<'\n';return 1;}}
