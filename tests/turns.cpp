#include "game.hpp"
#include "json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace hyperion;
namespace {
int checks=0;
void require(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
bool close(double a,double b){return std::abs(a-b)<1e-7;}
nlohmann::json snapshot(const State& state){auto path=(std::filesystem::temp_directory_path()/"hyperion-turn-snapshot.json").string();std::string error;require(saveState(state,path,error),"snapshot saves");std::ifstream input(path);nlohmann::json j;input>>j;std::filesystem::remove(path);return j;}
void idle(Game& game,int frames=1200){for(int i=0;i<frames;i++)game.simulate(.1,{1,1});}
}
int main(){try{
 // Planning time freezes every persisted actor, not just the displayed clock.
 for(Mode mode:{Mode::Galaxy,Mode::System,Mode::Surface,Mode::Ocean}){
  Game game;game.ensureEconomy();game.s.mode=mode;if(mode==Mode::Surface||mode==Mode::Ocean){game.s.activePlanet=2;game.parkFlight(2,{3200,1800});}
  game.s.mode=mode;game.s.waypoint={4000,2000};game.s.oceanTargetX=100;game.s.targetDepth=500;auto before=snapshot(game.s);
  for(int i=0;i<1200;i++)game.simulate(.1,mode==Mode::Galaxy?V2{}:V2{1,1});
  require(snapshot(game.s)==before,"idle strategy/expedition state is byte-equivalent, including resources, machinery, freight and deadlines");
 }
 Game game;game.system("HOME");game.s.selectedPlanet=2;game.descend();game.s.surface=game.s.waypoint={3200,1800};
 int turn=game.s.turns;double hours=game.s.hours;auto position=game.s.surface;
 game.moveTurn({1,0});require(game.s.turns==turn+1&&close(game.s.hours,hours+1),"one key command is one hour and one turn");require(close(game.s.surface.x,position.x+22),"movement uses a fixed expedition step");
 auto afterMove=snapshot(game.s);idle(game);require(snapshot(game.s)==afterMove,"held frame movement cannot repeat a turn");
 game.s.waypoint={game.s.surface.x+100,game.s.surface.y};hours=game.s.hours;position=game.s.surface;idle(game);require(close(game.s.hours,hours)&&close(game.s.surface.x,position.x),"waypoints only plan movement");
 game.action("moveWaypoint");require(close(game.s.surface.x,position.x+22)&&close(game.s.hours,hours+1),"Enter advances one waypoint step");
 turn=game.s.turns;game.action("found");require(game.s.turns==turn+1&&!game.s.economy.docked.empty(),"founding commits exactly one turn");auto colony=game.s.economy.docked;
 turn=game.s.turns;hours=game.s.hours;game.action("policy",colony);require(game.s.turns==turn+1&&close(game.s.hours,hours+1),"management orders commit turns");
 auto beforeFailure=snapshot(game.s);game.s.economy.credits=0;auto noFunds=snapshot(game.s);game.action("construct",colony+"|10");require(snapshot(game.s)==noFunds,"failed orders do not consume time or mutate the world");game.s.economy.credits=1600;
 turn=game.s.turns;hours=game.s.hours;game.action("planet","1");game.action("selectStar","HOME");game.waypoint(Contact{"planned","signal","Planned contact","",{3800,1800},0,0});require(game.s.turns==turn&&game.s.hours==hours,"selection and planning are free");
 game.s.battery=0;game.scan();require(game.s.hours==hours,"rejected scan costs no turn");game.endTurn();require(game.s.battery==4&&game.s.turns==turn+1,"end turn resolves one hour of battery recovery");
 game.endTurn();game.endTurn();turn=game.s.turns;game.scan();require(game.s.turns==turn+1&&game.s.battery==4,"even an empty valid scan spends one turn and twelve energy");
 auto beforeColony=snapshot(game.s)["economy"]["settlements"];turn=game.s.turns;game.endTurn();require(game.s.turns==turn+1&&snapshot(game.s)["economy"]["settlements"]!=beforeColony,"production and settlement conditions advance on committed turns");
 hours=game.s.hours;turn=game.s.turns;game.waitHours(6);require(close(game.s.hours,hours+6)&&game.s.turns==turn+6,"explicit long wait advances six turns");
 game.s.selectedPlanet=2;game.dive();game.s.hull=1;game.s.depth=1010;turn=game.s.turns;auto unsafe=snapshot(game.s);idle(game);require(snapshot(game.s)==unsafe,"pressure hazards wait for a committed turn");game.endTurn();require(game.s.mode==Mode::Surface&&game.s.hull==18&&game.s.turns==turn+1,"pressure exposure and emergency recovery resolve within one turn");
 // Ship, navigator, projectiles and shield regeneration also freeze in the chart.
 Game pilot;pilot.enterFlight();pilot.action("navigate");require(pilot.s.flight.autopilot,"navigator engaged");pilot.system("HOME");auto planned=snapshot(pilot.s);idle(pilot);require(snapshot(pilot.s)==planned,"active navigator and traffic freeze while planning");turn=pilot.s.turns;hours=pilot.s.hours;double flightTime=pilot.s.flight.time;pilot.endTurn();require(pilot.s.turns==turn+1&&close(pilot.s.hours,hours+1)&&pilot.s.flight.time>flightTime+54.9,"committing a turn resolves navigator and local traffic");
 pilot.enterFlight();hours=pilot.s.hours;turn=pilot.s.turns;pilot.simulate(.1);require(pilot.s.hours>hours&&pilot.s.turns==turn,"cockpit keeps real-time simulation without incrementing strategic turns");
 pilot.system("HOME");auto saved=snapshot(pilot.s);idle(pilot);require(snapshot(pilot.s)==saved,"returning from cockpit restores turn-based pause");
 // Courier expiry is resolved by strategic turns even on a deployed expedition.
 auto& f=game.s.flight;auto& issuer=game.s.economy.settlements.front();auto& destination=game.s.economy.settlements[1];f.courier={true,issuer.id,destination.id,70,game.s.hours+.5};auto treasury=issuer.treasury;
 auto frozen=snapshot(game.s);idle(game);require(snapshot(game.s)==frozen,"mission deadlines cannot expire during planning");game.endTurn();require(!f.courier.active&&issuer.treasury>=treasury+70,"turns expire courier escrow and return reserved funds");
 // Existing turn/hour counters survive native saves and idle after reopening.
 auto path=(std::filesystem::temp_directory_path()/"hyperion-turn-roundtrip.json").string();std::string error;require(saveState(pilot.s,path,error),"save turn-based strategy");Game restored;require(loadState(restored.s,path,error),"restore turn-based strategy");restored.universe.reset(restored.s.seed,restored.s.generation,restored.s.legacySystems);auto loaded=snapshot(restored.s);idle(restored);require(snapshot(restored.s)==loaded&&restored.s.turns==pilot.s.turns,"loaded strategy save remains paused until the next order");std::filesystem::remove(path);
 std::cout<<"PASS: "<<checks<<" turn-based checks; idle-world freeze, explicit actions, waypoint steps, economy, hazards, navigator resolution, cockpit real time, deadlines and save/reload\n";return 0;
 }catch(const std::exception& e){std::cerr<<"Turn-based failure: "<<e.what()<<" ("<<checks<<" checks)\n";return 1;}}
