#include "game.hpp"
#include "biosphere.hpp"
#include "wildlife.hpp"
namespace hyperion {
const std::array<Upgrade,4> upgradeDefs={{
 {"drive","HYPERSPACE APERTURE","Extends single-jump range by 185 light-years.",{55,27,0,28},8},
 {"sonar","MULTISPECTRAL SONAR","Widens the scanning envelope and passive contact range.",{20,16,8,20},6},
 {"pressure","HADAL PRESSURE SHELL","Adds 1,800 metres of certified diving depth.",{50,27,0,30},5},
 {"exosuit","EXPLORATION SYSTEMS","Improves movement speed and passive battery recharge.",{32,9,20,22},6}
}};
std::string modeName(Mode m){return m==Mode::Galaxy?"GALAXY":m==Mode::System?"ORBIT":m==Mode::Surface?"SURFACE":m==Mode::Flight?"FLIGHT":"OCEAN";}
void Game::reset(const std::string& seed){const std::string nextSeed=seed;s=State{};s.seed=nextSeed.empty()?"MERIDIAN-0001":nextSeed.substr(0,80);universe.reset(s.seed,s.generation,s.legacySystems);toast="";toastTime=0;toastPriority=Notice::Status;criticalSerial=0;lastTurn={};recentTurns.clear();trackedMission.clear();selectedMinor.clear();focusedContact.clear();transferNotice.clear();chartRecall={};surveyRecall={};diveState={};wreckGood=Components;diveReport={};diveDebrief.clear();radar=-1;heading=0;elapsed=0;
 log("Survey vessel NOMA commissioned at Meridian. The navigational atlas contains one authenticated star.");
 log("There is a recurring transmission beneath the water of Nacre. Its source remains unknown.");}
void Game::adoptGeneration(){
 if(s.generation>=2)return;s.legacySystems=s.visitedStars;s.legacySystems.insert(s.currentStar);s.legacySystems.insert(s.activeStar);s.legacySystems.insert(s.viewStar);s.legacySystems.insert(s.selectedStar);s.legacySystems.insert("HOME");
 for(const auto& id:s.economy.systems)s.legacySystems.insert(id);for(const auto& id:s.visitedPlanets){auto split=id.find('#');if(split!=std::string::npos)s.legacySystems.insert(id.substr(0,split));}for(const auto& [id,point]:s.locations){auto split=id.find('#');if(split!=std::string::npos)s.legacySystems.insert(id.substr(0,split));}
 s.generation=2;universe.reset(s.seed,s.generation,s.legacySystems);
}
void Game::notify(const std::string& t,Notice priority){
 if(priority==Notice::Critical)++criticalSerial;
 if(toastTime>0&&priority<toastPriority)return;
 toast=t;toastPriority=priority;toastTime=priority==Notice::Critical?8:priority==Notice::Important?6:4;
}
void Game::log(const std::string& t){s.log.insert(s.log.begin(),{s.hours,t});if(s.log.size()>120)s.log.resize(120);}
bool Game::inOrbit() {
 if(hasExpedition())return false;
 if(!s.flight.valid)return s.mode==Mode::System||s.mode==Mode::Galaxy;
 ensureFlight();const auto& f=s.flight;
 return !f.planets.empty()&&f.altitude()>200&&(!f.docked||(f.dockedPort>=0&&f.dockedPort<int(f.ports.size())&&f.ports[f.dockedPort].planet<0));
}
void Game::resumeExpedition(){
 if(!hasExpedition()){notify("No expedition is deployed / land or deploy with L first");return;}
 s.mode=expeditionLayer();s.expeditionMode=s.mode;s.viewStar=s.activeStar;s.selectedPlanet=s.activePlanet;
 transferNotice="Expedition resumed / world frozen / planned route retained";notify("Expedition resumed / awaiting orders");
}
void Game::galaxy(){transferNotice="World frozen / your orders resolve time / K resumes flight";
 if(s.mode==Mode::Surface||s.mode==Mode::Ocean){s.expeditionMode=s.mode;ensureFlight();s.flight.deployed=true;}
 s.mode=Mode::Galaxy;s.galaxy=currentStar().pos;s.selectedStar=s.currentStar;
}
void Game::system(const std::string& id){transferNotice="World frozen / your orders resolve time / K resumes flight";selectedMinor.clear();
 if(universe.star(id).id!=id){notify("Unknown stellar coordinates");return;}
 if(s.mode==Mode::Surface||s.mode==Mode::Ocean){s.expeditionMode=s.mode;ensureFlight();s.flight.deployed=true;}
 if(s.mode==Mode::Flight&&id==s.currentStar&&!s.flight.planets.empty())s.selectedPlanet=s.flight.nearestPlanet();
 s.viewStar=id;s.selectedStar=id;s.selectedPlanet=clamp(s.selectedPlanet,0,int(universe.planets(id).size())-1);s.mode=Mode::System;
}
bool Game::travel(const std::string& id){Star dst=universe.star(id);if(dst.id!=id){notify("Unknown stellar coordinates");return false;}double d=distance(currentStar().pos,dst.pos);int fuel=fuelCost(d);
 if(id==s.currentStar){system(id);return true;}if(!inOrbit()){notify("Ascend to orbit with U before engaging FTL");return false;}if(d>range()){notify("Star lies beyond the drive aperture");return false;}if(fuel>s.fuel){notify("Insufficient fuel for this jump");return false;}
 storeSecurity();s.economy.securityWitness.clear();s.flight.valid=false;s.flight.worldKey.clear();s.flight.traffic.clear();s.flight.bolts.clear();s.flight.planets.clear();s.flight.minorBodies.clear();s.flight.ports.clear();s.flight.savedTarget.clear();s.flight.savedDock.clear();s.flight.savedBerth.clear();s.flight.cancelNavigation();s.flight.docked=false;s.flight.deployed=false;s.fuel-=fuel;advanceTime(6+d/24,true);s.currentStar=id;s.viewStar=id;s.selectedStar=id;s.selectedPlanet=0;s.mode=Mode::System;s.totalDistance+=d;s.visitedStars.insert(id);
 log("FTL transit complete. "+dst.name+" entered the atlas, "+std::to_string(int(std::round(d)))+" ly from the previous survey position. "+std::to_string(dst.numPlanets)+" major bodies detected.");notify("Arrived at "+dst.name);soundEvent="jump";s.economy.docked.clear();ensureEconomy();discoverEconomy(s.economy,universe,id,s.hours);syncEconomy();return true;}
void Game::refuel(){
 if((s.mode!=Mode::System&&s.mode!=Mode::Galaxy)||!inOrbit()){notify("Hydrogen scoop requires planetary orbit / ascend with U first");return;}
 if(s.fuel>=s.maxFuel){notify("Fuel tanks are full");return;}
 ensureFlight();s.flight.cancelNavigation();s.flight.ship.throttle=0;s.flight.ship.assist=true;
 unsigned incident=criticalSerial;advanceTime(3.5,true);
 if(criticalSerial!=incident)return;
 s.fuel=s.maxFuel;s.flight.ship.fuel=100;log("Hydrogen skim at "+currentStar().name+": stellar fuel reserves replenished.");notify("Stellar hydrogen collected / tanks full");soundEvent="find";syncEconomy();
}
bool Game::orbitScan(){if(s.mode==Mode::System&&minorSelection()){notify("Fly-by survey / select this body in cockpit destinations and approach within 600 km");return false;}ensureEconomy();Stock previousCargo=shipStock();Planet p=planet();if(s.viewStar!=s.currentStar){notify("Remote telemetry only / jump to this system first");return false;}if(!inOrbit()){notify("Orbital spectrometry requires orbit / ascend with U first");return false;}std::string id="ORB:"+p.id;
 if(s.scanned.count(id)){notify("Orbital profile already complete");return false;}s.scanned.insert(id);s.resources[3]+=12;s.resources[0]+=5;s.resources[1]+=3;
 Discovery d{id,"Orbital spectrometry / "+p.name,"orbital",p.name,universe.star(s.viewStar).name,"The world has a "+atmosphere(p)+" atmosphere and "+p.bio+" biological readings. "+std::to_string(p.moonCount)+" moons orbit above its "+p.formation+"."+(p.generation>=2?" System architecture: "+universe.profile(p.starId).layout+".":std::string{}),0,s.hours};
 s.discovered.insert(s.discovered.begin(),d);if(s.discovered.size()>900)s.discovered.resize(900);log(p.name+": initial orbital geochemical profile completed. +12 research.");bool stowed=collectStockGains(previousCargo);notify(stowed?"Orbital scan archived / +12 research":"Orbital profile archived / hold full");soundEvent="find";if(turnBased())advanceTime(1,true);return true;}
bool Game::descend(){if(s.mode==Mode::System&&minorSelection()){notify("Minor body / fly-by surveys only / no expedition landing facilities");return false;}if(s.viewStar!=s.currentStar){notify("Travel to this system before descending");return false;}Planet p=planet();if(p.giant){notify("No solid surface / atmospheric descent unavailable");return false;}
 if(hasExpedition()){
  if(s.activeStar==s.viewStar&&s.activePlanet==s.selectedPlanet){resumeExpedition();return true;}
  auto destination=s.viewStar;int body=s.selectedPlanet;launch();if(hasExpedition())return false;s.viewStar=destination;s.selectedPlanet=body;
 }
 ensureFlight();double transfer=hyperion::flight::length(s.flight.ship.position-s.flight.planets[s.selectedPlanet].centre);s.fuel=std::max(0.,s.fuel-(.5+transfer*.0001));s.economy.docked.clear();Stock previousCargo=shipStock();s.activeStar=s.viewStar;s.activePlanet=s.selectedPlanet;s.surface=s.locations.count(p.id)?s.locations[p.id]:universe.shoreline(p);s.waypoint=s.surface;s.mode=s.expeditionMode=Mode::Surface;
 if(s.visitedPlanets.insert(p.id).second){s.resources[3]+=6;log("Planetfall: "+p.name+". "+p.dominant+" environment; "+atmosphere(p)+". A new surface expedition begins.");}
 collectStockGains(previousCargo);parkFlight(s.activePlanet,s.surface);advanceTime(.5+transfer/8000,true);transferNotice="Planetfall complete / world waits for your orders";notify("Planetfall / "+p.name);soundEvent="jump";return true;}
void Game::launch(){if(hasExpedition()&&expeditionLayer()==Mode::Ocean&&s.depth>80){notify("Ascend to 80 m or less before recovering the expedition / resume it with L");return;}selectedMinor.clear();
 if(!hasExpedition()){notify("No expedition is deployed / use the cockpit to lift off");return;}
 auto layer=expeditionLayer();Planet p=activePlanet();s.locations[p.id]=layer==Mode::Surface?s.surface:V2{s.oceanX,s.oceanAtY};
 s.viewStar=s.activeStar;s.selectedPlanet=s.activePlanet;s.mode=Mode::System;s.economy.docked.clear();
 parkFlight(s.activePlanet,s.locations[p.id],true);s.fuel=std::max(0.,s.fuel-.25);advanceTime(.25,true);if(layer==Mode::Ocean)finishDive();transferNotice="In orbit / 1 h resolved / K takes the helm";notify("Returned to planetary orbit / K takes the helm");
}
bool Game::dive(){if(s.mode!=Mode::Surface)return false;Planet p=activePlanet();if(p.water<.2){notify("No accessible deep ocean at this site");return false;}
 s.oceanX=s.surface.x;s.oceanAtY=s.surface.y;s.depth=clamp(s.depth,35.,150.);s.oceanTargetX=s.oceanX;s.targetDepth=s.depth;s.mode=s.expeditionMode=Mode::Ocean;restoreSurvey(true);beginDive(true);
 log("Submersible descent on "+p.name+". Ocean salinity: "+std::to_string(p.salinity)+"%. Pressure certification: "+std::to_string(int(maxDepth()))+" m.");transferNotice="Submersible deployed / 1 h resolved / planned destination retained";notify("Submersible deployed / pressure seal active");soundEvent="dive";advanceTime(1,true);return true;}
void Game::surface(bool consumeTurn){if(s.mode!=Mode::Ocean)return;if(consumeTurn&&s.depth>80){notify("Ascend to 80 m or less before surfacing / E ascends one turn");return;}rememberSurvey();surveyRecall.dive=true;s.surface={s.oceanX,s.oceanAtY};s.waypoint=s.surface;s.depth=45;s.mode=s.expeditionMode=Mode::Surface;if(consumeTurn)advanceTime(1,true);transferNotice=consumeTurn?"Surfaced / 1 h resolved / dive route remembered":"Surface expedition / world frozen";notify("Surfaced / "+activePlanet().name);finishDive(consumeTurn);}
std::vector<Contact> Game::targets(){std::vector<Contact> a;if(s.mode!=Mode::Surface&&s.mode!=Mode::Ocean)return a;Planet p=activePlanet();
 if(s.mode==Mode::Surface){double range=100+s.upgrades[1]*25;
 for(int x=int(std::floor((s.surface.x-range)/86));x<=int(std::floor((s.surface.x+range)/86));x++)for(int y=int(std::floor((s.surface.y-range)/86));y<=int(std::floor((s.surface.y+range)/86));y++)
 for(auto& o:universe.surfaceObjects(p,x,y))if(distance(o.pos,s.surface)<range)a.push_back(o);
 }else{double rx=76+s.upgrades[1]*21;
 for(int x=int(std::floor((s.oceanX-rx)/130));x<=int(std::floor((s.oceanX+rx)/130));x++)for(auto& o:universe.oceanObjects(p,x,s.oceanAtY))if(std::abs(o.pos.x-s.oceanX)<rx&&std::abs(o.depth-s.depth)<470+s.upgrades[1]*130)a.push_back(o);
 }
 if(s.mode==Mode::Ocean)for(const auto& c:wreckSites())a.push_back(c);
 LifeView view;view.centre=s.mode==Mode::Ocean?V2{s.oceanX,s.oceanAtY}:s.surface;view.observer=view.centre;view.observerDepth=s.depth;view.alert=s.mode==Mode::Ocean?diveAlert():.65;view.depth=s.depth;view.ocean=s.mode==Mode::Ocean;view.radius=100+s.upgrades[1]*25;view.hours=s.hours;view.limit=80;std::set<std::string> species;
 for(const auto& animal:visibleLife(s.economy,universe,p,view))if(species.insert(animal.contact.id).second)a.push_back(animal.contact);
 return a;}
bool Game::catalogue(const Contact& o,bool collect,int bonus){ensureEconomy();if(!s.scanned.insert(o.id).second)return false;Stock previousCargo=shipStock();Planet p=activePlanet();int gain=o.kind=="relic"?22:o.kind=="signal"?19:o.kind=="life"?12:o.kind=="vent"?13:o.kind=="mineral"?7:9;
 s.resources[3]+=bonus+gain+int(universe.hash(int32_t(o.n?o.n:1),int32_t(o.pos.x),4)%5);
 if(collect&&o.kind=="mineral"){auto local=universe.region(p,o.pos,o.depth);s.economy.hold[Ore]+=p.generation>=2?int(std::round(7+local.ore*6)):14;s.resources[1]+=p.generation>=2?int(std::round(3+local.crystal*3)):4+int(universe.hash(int32_t(o.n?o.n:1),7)%5);}
 if(collect&&(o.kind=="life"||o.kind=="flora")){auto& habitat=ensureContactHabitat(s.economy,universe,p,o);int samples=int(samplePopulation(habitat,o,std::min(double(p.generation>=2?phenotype(o).samples:10),std::floor(std::max(0.,cargoCapacity()-stockTotal(shipStock()))))));s.resources[2]+=samples;}
 if(collect&&o.kind=="vent"){s.economy.hold[Ore]+=12;s.resources[1]+=8;}
 if(o.kind=="relic"){if(collect)s.resources[1]+=14;s.relics++;}if(o.kind=="signal")s.signals++;
 s.discovered.insert(s.discovered.begin(),{o.id,o.name,o.kind,p.name,universe.star(s.activeStar).name,o.desc+((o.kind=="life"||o.kind=="flora")?populationReport(ensureContactHabitat(s.economy,universe,p,o),o):std::string{}),o.depth,s.hours});if(s.discovered.size()>900)s.discovered.resize(900);
 log(o.kind+" / "+o.name+". "+o.desc);
 if(o.kind=="signal"&&s.signals==1)log("The transmission contains a repeating interval. Its pulses are separated by the same irrational ratio across every known recording.");
 observeMissions();bool stowed=collectStockGains(previousCargo);notify(stowed?"New "+o.kind+" / +"+std::to_string(gain)+" research":"Discovery archived / ship hold full; some samples left in situ");soundEvent="find";return true;}
int Game::scan(){if(s.mode==Mode::Ocean)return oceanSonar();if(s.mode!=Mode::Surface&&s.mode!=Mode::Ocean){notify("Descend to begin an expedition");return 0;}if(s.battery<12){notify("Scanner battery low");return 0;}
 s.battery-=12;radar=0;int count=0;bool ocean=s.mode==Mode::Ocean;double range=ocean?260+s.upgrades[1]*190:34+s.upgrades[1]*14;
 for(const auto& o:targets()){double d=ocean?std::hypot((o.pos.x-s.oceanX)*5,o.depth-s.depth):distance(o.pos,s.surface);
 if(d<=range&&catalogue(o)){count++;if(count>=5)break;}}
 if(!count)notify("No uncharted returns inside the scanning envelope");advanceTime(1,true);soundEvent="sonar";return count;}
std::array<int,4> Game::upgradeCost(int i)const{std::array<int,4> cost{};for(int j=0;j<4;j++)cost[j]=int(std::ceil(upgradeDefs.at(i).cost[j]*(1+s.upgrades.at(i)*.65)));return cost;}
bool Game::canUpgrade(int i)const{if(i<0||i>=4||s.upgrades[i]>=upgradeDefs[i].max)return false;auto c=upgradeCost(i);for(int j=0;j<4;j++)if(s.resources[j]<c[j])return false;return true;}
bool Game::purchase(int i){if(!canUpgrade(i)){notify("Insufficient research materials");return false;}auto c=upgradeCost(i);for(int j=0;j<4;j++)s.resources[j]-=c[j];s.upgrades[i]++;log(std::string(upgradeDefs[i].name)+" upgraded to mark "+std::to_string(s.upgrades[i])+".");notify("System certification updated");soundEvent="find";if(turnBased())advanceTime(1,true);return true;}
void Game::waypoint(const Contact& o){focusedContact=o.id;if(s.mode==Mode::Surface)s.waypoint=o.pos;else if(s.mode==Mode::Ocean){s.oceanTargetX=o.pos.x;s.targetDepth=std::min(o.depth,maxDepth());}notify("Waypoint acquired / "+o.name);}
void Game::simulate(double dt,V2 movement,const hyperion::flight::Input& flightInput){
 ensureEconomy();dt=clamp(dt,0.,.1);elapsed+=dt;toastTime=std::max(0.,toastTime-dt);
 if(s.mode==Mode::Flight){
  s.hours+=dt/55;s.battery=clamp(s.battery+dt*(2.1+s.upgrades[3]*.65),0.,100.);
  updateFlight(dt,flightInput);syncEconomy();
 }else if(s.mode==Mode::Galaxy){
  // Camera movement is presentation, not a strategic action.
  s.galaxy.x=clamp(s.galaxy.x+movement.x*dt*350/s.galaxyZoom,-1.e7,1.e7);
  s.galaxy.y=clamp(s.galaxy.y+movement.y*dt*350/s.galaxyZoom,-1.e7,1.e7);
 }
 if(radar>=0){radar+=dt*.8;if(radar>1.3)radar=-1;}
}
bool Game::planningAction(const std::string& a){
 static const std::set<std::string> free={"wreckCargo","oceanRoute","plotWreckHome","oceanNext","oceanSelect","strategy","galaxy","system","planet","minorBody","plotMinor","selectStar","plotSettlement","expedition","flightTarget","flightNext","plotMission","weapon","inspectContact","combatTarget","combatOrder","combatFocus","target","fieldFocus","panel","tab","selectSettlement","reviewSettlement","cycleGood","cycleBuilding","cycleDestination","cycleFaction","help","settings","close","export","sound","new"};
 return free.count(a)>0;
}
bool Game::issueOrder(const std::string& a,const std::string& arg,bool blocked){
 if(blocked&&!planningAction(a)){notify("Simulation paused / resume before issuing an order");return false;}
 const double before=s.hours,hull=s.hull,shield=s.flight.ship.shield,battery=s.battery,fuel=s.fuel;const auto credits=s.economy.credits;const int turnsBefore=s.turns;
 double rations=0,alloy=0;for(const auto& site:s.economy.settlements)if(site.owner==0){rations+=site.stock[Rations];alloy+=site.stock[Alloy];}
 action(a,arg);
 // Action costs are often paid before advanceTime; reports include the whole order.
 if(s.hours>before+1e-8&&lastTurn.valid){lastTurn.hull=s.hull-hull;lastTurn.shield=s.flight.ship.shield-shield;lastTurn.battery=s.battery-battery;lastTurn.fuel=s.fuel-fuel;lastTurn.credits=s.economy.credits-credits;
  double nowRations=0,nowAlloy=0;for(const auto& site:s.economy.settlements)if(site.owner==0){nowRations+=site.stock[Rations];nowAlloy+=site.stock[Alloy];}lastTurn.rations=nowRations-rations;lastTurn.alloy=nowAlloy-alloy;
  if(lastTurn.startHour>before+1e-8){
   for(const auto& report:recentTurns)if(report.endHour>before+1e-8)for(const auto& event:report.events)if(std::find(lastTurn.events.begin(),lastTurn.events.end(),event)==lastTurn.events.end())lastTurn.events.push_back(event);
   lastTurn.startHour=before;lastTurn.endHour=s.hours;lastTurn.hours=s.hours-before;lastTurn.requested=lastTurn.hours;lastTurn.first=turnsBefore+1;
   recentTurns.erase(std::remove_if(recentTurns.begin(),recentTurns.end(),[&](const auto& report){return report.endHour>before+1e-8;}),recentTurns.end());recentTurns.insert(recentTurns.begin(),lastTurn);if(recentTurns.size()>12)recentTurns.resize(12);
  }else if(!recentTurns.empty())recentTurns.front()=lastTurn;
 }
 return true;
}
void Game::action(const std::string& a,const std::string& arg){if(a=="strategy"){openStrategy();return;}if(a=="expedition"){resumeExpedition();return;}if(a=="endTurn"){endTurn();return;}if(a=="waitTurns"){waitPlanned(arg);return;}if(a=="moveWaypoint"){moveTurn({},true);return;}if(a=="plotMinor"){if(auto* b=minorSelection()){if(s.viewStar==s.currentStar)flightAction("flightTarget","minor:"+b->id);else notify("Travel to this system before plotting a local fly-by");}return;}if(wreckAction(a,arg)||oceanAction(a,arg)||fieldAction(a,arg)||missionAction(a,arg)||shipyardAction(a,arg)||securityAction(a,arg)||flightAction(a,arg)||economicAction(a,arg))return;if(a=="galaxy")galaxy();else if(a=="system")system(arg.empty()?s.currentStar:arg);else if(a=="jump")travel(arg.empty()?s.selectedStar:arg);else if(a=="refuel")refuel();else if(a=="orbitScan")orbitScan();else if(a=="land")descend();else if(a=="launch")launch();else if(a=="dive")dive();else if(a=="surface")surface();else if(a=="scan")scan();else if(a=="minorBody"){for(const auto& body:universe.minorBodies(s.viewStar))if(body.id==arg){selectedMinor=arg;break;}}else if(a=="planet"){selectedMinor.clear();try{s.selectedPlanet=clamp(std::stoi(arg),0,int(universe.planets(s.viewStar).size())-1);}catch(...) {}}else if(a=="selectStar")s.selectedStar=arg;else if(a=="upgrade"){for(int i=0;i<4;i++)if(arg==upgradeDefs[i].key)purchase(i);}}
}
