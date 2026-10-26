#include "game.hpp"
#include "save_session.hpp"
#include "json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <limits>
using namespace hyperion;
namespace {
int checks=0;
void require(bool v,const char* message){++checks;if(!v)throw std::runtime_error(message);}
bool near(double a,double b){return std::abs(a-b)<1e-7;}
nlohmann::json snapshot(const State& s){auto path=std::filesystem::temp_directory_path()/"hyperion-review-snapshot.json";std::string error;require(saveState(s,path.string(),error),"snapshot saves");std::ifstream in(path);nlohmann::json j;in>>j;std::filesystem::remove(path);return j;}
}
int main(){try{
 Game game;game.system("HOME");game.s.selectedPlanet=2;require(game.descend()&&game.dive(),"deploy ocean expedition");game.s.oceanAtY=1800;game.s.oceanX=3200;game.s.depth=330;
 auto before=snapshot(game.s);double hours=game.s.hours;int turns=game.s.turns;
 game.system("HOME");require(game.hasExpedition()&&game.expeditionLayer()==Mode::Ocean,"chart retains deployed ocean layer");require(game.s.hours==hours&&game.s.turns==turns,"opening a chart is free");
 game.galaxy();game.system("HOME");game.resumeExpedition();auto after=snapshot(game.s);before["galaxy"]=after["galaxy"];require(after==before,"chart round trip preserves expedition and world state apart from atlas camera");
 game.system("HOME");game.s.fuel=10;game.refuel();require(game.s.fuel==10&&game.s.hours==hours,"chart cannot scoop from underwater");require(!game.orbitScan()&&game.s.hours==hours,"chart cannot scan from underwater");
 auto destination=game.universe.cell(1,0).front();require(!game.travel(destination.id)&&game.s.currentStar=="HOME","chart cannot FTL with a deployed expedition");
 game.resumeExpedition();game.s.hull=1;game.s.depth=1010;game.system("HOME");game.waitHours(6);require(game.s.hours==hours,"long shift cannot evade ocean pressure guard through chart");game.endTurn();require(game.expeditionLayer()==Mode::Surface&&game.s.hull==18,"ocean hazard resolves while viewing charts");require(game.toast.find("Emergency surface")!=std::string::npos&&game.toastPriority==Notice::Critical,"end turn preserves emergency notice");
 game.toastTime=0;game.launch();require(!game.hasExpedition()&&game.inOrbit()&&game.s.turns==turns+2,"explicit ascent takes exactly one turn");
 game.s.flight.ship.velocity={100,0,0};game.s.fuel=10;hours=game.s.hours;game.refuel();require(game.s.fuel==game.s.maxFuel&&near(game.s.flight.ship.fuel,100)&&near(game.s.hours,hours+4),"fuel scoop fills after transfer burn finishes");
 // An automatic orbital transfer must never masquerade as a deployed ocean dive.
 Game transfer;transfer.system("HOME");transfer.s.selectedPlanet=2;transfer.descend();transfer.dive();transfer.s.depth=transfer.maxDepth()+110;transfer.launch();require(transfer.hasExpedition(),"deep launch waits for physical ascent");while(transfer.s.depth>80)transfer.issueOrder("oceanAscend");transfer.launch();double transferHull=transfer.s.hull;require(transfer.dock(transfer.s.economy.settlements.front().id)&&near(transfer.s.hull,transferHull)&&!transfer.hasExpedition(),"orbital rendezvous cannot apply pressure from a previous dive");
 // Paused orders do not reach simulation, including deferred UI actions.
 game.ensureEconomy();before=snapshot(game.s);
 for(const auto& action:{"endTurn","wait","refuel","land","cockpit","orbitScan","jump","upgrade","found","construct","buy","navigate","service"})require(!game.issueOrder(action,"",true),"world order blocked while paused");
 require(snapshot(game.s)==before,"all blocked shortcuts preserve complete state");require(game.issueOrder("planet","1",true)&&game.s.selectedPlanet==1,"paused planet selection remains available");
 // Chart views retain access only to the actual deployed site.
 Game docked;docked.ensureEconomy();auto site=docked.s.economy.settlements.front();docked.s.mode=Mode::Surface;docked.s.activeStar=site.star;docked.s.activePlanet=site.planet;docked.s.surface=site.position;docked.parkFlight(site.planet,site.position);docked.s.economy.docked=site.id;
 docked.system("HOME");require(docked.canDock(site),"free charts retain local warehouse access");auto remote=docked.s.economy.settlements[1];docked.s.selectedPlanet=remote.planet;require(!docked.canDock(remote),"selecting another planet cannot authorize remote cargo");
 site.depth=35;docked.s.expeditionMode=Mode::Ocean;docked.s.oceanX=site.position.x;docked.s.depth=35;docked.s.oceanAtY=site.position.y+1000;require(!docked.canDock(site),"ocean access checks second horizontal coordinate");docked.s.oceanAtY=site.position.y;require(docked.canDock(site),"ocean berth accessible at actual coordinates");
 // Save/reload charts must restore the expedition layer before boarding.
 auto path=(std::filesystem::temp_directory_path()/"hyperion-review-roundtrip.json").string();std::string error;require(saveState(docked.s,path,error),"deployed chart saves");Game restored;require(loadState(restored.s,path,error),"deployed chart loads");restored.universe.reset(restored.s.seed,restored.s.generation,restored.s.legacySystems);restored.resumeExpedition();require(restored.s.mode==Mode::Ocean&&restored.s.depth==35,"saved ocean layer resumes correctly");hours=restored.s.hours;restored.enterFlight();require(restored.s.mode==Mode::Flight&&!restored.hasExpedition()&&near(restored.s.hours,hours+1),"boarding through a chart first recovers the submersible");auto chart=restored.surfacePosition(restored.s.flight.ship.position-restored.s.flight.planets[restored.s.activePlanet].centre);require(near(chart.x,site.position.x)&&near(chart.y,site.position.y),"boarding retains actual expedition position");std::filesystem::remove(path);
 // An orbital save must authorize the same physical berth before any cockpit frame.
 Game orbital;orbital.enterFlight();auto orbitalSite=orbital.s.economy.settlements.front().id;orbital.s.flight.dockAt(orbital.flightPort(orbitalSite,true));orbital.processFlightEvents();orbital.system("HOME");require(saveState(orbital.s,path,error),"orbital chart saves");Game loadedOrbit;require(loadState(loadedOrbit.s,path,error),"orbital chart loads");loadedOrbit.universe.reset(loadedOrbit.s.seed,loadedOrbit.s.generation,loadedOrbit.s.legacySystems);require(loadedOrbit.canDock(*findSettlement(loadedOrbit.s.economy,orbitalSite)),"loaded berth authorizes exact local stock before cockpit reconstruction");require(loadedOrbit.inOrbit(),"orbital eligibility reconstructs derived flight geometry after loading");std::filesystem::remove(path);
 // A short navigator approach arrives before a deadline inside the first quarter hour.
 Game courier;courier.enterFlight();auto& f=courier.s.flight;auto issuer=courier.s.economy.settlements.front().id;auto recipient=courier.s.economy.settlements[1].id;int port=courier.flightPort(recipient,true);auto berth=f.ports[port].position+f.ports[port].normal*90;f.target=port;f.docked=false;f.savedDock.clear();f.ship.position=berth+hyperion::flight::V3{.01,0,0};f.ship.velocity={};f.autopilot=true;f.route={berth};f.courier={true,issuer,recipient,70,courier.s.hours+.02};courier.system("HOME");courier.endTurn();require(f.deliveries==1&&!f.courier.active,"near-deadline arrival pays before expiry during a turn");
 // Exhaustion limits an already engaged navigator, rather than granting free cruise.
 hyperion::flight::Game pilot;pilot.planets.push_back({"Test","rock",{},1000,0,{0,1,0},false});pilot.ports.push_back({"Port","",{0,2000,0},{0,1,0},-1,"port","",0});pilot.docked=false;pilot.ship.position={10000,10000,10000};pilot.ship.fuel=.0001;pilot.autopilot=true;pilot.route={pilot.ship.position+hyperion::flight::V3{10000,0,0}};
 for(int i=0;i<400;i++)pilot.update(.05);require(pilot.ship.fuel==0&&pilot.speed()<=12.00001,"fuel-starved navigator uses emergency drive limit");
 // Two nearby berths: landing selects the nearest one on the actual world.
 hyperion::flight::Game landing;landing.planets=pilot.planets;auto& world=landing.planets.front();hyperion::flight::V3 n{0,1,0};double floor=world.radius+hyperion::flight::terrain(world,n)+2;landing.ports={{"Far","",{35,floor,0},n,0,"far","",0},{"Near","",{4,floor,0},n,0,"near","",0}};landing.docked=false;landing.ship.position={0,floor-1,0};landing.update(.01);require(landing.dockedPort==1,"touchdown chooses nearest berth instead of insertion order");
 // Nonfinite commands never poison a valid campaign.
 restored.s.mode=Mode::Surface;before=snapshot(restored.s);require(!restored.moveTurn({std::numeric_limits<double>::quiet_NaN(),0})&&snapshot(restored.s)==before,"invalid movement input rejected transactionally");
 // Two app sessions: newer disk state survives and the older session gets recovery.
 auto dir=std::filesystem::temp_directory_path()/"hyperion-review-save-conflict";std::filesystem::remove_all(dir);std::filesystem::create_directories(dir);path=(dir/"save.json").string();require(saveState(game.s,path,error),"initial campaign save");SaveSession first(path),second(path);std::string recovery;game.s.resources[0]+=7;require(first.save(game.s,error,recovery)==SaveResult::Saved,"first session saves normally");
 auto onDisk=snapshot(game.s);docked.s.resources[0]=19;require(second.save(docked.s,error,recovery)==SaveResult::Conflict&&!recovery.empty(),"older session exports recovery on conflict");State disk,backup;require(loadState(disk,path,error)&&snapshot(disk)==onDisk,"newer campaign remains untouched");require(loadState(backup,recovery,error)&&backup.resources[0]==19,"conflicting progress is recoverable");auto recoveryFile=recovery;docked.s.resources[0]=20;require(second.redirected()&&second.save(docked.s,error,recovery)==SaveResult::Saved,"later progress continues autosaving in recovery campaign");require(loadState(backup,recoveryFile,error)&&backup.resources[0]==20,"recovery campaign includes later progress");require(loadState(disk,path,error)&&snapshot(disk)==onDisk,"recovery autosaves never overwrite original campaign");require(second.acknowledge(error)&&!second.redirected()&&second.save(docked.s,error,recovery)==SaveResult::Saved,"explicit import or new campaign can acknowledge destination");
 std::filesystem::remove(path);require(second.save(docked.s,error,recovery)==SaveResult::Conflict,"external deletion also preserves progress as recovery");std::filesystem::remove_all(dir);
 std::cout<<"PASS: "<<checks<<" review regressions; pause gate, free charts, physical access, pressure, refuelling, deadlines, fuel exhaustion, nearest landing and concurrent save recovery\n";return 0;
 }catch(const std::exception& e){std::cerr<<"Review regression failed: "<<e.what()<<" ("<<checks<<" checks)\n";return 1;}}
