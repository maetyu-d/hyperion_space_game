#include "game.hpp"
#include "wildlife.hpp"
#include "flight_scene.hpp"
#include "flight/controls.hpp"
#include "save_session.hpp"
#include "renderer.hpp"
#include "audio.hpp"
#include "native_pointer.hpp"
#include "window_fit.hpp"
#include "display_preferences.hpp"
#include "flight/hud.hpp"
#include "json.hpp"
#include "raylib.h"
#include <filesystem>
#include <iostream>
#include <chrono>
#include <fstream>
namespace fs=std::filesystem;
using namespace hyperion;
namespace {
struct Options {bool wreckCheck=false;bool oceanCheck=false;bool continuityCheck=false;bool fieldworkCheck=false;bool plannerCheck=false;bool hudCheck=false;std::string hud;bool closeMaterialCheck=false;int closeMaterialShot=-1;bool minorCheck=false;int minorShot=-1;bool screenFitCheck=false,missionBoard=false,helmCheck=false,explicitSave=false,handlingCheck=false,planningCheck=false,turnCheck=false,wildlifeCheck=false,ecosystemCheck=false,livingCheck=false,generationCheck=false,shipyardCheck=false,securityCheck=false,combatCheck=false,transitionCheck=false,cloudOcclusionCheck=false;std::string seed="MERIDIAN-0001",save=defaultSavePath(),load,view,screenshot,captures,icon,benchmarkReport;int wildlifeShot=-1;bool wildlifeBaseline=false;int frames=0,width=1280,height=800,graphicsShot=-1;double depth=-1,lifeZoom=0;bool fresh=false,noSave=false,tour=false,uiCheck=false,economyCheck=false,flightCheck=false,graphicsCheck=false,settlementCheck=false,planetCheck=false,cloudCheck=false,materialCheck=false,surfaceDetailCheck=false,lightingCheck=false,waterCheck=false,architectureCheck=false,stabilityCheck=false,fullscreen=true,reducedEffects=false,benchmark=false,profileFlight=false;};
// Real generated catalogue, viewed without editing the universe or advancing its clock.
void prepareMinorGraphics(Game& g,int scene){
 g.reset(g.s.seed);g.enterFlight();auto& f=g.s.flight;f.cancelNavigation();f.launchAssist=0;f.docked=false;f.dockedPort=-1;f.savedDock.clear();g.s.economy.docked.clear();f.ship.velocity={};f.ship.throttle=0;f.ship.gear=false;
 MinorKind kind=scene==3||scene==6?MinorKind::Asteroid:scene==4||scene==5?MinorKind::Comet:MinorKind::Moon;int chosen=-1;
 for(int i=0;i<int(f.minorBodies.size());i++)if(f.minorBodies[i].source.kind==kind){chosen=i;if(kind!=MinorKind::Moon||f.minorBodies[i].source.parent==2)break;}
 if(chosen<0)throw std::runtime_error("Generated minor-body fixture lacks a requested family");
 const auto& b=f.minorBodies[chosen];for(int i=0;i<int(f.ports.size());i++)if(f.ports[i].minor==chosen)f.selectTarget(i);
 auto light=hyperion::flight::unit(hyperion::flight::V3{.6,.7,-.4}),side=hyperion::flight::unit(hyperion::flight::cross(light,{0,1,0}));
 double span=b.source.radius*(scene==2?1.65:scene==4?75:scene==6?12:3.2);
 f.ship.position=b.centre+light*(span*.7)+side*(span*.8);auto look=b.centre;if(scene==4)look=look-light*b.source.radius*35;
 f.orient(look-f.ship.position,{0,1,0});f.ship.previous=f.ship.position;f.recordIds();g.toastTime=0;
 if(scene==0){g.system(g.s.currentStar);g.selectedMinor=b.source.id;}
}
// A disposable no-save combat fixture drives the normal simulation and renderer.
void prepareCombatGraphics(Game& g,int scene){
 g.reset(g.s.seed);g.enterFlight();auto& f=g.s.flight;f.cancelNavigation();f.launchAssist=0;f.docked=false;f.dockedPort=-1;f.savedDock.clear();g.s.economy.docked.clear();f.ship.velocity={};f.ship.throttle=0;f.ship.gear=false;
 const auto& planet=f.planets[2];auto normal=hyperion::flight::unit(hyperion::flight::V3{.6,.7,-.4}),forward=hyperion::flight::unit(hyperion::flight::cross(normal,{0,1,0}));f.ship.position=planet.centre+normal*(planet.radius+600);f.orient(forward-normal*.15,normal);
 int home=0;for(int i=0;i<int(f.ports.size());i++)if(auto* port=findSettlement(g.s.economy,f.ports[i].settlement))if(port->owner==1&&f.ports[i].planet<0){home=i;break;}
 f.traffic.clear();auto right=hyperion::flight::unit(hyperion::flight::cross(f.ship.forward,f.ship.up));
 for(int i=0;i<(scene==4?4:scene==10?3:1);i++){hyperion::flight::Traffic t;t.home=home;t.kind=1;t.role=scene==4?i:scene==1||scene==14||scene==15?1:scene==2||scene==11?2:scene==12?3:0;t.id=f.ports[home].id+":pirate:"+std::to_string(t.role);t.faction=1;t.position=t.previous=f.ship.position+f.ship.forward*(scene==4?170+i*45:130)+right*(scene==4?(i-1)*65:0);t.forward=-f.ship.forward;t.health=hyperion::flight::enemyDef(t.role).hull;t.shield=hyperion::flight::enemyDef(t.role).shield;t.fireClock=scene==4?0:100;t.positioned=true;t.countermeasures=scene==14?2:0;f.traffic.push_back(t);}
 f.combatTarget=f.traffic.front().id;f.ship.combat.weapon=scene<3?scene:0;
 if(scene==3)f.lock=1;
 if(scene==5){f.ship.hull=g.s.hull=37;f.ship.shield=12;f.ship.combat.armour=31;f.ship.combat.systems={38,61,44,23,56};f.ship.combat.heat=82;f.ship.combat.energy=29;}
 if(scene==6){f.traffic.clear();f.combatTarget.clear();f.wrecks.push_back({f.ship.position+f.ship.forward*23,{},180,{4,2,3},"CHECK-WRECK"});}
 if(scene==7){g.system("HOME");g.action("combatOrder","1");}
 if(scene==8){f.dockAt(home);g.processFlightEvents();auto* site=findSettlement(g.s.economy,g.s.economy.docked);if(!site)throw std::runtime_error("Combat service fixture has no staffed berth");site->stock[Alloy]=site->stock[Components]=site->stock[Propellant]=100;g.s.economy.credits=5000;f.ship.hull=g.s.hull=62;f.ship.combat.armour=41;f.ship.combat.systems[hyperion::flight::Drive]=30;f.ship.combat.rounds=0;f.ship.combat.missiles=2;f.ship.combat.decoys=1;}
 if(scene==9)f.traffic.clear();
 if(scene==10){f.ship.combat.weapon=hyperion::flight::Resonance;for(int i=0;i<3;i++){auto& t=f.traffic[i];t.position=t.previous=f.ship.position+f.ship.forward*(145+i*5)+right*((i-1)*32);t.id=f.ports[home].id+":chain:"+std::to_string(i);}f.combatTarget=f.traffic[1].id;}
 if(scene==11){f.ship.combat.weapon=hyperion::flight::Kinetic;f.aimSystem=1;f.lock=1;f.traffic[0].shield=0;}
 if(scene==12)f.traffic[0].fireClock=0;
 if(scene==13){auto& t=f.traffic[0];t.role=1;t.id=f.ports[home].id+":pirate:1";t.position=t.previous=f.ports[home].position+f.ports[home].normal*110;t.health=12;t.modules={25,40,55};t.ammunition=t.missiles=t.countermeasures=0;t.serviceClock=7.9;auto* site=findSettlement(g.s.economy,f.ports[home].settlement);site->stock[Alloy]=site->stock[Components]=100;f.ship.position=t.position+f.ports[home].normal*210+right*170;f.orient(t.position-f.ship.position,f.ports[home].normal);f.combatTarget=t.id;}
 if(scene==14)f.lock=1;
 if(scene==15)f.ship.combat.weapon=hyperion::flight::Pulse;
 f.recordIds();g.toastTime=0;g.soundEvent.clear();
}
void prepareSecurityGraphics(Game& g,int scene){
 g.reset(g.s.seed);g.enterFlight();auto& f=g.s.flight;auto& e=g.s.economy;
 f.cancelNavigation();f.docked=false;f.dockedPort=-1;f.savedDock.clear();e.docked.clear();f.ship.gear=false;f.ship.velocity={};f.ship.throttle=0;
 const auto& planet=f.planets[2];auto normal=hyperion::flight::unit(hyperion::flight::V3{.6,.7,-.4});auto forward=hyperion::flight::unit(hyperion::flight::cross(normal,{0,1,0}));
 f.ship.position=planet.centre+normal*(planet.radius+600);f.orient(forward-normal*.15,normal);auto right=hyperion::flight::unit(hyperion::flight::cross(f.ship.forward,f.ship.up));
 int patrol=-1,raider=-1;for(int i=0;i<int(f.traffic.size());i++){auto& t=f.traffic[i];t.position=t.previous=f.ship.position+normal*(5000+i*150);t.positioned=true;t.fireClock=0;if(t.managed){if(t.pirate&&raider<0&&(scene!=2||t.role==3))raider=i;if(!t.pirate&&patrol<0&&(scene!=2||t.faction==2))patrol=i;}}
 if(patrol<0||raider<0)throw std::runtime_error("Security fixture missing actual campaign captains");
 auto& pirate=f.traffic[raider];auto& navy=f.traffic[patrol];
 pirate.position=pirate.previous=f.ship.position+f.ship.forward*150+right*28;navy.position=navy.previous=f.ship.position+f.ship.forward*155-right*38;
 pirate.forward=hyperion::flight::unit(navy.position-pirate.position);navy.forward=-pirate.forward;
 f.combatTarget=pirate.id;
 if(scene==0)g.system("HOME");
 if(scene==1){e.pirates[0].grievance=34;findCaptain(e,pirate.id)->grudge=34;findCaptain(e,pirate.id)->defeats=2;g.refreshSecurity();g.system("HOME");}
 if(scene==2){findCaptain(e,pirate.id)->faction=3;g.storeShipStock({});g.refreshSecurity();f.combatTarget=pirate.id;}
 if(scene==3){e.factions[navy.faction].warrant=375;g.refreshSecurity();f.combatTarget=navy.id;}
 if(scene==4){f.dockAt(g.flightPort(e.routes[0].from,true));g.processFlightEvents();g.system("HOME");g.action("escort",e.routes[0].id);}
 if(scene==5){e.pirates[0].grievance=34;g.action("pirateTruce",e.pirates[0].id);g.system("HOME");}
 f.recordIds();g.toastTime=0;
}
void prepareWildlifeGraphics(Game& g,int scene){
 g.reset(g.s.seed);g.ensureEconomy();g.system("HOME");g.s.selectedPlanet=2;auto p=g.universe.planet("HOME",2);V2 location{20,20};bool found=false;
 for(int x=-12;x<=12&&!found;x++)for(int y=-12;y<=12;y++){V2 q{(x+.5)*64,(y+.5)*64};auto r=g.universe.region(p,q);if(r.elevation>g.universe.seaLevel(p)+.06&&r.biomass>.12){location=q;found=true;break;}}
 if(scene<2||scene==6){g.descend();g.s.surface=g.s.waypoint=location;g.registerExpeditionHabitat();if(scene==1){auto& port=g.s.economy.settlements[2];port.position=location;port.habitat.clear();port.depth=0;port.built[Growhouse]=3;port.stock[Rations]=100;initializeEcosystems(g.s.economy,g.universe);}if(scene==6)g.endTurn();}
 else if(scene==2||scene==3){g.descend();g.dive();g.s.oceanX=20;g.s.oceanAtY=1;g.s.depth=g.s.targetDepth=350;g.registerExpeditionHabitat();if(scene==3)for(auto& [id,h]:g.s.economy.ecosystems)for(auto& pop:h.populations){h.detritus+=pop.mass*.95;pop.mass*=.05;}}
 else{int body=2;if(scene==7){bool found=false;for(int x=-2;x<=2&&!found;x++)for(int y=-2;y<=2&&!found;y++)for(const auto& star:g.universe.cell(x,y)){for(const auto& q:g.universe.planets(star.id))if(!q.giant&&q.life==0){g.s.currentStar=g.s.viewStar=g.s.selectedStar=star.id;g.s.selectedPlanet=body=q.index;found=true;break;}if(found)break;}if(!found)throw std::runtime_error("No lifeless native fixture world");}g.enterFlight();auto& f=g.s.flight;const auto& world=f.planets[body];if(scene==5){auto& port=g.s.economy.settlements[2];location=port.position;}
  if(scene==4){LifeView view;view.centre=location;view.hours=g.s.hours;view.radius=90;for(const auto& animal:visibleLife(g.s.economy,g.universe,p,view))if(!animal.flora){location=animal.position;break;}}
  auto n=g.surfaceNormal(location),east=hyperion::flight::unit(hyperion::flight::cross({0,1,0},n),{1,0,0});auto at=world.centre+n*(world.radius+hyperion::flight::terrain(world,n)+2.15);f.ship.position=at+n*(scene==5?16:3)-east*(scene==5?145:9);f.orient(at+east*(scene==5?25:0)+n*(scene==5?0:1)-f.ship.position,n);f.ship.velocity={};f.ship.throttle=0;f.ship.gear=false;f.docked=false;f.dockedPort=-1;f.savedDock.clear();f.launchAssist=0;g.s.economy.docked.clear();g.s.mode=Mode::Flight;
  if(scene==4){auto life=flightScene(g);const hyperion::flight::OrganismView* chosen=nullptr;double nearest=100;for(const auto& animal:life.organisms)if(!animal.flora&&animal.plan==2){double d=hyperion::flight::length(animal.position-f.ship.position);if(d<nearest&&d>8){chosen=&animal;nearest=d;}}if(chosen)f.orient(chosen->position+chosen->up*chosen->size*.8-f.ship.position,n);}
  f.recordIds();
 }
 g.toastTime=0;
}
void prepareEcosystemGraphics(Game& g,int scene){
 g.reset(g.s.seed);g.ensureEconomy();g.system("HOME");auto& e=g.s.economy;auto& site=e.settlements[2];site.owner=0;site.residents=48;site.built[Warehouse]=8;site.built[Habitat]=6;site.built[Reactor]=4;site.built[Growhouse]=3;site.built[MedLab]=2;site.built[Harvester]=4;site.built[Restoration]=0;site.stock={60,60,100,100,500,80,40,500,40};site.nextMigration=1e9;e.routes.clear();site.harvestPolicy=scene==1?2:0;
 auto* habitat=settlementHabitat(e,site);if(!habitat)throw std::runtime_error("Missing native ecological habitat");
 if(scene==1){harvestEcosystem(*habitat,1e8,2,3);g.advanceTime(36,true,false);}
 if(scene==2||scene==3){site.pollution=.95;habitat->pollution=.8;habitat->quality=.22;for(auto& pop:habitat->populations){habitat->detritus+=pop.mass*.9;pop.mass*=.1;}g.advanceTime(1,true,false);}
 if(scene==3){site.pollution=0;site.harvestPolicy=1;site.built[Restoration]=2;g.advanceTime(48,true,false);}
 if(scene==4||scene==5){g.s.selectedPlanet=2;g.descend();g.dive();g.s.oceanX=0;g.s.oceanAtY=0;g.s.depth=g.s.targetDepth=300;g.registerExpeditionHabitat();if(scene==5){for(auto& [id,h]:e.ecosystems)if(h.star=="HOME"&&h.planet==2){h.quality=.22;for(auto& pop:h.populations){h.detritus+=pop.mass*.95;pop.mass*=.05;}}}return;}
 std::cout<<"Ecology scene "<<scene<<" / "<<ecosystemSummary(*habitat)<<" / biodiversity "<<biodiversity(*habitat)<<" / harvested "<<habitat->harvested<<" / quality "<<habitat->quality<<'\n';g.toastTime=0;
}
void prepareLivingGraphics(Game& g,int scene){
 if(scene<4){prepareCombatGraphics(g,0);auto& f=g.s.flight;auto& t=f.traffic.front();t.genome={1,uint32_t(973+scene*6517),false};t.role=scene;t.health=hyperion::flight::enemyDef(t).hull;t.shield=hyperion::flight::enemyDef(t).shield;t.fireClock=0;f.ship.shield=200;f.ship.combat.systems.fill(100);f.ship.throttle=0;t.position=t.previous=f.ship.position+f.ship.forward*85;f.recordIds();}
 else if(scene==4||scene==5){g.reset(g.s.seed);g.system("HOME");g.s.selectedPlanet=2;g.descend();if(scene==5){g.dive();g.s.oceanAtY=0;g.s.oceanX=0;g.s.depth=300;g.s.targetDepth=300;}else g.s.surface=g.s.waypoint={0,0};}
 else{g.reset(g.s.seed);g.enterFlight();if(scene==7)g.system("HOME");else{auto& f=g.s.flight;const auto& port=f.ports[2];f.docked=false;f.dockedPort=-1;f.savedDock.clear();f.ship.position=port.position+port.normal*330;f.orient(port.position-f.ship.position,hyperion::flight::unit(hyperion::flight::cross(port.normal,{0,1,0})));f.ship.velocity={};f.ship.throttle=0;f.launchAssist=0;f.recordIds();}}
 g.toastTime=0;
}
void prepareGenerationGraphics(Game& g,int scene){
 auto seed=g.s.seed;g.reset(seed);std::string star="HOME";int body=2;
 if(scene==9){g.s.generation=1;g.universe.reset(g.s.seed,1);}
 if(scene>=1&&scene<=6){const char* requested[]={"","arid","frozen","volcanic","ocean","rocky",""};bool found=false;
  for(int x=-8;x<=8&&!found;x++)for(int y=-8;y<=8&&!found;y++){auto stars=g.universe.cell(x,y);for(const auto& candidate:stars){if(candidate.id=="HOME")continue;if(scene==6&&g.universe.profile(candidate.id).settlements!=6)continue;auto planets=g.universe.planets(candidate.id);for(const auto& p:planets)if(!p.giant&&(scene==6||p.type==requested[scene])){star=candidate.id;body=p.index;found=true;break;}if(found)break;}}
  if(!found)throw std::runtime_error("Missing procedural showcase world");
 }
 g.s.currentStar=g.s.viewStar=g.s.selectedStar=star;g.s.selectedPlanet=body;g.enterFlight();auto& f=g.s.flight;auto& e=g.s.economy;
 if(scene==6){g.system(star);return;}
 if(scene==7||scene==8){g.system("HOME");g.s.selectedPlanet=2;g.descend();if(scene==8)g.dive();return;}
 f.docked=false;f.dockedPort=-1;f.savedDock.clear();f.deployed=false;e.docked.clear();f.launchAssist=0;f.ship.gear=false;f.ship.throttle=.25;f.ship.velocity={};const auto& p=f.planets[body];auto n=hyperion::flight::unit(hyperion::flight::V3{.6,.7,-.4});f.ship.position=p.centre+n*(p.radius+(scene==0||scene==9?600:380));f.orient(-n+hyperion::flight::unit(hyperion::flight::cross(n,{0,1,0}))*.22,n+hyperion::flight::V3{0,1,0});f.recordIds();g.toastTime=0;
}
void prepareShipyardGraphics(Game& g,int scene){
 g.reset(g.s.seed);g.enterFlight();auto& e=g.s.economy;auto& f=g.s.flight;e.credits=30000;
 std::string portId;for(auto& p:e.settlements)if(p.owner==3)portId=p.id;
 if(scene==4||scene==5||scene==6||scene==7)for(auto& p:e.settlements)if(p.owner==1)portId=p.id;
 auto* port=findSettlement(e,portId);port->stock.fill(250);port->power=100;
 f.dockAt(g.flightPort(portId,true));g.processFlightEvents();
 if(scene>=4){e.shipOffers.erase(std::remove_if(e.shipOffers.begin(),e.shipOffers.end(),[&](const auto& o){return o.port==portId;}),e.shipOffers.end());for(int i=0;i<2;i++){e.shipyardClock[portId]=e.hour;advanceShipyards(e);}}
 if(scene==1){g.action("refitShip","0");g.action("refitShip","1");g.action("refitShip","3");}
 if(scene==2){g.s.hull=f.ship.hull=62;g.s.fuel=123;f.ship.combat.rounds=37;f.ship.combat.systems[hyperion::flight::Drive]=58;g.system("HOME");std::string selected;for(const auto& o:e.shipOffers)if(o.port==portId&&o.vessel.model==1)selected=o.vessel.serial;g.action("tradeShip",selected);if(e.vessel.model!=1)throw std::runtime_error("Shipyard fixture did not trade its hull");}
 if(scene>=3){int model=scene==3?0:scene==4?1:scene==5?2:3;
  if(model==1){for(auto& p:e.settlements)if(p.owner==3)portId=p.id;port=findSettlement(e,portId);f.dockAt(g.flightPort(portId,true));g.processFlightEvents();}
  if(model!=0){std::string selected;for(const auto& o:e.shipOffers)if(o.port==portId&&o.vessel.model==model)selected=o.vessel.serial;g.action("tradeShip",selected);if(e.vessel.model!=model)throw std::runtime_error("Missing manufactured hull in diagnostic");}
  if(scene==7){g.action("refitShip","4");g.s.hull=f.ship.hull=62;f.ship.combat.armour=41;f.ship.combat.systems[hyperion::flight::Drive]=30;f.ship.shield=12;g.system("HOME");}
  else{f.docked=false;f.dockedPort=-1;f.savedDock.clear();e.docked.clear();f.launchAssist=0;f.ship.gear=false;f.ship.velocity={};f.ship.throttle=.6;
   const auto& body=f.planets[2];auto n=hyperion::flight::unit(hyperion::flight::V3{.6,.7,-.4});f.ship.position=body.centre+n*(body.radius+600);f.orient(hyperion::flight::unit(hyperion::flight::cross(n,{0,1,0}))-n*.15,n);g.s.mode=Mode::Flight;}
 }
 f.recordIds();g.toastTime=0;
}
void selectView(Game& g,const std::string& view){if(view=="flight")g.enterFlight();else if(view=="galaxy")g.galaxy();else if(view=="system")g.system(g.s.currentStar);else if(view=="surface"||view=="ocean"){g.system(g.s.currentStar);g.s.selectedPlanet=g.s.currentStar=="HOME"?2:0;if(!g.descend())return;if(view=="ocean")g.dive();}}
void prepareEconomyCheck(Game& g) {
 g.ensureEconomy();g.s.economy.credits=12000;g.system("HOME");g.s.selectedPlanet=2;g.descend();g.s.surface=g.s.waypoint={3200,1800};
 Stock provision{80,400,200,240,200,100,120,180,200};g.storeShipStock(provision);g.action("found");std::string first=g.s.economy.docked;
 for(int facility:{Solar,Reactor,Mine,Refinery,Warehouse,Habitat,Observatory})g.action("construct",first+"|"+std::to_string(facility));
 g.action("settle",first);g.action("deposit",first+"|0");g.action("deposit",first+"|0");g.action("deposit",first+"|7");
 g.s.surface=g.s.waypoint={3650,1800};g.action("found");std::string second=g.s.economy.docked;
 for(int facility:{Solar,MedLab,Habitat})g.action("construct",second+"|"+std::to_string(facility));g.action("settle",second);
 g.action("route",first+"|"+second+"|1");
 g.action("civicResearch","0");g.waitHours(6);g.waitHours(6);g.waitHours(6);g.waitHours(6);
 g.s.resources={45,24,18,36};g.s.economy.hold={12,0,0,0,20,6,8,18,0};g.s.economy.credits=2400;
 g.s.economy.docked=second;g.s.economy.settlements.back().stock[Rations]=0;g.s.economy.settlements.back().stock[Biomass]=0;g.waitHours(6);
 g.notify("SIMULATION DEMONSTRATION / two outposts and a developing supply crisis");
}
// Disposable diagnostics use the same economy and presentation path as play.
// Only this explicitly requested, no-save fixture changes the campaign data.
void prepareSettlementGraphics(Game& g,int stage,bool sunlit=false,bool night=false){
 g.reset(g.s.seed);g.ensureEconomy();auto& e=g.s.economy;
 auto site=*std::find_if(e.settlements.begin(),e.settlements.end(),[](const Settlement& s){return s.owner==3;});
 site.id="VISUAL-PORT";site.name="Nacre / "+std::string(stage==12?"First Landing":"Frontier Works");site.owner=stage>=16&&stage<=19?stage-15:0;site.authority=std::max(1,site.owner);
 site.built={2,2,1,1,3,1,2,1,1,8,2,6,4,2};site.residents=64;site.condition=1;site.pollution=.05;site.ecosystem=550;site.stock={120,100,60,100,160,40,100,100,80};site.nextMigration=1e6;
 if(stage==12){site.built={1,0,0,0,1,0,0,0,0,2,1,1,1,0};site.residents=8;site.stock={0,20,0,20,30,3,5,10,0};}
 if(stage==14){site.stock={};site.stock[Rations]=40;site.built[Solar]=site.built[Reactor]=0;}
 if(stage==15){site.pollution=.93;site.condition=.26;site.ecosystem=12;site.stock[Components]=site.stock[Alloy]=0;}
 if(sunlit)site.position=g.surfacePosition(hyperion::flight::unit({.95,.1,.3}));
 if(night)site.position=g.surfacePosition(-hyperion::flight::unit({.6,.7,-.4}));
 e.settlements={site};e.routes.clear();e.contracts.clear();e.docked.clear();
 g.s.hours=.25;advanceEconomy(e,g.universe,g.s.hours);
 if(stage>=20){
  Settlement other=e.settlements.front();other.id="VISUAL-DEST";other.name="Nacre / Receiving Exchange";other.owner=3;other.position.x+=6000;e.settlements.push_back(other);
  TradeRoute r;r.id="VISUAL-FREIGHT";r.from=site.id;r.to=other.id;r.good=Components;r.owner=0;r.outbound=true;r.cargo[Components]=24;r.duration=6;r.remaining=stage==21?0:stage==22?3:5.98;r.status=stage==21?"Warehouse full / holding in orbit":"Components in transit";
  if(stage==21)e.settlements.back().stock[Ore]=storageCapacity(e.settlements.back());
  e.routes.push_back(r);
 }
 g.enterFlight();g.updateFlight(0);auto& f=g.s.flight;f.cancelNavigation();f.docked=false;f.dockedPort=-1;f.savedDock.clear();f.ship.velocity={};f.ship.throttle=0;e.docked.clear();
 int berth=g.flightPort(stage==21?"VISUAL-DEST":site.id,stage>=21);const auto& p=f.ports.at(berth);
 auto a=hyperion::flight::unit(hyperion::flight::cross({1,0,0},p.normal)),b=hyperion::flight::unit(hyperion::flight::cross(a,p.normal));
 f.target=berth;f.ship.position=p.position+p.normal*(stage>=21?330:300)+a*170+b*170;f.orient(p.position+p.normal*10-f.ship.position,p.normal);
 if(stage==22){auto it=std::find_if(f.traffic.begin(),f.traffic.end(),[](const hyperion::flight::Traffic& t){return !t.routeId.empty();});if(it!=f.traffic.end()){f.ship.position=it->position-it->forward*78+hyperion::flight::V3{22,19,0};f.orient(it->position-f.ship.position,{0,1,0});}}
 f.recordIds();g.toastTime=0;g.soundEvent.clear();
}
void preparePlanetGraphics(Game& g,int stage){
 g.reset(g.s.seed);int special=-1;
 if(stage==25||stage==26){std::string selected;const std::string type=stage==25?"frozen":"volcanic";
  for(int cy=-2;cy<=2&&selected.empty();cy++)for(int cx=-2;cx<=2&&selected.empty();cx++)for(const auto& star:g.universe.cell(cx,cy)){for(const auto& planet:g.universe.planets(star.id))if(planet.type==type){selected=star.id;special=planet.index;break;}if(!selected.empty())break;}
  if(!selected.empty())g.s.currentStar=g.s.viewStar=selected;
 }
 g.enterFlight();auto& f=g.s.flight;f.cancelNavigation();f.launchAssist=0;f.docked=false;f.dockedPort=-1;f.savedDock.clear();f.ship.velocity={};f.ship.throttle=0;g.s.economy.docked.clear();
 if(stage>=27&&stage<=30){
  auto it=std::find_if(g.s.economy.settlements.begin(),g.s.economy.settlements.end(),[](const Settlement& s){return s.owner==3;});int berth=g.flightPort(it->id,stage==30);f.target=berth;
  if(stage==27||stage==29||stage==30){f.dockAt(berth);f.events.clear();if(stage!=29)f.launch();if(stage==27)for(int i=0;i<20;i++)g.simulate(1./60);}
  else{const auto& p=f.ports[berth];auto tangent=hyperion::flight::unit(hyperion::flight::cross({1,0,0},p.normal));f.ship.position=p.position+p.normal*14+tangent*8;f.orient(p.position-f.ship.position,p.normal);f.ship.gear=true;f.autopilot=true;f.route={p.position+p.normal*.25};}
 }else{
  int body=special>=0?special:stage==24?1:2;const auto& p=f.planets[body];hyperion::flight::V3 n{.6,.7,-.4};double best=-1e9;
  for(int y=0;y<9;y++)for(int x=0;x<32;x++){double a=x*2*pi/32,lat=(y/8.-.5)*2.4;auto candidate=hyperion::flight::unit(hyperion::flight::V3{std::cos(a)*std::cos(lat),std::sin(lat),std::sin(a)*std::cos(lat)});double sun=hyperion::flight::dot(candidate,hyperion::flight::unit({.6,.7,-.4}));if(sun<.45)continue;double elevation=hyperion::flight::materialElevation(p,candidate),score=stage==31?-std::abs(elevation-(.28+.44*p.water-.006)):elevation+sun*.045;if(score>best){best=score;n=candidate;}}
  auto tangent=hyperion::flight::unit(hyperion::flight::cross({0,1,0},n));f.ship.position=p.centre+n*(p.radius+hyperion::flight::terrain(p,n)+2+(stage==31?2.5:stage==32?190:stage==23?9:6));f.orient(tangent-n*(stage==32?.7:.32),n);f.ship.gear=true;
 }
 f.recordIds();g.toastTime=0;g.soundEvent.clear();
}
void prepareMaterialGraphics(Game& g,int stage){
 g.reset(g.s.seed);int body=2;std::string type;
 if(stage==39||stage==45)type="arid";if(stage==40||stage==46)type="frozen";if(stage==41||stage==47)type="volcanic";if(stage==42||stage==48)type="toxic";if(stage==49||stage==50)type="rocky";
 if(!type.empty()){
  std::string selected;double bestWeather=1e9;
  for(int cy=-2;cy<=2;cy++)for(int cx=-2;cx<=2;cx++)for(const auto& star:g.universe.cell(cx,cy))for(const auto& planet:g.universe.planets(star.id))if(planet.type==type){double score=planet.pressure+planet.water*.6;if(score<bestWeather){bestWeather=score;selected=star.id;body=planet.index;}}
  if(!selected.empty())g.s.currentStar=g.s.viewStar=selected;
 }
 g.enterFlight();auto& f=g.s.flight;f.cancelNavigation();f.launchAssist=0;f.docked=false;f.dockedPort=-1;f.savedDock.clear();f.ship.velocity={};f.ship.throttle=0;f.ship.gear=stage<44;g.s.economy.docked.clear();
 if(stage==44)body=4;
 const auto& p=f.planets[body];auto n=hyperion::flight::unit(hyperion::flight::V3{.6,.7,-.4});double best=-1e9;
 for(int y=0;y<9;y++)for(int x=0;x<32;x++){double a=x*2*pi/32,lat=(y/8.-.5)*2.4;auto candidate=hyperion::flight::unit(hyperion::flight::V3{std::cos(a)*std::cos(lat),std::sin(lat),std::sin(a)*std::cos(lat)});double sun=hyperion::flight::dot(candidate,hyperion::flight::unit({.6,.7,-.4}));if(sun<.45)continue;double e=hyperion::flight::materialElevation(p,candidate),score=stage==43?-std::abs(e-(.28+.44*p.water-.006)):e+sun*.045;if(score>best){best=score;n=candidate;}}
 g.s.selectedPlanet=body;bool orbit=stage>=44&&stage!=49;if(orbit)n=hyperion::flight::unit(hyperion::flight::V3{.7,.3,.7});
 auto tangent=hyperion::flight::unit(hyperion::flight::cross({0,1,0},n));double height=orbit?p.radius*1.45:hyperion::flight::terrain(p,n)+2+(stage==43?3:6);
 f.ship.position=p.centre+n*(p.radius+height);f.orient(orbit?-n:tangent-n*.40,n);f.recordIds();g.toastTime=0;g.soundEvent.clear();
}
void prepareSurfaceDetailGraphics(Game& g,int stage){
 const int shot=stage-64,material[]={39,40,41,42,43,49,23};int base=material[shot%7];
 if(base==23)preparePlanetGraphics(g,base);else prepareMaterialGraphics(g,base);
 auto& f=g.s.flight;int body=f.nearestPlanet();g.s.selectedPlanet=body;const auto& p=f.planets[body];
 auto n=hyperion::flight::unit(f.ship.position-p.centre),tangent=hyperion::flight::unit(hyperion::flight::cross({0,1,0},n),{1,0,0});
 double height=hyperion::flight::terrain(p,n)+2+(shot<7?2.4:18.);
 f.ship.position=p.centre+n*(p.radius+height);f.orient(tangent-n*(shot<7?.72:.48),n);f.ship.gear=false;f.recordIds();
}
// Frozen no-save water poses use actual generated material boundaries.
void prepareWaterGraphics(Game& g,int shot){
 g.reset(g.s.seed);int body=2;std::string type=shot==5?"toxic":shot==6?"arid":shot==7?"frozen":"";
 if(!type.empty()){
  double best=1e9;std::string star;
  for(int cy=-2;cy<=2;cy++)for(int cx=-2;cx<=2;cx++)for(const auto& s:g.universe.cell(cx,cy))for(const auto& p:g.universe.planets(s.id))if(p.type==type){
   double score=std::abs(p.water-.45)*2.+p.pressure*.2;if(score<best){best=score;star=s.id;body=p.index;}
  }
  if(!star.empty())g.s.currentStar=g.s.viewStar=star;
 }
 g.enterFlight();auto& f=g.s.flight;f.cancelNavigation();f.launchAssist=0;f.docked=false;f.dockedPort=-1;f.savedDock.clear();f.ship.velocity={};f.ship.throttle=0;f.ship.gear=false;g.s.economy.docked.clear();
 const auto& p=f.planets.at(body);auto sun=hyperion::flight::unit(hyperion::flight::V3{.6,.7,-.4});hyperion::flight::V3 n=sun;double best=-1e9;
 double target=.28+.44*p.water-(shot==0||shot==11?.065:shot==7?.04:.003);
 auto score=[&](hyperion::flight::V3 v){double light=hyperion::flight::dot(v,sun);if(std::abs(v.y)>.56)return -1e9;
  if(shot==3?(light<-.025||light>.08):shot==4?(light>-.55):(light<.38))return -1e9;
  return -std::abs(hyperion::flight::materialElevation(p,v)-target)-std::abs(light-(shot==3?.02:shot==4?-.8:.65))*.001;
 };
 for(int y=0;y<33;y++)for(int x=0;x<96;x++){double lat=(y/32.-.5)*1.15,angle=x*2*pi/96;hyperion::flight::V3 v{std::cos(angle)*std::cos(lat),std::sin(lat),std::sin(angle)*std::cos(lat)};double value=score(v);if(value>best){best=value;n=v;}}
 for(double step:{.012,.004,.0013,.0004}){auto a=hyperion::flight::unit(hyperion::flight::cross({0,1,0},n)),b=hyperion::flight::unit(hyperion::flight::cross(n,a));auto origin=n;
  for(int y=-2;y<=2;y++)for(int x=-2;x<=2;x++){auto v=hyperion::flight::unit(origin+a*(x*step)+b*(y*step));double value=score(v);if(value>best){best=value;n=v;}}
 }
 auto a=hyperion::flight::unit(hyperion::flight::cross({0,1,0},n)),b=hyperion::flight::unit(hyperion::flight::cross(n,a));double step=.002;
 auto coast=hyperion::flight::unit(a*(hyperion::flight::materialElevation(p,hyperion::flight::unit(n+a*step))-hyperion::flight::materialElevation(p,hyperion::flight::unit(n-a*step)))+b*(hyperion::flight::materialElevation(p,hyperion::flight::unit(n+b*step))-hyperion::flight::materialElevation(p,hyperion::flight::unit(n-b*step))),a);
 double height=shot==2?35:shot==9?140:shot==1||shot==10?4.5:7.;
 f.ship.position=p.centre+n*(p.radius+hyperion::flight::terrain(p,n)+2+height);f.orient((shot==1||shot==2||shot==9||shot==10?coast:a)-n*(shot==2?1.8:shot==9?.9:shot==1||shot==10?.6:.28),n);
 if(shot==8){n=hyperion::flight::unit({.7,.3,.7});f.ship.position=p.centre+n*p.radius*2.7;f.orient(-n,{0,1,0});}
 if(shot==11)f.orient(sun-n*(2*hyperion::flight::dot(sun,n)),n);
 g.s.selectedPlanet=body;f.recordIds();g.toastTime=0;g.soundEvent.clear();
}
void prepareArchitectureGraphics(Game& g,int shot){
 const int poses[]={13,12,13,13,13,13,16,17,18,19,13,14,15,21,22,13,13,13,13,13,15,13,15,13,15};
 bool night=shot==10||shot==17;prepareSettlementGraphics(g,poses[shot],!night,night);
 auto& f=g.s.flight;const auto& port=f.ports.at(f.target);if(shot==13||shot==14)return;
 auto a=hyperion::flight::unit(hyperion::flight::cross({1,0,0},port.normal)),b=hyperion::flight::unit(hyperion::flight::cross(a,port.normal));
 f.ship.position=port.position+port.normal*65+a*65+b*75;f.orient(port.position+port.normal*8-f.ship.position,port.normal);
 if((shot>=2&&shot<=4)||(shot>=15&&shot!=19)){
  hyperion::flight::SettlementView view;const auto& site=g.s.economy.settlements.front();view.id=site.id;view.facilities=site.built;view.residents=site.residents;
  auto layout=hyperion::flight::layoutSettlement(view);int kind=shot==21||shot==22?hyperion::flight::Medical:shot==23||shot==24?hyperion::flight::Fabricator:shot==18||shot==20?hyperion::flight::Warehouse:shot==2||shot==15||shot==17?hyperion::flight::Habitat:shot==3?hyperion::flight::Smelter:hyperion::flight::Garden;
  auto it=std::find_if(layout.facilities.begin(),layout.facilities.end(),[&](const auto& x){return x.kind==kind&&(kind!=hyperion::flight::Habitat||x.ordinal==4)&&(kind!=hyperion::flight::Garden||x.ordinal==2);});
  if(it!=layout.facilities.end()){
   const auto& body=f.planets.at(port.planet);auto radial=hyperion::flight::unit(port.position+a*it->position.x+b*it->position.z-body.centre);
   auto centre=body.centre+radial*(body.radius+hyperion::flight::terrain(body,radial)+2.12),x=hyperion::flight::unit(a*std::cos(it->angle)+b*std::sin(it->angle)),z=hyperion::flight::unit(hyperion::flight::cross(x,radial));
   double distance=shot>=15?(shot==16?18:9):shot==3?38:29,height=shot>=15?(shot==16?4.2:5.2):8;
   f.ship.position=centre+radial*height-z*distance+x*(shot>=15?4:9);f.orient(centre+radial*(shot==4||shot==16?4:shot>=15?5.7:9)-f.ship.position,radial);
   if(shot==18||shot==20||shot>=21){double side=hyperion::flight::dot(x,hyperion::flight::unit(hyperion::flight::V3{.6,.7,-.4}))>=0?1:-1;f.ship.position=centre+radial*5.2+x*(side*12)-z*3;f.orient(centre+radial*5+x*(side*8)-z*2-f.ship.position,radial);}
  }
 }
 if(shot==19){f.ship.position=port.position+port.normal*6.5+a*19+b*70;f.orient(port.position+port.normal*5+a*8+b*57-f.ship.position,port.normal);}
 if(shot==5){f.ship.position=port.position+port.normal*16+a*27+b*86;f.orient(port.position+port.normal*9+b*56-f.ship.position,port.normal);}
 f.recordIds();
}
// Frozen close views exercise the normal generated models and material state.
void prepareCloseMaterialGraphics(Game& g,int shot){
 if(shot<8){const int poses[]={15,16,18,20,21,22,23,24};prepareArchitectureGraphics(g,poses[shot]);return;}
 if(shot<13){
  prepareCombatGraphics(g,0);auto& f=g.s.flight;auto& t=f.traffic.front();int faction=shot-8;
  t.faction=faction;t.role=faction%4;t.genome={1,uint32_t(17593+faction*6517),false};t.pirate=true;t.managed=true;
  t.health=hyperion::flight::enemyDef(t).hull*(shot==10?.35:shot==12?.15:1.);t.shield=0;t.charge=t.hitFlash=0;
  t.forward=hyperion::flight::unit({.2,.08,-1});double size=hyperion::flight::enemyDef(t).radius;
  auto sun=hyperion::flight::unit(hyperion::flight::V3{.6,.7,-.4}),side=hyperion::flight::unit(hyperion::flight::cross(sun,{0,1,0}));
  f.ship.position=t.position+sun*size*4.7+side*size*1.5;f.orient(t.position-f.ship.position,{0,1,0});f.ship.previous=f.ship.position;f.recordIds();return;
 }
 prepareMinorGraphics(g,shot==13?2:shot==14?3:5);auto& f=g.s.flight;const auto& b=f.minorBodies[f.ports[f.target].minor];
 auto n=hyperion::flight::unit(hyperion::flight::V3{.6,.7,-.4}),side=hyperion::flight::unit(hyperion::flight::cross(n,{0,1,0}));
 double r=minorRadius(b.source,{n.x,n.y,n.z});f.ship.position=b.centre+n*(r+std::max(2.2,b.source.radius*.10));f.orient(-n+side*.35,{0,1,0});f.ship.previous=f.ship.position;f.recordIds();
}
void prepareCloudOcclusionGraphics(Game& g,int shot){
 g.reset(g.s.seed);g.enterFlight();auto& f=g.s.flight;f.cancelNavigation();f.launchAssist=0;f.docked=false;f.dockedPort=-1;f.savedDock.clear();f.ship.velocity={};f.ship.throttle=0;f.ship.gear=false;g.s.economy.docked.clear();
 const auto& p=f.planets[2];auto n=hyperion::flight::unit(shot==4?hyperion::flight::V3{-.2,.8,.55}:hyperion::flight::V3{.7,.3,.7});auto tangent=hyperion::flight::unit(hyperion::flight::cross({0,1,0},n));
 double heights[]={0,0,0,0,0,0,19.95,22.1,33,47.95,65,p.radius*1.8};
 double pitch[]={.03,-.06,-.8,.02,-.025,.12,.18,.18,.14,-.08,-.3,-10};
 double height=shot<6?hyperion::flight::terrain(p,n)+3.2:heights[shot];
 f.ship.position=p.centre+n*(p.radius+height);f.orient(shot==11?-n:tangent+n*pitch[shot],shot==3?hyperion::flight::rotate(n,tangent,.55):n);f.recordIds();g.toastTime=0;g.soundEvent.clear();
}
void prepareFlightGraphics(Game& g,int stage){
 if(stage>=154){prepareCloseMaterialGraphics(g,std::clamp(stage-154,0,15));return;}
 if(stage>=140){prepareCloudOcclusionGraphics(g,std::clamp(stage-140,0,11));return;}
 if(stage>=112){prepareArchitectureGraphics(g,std::clamp(stage-112,0,24));return;}
 if(stage>=96){prepareWaterGraphics(g,std::clamp(stage-96,0,11));return;}
 if(stage>=78){
  const int poses[]={0,3,2,64,65,66,69,12,13,28,33,38,12,13,3};int shot=std::clamp(stage-78,0,14),pose=poses[shot];
  if(shot==12||shot==13)prepareSettlementGraphics(g,pose,true);else prepareFlightGraphics(g,pose);
  if(shot==14){auto& f=g.s.flight;const auto& p=f.planets[4];auto n=hyperion::flight::unit(hyperion::flight::V3{.7,-.2,-.6});f.ship.position=p.centre+n*p.radius*2.8;f.orient(-n,{0,1,0});f.recordIds();}
  if(pose==12||pose==13){auto& f=g.s.flight;const auto& port=f.ports.at(f.target);auto a=hyperion::flight::unit(hyperion::flight::cross({1,0,0},port.normal)),b=hyperion::flight::unit(hyperion::flight::cross(a,port.normal));f.ship.position=port.position+port.normal*65+a*65+b*75;f.orient(port.position+port.normal*8-f.ship.position,port.normal);f.recordIds();}
  return;
 }
 if(stage>=64){prepareSurfaceDetailGraphics(g,stage);return;}
 if(stage>=51){g.reset(g.s.seed);g.enterFlight();auto& f=g.s.flight;f.cancelNavigation();f.launchAssist=0;f.docked=false;f.dockedPort=-1;f.savedDock.clear();f.ship.velocity={};f.ship.throttle=0;f.ship.gear=false;g.s.economy.docked.clear();f.recordIds();g.toastTime=0;g.soundEvent.clear();return;}
 if(stage>=39){prepareMaterialGraphics(g,stage);return;}
 if(stage>=33){
  g.enterFlight();auto& f=g.s.flight;f.cancelNavigation();f.launchAssist=0;f.docked=false;f.dockedPort=-1;f.savedDock.clear();f.ship.velocity={};f.ship.throttle=0;f.ship.gear=false;g.s.economy.docked.clear();
  const auto& p=f.planets[2];auto sun=hyperion::flight::unit(hyperion::flight::V3{.6,.7,-.4});auto n=hyperion::flight::unit(hyperion::flight::V3{.7,.3,.7});
  if(stage==37)n=hyperion::flight::unit(hyperion::flight::cross(sun,{0,1,0})+sun*.025);
  if(stage==38)n=-sun;
  auto tangent=hyperion::flight::unit(hyperion::flight::cross({0,1,0},n));double height=stage==33?p.radius*1.8:stage==34?75:stage==35?33:stage==37?65:hyperion::flight::terrain(p,n)+5;
  f.ship.position=p.centre+n*(p.radius+height);f.orient(stage==33?-n:tangent+n*(stage==34?-.28:stage==37?-.08:.28),n);
  f.recordIds();g.toastTime=0;g.soundEvent.clear();return;
 }
 if(stage>=23){preparePlanetGraphics(g,stage);return;}
 if(stage>=12){prepareSettlementGraphics(g,stage);return;}
 g.enterFlight();g.ensureFlight();auto& f=g.s.flight;f.cancelNavigation();f.docked=false;f.dockedPort=-1;f.savedDock.clear();f.deployed=false;f.ship.velocity={};f.ship.throttle=0;f.ship.gear=false;f.ship.cruise=false;g.s.economy.docked.clear();
 auto pose=[&](int body,hyperion::flight::V3 normal,double scale){const auto& p=f.planets.at(body);normal=hyperion::flight::unit(normal);f.ship.position=p.centre+normal*p.radius*scale;f.orient(p.centre-f.ship.position,{0,1,0});};
 if(stage==0)pose(2,{.7,.3,.7},2.8);
 else if(stage==1){auto& p=f.planets[2];auto n=hyperion::flight::unit(hyperion::flight::V3{.2,.65,-.6});auto tangent=hyperion::flight::unit(hyperion::flight::cross({0,1,0},n));f.ship.position=p.centre+n*(p.radius+88);f.orient(tangent-n*.52,hyperion::flight::rotate(n,tangent,.12));f.ship.velocity=f.ship.forward*72;}
 else if(stage==2)pose(2,{-.6,.15,-.79},2.5);
 else if(stage==3)pose(4,{.65,.32,.64},2.8);
 else if(stage==4)pose(1,{.7,.3,.7},2.65);
 else if(stage==5||stage==6){
  auto it=std::find_if(g.s.economy.settlements.begin(),g.s.economy.settlements.end(),[](const Settlement& p){return p.owner==3;});
  auto id=it==g.s.economy.settlements.end()?g.s.economy.settlements.front().id:it->id;int berth=g.flightPort(id,stage==6);const auto& p=f.ports.at(berth);auto a=hyperion::flight::unit(hyperion::flight::cross({1,0,0},p.normal)),b=hyperion::flight::unit(hyperion::flight::cross(p.normal,a));
  f.target=berth;f.ship.position=p.position+p.normal*(stage==6?370:155)+a*(stage==6?180:90)+b*(stage==6?70:110);f.orient(p.position+p.normal*(stage==6?0:12)-f.ship.position,p.normal);
 }else if(stage==7&&!f.traffic.empty()){
  f.docked=true;f.update(0);f.docked=false;
  auto& t=f.traffic.front();auto forward=hyperion::flight::unit(t.forward,{0,0,-1});f.ship.position=t.position-forward*75+hyperion::flight::V3{0,17,0};f.orient(t.position-f.ship.position,{0,1,0});
 }else if(stage==8){pose(2,{.6,.7,-.4},5.);f.orient({.6,.7,-.4},{0,1,0});f.ship.velocity=f.ship.forward*420;f.ship.cruise=true;}
 else if(stage==10)pose(2,{-.35,-.82,.45},1.95);
 else if(stage==11){auto& p=f.planets[2];auto n=hyperion::flight::unit(hyperion::flight::V3{-.22,-.87,.44});auto tangent=hyperion::flight::unit(hyperion::flight::cross({0,1,0},n));f.ship.position=p.centre+n*(p.radius+hyperion::flight::terrain(p,n)+6);f.orient(tangent+n*.14,n);}
 else{g.system("HOME");g.s.selectedPlanet=2;}
 f.recordIds();g.toastTime=0;g.soundEvent.clear();
}
std::string backupPath(const std::string& path,const char* kind){auto timestamp=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();return (fs::path(path).parent_path()/(std::string(kind)+"-"+std::to_string(timestamp)+".json")).string();}
void prepareHUDGraphics(Game& g,int scene){
 if(scene==3)prepareFlightGraphics(g,70);else if(scene==4)prepareFlightGraphics(g,28);else if(scene==5||scene==6||scene==7||scene==11)prepareCombatGraphics(g,0);else prepareFlightGraphics(g,0);
 auto& f=g.s.flight;auto right=hyperion::flight::unit(hyperion::flight::cross(f.ship.forward,f.ship.up));
 if(scene==1){f.ship.assist=false;f.ship.velocity=f.ship.forward*42+right*25;}
 if(scene==2){f.ship.assist=false;f.ship.velocity=-f.ship.forward*28+f.ship.up*2;}
 if(scene==3){auto ground=f.landing();f.ship.velocity=f.ship.forward*4-ground.radial*1.2;f.ship.gear=true;}
 if(scene==4){f.ship.velocity=-f.landing().radial*7+right*4;f.ship.gear=true;}
 if(scene==5){f.ship.velocity=f.ship.forward*40;f.traffic.front().velocity=-f.ship.forward*8+right*2;}
 if(scene==6){auto& t=f.traffic.front();t.position=t.previous=f.ship.position-f.ship.forward*130+right*40;t.charge=.65;t.chargeWeapon=hyperion::flight::Seeker;}
 if(scene==11)g.system(g.s.currentStar);
 f.recordIds();g.toastTime=0;g.soundEvent.clear();
}

}
int main(int argc,char** argv){try{Options opt;
 for(int i=1;i<argc;i++){std::string a=argv[i];auto value=[&](){if(i+1>=argc)throw std::runtime_error("Missing value for "+a);return std::string(argv[++i]);};
 if(a=="--seed"){opt.seed=value();opt.fresh=true;}else if(a=="--save"){opt.save=value();opt.explicitSave=true;}else if(a=="--load")opt.load=value();else if(a=="--view")opt.view=value();else if(a=="--depth")opt.depth=std::stod(value());else if(a=="--life-zoom")opt.lifeZoom=clamp(std::stod(value()),.55,3.4);else if(a=="--frames")opt.frames=std::stoi(value());else if(a=="--size"){auto size=value();auto x=size.find('x');if(x==std::string::npos)throw std::runtime_error("Use --size WIDTHxHEIGHT");opt.width=clamp(std::stoi(size.substr(0,x)),720,3840);opt.height=clamp(std::stoi(size.substr(x+1)),450,2160);}else if(a=="--screenshot")opt.screenshot=value();else if(a=="--captures")opt.captures=value();else if(a=="--icon")opt.icon=value();else if(a=="--benchmark-report")opt.benchmarkReport=value();else if(a=="--profile-flight")opt.profileFlight=true;else if(a=="--flight-benchmark"){opt.graphicsShot=clamp(std::stoi(value()),0,50);opt.graphicsCheck=opt.benchmark=opt.fresh=opt.noSave=true;opt.frames=240;}else if(a=="--flight-shot"){opt.graphicsShot=clamp(std::stoi(value()),0,50);opt.graphicsCheck=true;opt.fresh=true;opt.noSave=true;opt.frames=8;}else if(a=="--wildlife-benchmark"){opt.wildlifeShot=clamp(std::stoi(value()),4,5);opt.wildlifeCheck=opt.graphicsCheck=opt.benchmark=opt.fresh=opt.noSave=true;opt.frames=240;}else if(a=="--wildlife-baseline")opt.wildlifeBaseline=true;else if(a=="--minor-body-benchmark"){opt.minorShot=clamp(std::stoi(value()),1,6);opt.minorCheck=opt.graphicsCheck=opt.benchmark=opt.fresh=opt.noSave=true;opt.frames=240;}else if(a=="--hud"){opt.hud=value();if(opt.hud!="minimal"&&opt.hud!="enhanced")throw std::runtime_error("Use --hud minimal|enhanced");}else if(a=="--hud-check"){opt.hudCheck=opt.graphicsCheck=opt.fresh=opt.noSave=true;opt.frames=480;}else if(a=="--close-material-check"){opt.closeMaterialCheck=opt.graphicsCheck=opt.fresh=opt.noSave=true;opt.frames=288;}else if(a=="--close-material-shot"){opt.closeMaterialShot=clamp(std::stoi(value()),0,15);opt.graphicsShot=154+opt.closeMaterialShot;opt.closeMaterialCheck=opt.graphicsCheck=opt.fresh=opt.noSave=true;opt.frames=18;}else if(a=="--close-material-benchmark"){opt.closeMaterialShot=clamp(std::stoi(value()),0,15);opt.graphicsShot=154+opt.closeMaterialShot;opt.closeMaterialCheck=opt.graphicsCheck=opt.benchmark=opt.fresh=opt.noSave=true;opt.frames=240;}else if(a=="--minor-body-check"){opt.minorCheck=opt.graphicsCheck=opt.fresh=opt.noSave=true;opt.frames=210;}else if(a=="--minor-body-shot"){opt.minorShot=clamp(std::stoi(value()),0,6);opt.minorCheck=opt.graphicsCheck=opt.fresh=opt.noSave=true;opt.frames=32;}else if(a=="--screen-fit-check"){opt.screenFitCheck=opt.graphicsCheck=opt.fresh=opt.noSave=true;opt.frames=320;}else if(a=="--mission-board"){opt.missionBoard=opt.noSave=true;}else if(a=="--helm-check"){opt.helmCheck=opt.handlingCheck=opt.graphicsCheck=opt.fresh=opt.noSave=true;opt.frames=1200;}else if(a=="--handling-check"){opt.handlingCheck=opt.graphicsCheck=opt.fresh=opt.noSave=true;opt.frames=720;}else if(a=="--wreck-check"){opt.wreckCheck=opt.graphicsCheck=opt.fresh=opt.noSave=true;opt.frames=540;}else if(a=="--ocean-survey-check"){opt.oceanCheck=opt.graphicsCheck=opt.fresh=opt.noSave=true;opt.frames=900;}else if(a=="--continuity-check"){opt.continuityCheck=opt.graphicsCheck=opt.fresh=opt.noSave=true;opt.frames=600;}else if(a=="--fieldwork-check"){opt.fieldworkCheck=opt.graphicsCheck=opt.fresh=opt.noSave=true;opt.frames=480;}else if(a=="--turn-planner-check"){opt.plannerCheck=opt.planningCheck=opt.turnCheck=opt.graphicsCheck=opt.fresh=opt.noSave=true;opt.frames=960;}else if(a=="--planning-check"){opt.planningCheck=opt.turnCheck=opt.graphicsCheck=opt.fresh=opt.noSave=true;opt.frames=480;}else if(a=="--strategy-check"){opt.turnCheck=opt.graphicsCheck=opt.fresh=opt.noSave=true;opt.frames=480;}else if(a=="--wildlife-visual-check"){opt.wildlifeCheck=opt.graphicsCheck=opt.fresh=opt.noSave=true;opt.frames=480;}else if(a=="--ecosystem-visual-check"){opt.ecosystemCheck=opt.graphicsCheck=opt.fresh=opt.noSave=true;opt.frames=180;}else if(a=="--living-visual-check"){opt.livingCheck=opt.graphicsCheck=opt.fresh=opt.noSave=true;opt.frames=480;}else if(a=="--generation-visual-check"){opt.generationCheck=opt.graphicsCheck=opt.fresh=opt.noSave=true;opt.frames=300;}else if(a=="--shipyard-visual-check"){opt.shipyardCheck=opt.graphicsCheck=opt.fresh=opt.noSave=true;opt.frames=480;}else if(a=="--security-visual-check"){opt.securityCheck=opt.graphicsCheck=opt.fresh=opt.noSave=true;opt.frames=360;}else if(a=="--combat-visual-check"){opt.combatCheck=opt.graphicsCheck=opt.fresh=opt.noSave=true;opt.frames=1440;}else if(a=="--surface-transition-check"){opt.transitionCheck=opt.graphicsCheck=opt.fresh=opt.noSave=true;opt.graphicsShot=28;opt.frames=440;}else if(a=="--architecture-visual-check"){opt.graphicsCheck=opt.architectureCheck=opt.fresh=opt.noSave=true;opt.frames=450;}else if(a=="--water-visual-check"){opt.graphicsCheck=opt.waterCheck=opt.fresh=opt.noSave=true;opt.frames=216;}else if(a=="--lighting-visual-check"){opt.graphicsCheck=opt.lightingCheck=opt.fresh=opt.noSave=true;opt.frames=270;}else if(a=="--surface-detail-check"){opt.graphicsCheck=opt.surfaceDetailCheck=opt.fresh=opt.noSave=true;opt.frames=252;}else if(a=="--planet-stability-check"){opt.graphicsCheck=opt.stabilityCheck=opt.fresh=opt.noSave=true;opt.frames=234;}else if(a=="--material-visual-check"){opt.graphicsCheck=opt.materialCheck=opt.fresh=opt.noSave=true;opt.frames=216;}else if(a=="--cloud-occlusion-check"){opt.graphicsCheck=opt.cloudOcclusionCheck=opt.fresh=opt.noSave=true;opt.frames=216;}else if(a=="--cloud-visual-check"){opt.graphicsCheck=opt.cloudCheck=opt.fresh=opt.noSave=true;opt.frames=108;}else if(a=="--planet-visual-check"){opt.graphicsCheck=opt.planetCheck=opt.fresh=opt.noSave=true;opt.frames=180;}else if(a=="--settlement-visual-check"){opt.graphicsCheck=opt.settlementCheck=opt.fresh=opt.noSave=true;opt.frames=198;}else if(a=="--flight-visual-check"){opt.graphicsCheck=true;opt.fresh=true;opt.noSave=true;opt.frames=216;}else if(a=="--flight-check"){opt.flightCheck=true;opt.fresh=true;opt.noSave=true;opt.frames=240;}else if(a=="--economy-check"){opt.economyCheck=true;opt.fresh=true;opt.noSave=true;opt.frames=180;}else if(a=="--ui-check"){opt.uiCheck=true;opt.fresh=true;opt.noSave=true;opt.frames=504;}else if(a=="--fresh")opt.fresh=true;else if(a=="--no-save")opt.noSave=true;else if(a=="--fullscreen")opt.fullscreen=true;else if(a=="--windowed")opt.fullscreen=false;else if(a=="--reduced-effects")opt.reducedEffects=true;else if(a=="--visual-check"){opt.tour=true;opt.fresh=true;opt.noSave=true;opt.frames=360;}else if(a=="--smoke"){opt.frames=24;opt.noSave=true;opt.fresh=true;}else if(a=="--help"){std::cout<<"Hyperion — native C++ exploration\n--seed TEXT --fresh --view galaxy|system|surface|ocean|flight --depth METRES --life-zoom FACTOR\n--save FILE --load FILE --no-save --frames N --size WIDTHxHEIGHT --screenshot FILE\n--hud minimal|enhanced --hud-check --close-material-check --close-material-shot 0..15 --close-material-benchmark 0..15 --minor-body-check --minor-body-benchmark 1..6 --minor-body-shot 0..6 --screen-fit-check --mission-board --helm-check --handling-check --wreck-check --ocean-survey-check --continuity-check --fieldwork-check --turn-planner-check --planning-check --strategy-check --wildlife-visual-check --ecosystem-visual-check --living-visual-check --generation-visual-check --shipyard-visual-check --security-visual-check --combat-visual-check --visual-check --economy-check --flight-check --flight-visual-check --settlement-visual-check --planet-visual-check --cloud-visual-check --cloud-occlusion-check --material-visual-check --planet-stability-check --surface-detail-check --lighting-visual-check --water-visual-check --architecture-visual-check --surface-transition-check --flight-shot 0..50 --captures DIRECTORY --smoke --fullscreen --reduced-effects\n--flight-benchmark 0..50 --benchmark-report FILE --profile-flight (no saves / no vsync)\n";return 0;}else throw std::runtime_error("Unknown argument: "+a);
 }
 if(opt.benchmark&&opt.frames<61)throw std::runtime_error("A flight benchmark needs at least 61 frames, including its 60-frame warmup");
 SaveSession saveSession(opt.save);Game game(opt.seed);std::string error;bool restored=false;
 if(!opt.load.empty()){if(!loadState(game.s,opt.load,error))throw std::runtime_error(error);restored=true;}
 else{auto source=automaticSaveSource(opt.save,opt.explicitSave?std::string{}:legacySavePath(),opt.fresh);
  if(!source.empty()){if(loadState(game.s,source,error)){restored=true;if(source!=opt.save)game.notify("Established campaign restored / Hyperion saves separately");}else game.notify("Save could not be restored / original file preserved");}
 }
 if(restored)game.universe.reset(game.s.seed,game.s.generation,game.s.legacySystems);
 if(restored&&!opt.noSave&&opt.load.empty()&&!game.s.economy.initialized&&fs::exists(opt.save)){
  auto backup=backupPath(opt.save,"before-societies");fs::copy_file(opt.save,backup);std::cout<<"Original expedition backed up to "<<backup<<'\n';
 }

 if(restored&&!opt.noSave&&opt.load.empty()&&fs::exists(opt.save)){
  std::ifstream previous(opt.save);auto json=nlohmann::json::parse(previous);if(json.value("nativeVersion",0)<5){auto backup=backupPath(opt.save,json.value("nativeVersion",0)<3?"before-frontier":json.value("nativeVersion",0)<4?"before-turns":"before-review");fs::copy_file(opt.save,backup);std::cout<<"Pre-merge expedition backed up to "<<backup<<'\n';}else if(json.contains("flight")&&json["flight"].value("schema",0)<2){auto backup=backupPath(opt.save,"before-combat");fs::copy_file(opt.save,backup);std::cout<<"Pre-combat expedition backed up to "<<backup<<'\n';}else if(json.contains("flight")&&json["flight"].value("combatRevision",0)<2){auto backup=backupPath(opt.save,"before-tactics");fs::copy_file(opt.save,backup);std::cout<<"Pre-tactics expedition backed up to "<<backup<<'\n';}else if(json.contains("economy")&&!json["economy"].contains("security")){auto backup=backupPath(opt.save,"before-factions");fs::copy_file(opt.save,backup);std::cout<<"Pre-factions expedition backed up to "<<backup<<'\n';}else if(json.contains("economy")&&!json["economy"].contains("shipyard")){auto backup=backupPath(opt.save,"before-shipyard");fs::copy_file(opt.save,backup);std::cout<<"Pre-shipyard expedition backed up to "<<backup<<'\n';}else if(!json.contains("generation")){auto backup=backupPath(opt.save,"before-procedural");fs::copy_file(opt.save,backup);std::cout<<"Established worlds backed up to "<<backup<<'\n';}else if(json.value("lifeRevision",0)<1){auto backup=backupPath(opt.save,"before-living-universe");fs::copy_file(opt.save,backup);std::cout<<"Established inhabitants backed up to "<<backup<<'\n';}else if(json.contains("economy")&&!json["economy"].contains("ecosystems")){auto backup=backupPath(opt.save,"before-ecosystems");fs::copy_file(opt.save,backup);std::cout<<"Pre-ecosystem campaign backed up to "<<backup<<'\n';}
 }
 game.adoptGeneration();
 if(opt.flightCheck){game.enterFlight();game.s.flight.dockAt(game.flightPort(game.s.economy.settlements.front().id,false));game.processFlightEvents();}
 if(error.empty()&&!restored&&!opt.flightCheck&&!opt.graphicsCheck&&!opt.economyCheck&&!opt.tour&&!opt.uiCheck&&opt.view.empty()){
  game.enterFlight();for(auto& site:game.s.economy.settlements)if(site.owner==3){int berth=game.flightPort(site.id,false);game.s.flight.dockAt(berth);game.s.flight.target=game.flightPort(site.id,true);game.s.flight.recordIds();game.processFlightEvents();break;}
  game.notify("Welcome aboard NOMA / Enter launches / J flies to the orbital exchange / O opens strategy");
 }
 if(opt.economyCheck)prepareEconomyCheck(game);if(!opt.view.empty())selectView(game,opt.view);if(opt.depth>=0&&game.s.mode==Mode::Ocean)game.s.depth=game.s.targetDepth=clamp(opt.depth,20.,game.maxDepth());
 if(opt.frames)SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN);
 SetConfigFlags(FLAG_WINDOW_RESIZABLE|FLAG_MSAA_4X_HINT|FLAG_WINDOW_HIGHDPI|(opt.benchmark?0:FLAG_VSYNC_HINT));SetTraceLogLevel(LOG_WARNING);InitWindow(opt.width,opt.height,"HYPERION");SetWindowMinSize(720,450);SetExitKey(KEY_NULL);SetTargetFPS(opt.benchmark?0:60);
 WindowFit windowFit;windowFit.update(true,true);if(opt.fullscreen)windowFit.toggle();NativePointer pointerEvents;
 Renderer renderer;if(!renderer.init()){std::cerr<<"Native graphics initialization failed\n";renderer.close();pointerEvents.detach();CloseWindow();return 2;}if(!opt.icon.empty()){bool result=renderer.makeIcon(opt.icon);renderer.close();pointerEvents.detach();CloseWindow();return result?0:3;}renderer.profileFlight(opt.profileFlight);renderer.hideWildlifeForAudit(opt.wildlifeBaseline);if(opt.hudCheck||opt.closeMaterialCheck||opt.minorCheck||opt.generationCheck||opt.stabilityCheck||opt.surfaceDetailCheck||opt.lightingCheck||opt.waterCheck||opt.architectureCheck||opt.cloudOcclusionCheck)renderer.freezeFlightVisualTime(42.);Audio audio;UI ui;ui.auditInterface=opt.plannerCheck||opt.fieldworkCheck||opt.continuityCheck||opt.oceanCheck||opt.wreckCheck;const auto displayPath=fs::path(defaultSavePath()).parent_path()/"display.txt";ui.enhancedHUD=opt.hud.empty()?(!opt.noSave&&loadEnhancedHUD(displayPath)):opt.hud=="enhanced";if(opt.missionBoard){ui.panel=true;ui.tab=5;ui.expanded={"missions"};}ui.reduced=opt.reducedEffects;ui.seedInput=game.s.seed;ui.savePath=opt.save;if(opt.lifeZoom>0){ui.surfaceZoom=clamp(opt.lifeZoom,1.,3.4);ui.oceanZoom=clamp(opt.lifeZoom,.55,2.1);}
 bool quit=false,drag=false,dragMoved=false;Vector2 dragStart{};V2 dragCamera{};double lastClick=-2,lastSave=0,runtime=0;auto runStart=std::chrono::steady_clock::now();int frame=0;bool allowSave=!opt.noSave&&error.empty();
 if(!error.empty())ui.saveWarning="AUTOSAVE DISABLED / original save could not be restored; import a valid save or export progress";
 auto save=[&](){
  if(!allowSave)return;std::string recovery;auto result=saveSession.save(game.s,error,recovery);
  if(result!=SaveResult::Saved){ui.saveWarning=error;game.notify(error,Notice::Important);}
  else if(!saveSession.redirected())ui.saveWarning.clear();
 };
 auto toggleHUD=[&](){ui.toggleFlightHUD();ui.menu=ui.sections=false;if(!opt.noSave&&!saveEnhancedHUD(displayPath,ui.enhancedHUD))game.notify("HUD changed / display preference could not be stored");};
 bool orderCommitted=false;
 auto order=[&](const std::string& action,const std::string& arg=std::string{}){if(orderCommitted&&!Game::planningAction(action))return false;double before=game.s.hours;auto previousGoal=game.diveState.goal.id;auto previousMode=game.s.mode;bool result=game.issueOrder(action,arg,ui.paused||ui.help||ui.settings);if(game.s.mode==Mode::Ocean&&(previousMode!=Mode::Ocean||action=="dive"||action=="oceanSelect"||action=="oceanNext"||(action=="oceanProceed"&&previousGoal!=game.diveState.goal.id))){ui.panel=false;ui.divePage=0;ui.scroll=0;}if(game.s.hours!=before){orderCommitted=true;if(game.turnBased()&&ui.panel&&ui.expanded.erase("turn-planner")){ui.expanded.insert("turn-report");ui.scroll=0;}}return result;};
 auto exportSave=[&](const char* kind){auto file=backupPath(opt.save,kind);if(saveState(game.s,file,error)){game.notify("Save exported / "+fs::path(file).filename().string());std::cout<<"Exported "<<file<<'\n';return true;}game.notify("Export failed / "+error);return false;};
 auto preserveCampaign=[&](const char* kind){
  try{if(!opt.noSave&&fs::exists(opt.save)){auto file=backupPath(opt.save,kind);fs::copy_file(opt.save,file);std::cout<<"Campaign file preserved at "<<file<<'\n';}return true;}
  catch(const std::exception& e){game.notify("Campaign backup failed / "+std::string(e.what()),Notice::Important);return false;}
 };
 auto importSave=[&](const std::string& file){State restoredState;if(!loadState(restoredState,file,error)){game.notify("Import rejected / "+error);return;}if(!exportSave("before-import")||!preserveCampaign("disk-before-import"))return;game.s=std::move(restoredState);game.universe.reset(game.s.seed,game.s.generation,game.s.legacySystems);game.adoptGeneration();game.lastTurn={};game.recentTurns.clear();game.chartRecall={};game.surveyRecall={};game.diveState={};game.diveReport={};game.diveDebrief.clear();game.focusedContact.clear();game.transferNotice.clear();ui.reportIndex=0;ui.workspaceMemory.clear();game.radar=-1;ui.seedInput=game.s.seed;ui.settings=false;ui.scroll=0;allowSave=!opt.noSave&&saveSession.acknowledge(error);ui.saveWarning=(!allowSave&&!opt.noSave)?"AUTOSAVE DISABLED / "+error:"";game.toastTime=0;game.notify("Exploration record restored");save();};
 nlohmann::json handlingScenes=nlohmann::json::array();hyperion::flight::V3 handlingVelocity,handlingPosition,handlingUp;double handlingFuel=0;
 nlohmann::json wreckScenes=nlohmann::json::array();std::string wreckId,wreckJob;double wreckStart=0,wreckFrozen=0,wreckHull=0,wreckQuality=0,wreckCredits=0;Stock wreckCargo{};int wreckCrew=0;
 nlohmann::json oceanScenes=nlohmann::json::array();double oceanFrozen=0,oceanStart=0,oceanDepth=0;Stock oceanCargo{};std::string oceanGoal,oceanExpectedNext;MovePlan oceanExpectedMove;
 nlohmann::json continuityScenes=nlohmann::json::array();double continuityFrozen=0,continuityStart=0,continuityTargetX=0,continuityTargetDepth=0;V2 continuityWaypoint{};std::string continuityContact;
 nlohmann::json fieldScenes=nlohmann::json::array();double fieldFrozenHour=0,fieldStartHour=0;size_t fieldReportCount=0;std::string fieldId;FieldQuote fieldExpected;
 nlohmann::json turnScenes=nlohmann::json::array();double turnFrozenHour=0;bool workspaceRestored=false;std::string plannerExpectedAction,plannerExpectedArg;
 nlohmann::json generationScenes=nlohmann::json::array();
 nlohmann::json shipyardScenes=nlohmann::json::array();
 nlohmann::json closeMaterialScenes=nlohmann::json::array();
 nlohmann::json hudScenes=nlohmann::json::array();double hudHours=0,hudFlightTime=0;hyperion::flight::V3 hudPosition;
 nlohmann::json minorScenes=nlohmann::json::array();
 nlohmann::json screenFitScenes=nlohmann::json::array();
 nlohmann::json securityScenes=nlohmann::json::array();
 nlohmann::json combatScenes=nlohmann::json::array();double combatStartTime=0,combatStartStock=0;bool combatSawEvasion=false;
 nlohmann::json stabilityScenes=nlohmann::json::array(),waterScenes=nlohmann::json::array(),architectureScenes=nlohmann::json::array(),cloudOcclusionScenes=nlohmann::json::array();std::vector<double> benchmarkSamples;std::array<double,6> profileTotals{};bool transitionLanded=false,transitionDeparted=false;double contactStep=0,contactTurn=0;
 while(!quit&&!WindowShouldClose()){
 auto frameStart=std::chrono::steady_clock::now();windowFit.update();auto viewport=uiViewport();auto uiCursor=uiMouse();if(IsWindowResized()){drag=false;dragMoved=false;}
 double dt=opt.frames?1./60:clamp(double(GetFrameTime()),1./240,.07);runtime+=dt;orderCommitted=false;ui.action.clear();ui.arg.clear();ui.clickConsumed=false;if(!IsMouseButtonDown(MOUSE_BUTTON_LEFT))ui.pointerCaptured=false;auto pointerFrame=pointerEvents.next();ui.pointerPressed=pointerFrame.pressed;ui.pointerReleased=pointerFrame.released;ui.pressPosition=uiPoint({float(pointerFrame.press.x),float(pointerFrame.press.y)});ui.releasePosition=uiPoint({float(pointerFrame.release.x),float(pointerFrame.release.y)});
 std::set<int> frameKeys;for(int key;(key=GetKeyPressed())>0;)frameKeys.insert(key);
 auto controlsHeld=[&](){
  for(int key:{KEY_W,KEY_A,KEY_S,KEY_D,KEY_Q,KEY_E,KEY_R,KEY_F,KEY_UP,KEY_DOWN,KEY_LEFT,KEY_RIGHT,KEY_ENTER,KEY_SPACE,KEY_Z,KEY_Y,KEY_LEFT_SHIFT,KEY_RIGHT_SHIFT,KEY_LEFT_ALT,KEY_RIGHT_ALT,KEY_K,KEY_O,KEY_M,KEY_G,KEY_L,KEY_TAB})if(IsKeyDown(key))return true;
  return IsMouseButtonDown(MOUSE_BUTTON_LEFT)||IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
 };
 ui.observeMode(int(game.s.mode),dt,controlsHeld());Mode inputMode=game.s.mode;
 std::set<int> diagnosticKeys;if(opt.continuityCheck&&frame%60==26){int stage=frame/60;if(stage==2||stage==5)diagnosticKeys.insert(KEY_K);if(stage==4||stage==6)diagnosticKeys.insert(KEY_L);}
 if(opt.wreckCheck&&frame%60==26){int stage=frame/60;if(stage==1||stage==2||stage==6)diagnosticKeys.insert(KEY_ENTER);if(stage==3)diagnosticKeys.insert(KEY_C);if(stage==5)diagnosticKeys.insert(KEY_K);}
 if(opt.oceanCheck){int stage=frame/60,phase=frame%60;if(stage==4&&(phase==26||phase==46))diagnosticKeys.insert(KEY_K);if((stage==3||stage==5||stage==6||stage==7||stage==9||stage==11||stage==12||stage==14)&&phase==26)diagnosticKeys.insert(KEY_ENTER);if((stage==2||stage==13)&&phase==26)diagnosticKeys.insert(KEY_SPACE);if(stage==13&&phase==26)diagnosticKeys.insert(KEY_LEFT_SHIFT);if(stage==13&&phase==31)diagnosticKeys.insert(KEY_I);}
 auto rawKeyPressed=[&](int key){return diagnosticKeys.count(key)||(!opt.graphicsCheck&&(frameKeys.count(key)>0||IsKeyPressed(key)));};
 auto keyPressed=[&](int key){return ui.handoff.ready()&&game.s.mode==inputMode&&rawKeyPressed(key);};
 bool command=IsKeyDown(KEY_LEFT_CONTROL)||IsKeyDown(KEY_RIGHT_CONTROL)||IsKeyDown(KEY_LEFT_SUPER)||IsKeyDown(KEY_RIGHT_SUPER);
 if(command&&rawKeyPressed(KEY_Q)){quit=true;break;}
 if(rawKeyPressed(KEY_F11)){windowFit.toggle();viewport=uiViewport();uiCursor=uiMouse();ui.pressPosition=uiPoint({float(pointerFrame.press.x),float(pointerFrame.press.y)});ui.releasePosition=uiPoint({float(pointerFrame.release.x),float(pointerFrame.release.y)});}if(rawKeyPressed(KEY_F12)){auto p=fs::path(backupPath(opt.save,"hyperion"));p.replace_extension(".png");renderer.screenshot(p.string());game.notify("Screenshot saved / "+p.filename().string());}
 if((keyPressed(KEY_F10)||(keyPressed(KEY_M)&&game.s.mode!=Mode::Flight))&&!ui.settings&&!command){ui.sound=audio.toggle();if(!ui.sound)game.notify("Sound off");else game.notify("Synthesized sonar audio enabled");}
 if(rawKeyPressed(KEY_ESCAPE)){
  if(ui.menu||ui.sections){ui.menu=ui.sections=false;}
  else if(ui.settings||ui.help){ui.settings=false;ui.help=false;}
  else if(ui.panel)ui.panel=false;else if(game.s.mode==Mode::Flight)ui.paused=!ui.paused;else if(game.s.mode==Mode::Ocean||game.s.mode==Mode::Surface)order("system");else if(game.hasExpedition())order("expedition");else if(game.s.mode==Mode::System)order("galaxy");else ui.paused=!ui.paused;save();
 }
 if(ui.settings){int code;while((code=GetCharPressed())>0){if(code>=32&&code<=126&&ui.seedInput.size()<80)ui.seedInput.push_back(char(code));}if(keyPressed(KEY_BACKSPACE)&&!ui.seedInput.empty())ui.seedInput.pop_back();}
 else if(!ui.help&&!command){
  auto piloting=[&](){return game.s.mode==Mode::Flight;};
  if(keyPressed(KEY_K)){order(piloting()?"strategy":game.diveReturnNeeded()?(game.s.mode==Mode::Ocean?"oceanReturn":"expedition"):"cockpit");ui.menu=ui.sections=false;save();}
  if(keyPressed(KEY_F8)&&piloting())toggleHUD();
  if(keyPressed(KEY_F9)){ui.toggleInterface();}
  if(keyPressed(KEY_TAB)){if(piloting())order("flightNext");else{ui.toggleInterface();}}
  if(keyPressed(KEY_P))ui.paused=!ui.paused;
  if(keyPressed(KEY_H)||keyPressed(KEY_SLASH))ui.help=true;
  if(keyPressed(KEY_B))ui.openPanel(4,true);
  if(keyPressed(KEY_N)){bool shift=IsKeyDown(KEY_LEFT_SHIFT)||IsKeyDown(KEY_RIGHT_SHIFT);order(piloting()||shift?"wait":"endTurn");save();}
  if(keyPressed(KEY_I)){if(game.s.mode==Mode::Ocean){ui.openPanel(0,ui.divePage==2);ui.divePage=2;ui.scroll=0;}else ui.openPanel(0,true);}
  if(keyPressed(KEY_R)&&!piloting()){if(game.s.mode==Mode::Ocean){ui.openPanel(0,ui.divePage==0);ui.divePage=0;}else{ui.openPanel(0);ui.expanded.erase("turn-report");ui.expanded.erase("before-turn");ui.expanded.erase("turn-planner");ui.expanded.insert("play-turn");}ui.scroll=0;}
  if(keyPressed(KEY_G)&&piloting())order("gear");
  if(keyPressed(KEY_G)&&!piloting()){order("galaxy");ui.scroll=0;save();}
  if((keyPressed(KEY_O)||(keyPressed(KEY_M)&&piloting()))){order(piloting()?"strategy":"system");save();}
  if(keyPressed(KEY_F)&&!piloting()){order("refuel");save();}
  if(piloting()){
   if(keyPressed(KEY_ENTER)){order("flightDock");save();}
   for(int i=0;i<4;i++)if(keyPressed(KEY_ONE+i))order("weapon",std::to_string(hyperion::flight::primaryWeapon(i)));
   if(keyPressed(KEY_U))order("combatFocus");
   if(keyPressed(KEY_T))order("combatTarget");
   if(keyPressed(KEY_J)){order("navigate");if(game.s.flight.autopilot)ui.panel=false;save();}
   if(keyPressed(KEY_L)){order("deploy");save();}
   if(keyPressed(KEY_C))order("cruise");
   if(keyPressed(KEY_X))order("assist");
   if(keyPressed(KEY_BACKSPACE))order("brake");
   if(keyPressed(KEY_E)&&game.s.flight.docked){ui.openPanel(5,true);ui.settlement=game.s.economy.docked;}
  }
  if(keyPressed(KEY_X)&&game.s.mode==Mode::Ocean){auto b=game.diveBrief();if(b.goal.kind=="wreck")order("wreckCut",b.goal.id);save();}
  if(keyPressed(KEY_C)&&game.s.mode==Mode::Ocean){auto b=game.diveBrief();if(b.hasGoal)order(b.goal.kind=="wreck"?"wreckSalvage":"fieldCollect",b.goal.id);save();}
  if(keyPressed(KEY_U)&&!piloting()){order("launch");save();}
  if(keyPressed(KEY_L)&&!piloting())order("expedition");
  if(keyPressed(KEY_J)&&game.s.mode==Mode::Galaxy){order("jump");save();}
  if(keyPressed(KEY_ENTER)&&!piloting()){if(game.s.mode==Mode::Galaxy)order("system",game.s.selectedStar);else if(game.s.mode==Mode::System)order(game.minorSelection()?"plotMinor":"land");else if(game.s.mode==Mode::Ocean&&!IsKeyDown(KEY_LEFT_SHIFT)&&!IsKeyDown(KEY_RIGHT_SHIFT)){auto next=game.nextDiveOrder();if(next.available)order(next.action,next.arg);else game.notify(next.text);}else if(game.s.mode==Mode::Surface||game.s.mode==Mode::Ocean)order(IsKeyDown(KEY_LEFT_SHIFT)||IsKeyDown(KEY_RIGHT_SHIFT)?"fieldTravel":"moveWaypoint");else if(ui.paused)ui.paused=false;save();}
  if(keyPressed(KEY_E)&&!piloting()){if(game.s.mode==Mode::Surface)order("dive");else if(game.s.mode==Mode::Ocean)order(game.s.depth>80?"oceanAscend":"surface");save();}
  if(keyPressed(KEY_SPACE)&&!piloting()){if(game.s.mode==Mode::System)order("orbitScan");else if(game.s.mode==Mode::Ocean&&(IsKeyDown(KEY_LEFT_SHIFT)||IsKeyDown(KEY_RIGHT_SHIFT)||diagnosticKeys.count(KEY_LEFT_SHIFT)))order("oceanListen");else order("scan");save();}
  if(keyPressed(KEY_V))ui.reduced=!ui.reduced;

 }
 if(command&&keyPressed(KEY_S)&&!ui.settings)exportSave("hyperion-export");
 if(IsFileDropped()){auto files=LoadDroppedFiles();if(files.count>0)importSave(files.paths[0]);UnloadDroppedFiles(files);}
 ui.observeMode(int(game.s.mode),0,controlsHeld());
 float wheel=GetMouseWheelMove();bool inPanel=ui.panel&&uiCursor.x>UILayout::panelLeft(float(viewport.width))&&uiCursor.y>=UILayout::header;
 if(inPanel&&!ui.menu&&!ui.sections)ui.scroll=std::max(0.f,ui.scroll-wheel*45);else if(!ui.settings&&!ui.help&&!ui.menu&&!ui.sections){double factor=std::pow(1.12,double(wheel));if(keyPressed(KEY_EQUAL)||keyPressed(KEY_KP_ADD))factor*=1.18;if(keyPressed(KEY_MINUS)||keyPressed(KEY_KP_SUBTRACT))factor/=1.18;
  if(game.s.mode==Mode::Galaxy){double old=game.s.galaxyZoom,next=clamp(old*factor,.2,4.5);game.s.galaxy.x+=(uiCursor.x-viewport.width*.5)*(1/old-1/next);game.s.galaxy.y+=(uiCursor.y-viewport.height*.5)*(1/old-1/next);game.s.galaxyZoom=next;}
  else if(game.s.mode==Mode::Surface)ui.surfaceZoom=clamp(ui.surfaceZoom*factor,1.,3.4);else if(game.s.mode==Mode::Ocean)ui.oceanZoom=clamp(ui.oceanZoom*factor,.55,2.1);
 }
 bool pendingWorldClick=false;Vector2 mouse=ui.pointerReleased?ui.releasePosition:uiCursor;auto pressPoint=ui.pointerPressed?ui.pressPosition:mouse;bool worldArea=ui.handoff.ready()&&game.s.mode!=Mode::Flight&&(ui.clean||(pressPoint.y>UILayout::header+8&&pressPoint.y<viewport.height-(game.s.mode==Mode::Ocean?84:UILayout::footer)))&&!(ui.panel&&pressPoint.x>UILayout::panelLeft(float(viewport.width))&&pressPoint.y>=UILayout::header)&&!ui.settings&&!ui.help&&!ui.menu&&!ui.sections&&!ui.paused;
 if(ui.pointerPressed&&worldArea){drag=true;dragMoved=false;dragStart=ui.pressPosition;dragCamera=game.s.galaxy;}
 if(drag&&(IsMouseButtonDown(MOUSE_BUTTON_LEFT)||ui.pointerReleased)){if(std::hypot(mouse.x-dragStart.x,mouse.y-dragStart.y)>5)dragMoved=true;if(dragMoved&&game.s.mode==Mode::Galaxy){game.s.galaxy={dragCamera.x-(mouse.x-dragStart.x)/game.s.galaxyZoom,dragCamera.y-(mouse.y-dragStart.y)/game.s.galaxyZoom};}}
 if(ui.pointerReleased&&drag){pendingWorldClick=!dragMoved;drag=false;}
 if(!ui.settings&&!ui.help&&!ui.paused&&!ui.panel&&!ui.menu&&!ui.sections&&!command&&IsWindowFocused()&&ui.handoff.ready()&&(game.s.mode==Mode::Surface||game.s.mode==Mode::Ocean)){
  V2 step{double((keyPressed(KEY_D)||keyPressed(KEY_RIGHT)?1:0)-(keyPressed(KEY_A)||keyPressed(KEY_LEFT)?1:0)),double((keyPressed(KEY_S)||keyPressed(KEY_DOWN)?1:0)-(keyPressed(KEY_W)||keyPressed(KEY_UP)?1:0))};
  if(std::hypot(step.x,step.y)>0){if(!orderCommitted&&game.moveTurn(step)){orderCommitted=true;save();}}
 }
 V2 movement;if(!opt.graphicsCheck&&ui.handoff.ready()&&!ui.settings&&!ui.help&&!ui.paused&&IsWindowFocused()){movement.x=(IsKeyDown(KEY_D)||IsKeyDown(KEY_RIGHT)?1:0)-(IsKeyDown(KEY_A)||IsKeyDown(KEY_LEFT)?1:0);movement.y=(IsKeyDown(KEY_S)||IsKeyDown(KEY_DOWN)?1:0)-(IsKeyDown(KEY_W)||IsKeyDown(KEY_UP)?1:0);hyperion::flight::Input input;
  if(game.s.mode==Mode::Flight&&!ui.panel&&!ui.menu&&!ui.sections&&!command){
   input.pitch=(IsKeyDown(KEY_DOWN)?1:0)-(IsKeyDown(KEY_UP)?1:0);input.yaw=(IsKeyDown(KEY_RIGHT)?1:0)-(IsKeyDown(KEY_LEFT)?1:0);
   input.roll=(IsKeyDown(KEY_Q)?1:0)-(IsKeyDown(KEY_E)&&!game.s.flight.docked?1:0);input.throttle=(IsKeyDown(KEY_W)?1:0)-(IsKeyDown(KEY_S)?1:0);
   input.lift=(IsKeyDown(KEY_R)?1:0)-(IsKeyDown(KEY_F)?1:0);input.strafe=(IsKeyDown(KEY_D)?1:0)-(IsKeyDown(KEY_A)?1:0);
   input.missile=IsKeyDown(KEY_Z);input.decoy=IsKeyDown(KEY_Y);
   input.precision=IsKeyDown(KEY_LEFT_ALT)||IsKeyDown(KEY_RIGHT_ALT);input.boost=IsKeyDown(KEY_LEFT_SHIFT)||IsKeyDown(KEY_RIGHT_SHIFT);input.fire=IsKeyDown(KEY_SPACE)||(!ui.pointerCaptured&&uiCursor.y>180&&uiCursor.y<UILayout::cockpitTop(viewport.width,viewport.height)&&IsMouseButtonDown(MOUSE_BUTTON_LEFT));
   if(IsMouseButtonDown(MOUSE_BUTTON_RIGHT)){auto delta=GetMouseDelta();auto look=hyperion::flight::pointerLook(delta.x,delta.y,dt);input.yaw=look.yaw;input.pitch=look.pitch;}
  }
  double priorClock=game.s.flight.fireClock,priorShield=game.s.flight.ship.shield,priorHull=game.s.hull;int priorMissiles=game.s.flight.ship.combat.missiles,priorDecoys=game.s.flight.ship.combat.decoys,priorKills=game.s.flight.kills;
  game.simulate(dt,movement,input);
  if(game.s.mode==Mode::Flight&&audio.active()){auto& f=game.s.flight;const char* shots[]={"pulse","kinetic","plasma","seeker","resonance"};if(f.fireClock>priorClock+.001)audio.play(shots[f.ship.combat.weapon]);if(f.ship.combat.missiles<priorMissiles)audio.play("seeker");if(f.ship.combat.decoys<priorDecoys)audio.play("decoy");if(f.ship.shield<priorShield||game.s.hull<priorHull)audio.play("impact");if(f.kills>priorKills)audio.play("explosion");}
 }
 if(opt.tour){if(frame==0)game.galaxy();if(frame==90)game.system("HOME");if(frame==180){game.s.selectedPlanet=2;game.descend();}if(frame==270)game.dive();}
 if(opt.uiCheck&&frame%21==0){int stage=frame/21;ui.help=ui.settings=ui.menu=ui.sections=ui.paused=false;ui.panel=true;ui.scroll=0;ui.expanded.clear();
  if(stage==0){selectView(game,"ocean");ui.tab=0;}
  else if(stage==1)ui.tab=1;
  else if(stage==2){ui.tab=2;ui.researchPage=0;}
  else if(stage==3){ui.tab=2;ui.researchPage=1;}
  else if(stage==4){ui.tab=6;ui.factionChoice=4;}
  else if(stage==5)ui.tab=7;
  else if(stage==6){ui.panel=false;ui.menu=true;}
  else if(stage==7){ui.tab=0;ui.sections=true;}
  else if(stage==8){ui.panel=false;ui.help=true;}
  else if(stage==9){ui.panel=false;ui.settings=true;}
  else if(stage==10){game.reset(game.s.seed);game.enterFlight();auto site=std::find_if(game.s.economy.settlements.begin(),game.s.economy.settlements.end(),[](const Settlement& s){return s.owner==3;});game.s.flight.dockAt(game.flightPort(site->id,false));game.processFlightEvents();ui.panel=false;}
  else if(stage==11)ui.tab=0;
  else if(stage==12){ui.tab=5;ui.settlement=game.s.economy.docked;}
  else if(stage==13){ui.tab=4;ui.expanded.insert("sites");}
  else if(stage==14){ui.tab=6;ui.factionChoice=2;}
  else if(stage==15){ui.panel=false;ui.menu=true;}
  else if(stage==16){preparePlanetGraphics(game,28);ui.panel=false;}
  else if(stage==17){game.system("HOME");game.s.selectedPlanet=2;ui.panel=false;}
  else if(stage==18){game.galaxy();ui.panel=false;}
  else if(stage==19){selectView(game,"surface");ui.panel=false;}
  else if(stage==20){prepareEconomyCheck(game);ui.tab=4;ui.settlement=game.s.economy.settlements.back().id;ui.expanded={"operations","construction","warehouse"};}
  else if(stage==21){ui.tab=5;ui.settlement=game.s.economy.docked;ui.expanded={"services","cargo","contracts","freight"};ui.scroll=350;}
  else if(stage==22){ui.tab=4;ui.settlement=game.s.economy.settlements.back().id;ui.expanded={"construction","facilities"};ui.scroll=300;}
  else{ui.panel=false;ui.menu=true;ui.paused=true;}
 }

 if(opt.economyCheck){int stage=frame/15;ui.panel=stage<10;ui.help=ui.settings=false;
  auto& sites=game.s.economy.settlements;ui.settlement=sites.back().id;
  if(stage==0){ui.tab=4;ui.scroll=0;}else if(stage==1){ui.tab=4;ui.scroll=570;}else if(stage==2){ui.tab=4;ui.scroll=1200;}
  else if(stage==3){ui.settlement=sites.front().id;if(frame%15==0){game.system("HOME");game.dock(sites.front().id);}ui.tab=5;ui.scroll=0;}else if(stage==4){if(frame%15==0){game.system("HOME");game.dock(sites.back().id);}ui.tab=5;ui.scroll=680;}else if(stage==5){ui.tab=5;ui.scroll=1500;}
  else if(stage==6){ui.tab=6;ui.scroll=0;}else if(stage==7){ui.tab=6;ui.scroll=680;}else if(stage==8){ui.tab=7;ui.scroll=0;}else if(stage==9){ui.tab=2;ui.scroll=220;}
  else{const auto& site=sites.back();game.s.mode=Mode::Surface;game.s.activeStar=site.star;game.s.activePlanet=site.planet;game.s.surface=game.s.waypoint=site.position;}
 }
 if(opt.flightCheck){
  int stage=frame/30;ui.help=ui.settings=false;ui.panel=stage==1||stage==2||stage==5||stage==6;
  if(stage==0)game.s.mode=Mode::Flight;
  if(stage==1){ui.tab=0;ui.scroll=0;}
  if(stage==2){ui.tab=5;ui.scroll=0;ui.settlement=game.s.economy.docked;}
  if(stage==3&&frame%30==0){game.action("flightNext");game.action("navigate");}
  if(stage==4&&frame%30==0){auto& site=game.s.economy.settlements[1];game.s.flight.dockAt(game.flightPort(site.id,true));game.processFlightEvents();game.s.mode=Mode::Flight;}
  if(stage==5){ui.tab=4;ui.scroll=0;}
  if(stage==6){ui.tab=6;ui.scroll=0;}
  if(stage==7&&frame%30==0){auto& f=game.s.flight;auto& p=f.planets[2];f.cancelNavigation();f.docked=false;f.savedDock.clear();f.ship.position=p.centre+p.portNormal*(p.radius+1800);f.ship.velocity={};f.ship.throttle=0;f.orient(p.centre-f.ship.position,{0,1,0});game.s.economy.docked.clear();game.s.mode=Mode::Flight;}

 }
 if(opt.graphicsCheck&&!opt.oceanCheck&&!opt.wreckCheck&&!opt.continuityCheck&&!opt.fieldworkCheck&&!opt.hudCheck&&!opt.minorCheck&&!opt.screenFitCheck&&!opt.handlingCheck&&!opt.combatCheck&&!opt.securityCheck&&!opt.shipyardCheck&&!opt.generationCheck&&!opt.livingCheck&&!opt.ecosystemCheck&&!opt.wildlifeCheck&&!opt.turnCheck){int stage=opt.graphicsShot>=0?opt.graphicsShot:opt.closeMaterialCheck?154+frame/18:opt.cloudOcclusionCheck?140+frame/18:(opt.architectureCheck?112:opt.waterCheck?96:opt.lightingCheck?78:opt.surfaceDetailCheck?64:opt.stabilityCheck?51:opt.materialCheck?39:opt.cloudCheck?33:opt.planetCheck?23:opt.settlementCheck?12:0)+frame/18;if(frame==0||(opt.graphicsShot<0&&frame%18==0))prepareFlightGraphics(game,stage);if(stage>=12&&!opt.closeMaterialCheck&&!opt.stabilityCheck&&!opt.surfaceDetailCheck&&!opt.lightingCheck&&!opt.waterCheck&&!opt.architectureCheck&&!opt.cloudOcclusionCheck){auto before=game.s.flight.ship;bool docked=game.s.flight.docked;game.simulate(opt.transitionCheck?.05:dt);
   if(opt.transitionCheck){auto& f=game.s.flight;if(!docked&&f.docked){transitionLanded=true;contactStep=hyperion::flight::length(f.ship.position-before.position);contactTurn=std::acos(clamp(hyperion::flight::dot(f.ship.forward,before.forward),-1.,1.))*180/pi;}
    if(transitionLanded&&!transitionDeparted&&f.touchdownAge>.6){game.action("flightDock");transitionDeparted=true;}
    if(frame%40==0)std::cout<<"Surface transition / frame "<<frame<<" / clearance "<<f.altitude()<<" / speed "<<f.speed()<<" / docked "<<f.docked<<" / departure "<<f.launchAssist<<'\n';
   }
  }ui.panel=ui.help=ui.settings=false;game.toastTime=0;game.s.flight.noticeAge=10;}
 if(opt.architectureCheck&&(frame/18==18||frame/18==19||frame/18==21||frame/18==23)){
  int phase=frame%18;double step=phase>=1&&phase<=8?.0005:phase>=9&&phase<=16?-.0005:0.;
  auto& f=game.s.flight;f.ship.position+=hyperion::flight::unit(hyperion::flight::cross(f.ship.forward,f.ship.up))*step;
 }
 if(opt.stabilityCheck){int scene=frame/18,body=scene<12?scene%6:2;auto& f=game.s.flight;const auto& p=f.planets.at(body);double distance=scene<6?9000.:28000.;if(scene==12)distance=p.radius*std::min(opt.reducedEffects?810:1080,GetRenderHeight())/(.7002*130.)*(frame%2?1.002:.998);auto n=hyperion::flight::rotate(hyperion::flight::unit(hyperion::flight::V3{.6,.7,-.4}),{0,1,0},(frame%18-8.5)*.00012);f.ship.position=p.centre+n*distance;f.orient(p.centre-f.ship.position,{0,1,0});ui.clean=true;
  if(frame%18==0)stabilityScenes.push_back({{"scene",scene},{"body",body},{"name",p.name},{"distance",distance},{"radiusPixels",p.radius*GetRenderHeight()/(1.400415*std::sqrt(distance*distance-p.radius*p.radius))},{"cropSize",640},{"visualTime",42.},{"kind",p.kind},{"meshLevels",nlohmann::json::array()}});
 }
 if(opt.closeMaterialCheck||opt.surfaceDetailCheck||opt.lightingCheck||opt.waterCheck||opt.architectureCheck||opt.cloudOcclusionCheck)ui.clean=true;
 if(opt.cloudOcclusionCheck){
  auto& f=game.s.flight;const auto& p=f.planets[2];auto n=hyperion::flight::unit(f.ship.position-p.centre),tangent=hyperion::flight::unit(hyperion::flight::cross({0,1,0},n));
  double radius=hyperion::flight::length(f.ship.position-p.centre);n=hyperion::flight::unit(n*radius+tangent*1.7);
  f.ship.position=p.centre+n*std::max(radius,p.radius+hyperion::flight::terrain(p,n)+3.2);
 }
 renderer.auditCloudOcclusion(opt.cloudOcclusionCheck&&frame%18==17);
 if(opt.waterCheck){
  double seconds=frame/18==10?44.:42.;renderer.freezeFlightVisualTime(seconds);
  if(frame%18==0){const auto& f=game.s.flight;const auto& p=f.planets.at(game.s.selectedPlanet);auto n=hyperion::flight::unit(f.ship.position-p.centre);
   waterScenes.push_back({{"scene",frame/18},{"name",p.name},{"kind",p.kind},{"water",p.water},{"temperature",p.temperature},{"atmosphere",p.atmosphere},{"latitude",n.y},{"elevation",hyperion::flight::materialElevation(p,n)},{"sea",.28+.44*p.water},{"clearance",f.altitude()},{"visualTime",seconds}});
  }
 }
 if(opt.handlingCheck){int scene=frame/120,phase=frame%120;
  if(phase==0){
   prepareFlightGraphics(game,6);auto& f=game.s.flight;const auto& p=f.planets[2];auto normal=hyperion::flight::unit(hyperion::flight::V3{.6,.7,-.4}),tangent=hyperion::flight::unit(hyperion::flight::cross(normal,{0,1,0}));
   f.cancelNavigation();f.launchAssist=0;f.docked=false;f.dockedPort=-1;f.savedDock.clear();f.ship.velocity={};f.ship.throttle=0;f.ship.assist=true;f.ship.cruise=false;f.ship.gear=false;f.resetDynamics();
   f.ship.position=p.centre+normal*(p.radius+600);f.orient(tangent,normal);game.s.economy.docked.clear();game.storeShipStock({});game.s.fuel=game.s.maxFuel;game.s.hull=100;
   if(scene==0){f.ship.throttle=1;}
   if(scene==1){f.ship.assist=false;f.ship.velocity=tangent*85;f.ship.angularVelocity=normal*.24;}
   if(scene==2||scene==3){f.ship.velocity=tangent*120;game.action("brake");if(scene==3){Stock cargo{};cargo[Ore]=440;game.storeShipStock(cargo);}}
   if(scene==4){f.ship.throttle=1;f.ship.cruise=true;}
   if(scene==5){f.ship.position=p.centre+normal*(p.radius+hyperion::flight::terrain(p,normal)+45);f.orient(tangent,normal);f.ship.gear=true;}
   if(scene==6){f.ship.throttle=.65;}
   if(scene==7){f.ship.assist=false;f.ship.velocity=tangent*80+normal*30;f.setAssist(true);}
   if(scene==8){f.ship.assist=false;f.ship.cruise=true;f.ship.throttle=1;hyperion::flight::Input warm;warm.boost=true;for(int i=0;i<120;i++)game.simulate(1./60,{},warm);}
   if(scene==9){game.s.economy.vessel.model=f.ship.vessel.model=2;game.s.maxFuel=360;game.s.fuel=360;Stock cargo{};cargo[Ore]=880;game.storeShipStock(cargo);f.ship.combat.systems[hyperion::flight::Drive]=50;f.ship.position=p.centre+normal*(p.radius+hyperion::flight::terrain(p,normal)+55);f.ship.velocity=tangent*90;f.brake();}
   handlingUp=normal;handlingVelocity=f.ship.velocity;handlingPosition=f.ship.position;handlingFuel=game.s.fuel;ui.panel=ui.help=ui.settings=ui.menu=ui.sections=false;ui.clean=false;
  }
  hyperion::flight::Input helm;if(scene==4||(scene==8&&phase<25))helm.boost=true;
  if(scene==6){helm.precision=helm.boost=true;helm.strafe=helm.lift=.35;helm.yaw=.1;}
  if(scene==8&&phase==25)game.s.flight.brake();
  game.simulate(1./60,{},helm);game.toastTime=0;
  if(phase==119){auto& f=game.s.flight;
   bool ok=scene==0?f.speed()>40&&f.speed()<85:scene==1?hyperion::flight::length(f.ship.velocity-handlingVelocity)<1&&hyperion::flight::length(f.ship.angularVelocity)>.2&&game.s.fuel==handlingFuel:scene==2?f.speed()<70&&f.speed()>20:scene==3?f.speed()>70&&f.speed()<110:scene==4?f.speed()>190&&f.speed()<260:scene==5?std::abs(hyperion::flight::length(f.ship.position-handlingPosition))<.3&&f.engineLoad>0:scene==6?f.precisionActive&&f.speed()>4&&f.speed()<14&&f.ship.boostLevel==0:scene==7?f.ship.assistCaptureTime==0&&f.ship.throttle>.9&&f.speed()>75&&f.speed()<88:scene==8?f.ship.assist&&f.ship.throttle==0&&!f.ship.cruise&&f.ship.boostLevel<.005&&f.speed()>100&&f.speed()<260:f.cargoMass==880&&std::abs(hyperion::flight::dot(f.ship.velocity,handlingUp))<.3&&f.speed()<90&&game.s.hull==100;
   if(!ok)throw std::runtime_error("Native handling scenario failed: "+std::to_string(scene)+" / speed "+std::to_string(f.speed()));
   handlingScenes.push_back({{"scene",scene},{"speed",f.speed()},{"brakingKm",f.stoppingDistance()},{"spin",hyperion::flight::length(f.ship.angularVelocity)},{"engineLoad",f.engineLoad},{"cargo",f.cargoMass},{"verticalSpeed",hyperion::flight::dot(f.ship.velocity,handlingUp)},{"precision",f.precisionActive},{"boostPressure",f.ship.boostLevel},{"captureTime",f.ship.assistCaptureTime},{"fuelSpent",handlingFuel-game.s.fuel},{"hull",game.s.hull},{"exercised",ok}});
  }
 }
 if(opt.wildlifeCheck){int scene=opt.wildlifeShot>=0?opt.wildlifeShot:frame/60,phase=frame%60;if(frame==0||(opt.wildlifeShot<0&&phase==0)){prepareWildlifeGraphics(game,scene);ui.panel=false;ui.help=ui.settings=ui.menu=ui.sections=false;ui.scroll=0;}if(game.s.mode==Mode::Flight)game.simulate(1./60);else if(phase==22)game.endTurn();game.toastTime=0;}
 if(opt.ecosystemCheck&&frame%30==0){int scene=frame/30;prepareEcosystemGraphics(game,scene);ui.panel=scene<4;ui.tab=4;ui.settlement=game.s.economy.settlements[2].id;ui.expanded={"environment","food-web"};ui.scroll=scene<4?370:0;ui.help=ui.settings=ui.menu=ui.sections=false;game.toastTime=0;}
 if(opt.livingCheck){int scene=frame/60,phase=frame%60;if(phase==0){prepareLivingGraphics(game,scene);ui.panel=scene==7;ui.tab=6;ui.factionChoice=1;ui.expanded={"security","pirates"};ui.scroll=0;ui.help=ui.settings=ui.menu=ui.sections=false;}if(scene<4)game.simulate(1./60);game.toastTime=0;}
 if(opt.generationCheck){int scene=frame/30,phase=frame%30;
  if(phase==0){prepareGenerationGraphics(game,scene);ui.panel=scene==6;ui.tab=4;ui.scroll=0;ui.settlement.clear();if(scene==6)for(const auto& port:game.s.economy.settlements)if(port.star==game.s.currentStar){ui.settlement=port.id;break;}ui.help=ui.settings=ui.menu=ui.sections=false;ui.expanded.clear();}
  if(game.s.mode==Mode::Flight)game.simulate(1./60);
 }
 if(opt.shipyardCheck){int scene=frame/60,phase=frame%60;
  if(phase==0){prepareShipyardGraphics(game,scene);ui.panel=scene<3||scene==7;ui.tab=5;ui.settlement=game.s.economy.docked;ui.scroll=scene==0?310:scene==1?265:scene==2?300:scene==7?325:0;ui.expanded=scene==1?std::set<std::string>{"shipyard","ship-refits"}:scene==7?std::set<std::string>{"services"}:std::set<std::string>{"shipyard","ship-stock"};ui.help=ui.settings=ui.menu=ui.sections=false;}
  if(scene==7&&phase==30)game.action("service");game.simulate(1./60);game.toastTime=0;
  if(phase==59){auto& e=game.s.economy;auto& f=game.s.flight;bool old=false;for(const auto& o:e.shipOffers)if(o.vessel.serial=="NOMA-01"&&o.hull==62&&o.equipment.rounds==37)old=true;
   bool exercised=scene==0?!e.shipOffers.empty():scene==1?e.vessel.fitted[0]==1&&e.vessel.fitted[1]==1&&e.vessel.fitted[3]==1:scene==2?e.vessel.model==1&&old&&game.s.turns==1:scene==7?game.s.hull==100&&f.ship.combat.systems[hyperion::flight::Drive]==100:f.speed()>5;
   if(!exercised)throw std::runtime_error("Shipyard native scenario failed: "+std::to_string(scene));
   shipyardScenes.push_back({{"scene",scene},{"model",e.vessel.model},{"fitted",e.vessel.fitted},{"capacity",game.cargoCapacity()},{"tank",game.s.maxFuel},{"hull",game.s.hull},{"shieldCapacity",hyperion::flight::shieldCapacity(e.vessel)},{"credits",e.credits},{"speed",f.speed()},{"turns",game.s.turns},{"oldHullRetained",old},{"exercised",exercised}});
  }
 }
 if(opt.securityCheck){int scene=frame/60,phase=frame%60;
  if(phase==0){prepareSecurityGraphics(game,scene);ui.panel=scene!=2;ui.tab=scene==3?0:scene==4?5:6;ui.factionChoice=1;ui.settlement=game.s.economy.docked;ui.scroll=scene==1||scene==5?235:scene==4?390:0;ui.expanded=scene==0?std::set<std::string>{"security"}:scene==3?std::set<std::string>{"combat"}:scene==4?std::set<std::string>{"freight"}:std::set<std::string>{"pirates"};ui.help=ui.settings=ui.menu=ui.sections=false;combatStartTime=game.s.flight.time;}
  game.simulate(1./60);game.toastTime=0;
  if(phase==59){auto& e=game.s.economy;auto& f=game.s.flight;int neutral=0,hostile=0,shots=0;for(const auto& t:f.traffic)if(t.managed){if(t.hostile)hostile++;else neutral++;shots+=t.salvos;}
   bool exercised=scene==0?neutral>=4&&f.time==combatStartTime:scene==1?e.pirates[0].grievance==34&&f.time==combatStartTime:scene==2?shots>=2:scene==3?e.factions[1].warrant==375&&hostile>=4:scene==4?e.routes[0].escortUntil>e.hour:!hostile&&e.pirates[0].truceUntil>e.hour;
   if(!exercised)throw std::runtime_error("Security native scenario failed: "+std::to_string(scene));
   securityScenes.push_back({{"scene",scene},{"neutralContacts",neutral},{"hostileContacts",hostile},{"npcShots",shots},{"vendetta",e.pirates[0].grievance},{"warrant",e.factions[1].warrant},{"flightTime",f.time},{"exercised",exercised}});
  }
 }
 if(opt.combatCheck){int scene=frame/90,phase=frame%90;
  if(phase==0){prepareCombatGraphics(game,scene);ui.panel=scene==5||scene==7||scene==8||scene==11||scene==13;ui.tab=scene==8?5:0;ui.scroll=0;ui.expanded=scene==8?std::set<std::string>{"services"}:std::set<std::string>{"combat"};ui.help=scene==9;ui.settings=ui.menu=ui.sections=false;ui.settlement=game.s.economy.docked;combatStartTime=game.s.flight.time;combatStartStock=stockTotal(game.shipStock());combatSawEvasion=false;}
  auto& f=game.s.flight;hyperion::flight::Input input;
  if(scene<4||scene==10||scene==11||scene==14||scene==15){int target=f.combatTargetIndex();if(target>=0)f.orient(f.traffic[target].position-f.ship.position,f.ship.up);input.fire=scene<3||scene==10||scene==11||scene==15;input.missile=(scene==3||scene==14)&&phase==8;}
  if(scene==6&&phase==10)game.action("salvage");
  if(scene==8&&phase==30)game.action("rearm");if(scene==8&&phase==60)game.action("service");
  if(scene==7){game.simulate(1./60);if(phase==40&&f.time!=combatStartTime)throw std::runtime_error("Combat advanced while planning");if(phase==45)game.endTurn();}
  else game.simulate(1./60,{},input);
  for(const auto& enemy:f.traffic)if(enemy.state==5)combatSawEvasion=true;
  game.toastTime=0;f.noticeAge=10;
  if(phase==89){bool exercised=scene<3?f.ship.combat.energy<100:scene==3?f.ship.combat.missiles==5:scene==4?f.ship.shield<100:scene==5?f.ship.combat.systems[hyperion::flight::WeaponMounts]==23:scene==6?stockTotal(game.shipStock())==combatStartStock+9:scene==7?f.time>combatStartTime+54:scene==8?f.ship.combat.rounds==600&&f.ship.combat.missiles==8&&f.ship.combat.decoys==8&&game.s.hull==100&&f.ship.combat.systems[hyperion::flight::Drive]==100:scene==9?ui.help:scene==10?f.ship.combat.shots>0&&std::count_if(f.traffic.begin(),f.traffic.end(),[](const hyperion::flight::Traffic& t){return t.energy<100||t.shield<30;})>=2:scene==11?f.traffic[0].modules[0]<70:scene==12?f.traffic[0].salvos>0:scene==13?f.traffic[0].ammunition==180&&f.traffic[0].health==hyperion::flight::enemyDef(1).hull:scene==14?f.traffic[0].countermeasures==1:combatSawEvasion;
   if(!exercised)throw std::runtime_error("Combat visual scenario did not exercise its system: "+std::to_string(scene));
   combatScenes.push_back({{"scene",scene},{"flightTime",f.time},{"hull",game.s.hull},{"shield",f.ship.shield},{"rounds",f.ship.combat.rounds},{"missiles",f.ship.combat.missiles},{"heat",f.ship.combat.heat},{"energy",f.ship.combat.energy},{"projectiles",f.bolts.size()},{"effects",f.effects.size()},{"aimSystem",f.aimSystem},{"enemyModules",f.traffic.empty()?std::array<double,3>{}:f.traffic[0].modules},{"enemyAmmo",f.traffic.empty()?0:f.traffic[0].ammunition},{"enemyDecoys",f.traffic.empty()?0:f.traffic[0].countermeasures},{"enemyState",f.traffic.empty()?0:f.traffic[0].state},{"sawEvasion",combatSawEvasion},{"kills",f.kills},{"order",f.combatOrder},{"exercised",exercised}});
  }
 }
 if(opt.oceanCheck){int stage=frame/60,phase=frame%60;
  if(phase==0){
   auto deploy=[&](){game.reset(game.s.seed);game.ensureEconomy();game.system("HOME");game.s.selectedPlanet=2;if(!game.descend())throw std::runtime_error("Ocean diagnostic cannot deploy");game.s.surface=game.s.waypoint={3200,1800};game.parkFlight(2,game.s.surface);game.storeShipStock({});game.s.economy.routes.clear();game.s.economy.contracts.clear();game.s.economy.missions.clear();game.s.flight.traffic.clear();for(auto& site:game.s.economy.settlements)site.owner=1;if(!game.dive()||game.diveState.goal.id.empty())throw std::runtime_error("Ocean diagnostic has no genuine survey site");};
   if(stage==0||stage==6||stage==7||stage==8||stage==12||stage==13)deploy();
   if(stage==1){auto sites=game.diveSites();auto site=std::find_if(sites.begin(),sites.end(),[&](const auto& c){return c.id!=game.diveState.goal.id;});if(site==sites.end())throw std::runtime_error("Missing alternative generated dive site");oceanGoal=site->id;}
   if(stage==2){auto c=game.diveState.goal;game.s.oceanX=c.pos.x-30;game.s.depth=c.depth;game.s.oceanTargetX=c.pos.x;game.s.targetDepth=c.depth;game.registerExpeditionHabitat();game.diveState.revealed.clear();game.s.battery=50;oceanGoal=c.id;}
   if(stage==5||stage==11){int attempts=0;while(!(game.diveBrief().atBuoy&&game.diveBrief().nearSurface)&&attempts++<30)game.issueOrder("oceanReturn");if(!game.diveBrief().atBuoy||!game.diveBrief().nearSurface)throw std::runtime_error("Ocean return fixture failed to reach launch buoy");}
   if(stage==6){game.s.depth=game.s.targetDepth=game.maxDepth()+40;game.s.hull=80;}
   if(stage==7)game.s.battery=0;
   if(stage==8){auto sites=game.diveSites();auto c=std::find_if(sites.begin(),sites.end(),[](const auto& c){return c.kind=="mineral"||c.kind=="vent";});if(c==sites.end())throw std::runtime_error("No geological contact for sonar selection");game.s.oceanX=game.s.oceanTargetX=c->pos.x;game.s.depth=game.s.targetDepth=c->depth;game.s.battery=50;game.registerExpeditionHabitat();game.oceanSonar();oceanGoal=c->id;}
   if(stage==12){game.s.oceanTargetX=game.s.oceanX+45;game.s.targetDepth=game.s.depth+100;oceanExpectedMove=game.planMove();}
   if(stage==13){auto sites=game.diveSites();auto c=std::find_if(sites.begin(),sites.end(),[](const auto& c){return c.kind=="life"||c.kind=="flora"||c.kind=="vent"||c.kind=="signal";});if(c==sites.end())throw std::runtime_error("No acoustic site for quiet sonar");game.issueOrder("oceanSelect",c->id);game.s.oceanX=c->pos.x+20;game.s.depth=c->depth;game.s.oceanTargetX=c->pos.x;game.s.targetDepth=c->depth;game.s.battery=50;game.registerExpeditionHabitat();game.diveState.revealed.clear();oceanGoal=c->id;}
   if(stage==14){game.issueOrder("fieldStudy",oceanGoal);auto next=game.nextDiveOrder();if(next.action!="oceanNext")throw std::runtime_error("Quiet study did not offer a real continuation");oceanExpectedNext=next.arg;}

   ui.workspaceMemory.clear();ui.panel=stage!=0&&stage<13;ui.tab=0;ui.divePage=stage==8?1:0;ui.scroll=0;ui.expanded={"play-turn"};if(stage==1)ui.expanded.insert("dive-sites");if(stage==8)ui.expanded.insert("dive-contacts");if(stage==11)ui.expanded.insert("dive-report");ui.help=ui.settings=ui.paused=ui.menu=ui.sections=false;ui.observeMode(int(game.s.mode),0,false);oceanFrozen=oceanStart=game.s.hours;oceanDepth=game.s.depth;oceanCargo=game.shipStock();if(stage==14)ui.paused=true;
  }
  std::string action=stage==0?"orders":stage==1||stage==8?"oceanSelect":stage==10?"fieldCollect":"";
  std::string arg=stage==1||stage==8||stage==10?oceanGoal:"";int clickPhase=44;
  if(!action.empty()&&phase>=24&&phase<clickPhase&&ui.handoff.ready()&&!ui.auditButtons.count(action+"|"+arg))ui.scroll+=28;
  if(phase==clickPhase&&!action.empty()){auto hit=ui.auditButtons.find(action+"|"+arg);if(hit==ui.auditButtons.end())throw std::runtime_error("Ocean control not visible / "+action+" / "+std::to_string(stage));ui.pointerPressed=true;ui.pressPosition={hit->second.x+hit->second.width*.5f,hit->second.y+hit->second.height*.5f};plannerExpectedAction=action;plannerExpectedArg=arg;}
  if(stage==0&&phase==52){auto hit=ui.auditButtons.find("oceanSelect|"+game.diveState.goal.id);if(hit==ui.auditButtons.end())throw std::runtime_error("Current site control missing");ui.pointerPressed=true;ui.pressPosition={hit->second.x+hit->second.width*.5f,hit->second.y+hit->second.height*.5f};plannerExpectedAction="oceanSelect";plannerExpectedArg=game.diveState.goal.id;}
  if(stage==0&&phase==53&&ui.panel)throw std::runtime_error("Selecting the current site failed to close the drawer");
  if(stage==14&&phase==47)ui.paused=false;
  if(!diagnosticKeys.empty())oceanFrozen=game.s.hours;
  if(game.s.hours!=oceanFrozen)throw std::runtime_error("Ocean presentation advanced campaign time");
  if(phase==45){double cost=game.s.hours-oceanStart;
   if((stage<=1||stage==8||stage==14)&&cost!=0)throw std::runtime_error("Ocean planning spent time");
   if((stage==2||stage==3||stage==5||stage==6||stage==9||stage==10||stage==11||stage==12||stage==13)&&std::abs(cost-1)>1e-7)throw std::runtime_error("Ocean order did not resolve exactly one hour");
   if(stage==0&&(!ui.panel||ui.tab!=0||ui.divePage!=0))throw std::runtime_error("Dive operations control did not open the playable loop");
   if(stage==1&&(game.diveState.goal.id!=oceanGoal||!game.planMove().valid||ui.panel))throw std::runtime_error("Site selection failed to plot its genuine generated contact");
   if(stage==2&&(!game.diveState.revealed.count(oceanGoal)||game.s.scanned.count(oceanGoal)||game.shipStock()!=oceanCargo||game.s.battery!=42))throw std::runtime_error("Sonar must reveal without harvesting or studying");
   if(stage==3&&(!game.s.scanned.count(oceanGoal)||game.shipStock()[Research]<=oceanCargo[Research]||game.shipStock()[Biomass]!=oceanCargo[Biomass]||game.diveBrief().phase!=3))throw std::runtime_error("Study failed to record actual evidence and direct recovery");
   if(stage==4&&(cost<=0||cost>6||game.shipStock()!=oceanCargo||game.s.depth>80||!game.diveReturnNeeded()||game.s.mode!=Mode::Ocean))throw std::runtime_error("Return did not use bounded physical movement with conserved evidence");
   if(stage==5&&(game.s.mode!=Mode::Surface||!game.s.scanned.count("DIVE:complete:"+oceanGoal)||game.shipStock()!=oceanCargo))throw std::runtime_error("Recovery failed to finish the real survey at its buoy");
   if(stage==6&&(std::abs(game.s.depth-(oceanDepth-185))>1e-7||game.s.depth>game.maxDepth()))throw std::runtime_error("Emergency ascent did not physically reach safe depth");
   if(stage==7&&(cost!=3||game.s.battery!=12))throw std::runtime_error("Sonar recharge did not stop at scanner readiness");
   if(stage==8&&(game.diveState.goal.id!=oceanGoal||ui.scroll!=0||ui.panel||game.shipStock()!=oceanCargo))throw std::runtime_error("Sonar contact selection failed to return to the next playable action for free");
   if(stage==9&&(!game.s.scanned.count(oceanGoal)||game.diveState.studies!=1||game.diveState.collections!=0||game.shipStock()[Research]<=oceanCargo[Research]))throw std::runtime_error("Enter failed to study the selected sonar contact");
   if(stage==10&&(game.diveState.collections!=1||stockTotal(game.shipStock())<=stockTotal(oceanCargo)||!game.s.scanned.count("FIELD:take:"+oceanGoal)))throw std::runtime_error("Optional geological collection did not use its real finite cache");
   if(stage==11&&(game.s.mode!=Mode::Surface||!game.diveReport.valid||!game.diveReport.complete||game.diveReport.studies!=1||game.diveReport.collections!=1||game.shipStock()!=oceanCargo||game.diveReport.delta!=oceanCargo))throw std::runtime_error("Recovery debrief did not conserve the actual study and collection evidence");
   if(stage==12&&(std::abs(game.s.oceanX-oceanExpectedMove.position.x)>1e-7||std::abs(game.s.depth-oceanExpectedMove.depth)>1e-7||game.s.mode!=Mode::Ocean))throw std::runtime_error("Contextual Enter lost the precise manual transect preview");
   if(stage==13&&(game.s.battery!=52||!game.diveState.revealed.count(oceanGoal)||game.s.scanned.count(oceanGoal)||game.shipStock()!=oceanCargo||std::abs(game.diveAlert()-.15)>1e-7||!ui.panel||ui.divePage!=2))throw std::runtime_error("Quiet listening did not use its real power, acoustic returns and undisturbed wildlife");
   if(stage==14&&(game.diveState.goal.id!=oceanExpectedNext||game.s.scanned.count(oceanExpectedNext)||game.shipStock()!=oceanCargo||game.diveState.studies!=1||ui.panel||!ui.paused))throw std::runtime_error("Free continuation failed to replan to genuine unstudied evidence with a clear chart");

  }
  if(stage==4&&phase==49&&(game.s.mode!=Mode::Ocean||game.s.hours-oceanStart<=6||game.s.hours-oceanStart>12||game.shipStock()!=oceanCargo))throw std::runtime_error("Shallow return K shortcut boarded instead of continuing physical recovery");
  if(phase==49)oceanScenes.push_back({{"scene",stage},{"panel",ui.panel},{"divePage",ui.divePage},{"alert",game.diveAlert()},{"mode",modeName(game.s.mode)},{"hours",game.s.hours},{"startHour",oceanStart},{"depth",game.s.depth},{"battery",game.s.battery},{"goal",game.diveState.goal.id},{"revealed",game.diveState.revealed.size()},{"completed",game.s.scanned.count("DIVE:complete:"+oceanGoal)!=0},{"research",game.shipStock()[Research]},{"studies",game.diveState.studies},{"collections",game.diveState.collections},{"debriefValid",game.diveReport.valid},{"debriefComplete",game.diveReport.complete},{"debriefStudies",game.diveReport.studies},{"debriefCollections",game.diveReport.collections},{"debriefHours",game.diveReport.hours},{"netCargo",game.diveReport.delta},{"report",game.lastTurn.summary()},{"handoffSettled",ui.handoff.ready()}});
 }
 if(opt.wreckCheck){int stage=frame/60,phase=frame%60;
  if(phase==0){
   if(stage==0){game.reset(game.s.seed);game.system("HOME");game.s.selectedPlanet=2;if(!game.descend())throw std::runtime_error("Wreck fixture cannot deploy");game.s.surface=game.s.waypoint={3200,1800};game.parkFlight(2,game.s.surface);Stock supplies{};supplies[Medicine]=4;game.storeShipStock(supplies);game.s.economy.missions.clear();game.s.economy.contracts.clear();game.s.economy.routes.clear();game.s.flight.traffic.clear();for(auto& site:game.s.economy.settlements)site.owner=1;if(!game.dive()||game.s.economy.oceanWrecks.empty())throw std::runtime_error("No real wreck generated");wreckId=game.s.economy.oceanWrecks.front().contact.id;for(const auto& m:game.s.economy.missions)if(m.kind==WreckMission&&m.target==wreckId){wreckJob=m.id;break;}if(wreckJob.empty())throw std::runtime_error("Wreck commission missing");game.issueOrder("acceptMission",wreckJob);}
   auto* wreck=findOceanWreck(game.s.economy,wreckId);if(!wreck)throw std::runtime_error("Wreck fixture lost its persistent record");
   if(stage==1){game.s.oceanX=wreck->contact.pos.x-14;game.s.oceanTargetX=wreck->contact.pos.x;game.s.depth=wreck->contact.depth-25;game.s.targetDepth=wreck->contact.depth;game.s.battery=90;game.registerExpeditionHabitat();game.oceanSonar();}
   if(stage==3){while(findOceanWreck(game.s.economy,wreckId)->crew>0)game.issueOrder("wreckRescue",wreckId);game.issueOrder("wreckCargo",std::to_string(Components));}
   if(stage==4){auto& h=ensureEcosystem(game.s.economy,game.universe,game.activePlanet(),wreck->contact.pos,wreck->contact.depth);wreckQuality=h.quality;}
   if(stage==6){int calls=0;while(game.diveReturnNeeded()&&calls++<80)game.issueOrder("oceanReturn");if(game.diveReturnNeeded())throw std::runtime_error("Physical wreck return failed");}
   if(stage==7){auto home=*findSettlement(game.s.economy,wreck->home);game.system(home.star);game.s.selectedPlanet=home.planet;if(!game.descend())throw std::runtime_error("Receiver deployment failed");game.s.surface=game.s.waypoint=home.position;game.parkFlight(home.planet,home.position);if(home.depth>0){game.dive();game.s.oceanX=home.position.x;game.s.oceanAtY=home.position.y;game.s.depth=home.depth;}game.dock(home.id);if(game.s.economy.docked!=home.id)throw std::runtime_error("Receiver is not physically docked");auto* receiver=findSettlement(game.s.economy,home.id);receiver->built={};receiver->built[Warehouse]=6;receiver->built[Habitat]=4;receiver->stock[Rations]=500;receiver->health=95;}
   ui.workspaceMemory.clear();ui.oceanZoom=1.45;ui.panel=stage==0||stage==4||stage==7||stage==8;ui.tab=stage==8?5:0;ui.divePage=stage==4?2:0;ui.scroll=0;ui.expanded=stage==8?std::set<std::string>{"missions"}:std::set<std::string>{"play-turn"};ui.help=ui.settings=ui.paused=ui.menu=ui.sections=false;ui.observeMode(int(game.s.mode),0,false);wreckStart=wreckFrozen=game.s.hours;wreckHull=game.s.hull;wreckCargo=game.shipStock();wreckCrew=game.rescuedCrew();wreckCredits=game.s.economy.credits;
  }
  std::string action=stage==0?"oceanSelect":stage==4?"wreckCut":stage==7?"wreckDeliver":stage==8?"debriefMission":"",arg=stage==8?wreckJob:wreckId;
  if(!action.empty()&&phase>=24&&phase<44&&ui.handoff.ready()&&!ui.auditButtons.count(action+"|"+arg))ui.scroll+=28;
  if(phase==44&&!action.empty()){auto hit=ui.auditButtons.find(action+"|"+arg);if(hit==ui.auditButtons.end())throw std::runtime_error("Wreck control not visible / "+action+" / "+std::to_string(stage));ui.pointerPressed=true;ui.pressPosition={hit->second.x+hit->second.width*.5f,hit->second.y+hit->second.height*.5f};plannerExpectedAction=action;plannerExpectedArg=arg;}
  if(!diagnosticKeys.empty())wreckFrozen=game.s.hours;if(game.s.hours!=wreckFrozen)throw std::runtime_error("Wreck presentation advanced campaign time");
  if(phase==45){auto* w=findOceanWreck(game.s.economy,wreckId);double cost=game.s.hours-wreckStart;
   if(stage==0&&(cost!=0||ui.panel||game.diveState.goal.id!=wreckId))throw std::runtime_error("Wreck selection was not free or did not close the drawer");
   if(stage!=0&&stage!=5&&std::abs(cost-1)>1e-7)throw std::runtime_error("Wreck work did not spend its labelled single hour / "+std::to_string(stage));
   if(stage==1&&(!w->surveyed||!game.s.scanned.count(wreckId)))throw std::runtime_error("Enter failed to survey the real wreck");
   if(stage==2&&(game.rescuedCrew()<=wreckCrew||game.shipStock()[Medicine]!=wreckCargo[Medicine]-1))throw std::runtime_error("Enter rescue failed to transfer finite crew and medicine");
   if(stage==3&&(game.shipStock()[Components]<=wreckCargo[Components]||game.rescuedCrew()!=wreckCrew))throw std::runtime_error("C failed to recover selected cargo while preserving passengers");
   if(stage==4){auto* h=contactHabitat(game.s.economy,game.universe,game.activePlanet(),w->contact);if(!w->breached||game.s.hull>=wreckHull||!h||h->quality>=wreckQuality||game.diveAlert()<.9)throw std::runtime_error("Cutting did not use its actual hull and ecosystem consequences");}
   if(stage==5&&(cost<=0||cost>6||game.s.mode!=Mode::Ocean||game.shipStock()!=wreckCargo||game.rescuedCrew()!=wreckCrew))throw std::runtime_error("Wreck return teleported or lost cargo / passengers");
   if(stage==6&&(game.s.mode!=Mode::Surface||game.shipStock()!=wreckCargo||game.rescuedCrew()!=wreckCrew))throw std::runtime_error("Wreck recovery failed to preserve cargo and passengers");
   if(stage==7&&(w->aboard!=0||w->deliveredCrew!=w->initialCrew||stockTotal(w->delivered)<1))throw std::runtime_error("Receiver failed to receive actual crew and goods");
   if(stage==8){auto m=std::find_if(game.s.economy.missions.begin(),game.s.economy.missions.end(),[&](const auto& m){return m.id==wreckJob;});if(m==game.s.economy.missions.end()||m->state!=MissionComplete||m->escrow!=0||game.s.economy.credits<=wreckCredits)throw std::runtime_error("Wreck debrief failed to pay funded commission");}
   for(int g=0;g<goodCount;g++)if(std::abs(w->initial[g]-w->remaining[g]-w->recovered[g]-w->delivered[g])>1e-7)throw std::runtime_error("Native wreck cargo budget broke");
  }
  if(phase==49){auto* w=findOceanWreck(game.s.economy,wreckId);wreckScenes.push_back({{"scene",stage},{"mode",modeName(game.s.mode)},{"hours",game.s.hours},{"startHour",wreckStart},{"hull",game.s.hull},{"battery",game.s.battery},{"crewInside",w->crew},{"crewAboard",w->aboard},{"crewDelivered",w->deliveredCrew},{"remaining",w->remaining},{"recovered",w->recovered},{"delivered",w->delivered},{"surveyed",w->surveyed},{"breached",w->breached},{"panel",ui.panel},{"handoffSettled",ui.handoff.ready()}});}
 }
 if(opt.continuityCheck){int stage=frame/60,phase=frame%60;
  if(phase==0){
   auto deploy=[&](){game.reset(game.s.seed);game.system("HOME");game.s.selectedPlanet=2;if(!game.descend())throw std::runtime_error("Continuity fixture cannot deploy");game.s.surface=game.s.waypoint={3200,1800};game.parkFlight(2,game.s.surface);game.s.economy.routes.clear();game.s.economy.contracts.clear();game.s.economy.missions.clear();game.s.flight.traffic.clear();for(auto& site:game.s.economy.settlements)site.owner=1;};
   if(stage==0){game.reset(game.s.seed);game.ensureEconomy();game.system("HOME");game.s.selectedPlanet=1;ui.workspaceMemory.clear();ui.handoff.mode=int(Mode::System);ui.handoff.remaining=0;ui.handoff.release=false;ui.panel=true;ui.tab=4;ui.scroll=40;ui.settlement=game.s.economy.settlements.front().id;ui.expanded={"warehouse"};game.enterFlight();ui.observeMode(int(game.s.mode),0,false);}
   if(stage==3){deploy();continuityWaypoint=game.s.waypoint={3310,1840};ui.workspaceMemory.clear();ui.panel=true;ui.tab=0;ui.scroll=0;ui.expanded={"play-turn"};}
   if(stage==5){if(!game.dive())throw std::runtime_error("Continuity fixture cannot dive");continuityTargetX=game.s.oceanTargetX=game.s.oceanX+90;continuityTargetDepth=game.s.targetDepth=std::min(game.maxDepth(),game.s.depth+240);ui.panel=true;ui.tab=0;ui.scroll=0;ui.expanded={"play-turn"};}
   if(stage==8){deploy();auto p=game.activePlanet();Contact contact;bool found=false;for(int x=30;x<40&&!found;x++)for(int y=20;y<30&&!found;y++)for(const auto& c:game.universe.surfaceObjects(p,x,y))if(c.kind=="mineral"){contact=c;found=true;break;}if(!found)throw std::runtime_error("Missing actual approach contact");game.s.surface=game.s.waypoint={contact.pos.x-85,contact.pos.y};game.registerExpeditionHabitat();continuityContact=game.focusedContact=contact.id;for(int x=int(game.s.surface.x/86)-8;x<=int(game.s.surface.x/86)+8;x++)for(int y=int(game.s.surface.y/86)-8;y<=int(game.s.surface.y/86)+8;y++)for(const auto& c:game.universe.surfaceObjects(p,x,y))if(c.kind=="signal"||c.kind=="relic")game.s.scanned.insert(c.id);ui.panel=true;ui.tab=0;ui.scroll=0;ui.expanded={"play-turn"};}
   if(stage==9){game.s.battery=0;game.s.economy.missions.clear();game.s.economy.contracts.clear();ui.panel=true;ui.tab=0;ui.scroll=0;ui.expanded={"play-turn"};}
   ui.help=ui.settings=ui.paused=ui.menu=ui.sections=false;continuityFrozen=continuityStart=game.s.hours;ui.observeMode(int(game.s.mode),0,false);
  }
  std::string action=stage==0?"strategy":stage==1||stage==3?"cockpit":stage==7?"dive":stage==8?"fieldApproach":stage==9?"fieldRecharge":"";
  std::string arg=stage==8?continuityContact:"";int clickPhase=stage>=8?44:26;
  if(stage>=8&&phase>=24&&phase<44&&ui.handoff.ready()&&!ui.auditButtons.count(action+"|"+arg))ui.scroll+=28;
  if(phase==clickPhase&&!action.empty()){auto hit=ui.auditButtons.find(action+"|"+arg);if(hit==ui.auditButtons.end())throw std::runtime_error("Continuity control not visible / "+action+" / "+std::to_string(stage));ui.pointerPressed=true;ui.pressPosition={hit->second.x+hit->second.width*.5f,hit->second.y+hit->second.height*.5f};plannerExpectedAction=action;plannerExpectedArg=arg;}
  if(!diagnosticKeys.empty())continuityFrozen=game.s.hours;
  if(game.s.hours!=continuityFrozen)throw std::runtime_error("Continuity presentation advanced time");
  if(phase==45){
   double cost=stage==4||stage==5||stage==6||stage==7?1:stage==8||stage==9?3:0;if(std::abs(game.s.hours-continuityStart-cost)>1e-7)throw std::runtime_error("Mode/order cost differs from quoted cost / "+std::to_string(stage));
   if((stage==0||stage==2)&&(game.s.mode!=Mode::System||game.s.selectedPlanet!=1||ui.tab!=4||!ui.panel||ui.settlement!=game.s.economy.settlements.front().id))throw std::runtime_error("Chart or workspace did not return through the real mode control");
   if((stage==1||stage==3||stage==5)&&(game.s.mode!=Mode::Flight||ui.panel))throw std::runtime_error("Boarding failed to clear the cockpit viewport");
   if(stage==4&&(game.s.mode!=Mode::Surface||distance(game.s.waypoint,continuityWaypoint)>1e-6))throw std::runtime_error("Physical redeployment lost its surface route");
   if(stage==6&&(game.s.mode!=Mode::Surface||!game.surveyRecall.dive))throw std::runtime_error("Ocean target lost during redeployment");
   if(stage==7&&(game.s.mode!=Mode::Ocean||std::abs(game.s.oceanTargetX-continuityTargetX)>1e-6||std::abs(game.s.targetDepth-continuityTargetDepth)>1e-6||game.s.depth>=continuityTargetDepth))throw std::runtime_error("Dive failed to restore target from a real surface start");
   if(stage==8&&!game.fieldQuote("fieldStudy",continuityContact).available)throw std::runtime_error("Approach failed to hand back a study choice");
   if(stage==9&&game.s.battery!=12)throw std::runtime_error("Recharge overshot or did not reach scanner readiness");
  }
  if(phase==49){continuityScenes.push_back({{"scene",stage},{"mode",modeName(game.s.mode)},{"hours",game.s.hours},{"startHour",continuityStart},{"waypointX",game.s.waypoint.x},{"oceanTargetX",game.s.oceanTargetX},{"depth",game.s.depth},{"targetDepth",game.s.targetDepth},{"battery",game.s.battery},{"panelTab",ui.tab},{"panel",ui.panel},{"handoffSettled",ui.handoff.ready()},{"report",game.lastTurn.summary()}});if(!ui.handoff.ready())throw std::runtime_error("Continuity handoff did not settle");}
 }
 if(opt.fieldworkCheck){int stage=frame/60,phase=frame%60;
  if(phase==0){
   auto deploy=[&](){game.reset(game.s.seed);game.system("HOME");game.s.selectedPlanet=2;if(!game.descend())throw std::runtime_error("Fieldwork diagnostic cannot deploy");game.storeShipStock({});game.s.battery=50;game.s.economy.routes.clear();game.s.economy.contracts.clear();game.s.economy.missions.clear();game.s.flight.traffic.clear();for(auto& site:game.s.economy.settlements)site.owner=1;};
   auto find=[&](const std::string& kind){auto p=game.activePlanet();auto at=game.s.surface;for(int x=int(at.x/86)-6;x<=int(at.x/86)+6;x++)for(int y=int(at.y/86)-6;y<=int(at.y/86)+6;y++)for(const auto& c:game.universe.surfaceObjects(p,x,y))if(c.kind==kind){game.s.surface=game.s.waypoint=c.pos;game.registerExpeditionHabitat();game.focusedContact=fieldId=c.id;return;}throw std::runtime_error("Missing generated fieldwork contact");};
   if(stage==0){deploy();find("mineral");fieldExpected=game.fieldQuote("fieldStudy",fieldId);}
   if(stage==1)fieldExpected=game.fieldQuote("fieldCollect",fieldId);
   if(stage==2){game.s.hull=91;Stock cargo{};cargo[Alloy]=4;cargo[Components]=2;game.storeShipStock(cargo);fieldExpected=game.fieldQuote("fieldRepair");}
   if(stage==3){deploy();auto p=game.activePlanet();for(int x=int(game.s.surface.x/86)-10;x<=int(game.s.surface.x/86)+10;x++)for(int y=int(game.s.surface.y/86)-10;y<=int(game.s.surface.y/86)+10;y++)for(const auto& c:game.universe.surfaceObjects(p,x,y))if(c.kind=="relic"||c.kind=="signal")game.s.scanned.insert(c.id);game.s.waypoint={game.s.surface.x+250,game.s.surface.y};fieldExpected=game.fieldQuote("fieldTravel");}
   if(stage==4){game.s.waypoint={game.s.surface.x+110,game.s.surface.y};fieldExpected=game.fieldQuote("fieldPush");}
   if(stage==5){deploy();if(!game.dive())throw std::runtime_error("Fieldwork diagnostic cannot dive");auto sites=game.fieldSites();auto c=std::find_if(sites.begin(),sites.end(),[&](const auto& c){return c.depth<game.maxDepth()&&c.depth>=20&&std::abs(c.pos.x-game.s.oceanX)<76;});if(c==sites.end())throw std::runtime_error("Missing generated ocean contact");game.s.oceanX=game.s.oceanTargetX=c->pos.x;game.s.depth=game.s.targetDepth=c->depth;game.registerExpeditionHabitat();game.focusedContact=fieldId=c->id;game.issueOrder("oceanSelect",c->id);fieldExpected=game.fieldQuote("fieldStudy",fieldId);}
   if(stage==6){deploy();find("flora");game.issueOrder("fieldStudy",fieldId);for(auto& [id,h]:game.s.economy.ecosystems)for(auto& population:h.populations)population.mass=population.capacity*.2;fieldExpected=game.fieldQuote("fieldCollect",fieldId);if(fieldExpected.available)throw std::runtime_error("Depleted habitat still offers samples");}
   if(stage==7){game.reset(game.s.seed);game.ensureEconomy();game.system("HOME");auto& colony=game.s.economy.settlements.front();colony.owner=0;colony.alerts=1;colony.health=35;colony.residents=12;fieldId=colony.id;}
   ui.workspaceMemory.clear();ui.panel=true;ui.tab=0;ui.scroll=0;ui.expanded={"play-turn"};ui.help=ui.settings=ui.paused=ui.menu=ui.sections=false;fieldFrozenHour=fieldStartHour=game.s.hours;fieldReportCount=game.recentTurns.size();
  }
  std::string action=stage==0||stage==5?"fieldStudy":stage==1?"fieldCollect":stage==2?"fieldRepair":stage==3?"fieldTravel":stage==4?"fieldPush":stage==7?"reviewSettlement":"";
  std::string arg=stage==0||stage==1||stage==5||stage==7?fieldId:"";
  if(phase>=24&&phase<44&&ui.handoff.ready()&&!action.empty()&&!ui.auditButtons.count(action+"|"+arg))ui.scroll+=28;
  if(phase==44&&!action.empty()){auto hit=ui.auditButtons.find(action+"|"+arg);if(hit==ui.auditButtons.end())throw std::runtime_error("Fieldwork control not visible / "+action+" / "+std::to_string(stage));ui.pointerPressed=true;ui.pressPosition={hit->second.x+hit->second.width*.5f,hit->second.y+hit->second.height*.5f};plannerExpectedAction=action;plannerExpectedArg=arg;}
  if(game.s.hours!=fieldFrozenHour)throw std::runtime_error("Fieldwork presentation advanced the campaign");
  if(phase==45){
   if(stage<=5&&std::abs(game.s.hours-fieldStartHour-(stage==3?6:1))>1e-7)throw std::runtime_error("Field order did not resolve its expected turns");
   if((stage==0||stage==5)&&!game.s.scanned.count("FIELD:record:"+fieldId))throw std::runtime_error("Study pointer did not archive the actual contact");
   if(stage==1&&!game.s.scanned.count("FIELD:take:"+fieldId))throw std::runtime_error("Collect pointer did not exhaust its finite cache");
   if(stage==2&&(game.s.hull!=99||game.shipStock()[Alloy]!=2||game.shipStock()[Components]!=1))throw std::runtime_error("Patch pointer did not spend supplies");
   if(stage==3&&(!game.planMove().valid||game.lastTurn.hours!=6||game.recentTurns.size()!=fieldReportCount+1))throw std::runtime_error("Follow route pointer did not preserve the combined turn report");
   if(stage==4&&(distance(game.s.surface,fieldExpected.move.position)>1e-6||std::abs(game.lastTurn.hull+fieldExpected.wear)>1e-6))throw std::runtime_error("Push pointer disagrees with its preview");
   if(stage==6&&(game.s.hours!=fieldStartHour||ui.auditButtons.count("fieldCollect|"+fieldId)))throw std::runtime_error("Depleted sampling was not a disabled order");
   if(stage==7&&(!ui.panel||ui.tab!=4||ui.settlement!=fieldId))throw std::runtime_error("Next-move outpost link did not open its settlement");
  }
  if(phase==49){fieldScenes.push_back({{"scene",stage},{"mode",modeName(game.s.mode)},{"hours",game.s.hours},{"startHour",fieldStartHour},{"hull",game.s.hull},{"battery",game.s.battery},{"report",game.lastTurn.summary()},{"contact",fieldId},{"routeValid",game.planMove().valid},{"panelTab",ui.tab},{"handoffSettled",ui.handoff.ready()}});}
 }
 if(opt.turnCheck){int stage=frame/60,phase=frame%60;
  if(phase==0){
   ui.panel=false;ui.help=ui.settings=ui.paused=ui.menu=ui.sections=false;
   if(opt.planningCheck){
    if(stage==0){prepareCombatGraphics(game,5);auto& flight=game.s.flight;flight.traffic.resize(1);auto& enemy=flight.traffic.front();enemy.dead=false;enemy.hostile=true;enemy.kind=1;enemy.position=flight.ship.position+flight.ship.forward*80;game.system(game.s.currentStar);ui.panel=true;ui.tab=0;ui.scroll=0;ui.expanded={"before-turn"};}
    if(stage==1){auto site=std::find_if(game.s.economy.settlements.begin(),game.s.economy.settlements.end(),[&](const auto& site){return site.star==game.s.currentStar&&site.depth==0&&game.universe.planet(site.star,site.planet).water>=.2;});if(site==game.s.economy.settlements.end())throw std::runtime_error("Planning diagnostic has no ocean site");game.s.flight.dockAt(game.flightPort(site->id,false));game.processFlightEvents();game.disembark();game.dive();game.s.depth=game.maxDepth()+40;game.s.hull=80;game.system(game.s.currentStar);ui.panel=true;ui.tab=0;ui.scroll=0;ui.expanded={"before-turn"};}
    if(stage==2){game.reset(game.s.seed);game.ensureEconomy();game.s.mode=Mode::System;auto& site=game.s.economy.settlements.front();site.owner=0;site.built={};site.built[Habitat]=2;site.stock={};site.stock[Rations]=1;site.health=90;site.morale=70;site.loyalty=90;site.alerts=0;game.s.economy.routes.clear();game.s.economy.contracts.clear();Contract contract;contract.id="PLANNING-CHECK";contract.settlement=site.id;contract.deadline=.5;contract.accepted=true;contract.escrow=70;site.treasury-=70;game.s.economy.contracts.push_back(contract);ui.workspaceMemory.clear();ui.panel=true;ui.tab=0;ui.scroll=0;ui.expanded={"before-turn"};}
    if(stage==3){game.waitHours(6);if(game.s.hours!=1||game.lastTurn.interruption.empty())throw std::runtime_error("Planning contract/crisis wait did not interrupt");ui.panel=true;ui.tab=0;ui.scroll=0;ui.expanded={"turn-report"};}
    if(stage==4){auto& site=game.s.economy.settlements.front();site.health=44.8;site.alerts=1;game.waitHours(6);if(game.s.hours!=2||game.lastTurn.interruption.find(site.name)==std::string::npos)throw std::runtime_error("Planning medical crisis did not interrupt");ui.panel=true;ui.tab=0;ui.scroll=0;ui.expanded={"turn-report"};}
    if(stage==5){ui.panel=true;ui.tab=4;ui.scroll=160;ui.settlement=game.s.economy.settlements.front().id;ui.expanded={"construction","warehouse"};game.enterFlight();}
    if(stage==6){ui.tab=0;ui.scroll=15;ui.settlement="different-flight-target";ui.expanded={"combat"};game.system(game.s.currentStar);ui.observeMode(int(game.s.mode),0,false);workspaceRestored=ui.panel&&ui.tab==4&&ui.scroll==160&&ui.settlement==game.s.economy.settlements.front().id&&ui.expanded.count("warehouse");if(!workspaceRestored)throw std::runtime_error("Strategy workspace was not restored after cockpit UI edits");}
    if(stage==7){ui.panel=true;ui.tab=0;ui.scroll=0;ui.expanded={"before-turn"};game.s.battery=13;}
    if(opt.plannerCheck&&stage>=8){
     auto quiet=[&](){game.reset(game.s.seed);game.ensureEconomy();game.s.mode=Mode::System;game.s.economy.routes.clear();game.s.economy.contracts.clear();game.s.economy.missions.clear();game.s.economy.pirates.clear();for(auto& site:game.s.economy.settlements)site.owner=1;};
     if(stage==8){quiet();ui.workspaceMemory.clear();ui.plannedHours=6;ui.openPanel(0);ui.scroll=0;ui.expanded={"turn-planner"};}
     if(stage==9){ui.openPanel(0);ui.scroll=0;ui.plannedHours=3;ui.expanded={"turn-planner"};}
     if(stage==10){quiet();game.s.economy.research=0;game.s.economy.researchProgress=technologies[0].hours-.1;ui.openPanel(0);ui.scroll=0;ui.plannedHours=6;ui.expanded={"turn-planner"};}
     if(stage==11){game.endTurn();game.endTurn();ui.openPanel(0);ui.scroll=0;ui.reportIndex=0;ui.expanded={"turn-report"};}
     if(stage==12){game.system("HOME");game.s.selectedPlanet=2;if(!game.descend())throw std::runtime_error("Planner surface deployment failed");game.s.waypoint={game.s.surface.x+110,game.s.surface.y+30};ui.openPanel(0);ui.scroll=0;ui.plannedHours=1;ui.expanded={"turn-planner"};}
     if(stage==13){if(!game.dive())throw std::runtime_error("Planner ocean deployment failed");game.s.oceanTargetX=game.s.oceanX+90;game.s.targetDepth=std::min(game.maxDepth(),game.s.depth+200);ui.openPanel(0);ui.scroll=0;ui.divePage=2;ui.expanded={"turn-planner"};}
     if(stage==14){quiet();auto& site=game.s.economy.settlements.front();Contract c;c.id="PLANNER-LINK";c.settlement=site.id;c.accepted=true;c.deadline=3;site.treasury-=c.escrow;game.s.economy.contracts.push_back(c);ui.openPanel(0);ui.scroll=0;ui.expanded={"turn-planner"};}
     if(stage==15){ui.plannedHours=24;game.enterFlight();ui.observeMode(int(game.s.mode),0,false);ui.plannedHours=1;}
    }
   }else{
   if(stage==0){prepareCombatGraphics(game,5);game.s.flight.traffic.clear();game.s.flight.ship.throttle=.45;game.s.flight.ship.assist=false;game.s.flight.ship.cruise=true;}
   if(stage==1){auto ship=game.s.flight.ship;game.system(game.s.currentStar);if(ship.throttle!=game.s.flight.ship.throttle||ship.assist!=game.s.flight.ship.assist||ship.cruise!=game.s.flight.ship.cruise)throw std::runtime_error("Chart changed manual helm");}
   if(stage==2){game.endTurn();ui.panel=true;ui.tab=0;ui.expanded.insert("turn-report");}
   if(stage==3){ui.panel=true;game.enterFlight();}
   if(stage==4){auto site=std::find_if(game.s.economy.settlements.begin(),game.s.economy.settlements.end(),[&](const auto& site){return site.star==game.s.currentStar&&site.depth==0&&game.universe.planet(site.star,site.planet).water>=.2;});if(site==game.s.economy.settlements.end())throw std::runtime_error("Strategy diagnostic has no ocean settlement");game.s.flight.dockAt(game.flightPort(site->id,false));game.processFlightEvents();if(!game.disembark())throw std::runtime_error("Strategy deployment failed");game.s.waypoint={game.s.surface.x+120,game.s.surface.y+60};}
   if(stage==5){if(!game.dive())throw std::runtime_error("Strategy ocean deployment failed");game.s.oceanTargetX=game.s.oceanX+80;game.s.targetDepth=std::min(game.maxDepth(),game.s.depth+300);}
   if(stage==6){game.system(game.s.currentStar);ui.panel=true;ui.tab=4;}
   if(stage==7)game.enterFlight();
   }
   turnFrozenHour=game.s.hours;
  }
  if(opt.plannerCheck&&stage>=8){
   std::string action=stage==8||stage==13?"planHours":stage==9||stage==10||stage==12?"waitTurns":stage==11?"reportOlder":stage==14?"reviewSettlement":"";
   std::string arg=stage==8?"12":stage==13?"24":stage==9?"3":stage==10?"6":stage==12?"1":stage==14?game.s.economy.settlements.front().id:"";
   if(stage==14&&phase>=21&&phase<44&&ui.handoff.ready()&&!ui.auditButtons.count(action+"|"+arg))ui.scroll+=24;
   if(phase==(stage==14?44:26)&&!action.empty()){auto hit=ui.auditButtons.find(action+"|"+arg);if(hit==ui.auditButtons.end())throw std::runtime_error("Planner control is not visible / "+action+" / "+std::to_string(stage));ui.pointerPressed=true;ui.pressPosition={hit->second.x+hit->second.width*.5f,hit->second.y+hit->second.height*.5f};plannerExpectedAction=action;plannerExpectedArg=arg;}
   if(phase==27){
    if(stage==8&&ui.plannedHours!=12)throw std::runtime_error("Duration pointer did not apply");
    if(stage==9&&(game.s.hours!=3||!ui.expanded.count("turn-report")))throw std::runtime_error("Planned wait/report pointer failed");
    if(stage==10&&(game.s.hours!=1||game.lastTurn.interruption.find("Research completed")==std::string::npos))throw std::runtime_error("Research did not interrupt planned wait");
    if(stage==11&&ui.reportIndex!=1)throw std::runtime_error("Report history pointer failed");
    if(stage==12&&!game.planMove().valid)throw std::runtime_error("Wait erased the planned waypoint");
    if(stage==13&&ui.plannedHours!=24)throw std::runtime_error("Dive planner duration pointer failed");
    if(stage==15){game.system(game.s.currentStar);ui.observeMode(int(game.s.mode),0,false);if(ui.plannedHours!=24)throw std::runtime_error("Flight lost the planned wait duration");}
   }
   if(stage==14&&phase==45&&(!ui.panel||ui.tab!=4||ui.settlement!=arg))throw std::runtime_error("Timeline link did not inspect its settlement");
  }
  if(game.s.hours!=turnFrozenHour)throw std::runtime_error("Presentation handoff advanced campaign time");
  ui.observeMode(int(game.s.mode),0,false);
  if(phase==49){if(game.s.mode==Mode::Flight&&ui.panel)throw std::runtime_error("Cockpit handoff retained a blocking panel");turnScenes.push_back({{"scene",stage},{"mode",modeName(game.s.mode)},{"hours",game.s.hours},{"turns",game.s.turns},{"handoffSettled",ui.handoff.ready()},{"throttle",game.s.flight.ship.throttle},{"hull",game.s.hull},{"report",game.lastTurn.summary()},{"warnings",game.outlook().warnings},{"briefingItems",game.outlook().items.size()},{"interruption",game.lastTurn.interruption},{"workspaceRestored",workspaceRestored},{"plannedHours",ui.plannedHours},{"reports",game.recentTurns.size()},{"reportIndex",ui.reportIndex}});if(!ui.handoff.ready())throw std::runtime_error("Mode handoff did not settle");}
 }
 if(opt.minorCheck&&(frame==0||(opt.minorShot<0&&frame%30==0))){int stage=opt.minorShot<0?frame/30:opt.minorShot;prepareMinorGraphics(game,stage);ui.panel=stage==0||stage==5;ui.tab=0;ui.scroll=0;ui.expanded.clear();ui.help=ui.settings=ui.paused=ui.menu=ui.sections=false;ui.clean=false;ui.destinationFilter=3;}
 if(opt.missionBoard&&frame==0){ui.openPanel(5);ui.expanded.insert("missions");}
 if(opt.screenFitCheck){
  int stage=frame/40,phase=frame%40;
  if(phase==0){
   if(IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE))windowFit.toggle();
   const int sizes[][2]={{720,450},{800,500},{1280,720},{900,800},{3840,2160},{1280,800},{720,450},{800,500}};
   SetWindowSize(sizes[stage][0],sizes[stage][1]);windowFit.update(true,true);
   if(stage==5)windowFit.toggle();
   prepareFlightGraphics(game,stage==3?11:0);game.toastTime=0;
   ui.panel=stage==1||stage==3||stage==7;ui.tab=stage==3?5:stage==7?4:0;ui.scroll=0;ui.expanded.clear();ui.menu=ui.sections=ui.settings=ui.paused=ui.help=false;
   if(stage==3)ui.expanded={"missions"};if(stage==6)ui.help=true;
   if(stage==7)game.system(game.s.currentStar);
  }
  if(phase==32&&stage!=6){
   auto fitted=uiViewport();Vector2 nativePoint{fitted.native(fitted.width-54),fitted.native(30)};
   ui.pointerPressed=true;ui.pressPosition=uiPoint(nativePoint);
  }
 }
 if(opt.closeMaterialCheck){
  ui.handoff.mode=int(game.s.mode);ui.handoff.remaining=0;ui.handoff.release=false;
  auto& f=game.s.flight;int shot=opt.closeMaterialShot>=0?opt.closeMaterialShot:frame/18;
  // A short, reversible camera move also checks subpixel texture filtering.
  if(frame%18>=9){auto step=hyperion::flight::unit(hyperion::flight::cross(f.ship.forward,f.ship.up))*.0005;f.ship.position+=step;}
  if(!hyperion::flight::finite(f.ship.position)||game.s.hours!=(shot<8?.25:0.)||game.s.hull!=100)throw std::runtime_error("Close material fixture changed simulation state");
 }
 if(opt.hudCheck){int scene=frame/40,phase=frame%40;
  if(phase==0){prepareHUDGraphics(game,scene);ui.enhancedHUD=true;ui.clean=false;ui.panel=scene==7;ui.tab=0;ui.scroll=0;ui.expanded.clear();ui.help=ui.menu=ui.sections=ui.paused=false;ui.settings=scene==10;hudHours=game.s.hours;hudFlightTime=game.s.flight.time;hudPosition=game.s.flight.ship.position;}
  if(scene==8&&phase==0)ui.menu=true;
  if(scene==8&&phase==6){ui.pointerPressed=true;ui.pressPosition={float(uiViewport().width-270+20),float(54+8+6*34+10)};}
  if(scene==9&&(phase==6||phase==15))ui.toggleInterface();
  if(scene==11&&phase==12)game.enterFlight();
  ui.handoff.mode=int(game.s.mode);ui.handoff.remaining=0;ui.handoff.release=false;
  if(game.s.hours!=hudHours||game.s.flight.time!=hudFlightTime||hyperion::flight::length(game.s.flight.ship.position-hudPosition)>1e-8)throw std::runtime_error("HUD changed the simulation");
 }
 renderer.draw(game,ui,dt);
 if(!plannerExpectedAction.empty()){if(ui.action!=plannerExpectedAction||ui.arg!=plannerExpectedArg)throw std::runtime_error("Native planner pointer missed its semantic control");plannerExpectedAction.clear();plannerExpectedArg.clear();}

 if(opt.hudCheck){int scene=frame/40,phase=frame%40;
  if(scene==8&&phase==6&&ui.action!="flightHUD")throw std::runtime_error("Optional HUD menu control missed the fitted pointer");
  if(phase==29){if(ui.enhancedHUD!=(scene!=8))throw std::runtime_error("HUD toggle was lost or not applied");hudScenes.push_back({{"scene",scene},{"enhanced",ui.enhancedHUD},{"hidden",ui.clean},{"panel",ui.panel},{"hours",game.s.hours},{"flightTime",game.s.flight.time},{"width",GetScreenWidth()},{"height",GetScreenHeight()}});}
  if(!opt.captures.empty()&&(phase==29||(scene==9&&phase==12)||(scene==11&&phase==6))){fs::create_directories(opt.captures);renderer.screenshot((fs::path(opt.captures)/("hud-"+std::to_string(scene)+(phase==12?"-hidden":phase==6?"-strategy":"")+".png")).string());}
 }

 if(opt.closeMaterialCheck&&frame%18==17){
  int shot=opt.closeMaterialShot>=0?opt.closeMaterialShot:frame/18;const auto& f=game.s.flight;
  closeMaterialScenes.push_back({{"scene",shot},{"hours",game.s.hours},{"hull",game.s.hull},{"minorDetailFields",renderer.flightMinorDetailFields()}});
  if(shot>=8&&shot<13){const auto& t=f.traffic.front();closeMaterialScenes.back()["faction"]=t.faction;closeMaterialScenes.back()["genome"]=t.genome.seed;closeMaterialScenes.back()["health"]=t.health;}
  if(shot>=13&&renderer.flightMinorDetailFields()>2)throw std::runtime_error("Close material fixture exceeded its material budget");
 }
 if(opt.minorCheck&&renderer.flightMinorDetailFields()>2)throw std::runtime_error("Minor material cache exceeded its two-field budget");
 if(opt.minorCheck&&frame%30==24&&(opt.minorShot<0||frame==24)){if(game.s.mode==Mode::Flight&&renderer.flightVisibleMinorBodies()<1)throw std::runtime_error("No minor body visible in generated flight fixture");const auto& f=game.s.flight;const int bodyIndex=f.ports[f.target].minor;const auto& body=f.minorBodies[bodyIndex];int materialWidth=renderer.flightMinorMaterialWidth(f,bodyIndex);int materialScene=opt.minorShot<0?frame/30:opt.minorShot;if(game.s.mode==Mode::Flight&&materialScene!=4&&materialScene!=6&&materialWidth!=2048)throw std::runtime_error("Close minor-body material did not resolve to 2048 pixels");minorScenes.push_back({{"scene",opt.minorShot<0?frame/30:opt.minorShot},{"id",body.source.id},{"kind",minorKindName(body.source.kind)},{"count",f.minorBodies.size()},{"visibleBodies",renderer.flightVisibleMinorBodies()},{"materialWidth",materialWidth},{"detailFields",renderer.flightMinorDetailFields()},{"hours",game.s.hours},{"hull",game.s.hull}});if(!opt.captures.empty()){fs::create_directories(opt.captures);renderer.screenshot((fs::path(opt.captures)/("minor-body-"+std::to_string(opt.minorShot<0?frame/30:opt.minorShot)+".png")).string());if(game.s.mode==Mode::Flight&&(materialScene==2||materialScene==3||materialScene==5))if(!renderer.exportFlightMinorMaterial(f,bodyIndex,(fs::path(opt.captures)/("minor-material-"+std::to_string(materialScene)+".png")).string()))throw std::runtime_error("Minor material export failed");}}

 if(opt.screenFitCheck){
  int stage=frame/40,phase=frame%40;
  if(phase==32&&stage!=6){if(ui.action!="menu")throw std::runtime_error("Fitted pointer missed the actual menu button");ui.action.clear();ui.pointerPressed=false;}
  if(phase==35){
   auto fitted=uiViewport();if(fitted.width<959.9f||fitted.height<599.9f||!windowFit.contained())throw std::runtime_error("Window or UI extends beyond the display");
   screenFitScenes.push_back({{"stage",stage},{"windowWidth",GetScreenWidth()},{"windowHeight",GetScreenHeight()},{"renderWidth",GetRenderWidth()},{"renderHeight",GetRenderHeight()},{"uiWidth",fitted.width},{"uiHeight",fitted.height},{"uiScale",fitted.scale},{"fullscreen",IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)},{"windowContained",windowFit.contained()},{"menuHitTest",stage==6?"modal":"passed"}});
   if(!opt.captures.empty()){fs::create_directories(opt.captures);renderer.screenshot((fs::path(opt.captures)/("screen-fit-"+std::to_string(stage)+".png")).string());}
  }
 }

 if(opt.handlingCheck&&!opt.captures.empty()&&(frame%120==(frame/120==7?20:109))){fs::create_directories(opt.captures);if(!renderer.screenshot((fs::path(opt.captures)/("handling-"+std::to_string(frame/120)+".png")).string()))throw std::runtime_error("Handling capture failed");}
 if(opt.continuityCheck&&!opt.captures.empty()&&(frame%60==29||frame%60==49)){fs::create_directories(opt.captures);if(!renderer.screenshot((fs::path(opt.captures)/("continuity-"+std::to_string(frame/60)+"-"+std::to_string(frame%60)+".png")).string()))throw std::runtime_error("Continuity capture failed");}
 if(opt.wreckCheck&&!opt.captures.empty()&&(frame%60==29||frame%60==49)){fs::create_directories(opt.captures);if(!renderer.screenshot((fs::path(opt.captures)/("wreck-"+std::to_string(frame/60)+"-"+std::to_string(frame%60)+".png")).string()))throw std::runtime_error("Wreck capture failed");}
 if(opt.oceanCheck&&!opt.captures.empty()&&(frame%60==29||frame%60==49)){fs::create_directories(opt.captures);if(!renderer.screenshot((fs::path(opt.captures)/("ocean-"+std::to_string(frame/60)+"-"+std::to_string(frame%60)+".png")).string()))throw std::runtime_error("Ocean capture failed");}
 if(opt.fieldworkCheck&&!opt.captures.empty()&&(frame%60==29||frame%60==49)){fs::create_directories(opt.captures);if(!renderer.screenshot((fs::path(opt.captures)/("fieldwork-"+std::to_string(frame/60)+"-"+std::to_string(frame%60)+".png")).string()))throw std::runtime_error("Fieldwork capture failed");}
 if(opt.turnCheck&&!opt.captures.empty()&&(frame%60==9||frame%60==49)){fs::create_directories(opt.captures);if(!renderer.screenshot((fs::path(opt.captures)/("strategy-"+std::to_string(frame/60)+"-"+std::to_string(frame%60)+".png")).string()))throw std::runtime_error("Strategy capture failed");}
 if(opt.wildlifeCheck&&(frame%60==16||frame%60==49)){int scene=opt.wildlifeShot>=0?opt.wildlifeShot:frame/60;int drawn=game.s.mode==Mode::Flight?renderer.flightVisibleOrganisms():0;std::cout<<"Wildlife scene "<<scene<<" / phase "<<frame%60<<" / 3D bodies "<<drawn<<" / candidates "<<(game.s.mode==Mode::Flight?flightScene(game).organisms.size():0)<<" / altitude "<<(game.s.mode==Mode::Flight?game.s.flight.altitude():0)<<" / time "<<game.s.hours<<'\n';if(scene==4&&!opt.wildlifeBaseline&&drawn<3)throw std::runtime_error("Wildlife close flight fixture has no visible bodies");if(scene==7&&drawn!=0)throw std::runtime_error("Lifeless world has visible creatures");if(!opt.captures.empty()){fs::create_directories(opt.captures);if(!renderer.screenshot((fs::path(opt.captures)/("wildlife-"+std::to_string(scene)+"-"+std::to_string(frame%60)+".png")).string()))throw std::runtime_error("Wildlife capture failed");}}
 if(opt.ecosystemCheck&&frame%30==24&&!opt.captures.empty()){fs::create_directories(opt.captures);if(!renderer.screenshot((fs::path(opt.captures)/("ecosystem-"+std::to_string(frame/30)+".png")).string()))throw std::runtime_error("Ecosystem capture failed");}
 if(opt.livingCheck&&frame%60==49){int scene=frame/60;auto& f=game.s.flight;if(scene<4){auto& t=f.traffic.front();if(!hyperion::flight::finite(t.position)||t.health<=0)throw std::runtime_error("Invalid generated flight actor");std::cout<<"Living actor "<<scene<<" / "<<hyperion::flight::vesselDesignation(t.genome)<<" / "<<hyperion::flight::pilotTemperament(t.genome)<<" / weapon "<<hyperion::flight::enemyDef(t).weapon<<" / salvos "<<t.salvos<<'\n';}if(!opt.captures.empty()){fs::create_directories(opt.captures);if(!renderer.screenshot((fs::path(opt.captures)/("living-"+std::to_string(scene)+".png")).string()))throw std::runtime_error("Living universe capture failed");}}
 if(opt.generationCheck&&frame%30==24){int scene=frame/30;auto p=game.planet();int ports=0,links=0;for(const auto& port:game.s.economy.settlements)if(port.star==game.s.currentStar)ports++;for(const auto& link:game.s.economy.routes){auto* from=findSettlement(game.s.economy,link.from);if(from&&from->star==game.s.currentStar)links++;}
  auto audit=game.s.mode==Mode::Flight?renderer.auditGeneration(game.s.flight):hyperion::flight::Renderer::GenerationAudit{};if(audit.maximumError>.0065)throw std::runtime_error("Generated CPU/GPU coastline mismatch: "+std::to_string(audit.maximumError));
  generationScenes.push_back({{"scene",scene},{"star",game.s.currentStar},{"world",p.name},{"type",p.type},{"formation",p.formation},{"generation",p.generation},{"layout",game.universe.profile(p.starId).layout},{"charters",ports},{"supplyLinks",links},{"gpuSamples",audit.samples},{"gpuMaximumError",audit.maximumError},{"turns",game.s.turns}});
  if(!opt.captures.empty()){fs::create_directories(opt.captures);if(!renderer.screenshot((fs::path(opt.captures)/("generation-"+std::to_string(scene)+".png")).string()))throw std::runtime_error("Generation capture failed");}
 }
 if(opt.shipyardCheck&&!opt.captures.empty()&&(frame%60==49||(frame/60==7&&frame%60==22))){fs::create_directories(opt.captures);if(!renderer.screenshot((fs::path(opt.captures)/("shipyard-"+std::to_string(frame/60)+(frame%60==22?"-quote":"")+".png")).string()))throw std::runtime_error("Shipyard capture failed");}
 if(opt.securityCheck&&!opt.captures.empty()&&frame%60==49){fs::create_directories(opt.captures);if(!renderer.screenshot((fs::path(opt.captures)/("security-"+std::to_string(frame/60)+".png")).string()))throw std::runtime_error("Security capture failed");}
 if(opt.combatCheck&&!opt.captures.empty()&&(frame%90==74||(frame/90==7&&frame%90==30)||(frame/90==8&&frame%90==22)||(frame/90==12&&frame%90==20))){fs::create_directories(opt.captures);if(!renderer.screenshot((fs::path(opt.captures)/("combat-"+std::to_string(frame/90)+(frame%90==30?"-planning":frame%90==22?"-quote":frame%90==20?"-charge":"")+".png")).string()))throw std::runtime_error("Combat capture failed");}
 if(opt.stabilityCheck){auto& scene=stabilityScenes.back();int level=renderer.flightPlanetDetail(scene["body"].get<int>());if(level<0)throw std::runtime_error("Missing stability mesh state");scene["meshLevels"].push_back(level);}
 if(opt.graphicsCheck&&!opt.oceanCheck&&!opt.wreckCheck&&!opt.hudCheck&&!opt.plannerCheck&&!opt.fieldworkCheck&&!opt.continuityCheck){ui.action.clear();pendingWorldClick=false;}
 if(ui.clickConsumed)drag=false;
 if(pendingWorldClick&&!ui.clickConsumed){renderer.pointer(game,ui,mouse,runtime-lastClick<.35);lastClick=runtime;save();}
 if(opt.transitionCheck&&!opt.captures.empty()&&frame%40==39){fs::create_directories(opt.captures);renderer.screenshot((fs::path(opt.captures)/("transition-"+std::to_string(frame/40)+".png")).string());}
 if(opt.graphicsCheck&&!opt.oceanCheck&&!opt.wreckCheck&&!opt.continuityCheck&&!opt.fieldworkCheck&&!opt.hudCheck&&!opt.minorCheck&&!opt.screenFitCheck&&!opt.handlingCheck&&!opt.combatCheck&&!opt.securityCheck&&!opt.shipyardCheck&&!opt.generationCheck&&!opt.livingCheck&&!opt.ecosystemCheck&&!opt.wildlifeCheck&&!opt.turnCheck&&!opt.stabilityCheck&&opt.graphicsShot<0&&!opt.captures.empty()&&frame%18==17){fs::create_directories(opt.captures);if(!renderer.screenshot((fs::path(opt.captures)/(std::string(opt.closeMaterialCheck?"close-material-":opt.cloudOcclusionCheck?"cloud-occlusion-":opt.architectureCheck?"architecture-":opt.waterCheck?"water-":opt.lightingCheck?"lighting-":opt.surfaceDetailCheck?"surface-detail-":opt.materialCheck?"material-":opt.cloudCheck?"cloud-":opt.planetCheck?"planet-":opt.settlementCheck?"settlement-":"flight-beauty-")+std::to_string(frame/18)+".png")).string()))throw std::runtime_error("Graphics capture failed");}
 if(opt.stabilityCheck&&!opt.captures.empty()){fs::create_directories(opt.captures);if(!renderer.screenshot((fs::path(opt.captures)/("stability-"+std::to_string(frame/18)+"-"+std::to_string(frame%18)+".png")).string(),true))throw std::runtime_error("Planet stability capture failed");}
 if(opt.architectureCheck&&(frame/18==18||frame/18==19||frame/18==21||frame/18==23)&&!opt.captures.empty()){
  fs::create_directories(opt.captures);if(!renderer.screenshot((fs::path(opt.captures)/("texture-stability-"+std::to_string(frame/18)+"-"+std::to_string(frame%18)+".png")).string(),true))throw std::runtime_error("Building texture stability capture failed");
 }
 if(opt.surfaceDetailCheck&&!opt.captures.empty()&&frame==17){if(!renderer.exportFlightDetail((fs::path(opt.captures)/"surface-detail-atlas.png").string()))throw std::runtime_error("Could not export surface detail atlas");}
 if(opt.cloudOcclusionCheck&&frame%18==17){
  auto q=renderer.cloudOcclusionStats(),a=renderer.cloudOcclusionStats(true);const auto& f=game.s.flight;const auto& p=f.planets[2];
  cloudOcclusionScenes.push_back({{"scene",frame/18},{"testedPixels",q.testedPixels},{"leakingPixels",q.leakingPixels},{"maxLeakAlpha",q.maxLeakAlpha},{"auroraLeakingPixels",a.leakingPixels},{"maxAuroraLeak",a.maxLeakAlpha},{"maxAuroraLight",a.maxCloudAlpha},{"maxCloudAlpha",q.maxCloudAlpha},{"eyeAltitude",hyperion::flight::length(f.ship.position-p.centre)-p.radius}});
  std::cout<<"Cloud occlusion / scene "<<frame/18<<" / tested "<<q.testedPixels<<" / leaks "<<q.leakingPixels<<" / aurora leaks "<<a.leakingPixels<<" / max cloud "<<q.maxCloudAlpha<<'\n';
  if(q.leakingPixels||a.leakingPixels)throw std::runtime_error("Clouds leaked across foreground terrain");
 }
 if(opt.architectureCheck&&frame%18==17){auto m=renderer.flightArchitectureStats();architectureScenes.push_back({{"scene",frame/18},{"models",m.models},{"closeTriangles",m.closeTriangles},{"closePorts",m.closePorts},{"detailedTriangles",m.detailedTriangles},{"distantTriangles",m.distantTriangles},{"degenerateTriangles",m.degenerateTriangles},{"invalidVertices",m.invalidVertices},{"invalidNormals",m.invalidNormals}});if(m.invalidVertices||m.invalidNormals||m.degenerateTriangles)throw std::runtime_error("Invalid architecture mesh data");}
 if(opt.lightingCheck&&!opt.captures.empty()&&frame%18==17)renderer.exportFlightShadow((fs::path(opt.captures)/("sun-shadow-"+std::to_string(frame/18)+".png")).string());
 if(opt.materialCheck&&!opt.captures.empty()&&frame%18==17){if(!renderer.exportFlightMaterial(game.s.selectedPlanet,(fs::path(opt.captures)/("geology-"+std::to_string(frame/18)+".png")).string()))throw std::runtime_error("Could not export material diagnostic");}
 if(opt.flightCheck&&!opt.captures.empty()&&frame%30==29){fs::create_directories(opt.captures);renderer.screenshot((fs::path(opt.captures)/("frontier-"+std::to_string(frame/30)+".png")).string());}
 if(opt.economyCheck&&!opt.captures.empty()&&frame%15==14){fs::create_directories(opt.captures);renderer.screenshot((fs::path(opt.captures)/("economy-"+std::to_string(frame/15)+".png")).string());}
 if(opt.uiCheck&&!opt.captures.empty()&&frame%21==20){fs::create_directories(opt.captures);renderer.screenshot((fs::path(opt.captures)/("ui-"+std::to_string(frame/21)+".png")).string());}
 if(!game.soundEvent.empty()){audio.play(game.soundEvent);game.soundEvent.clear();}
 if(!ui.action.empty()){
  if(ui.action=="panel"||ui.action=="tab"){int tab=std::stoi(ui.arg);if(tab==5&&!game.s.economy.docked.empty())ui.settlement=game.s.economy.docked;ui.openPanel(tab,ui.action=="panel");}
  else if(ui.action=="oceanClose"){ui.panel=false;}
  else if(ui.action=="divePage"){ui.divePage=clamp(std::stoi(ui.arg),0,2);ui.scroll=0;}
  else if(ui.action=="orders"){if(game.s.mode==Mode::Ocean){ui.openPanel(0,ui.divePage==0);ui.divePage=0;}else ui.openPanel(0);ui.expanded.erase("turn-report");ui.expanded.erase("before-turn");ui.expanded.erase("turn-planner");ui.expanded.insert("play-turn");ui.scroll=0;}
  else if(ui.action=="missionBoard"){game.trackedMission=ui.arg;ui.openPanel(5);ui.expanded.insert("missions");ui.scroll=0;}
  else if(ui.action=="planHours"){int hours=std::stoi(ui.arg);if(hours==1||hours==3||hours==6||hours==12||hours==24)ui.plannedHours=hours;}
  else if(ui.action=="reportOlder")ui.reportIndex=std::min(ui.reportIndex+1,std::max(0,int(game.recentTurns.size())-1));
  else if(ui.action=="reportNewer")ui.reportIndex=std::max(0,ui.reportIndex-1);
  else if(ui.action=="menu"){ui.menu=!ui.menu;ui.sections=false;}
  else if(ui.action=="sections"){ui.sections=!ui.sections;ui.menu=false;}
  else if(ui.action=="dismissMenu")ui.menu=ui.sections=false;
  else if(ui.action=="pause"){ui.paused=!ui.paused;ui.menu=ui.sections=false;}
  else if(ui.action=="flightHUD"){toggleHUD();}
  else if(ui.action=="clean"){ui.toggleInterface();}
  else if(ui.action=="disclosure"){if(!ui.expanded.erase(ui.arg))ui.expanded.insert(ui.arg);}
  else if(ui.action=="filterDestinations")ui.destinationFilter=clamp(std::stoi(ui.arg),0,3);
  else if(ui.action=="selectGood")ui.marketGood=clamp(std::stoi(ui.arg),0,goodCount-1);
  else if(ui.action=="selectBuilding"){ui.buildingChoice=clamp(std::stoi(ui.arg),0,buildingCount-1);ui.expanded.erase("facilities");}
  else if(ui.action=="selectFaction")ui.factionChoice=clamp(std::stoi(ui.arg),1,factionCount-1);
  else if(ui.action=="researchPage"){ui.researchPage=clamp(std::stoi(ui.arg),0,1);ui.scroll=0;}
  else if(ui.action=="reviewSettlement"){ui.settlement=ui.arg;ui.openPanel(4);ui.scroll=0;}
  else if(ui.action=="selectSettlement"){ui.settlement=ui.arg;ui.expanded.erase("sites");ui.scroll=0;}
  else if(ui.action=="cycleGood"){ui.marketGood=(ui.marketGood+1)%goodCount;}
  else if(ui.action=="cycleBuilding"){ui.buildingChoice=(ui.buildingChoice+1)%buildingCount;}
  else if(ui.action=="cycleDestination"){ui.routeDestination++;}
  else if(ui.action=="cycleFaction"){ui.factionChoice=ui.factionChoice%4+1;}
  else if(ui.action=="help"){ui.help=true;ui.menu=ui.sections=false;}
  else if(ui.action=="settings"){ui.settings=true;ui.menu=ui.sections=false;ui.seedInput=game.s.seed;}
  else if(ui.action=="close"){ui.settings=false;ui.help=false;}
  else if(ui.action=="export")exportSave("hyperion-export");
  else if(ui.action=="sound")ui.sound=audio.toggle();
  else if(ui.action=="new"){if(!ui.seedInput.empty()&&exportSave("before-new-universe")&&preserveCampaign("disk-before-new-universe")){game.reset(ui.seedInput);ui.workspaceMemory.clear();ui.settings=false;ui.scroll=0;allowSave=!opt.noSave&&saveSession.acknowledge(error);ui.saveWarning=(!allowSave&&!opt.noSave)?"AUTOSAVE DISABLED / "+error:"";save();game.notify("New procedural universe created / previous expedition backed up");}}
  else if(ui.action=="target"){for(auto& o:game.targets())if(o.id==ui.arg){game.waypoint(o);break;}}
  else {ui.menu=ui.sections=false;order(ui.action,ui.arg);if(ui.action=="oceanSelect"){ui.scroll=0;ui.expanded.erase("dive-sites");ui.expanded.erase("dive-contacts");}if(ui.action=="navigate"&&game.s.flight.autopilot)ui.panel=false;if(ui.action=="found")ui.settlement=game.s.economy.docked;save();}
 }
 if(opt.plannerCheck&&frame/60>=8)turnFrozenHour=game.s.hours;
 if(opt.fieldworkCheck)fieldFrozenHour=game.s.hours;
 if(opt.continuityCheck)continuityFrozen=game.s.hours;
 if(opt.oceanCheck)oceanFrozen=game.s.hours;
 if(opt.wreckCheck)wreckFrozen=game.s.hours;
 if(runtime-lastSave>10){save();lastSave=runtime;}
 if(opt.tour&&!opt.captures.empty()&&(frame==89||frame==179||frame==269||frame==359)){fs::create_directories(opt.captures);auto file=fs::path(opt.captures)/(std::to_string(frame/90)+"-"+modeName(game.s.mode)+".png");renderer.screenshot(file.string());}
 if(opt.benchmark&&frame>=60){benchmarkSamples.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-frameStart).count());for(int i=0;i<6;i++)profileTotals[i]+=renderer.flightProfile()[i];}
 frame++;if(opt.frames&&frame>=opt.frames){if(!opt.screenshot.empty()){auto p=fs::path(opt.screenshot);if(p.has_parent_path())fs::create_directories(p.parent_path());renderer.screenshot(opt.screenshot);}quit=true;}
 }
 if(opt.wreckCheck){if(wreckScenes.size()!=9)throw std::runtime_error("Incomplete wreck diagnostic");if(!opt.captures.empty()){std::ofstream out(fs::path(opt.captures)/"wreck-scenes.json");out<<wreckScenes.dump(2)<<'\n';}std::cout<<"Wreck expeditions verified / "<<wreckScenes.dump()<<'\n';}
 if(opt.oceanCheck){if(oceanScenes.size()!=15)throw std::runtime_error("Incomplete ocean survey diagnostic");if(!opt.captures.empty()){std::ofstream out(fs::path(opt.captures)/"ocean-scenes.json");out<<oceanScenes.dump(2)<<'\n';}std::cout<<"Ocean surveys verified / "<<oceanScenes.dump()<<'\n';}
 if(opt.continuityCheck){if(continuityScenes.size()!=10)throw std::runtime_error("Incomplete continuity diagnostic");if(!opt.captures.empty()){std::ofstream out(fs::path(opt.captures)/"continuity-scenes.json");out<<continuityScenes.dump(2)<<'\n';}std::cout<<"Continuity verified / "<<continuityScenes.dump()<<'\n';}
 if(opt.fieldworkCheck){if(fieldScenes.size()!=8)throw std::runtime_error("Incomplete fieldwork diagnostic");if(!opt.captures.empty()){std::ofstream out(fs::path(opt.captures)/"fieldwork-scenes.json");out<<fieldScenes.dump(2)<<'\n';}std::cout<<"Fieldwork orders verified / "<<fieldScenes.dump()<<'\n';}
 if(opt.generationCheck&&!opt.captures.empty()){std::ofstream out(fs::path(opt.captures)/"generation-scenes.json");out<<generationScenes.dump(2)<<'\n';std::cout<<"Procedural visual check: ten scenes / shared CPU and GPU geography verified\n";}
 if(opt.shipyardCheck&&!opt.captures.empty()){std::ofstream out(fs::path(opt.captures)/"shipyard-scenes.json");out<<shipyardScenes.dump(2)<<'\n';std::cout<<"Shipyard visual check: eight native stock, refit, trade, hull and repair scenarios verified\n";}
 if(opt.securityCheck&&!opt.captures.empty()){std::ofstream out(fs::path(opt.captures)/"security-scenes.json");out<<securityScenes.dump(2)<<'\n';std::cout<<"Security visual check: six actual faction, piracy, warrant, escort and truce scenarios verified\n";}
 if(opt.hudCheck){if(hudScenes.size()!=12)throw std::runtime_error("Incomplete optional HUD check");if(!opt.captures.empty()){std::ofstream out(fs::path(opt.captures)/"hud-scenes.json");out<<hudScenes.dump(2)<<'\n';}std::cout<<"Optional HUD: "<<hudScenes.dump()<<'\n';}
 if(opt.closeMaterialCheck){if(!opt.benchmark&&closeMaterialScenes.size()!=size_t(opt.closeMaterialShot>=0?1:16))throw std::runtime_error("Incomplete close material diagnostic");if(!opt.captures.empty()){std::ofstream out(fs::path(opt.captures)/"close-material-scenes.json");out<<closeMaterialScenes.dump(2)<<'\n';}std::cout<<"Close material scenes: "<<closeMaterialScenes.dump()<<'\n';}
 if(opt.minorCheck){if(minorScenes.size()!=size_t(opt.minorShot<0?7:1))throw std::runtime_error("Incomplete minor-body diagnostic");if(!opt.captures.empty()){std::ofstream out(fs::path(opt.captures)/"minor-body-scenes.json");out<<minorScenes.dump(2)<<'\n';}std::cout<<"Generated minor bodies: "<<minorScenes.dump()<<'\n';}
 if(opt.screenFitCheck){
  if(screenFitScenes.size()!=8)throw std::runtime_error("Incomplete screen-fit check");
  if(!opt.captures.empty()){std::ofstream out(fs::path(opt.captures)/"screen-fit-scenes.json");out<<screenFitScenes.dump(2)<<'\n';}
  std::cout<<"Screen fit: 8 scenarios / small windows, monitor work area, fullscreen, pointer alignment, modals and strategy verified\n";
 }
 if(opt.combatCheck&&!opt.captures.empty()){std::ofstream out(fs::path(opt.captures)/"combat-scenes.json");out<<combatScenes.dump(2)<<'\n';std::cout<<"Combat visual check: 16 scenarios / real weapons, enemies, damage, salvage and strategic clock verified\n";}
 if(opt.cloudOcclusionCheck){
  int tested=0;float cloud=0;for(const auto& q:cloudOcclusionScenes){tested+=q["testedPixels"].get<int>();cloud=std::max(cloud,q["maxCloudAlpha"].get<float>());}
  if(tested<10000||cloud<.05f)throw std::runtime_error("Cloud occlusion diagnostic did not exercise both opaque terrain and visible weather");
  if(!opt.captures.empty()){fs::create_directories(opt.captures);std::ofstream out(fs::path(opt.captures)/"cloud-occlusion.json");out<<cloudOcclusionScenes.dump(2)<<'\n';}
 }
 if(opt.architectureCheck&&!opt.captures.empty()){fs::create_directories(opt.captures);std::ofstream out(fs::path(opt.captures)/"architecture-scenes.json");out<<architectureScenes.dump(2)<<'\n';}
 if(opt.waterCheck&&!opt.captures.empty()){fs::create_directories(opt.captures);std::ofstream out(fs::path(opt.captures)/"water-scenes.json");out<<waterScenes.dump(2)<<'\n';}
 if(opt.stabilityCheck&&!opt.captures.empty()){std::ofstream out(fs::path(opt.captures)/"scenes.json");out<<stabilityScenes.dump(2)<<'\n';}
 if(opt.handlingCheck){if(handlingScenes.size()!=size_t(opt.helmCheck?10:6))throw std::runtime_error("Incomplete handling diagnostic");if(!opt.captures.empty()){std::ofstream out(fs::path(opt.captures)/"handling-scenes.json");out<<handlingScenes.dump(2)<<'\n';}std::cout<<"Ship handling verified / "<<handlingScenes.dump()<<'\n';}
 if(opt.turnCheck){if(turnScenes.size()!=size_t(opt.plannerCheck?16:8))throw std::runtime_error("Incomplete strategy diagnostic");if(!opt.captures.empty()){std::ofstream out(fs::path(opt.captures)/"strategy-scenes.json");out<<turnScenes.dump(2)<<'\n';}std::cout<<"Strategy handoffs verified / "<<turnScenes.dump()<<'\n';}
 if(opt.transitionCheck){if(!transitionLanded||!transitionDeparted||game.s.flight.launchAssist>0)throw std::runtime_error("Surface transition diagnostic did not complete landing and departure");std::cout<<"Surface transition verified / contact step "<<contactStep<<" / contact turn "<<contactTurn<<" degrees / hull "<<game.s.hull<<'\n';}
 if(opt.benchmark&&!benchmarkSamples.empty()){
  double total=0;for(double t:benchmarkSamples)total+=t;auto sorted=benchmarkSamples;std::sort(sorted.begin(),sorted.end());auto percentile=[&](double p){return sorted[std::min(sorted.size()-1,size_t(p*(sorted.size()-1)))];};
  nlohmann::json report={{"scene",opt.wildlifeShot>=0?opt.wildlifeShot:opt.graphicsShot},{"wildlifeBaseline",opt.wildlifeBaseline},{"wildlifeBodies",renderer.flightVisibleOrganisms()},{"warmupFrames",60},{"measuredFrames",benchmarkSamples.size()},{"outputWidth",GetRenderWidth()},{"outputHeight",GetRenderHeight()},{"reducedEffects",opt.reducedEffects},{"vsync",false},{"fps",benchmarkSamples.size()*1000/total},{"meanMs",total/benchmarkSamples.size()},{"p50Ms",percentile(.50)},{"p95Ms",percentile(.95)},{"p99Ms",percentile(.99)}};
  if(opt.profileFlight){const char* names[]={"sky","planetsAndAtmosphere","auroras","portsAndTraffic","cockpit","bloomAndPost"};for(int i=0;i<6;i++)report["profileMs"][names[i]]=profileTotals[i]/benchmarkSamples.size();}
  if(!opt.benchmarkReport.empty()){auto p=fs::path(opt.benchmarkReport);if(p.has_parent_path())fs::create_directories(p.parent_path());std::ofstream out(p);out<<report.dump(2)<<'\n';}std::cout<<"Flight benchmark: "<<report.dump()<<'\n';
 }
 save();std::cout<<"Hyperion native run: "<<frame<<" frames / shaders "<<(renderer.ready()?"OK":"FAILED")<<" / "<<GetRenderWidth()<<"x"<<GetRenderHeight()<<"\n";
 std::cout<<"Measured render rate: "<<frame/std::chrono::duration<double>(std::chrono::steady_clock::now()-runStart).count()<<" frames/s\n";renderer.close();pointerEvents.detach();CloseWindow();return 0;
 }catch(const std::exception& e){std::cerr<<"Hyperion: "<<e.what()<<'\n';return 1;}}
