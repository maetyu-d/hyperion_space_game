#include "game.hpp"
#include "economy_storage.hpp"
#include "flight_storage.hpp"
#include "security_storage.hpp"
#include "json.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace hyperion;
namespace {
int checks=0;
void require(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
bool close(double a,double b){return std::abs(a-b)<1e-6;}
int64_t money(const Economy& e){int64_t sum=e.credits;for(const auto& f:e.factions)sum+=f.treasury;for(const auto& p:e.settlements)sum+=p.treasury;for(const auto& b:e.pirates)sum+=b.treasury;for(const auto& c:e.contracts)sum+=c.escrow;return sum;}
hyperion::flight::Traffic& pirate(Game& g){for(auto& t:g.s.flight.traffic)if(t.managed&&t.pirate)return t;throw std::runtime_error("No pirate");}
hyperion::flight::Traffic& navy(Game& g){for(auto& t:g.s.flight.traffic)if(t.managed&&!t.pirate)return t;throw std::runtime_error("No patrol");}
hyperion::flight::Game arena(){hyperion::flight::Game f;f.valid=true;f.docked=false;f.planets.push_back({"Test","",{10000,0,0},500,3,{0,1,0}});f.ports.push_back({"Relay","",{5000,5000,5000},{0,1,0},-1,"relay:test","",0});f.ship.gear=false;f.orient({0,0,-1},{0,1,0});return f;}
hyperion::flight::Traffic actor(std::string id,hyperion::flight::V3 at,bool pirate){hyperion::flight::Traffic t;t.id=id;t.name=id;t.kind=1;t.managed=true;t.pirate=pirate;t.hostile=false;t.band=pirate?"band:test":"";t.faction=pirate?1:3;t.health=100;t.shield=30;t.position=t.previous=at;t.positioned=true;t.forward=hyperion::flight::unit(-at);return t;}
}
int main(){try{
 {Universe u("MERIDIAN-0001");Economy before;initializeEconomy(before,u,0);auto cash=money(before);Stock stock{};for(const auto& p:before.settlements)for(int k=0;k<goodCount;k++)stock[k]+=p.stock[k];discoverSecurity(before,u,"HOME");
  require(before.pirates.size()==1&&before.captains.size()==7,"one persistent syndicate and seven distinct captains per system");require(money(before)==cash,"initial diversion and shipyard payments conserve all credits");
  for(const auto& p:before.settlements)for(int k=0;k<goodCount;k++)stock[k]-=p.stock[k];for(const auto& b:before.pirates)for(int k=0;k<goodCount;k++)stock[k]-=b.stock[k];
  require(close(stock[Alloy],28)&&close(stock[Components],14)&&close(stock[Propellant],14),"seven initial ships consume actual construction materials");
  auto saved=encodeSecurity(before);discoverSecurity(before,u,"HOME");require(encodeSecurity(before)==saved,"discovery cannot divert funds or duplicate crews twice");
  Economy other;initializeEconomy(other,u,0);discoverSecurity(other,u,"HOME");require(encodeSecurity(other)==saved,"seeded organizations and named captains are deterministic");
 }
 {Game g;g.enterFlight();auto& f=g.s.flight;require(!navy(g).hostile&&pirate(g).hostile,"legitimate patrols begin neutral while loaded NOMA attracts piracy");require(navy(g).band.empty()&&!pirate(g).band.empty(),"pirate allegiance is independent of the host authority");require(f.enemiesOf(navy(g),pirate(g))&&f.enemiesOf(pirate(g),navy(g)),"patrols and pirates fight each other");
  g.storeShipStock({});g.refreshSecurity();require(!pirate(g).hostile,"pirates do not demand an empty hold without a grudge");auto& band=g.s.economy.pirates[0];band.grievance=20;g.refreshSecurity();require(pirate(g).hostile&&pirate(g).reason.find("Vendetta")!=std::string::npos,"a vendetta persists even with an empty hold");
  auto& e=g.s.economy;auto bank=money(e),credit=e.credits,price=pirateTrucePrice(band),pirateCash=band.treasury;g.action("pirateTruce",band.id);
  require(e.credits==credit-price&&band.treasury==pirateCash+price&&money(e)==bank,"safe passage transfers its full price to the pirate treasury");require(!pirate(g).hostile&&band.grievance==0&&close(band.truceUntil,e.hour+24),"truce settles crew vendettas for 24 simulated hours");for(const auto& c:e.captains)if(c.band==band.id)require(c.grudge==0,"truce settles every member's personal grudge");
  auto frozen=encodeEconomy(e);g.action("pirateTruce",band.id);require(encodeEconomy(e)==frozen,"repeated truce cannot consume money or a turn");
  auto id=pirate(g).id;int target=int(&pirate(g)-f.traffic.data());f.damageTraffic(target,1,hyperion::flight::Pulse,pirate(g).position,true);g.processFlightEvents();g.refreshSecurity();require(band.truceUntil==0&&band.grievance>=10&&pirate(g).hostile,"attacking a protected pirate breaks the agreement and starts a feud");require(findCaptain(e,id)->grudge>0,"the attacked captain remembers the player");
 }
 {Game g;g.enterFlight();auto& e=g.s.economy;auto& f=g.s.flight;auto id=navy(g).id;int faction=navy(g).faction,index=int(&navy(g)-f.traffic.data());double standing=e.factions[faction].standing;
  f.damageTraffic(index,1,hyperion::flight::Pulse,navy(g).position,true);g.processFlightEvents();g.refreshSecurity();require(e.factions[faction].warrant==75&&e.factions[faction].standing==standing-8,"a shield hit against a neutral patrol creates a warrant and standing loss");require(navy(g).hostile&&navy(g).enemies[0],"wanted status also authorizes interdiction of player freight");
  f.damageTraffic(index,1,hyperion::flight::Pulse,navy(g).position,true);g.processFlightEvents();require(e.factions[faction].warrant==75,"one assault is recorded once per encounter, not once per projectile");
  auto* c=findCaptain(e,id);f.damageTraffic(index,1000,hyperion::flight::Plasma,navy(g).position,true);g.processFlightEvents();require(c->destroyed&&c->defeats==1&&e.factions[faction].warrant==375,"patrol destruction creates persistent losses and additional restitution");
  auto* port=findSettlement(e,c->home);f.dockAt(g.flightPort(port->id,true));g.processFlightEvents();auto credits=e.credits,bank=money(e);g.action("restitution",std::to_string(faction));require(e.factions[faction].warrant==0&&e.credits==credits-375&&money(e)==bank,"restitution at the issuing authority conserves money and clears the warrant");require(c->grudge==0,"formal restitution settles the patrol captain's personal feud");
 }
 {Game g;g.enterFlight();auto& e=g.s.economy;auto& f=g.s.flight;auto id=pirate(g).id;int target=int(&pirate(g)-f.traffic.data());auto bank=money(e),credits=e.credits;f.damageTraffic(target,1000,hyperion::flight::Plasma,pirate(g).position,true);g.processFlightEvents();
  auto* c=findCaptain(e,id);require(c->destroyed&&c->defeats==1&&c->grudge>=20,"defeated pirate captains survive as named persistent enemies");require(e.credits==credits+240&&money(e)==bank,"the bounty is paid by an eligible authority's actual treasury");require(e.pirates[0].grievance>=20,"pirate crew losses create a collective vendetta");
  auto paid=e.credits;g.securityEvent({"shipLoss",-1,1,"","NOMA",id});require(e.credits==paid&&c->defeats==1,"duplicate loss events cannot pay a second bounty");
  auto& band=e.pirates[0];band.stock={};e.hour=c->readyAt;advanceSecurity(e);require(c->destroyed,"a timer alone cannot respawn a destroyed ship");band.stock[Alloy]=8;band.stock[Components]=4;band.stock[Propellant]=4;band.treasury=240;e.hour=c->readyAt;advanceSecurity(e);require(!c->destroyed&&c->hull==hyperion::flight::enemyDef(c->role,c->genome).hull,"funded replacement restores the same captain's ship");require(band.treasury==0&&close(stockTotal(band.stock),0),"replacement consumes the entire specified material recipe and fee");require(c->defeats==1&&c->grudge>=20,"replacement preserves career history and hatred");g.refreshSecurity();require(!pirate(g).dead,"the canonical replacement rejoins actual local flight");
 }
 {Game g;g.ensureEconomy();auto& e=g.s.economy;auto& band=e.pirates[0];for(auto& r:e.routes){r.outbound=false;r.nextDeparture=1e9;}auto& route=e.routes[0];route.outbound=true;route.cargo={};route.cargo[Rations]=24;route.remaining=6;band.nextRaid=e.hour;e.securityWitness.clear();auto* haven=findSettlement(e,band.haven);haven->stock[Rations]=1000;
  double original=route.cargo[Rations]+band.stock[Rations];advanceSecurity(e);double first=24-route.cargo[Rations];require(first>0&&first<24,"offscreen piracy removes a partial shipment while survivors continue");require(close(route.cargo[Rations]+band.stock[Rations],original),"offscreen piracy transfers goods without copying or deleting them");
  for(auto& c:e.captains)if(c.band.empty()){c.destroyed=true;c.readyAt=e.hour+100;}route.cargo[Rations]=24;band.nextRaid=e.hour;advanceSecurity(e);require(24-route.cargo[Rations]>first,"patrol casualties make the supply network more vulnerable");
  for(auto& c:e.captains)if(c.band.empty()&&c.faction==3)c.destroyed=false;
  route.cargo[Rations]=24;route.escortUntil=e.hour+24;band.nextRaid=e.hour;advanceSecurity(e);require(route.cargo[Rations]==24,"a commissioned escort deters an offscreen raid");
  route.escortUntil=0;route.owner=0;band.truceUntil=e.hour+24;band.grievance=0;band.nextRaid=e.hour;advanceSecurity(e);require(route.cargo[Rations]==24,"safe passage covers player-owned freight packets");
  route.owner=3;band.nextRaid=e.hour;advanceSecurity(e);require(route.cargo[Rations]<24,"NOMA's truce does not shield an unrelated faction's freight");
  route.cargo[Rations]=24;e.securityWitness="HOME";band.nextRaid=e.hour;advanceSecurity(e);require(route.cargo[Rations]==24,"a witnessed system resolves raids through actual flight instead of abstract duplicate attacks");
 }
 {auto f=arena();auto patrol=actor("patrol",{0,0,-200},false),raider=actor("pirate",{50,0,-200},true);f.traffic={patrol,raider};f.cycleCombatTarget();require(f.combatTarget.empty(),"target cycling excludes non-hostile contacts");f.traffic[1].hostile=true;f.cycleCombatTarget();require(f.combatTarget=="pirate","target cycling includes an actual hostile contact");
  f.traffic[1].band="passage";auto convoy=actor("convoy",{20,0,-200},false);convoy.kind=2;convoy.band="passage";require(!f.enemiesOf(f.traffic[1],convoy),"physical freighters honor a purchased pirate safe-passage marker");
  auto before=f.traffic[0].shield;f.chainArc(1,14);require(f.traffic[0].shield==before,"resonance cannot chain into a neutral patrol");
  f.traffic[1].hostile=false;double shield=f.ship.shield;for(int i=0;i<200;i++)f.update(.025);require(f.traffic[0].salvos>0&&f.traffic[1].salvos>0,"patrols and pirates exchange actual shots independently of the player");require(f.ship.shield==shield,"neutral NPC firefights do not target the player's ship");require(f.traffic[0].shield<30||f.traffic[1].shield<30,"NPC projectiles inflict actual damage on opposing armed ships");
  auto a=actor("a",{},false),b=actor("b",{},false);a.faction=1;b.faction=2;a.enemies[2]=true;require(f.enemiesOf(a,b),"inter-faction war admits military targets");a.enemies[2]=false;require(!f.enemiesOf(a,b),"a faction patrol does not fire on a peaceful military contact");
 }
 {Game g;g.enterFlight();auto& e=g.s.economy;auto& f=g.s.flight;auto* band=&e.pirates[0];auto id=pirate(g).id;auto target=int(&pirate(g)-f.traffic.data());pirate(g).health=41;pirate(g).modules={11,22,33};pirate(g).ammunition=19;g.storeSecurity();require(findCaptain(e,id)->rounds==19&&findCaptain(e,id)->hull==41,"flight wear and magazines are stored in the campaign's crew record");
  f.valid=false;f.worldKey.clear();f.traffic.clear();g.ensureFlight();require(pirate(g).health==41&&pirate(g).ammunition==19&&pirate(g).modules[0]==11,"rebuilding a system cannot heal or rearm its enemies");
  auto& route=e.routes[0];route.outbound=true;route.cargo={};route.cargo[Components]=20;auto original=band->stock[Components];f.events.push_back({"freightLoss",-1,3,route.id,id,"freight"});g.processFlightEvents();require(!route.outbound&&stockTotal(route.cargo)==0&&band->stock[Components]==original+20,"physical pirate victory takes the actual freight packet into pirate inventory");g.processFlightEvents();require(band->stock[Components]==original+20,"freight loot cannot be collected twice");
  (void)target;
 }
 {Game g;g.enterFlight();g.system("HOME");auto before=encodeEconomy(g.s.economy);double flight=g.s.flight.time;for(int i=0;i<240;i++)g.simulate(.1);require(encodeEconomy(g.s.economy)==before&&g.s.flight.time==flight,"strategy thinking time freezes crews, raids, feuds, economy and flight");
  auto turns=g.s.turns;g.action("inspectContact",navy(g).id);require(g.s.turns==turns,"inspecting a captain is a free planning action");g.action("pirateTruce",g.s.economy.pirates[0].id);require(g.s.turns==turns+1,"a successful political agreement commits exactly one strategy turn");
 }
 {Game g;g.enterFlight();auto& e=g.s.economy;auto& f=g.s.flight;auto port=e.settlements[0].id;f.dockAt(g.flightPort(port,true));g.processFlightEvents();g.system("HOME");auto credits=e.credits;auto turns=g.s.turns;auto route=e.routes[0].id;g.action("escort",route);require(e.credits==credits-100&&g.s.turns==turns+1&&e.routes[0].escortUntil>e.hour,"an escort order transfers money and commits a strategy turn");turns=g.s.turns;credits=e.credits;g.action("escort",route);require(e.credits==credits&&g.s.turns==turns,"duplicate escort orders are free failures");
  e.factions[1].relations[2]=e.factions[2].relations[1]=-64;require(!factionsAtWar(e,1,2),"an embargo alone is not automatically war");e.factions[2].relations[1]=-65;require(factionsAtWar(e,1,2)&&factionsAtWar(e,2,1),"war is symmetric at the escalation threshold");
 }
 {Game g;g.enterFlight();auto& e=g.s.economy;e.pirates[0].grievance=43;e.factions[1].warrant=987;e.routes[0].escortUntil=23;pirate(g).ammunition=47;g.storeSecurity();auto encoded=encodeEconomy(e);auto decoded=decodeEconomy(encoded,g.universe,g.s.hours);require(encodeEconomy(decoded)==encoded,"security, political debts, finite stores and career histories round trip exactly");
  auto bad=encoded;bad["security"]["captains"][0]["rounds"]=1.5;bool rejected=false;try{decodeEconomy(bad,g.universe,g.s.hours);}catch(...){rejected=true;}require(rejected,"fractional ammunition is rejected");bad=encoded;bad["security"]["pirates"][0]["haven"]="missing";rejected=false;try{decodeEconomy(bad,g.universe,g.s.hours);}catch(...){rejected=true;}require(rejected,"unknown pirate home cannot enter a campaign");bad=encoded;bad["security"]["captains"].push_back(bad["security"]["captains"][0]);rejected=false;try{decodeEconomy(bad,g.universe,g.s.hours);}catch(...){rejected=true;}require(rejected,"duplicate captain IDs are rejected");
  auto path=std::filesystem::temp_directory_path()/"hyperion-security-326-roundtrip.json";std::string error;require(saveState(g.s,path.string(),error),"save persistent politics campaign");State loaded;require(loadState(loaded,path.string(),error),"reload persistent politics campaign");require(loaded.economy.factions[1].warrant==987&&loaded.economy.pirates[0].grievance==43,"warrants and vendettas survive complete save loading");std::filesystem::remove(path);
  encoded.erase("security");encoded.erase("missions");for(auto& v:encoded["factions"])v.erase("warrant");for(auto& v:encoded["routes"])v.erase("escortUntil");auto legacy=decodeEconomy(encoded,g.universe,g.s.hours);require(legacy.captains.empty()&&legacy.factions[1].warrant==0,"legacy economies migrate with clean legal history");
 }
 {Game g;g.enterFlight();auto& e=g.s.economy;auto& p=e.settlements[0];g.s.flight.dockAt(g.flightPort(p.id,true));g.processFlightEvents();e.factions[p.owner].warrant=75;
  auto before=encodeEconomy(e);auto ammo=g.s.flight.ship.combat.rounds;g.action("rearm");g.action("service");g.action("courier");require(g.s.flight.ship.combat.rounds==ammo&&!g.s.flight.courier.active&&encodeEconomy(e)==before,"a wanted captain cannot rearm, repair or obtain an authority courier commission");
  auto cargo=g.shipStock();std::string message;require(!trade(e,p,cargo,Components,1,true,message),"warrants embargo strategic goods even with positive standing");require(!recruitColonists(e,p,message),"wanted expeditions cannot recruit an authority's volunteers");
  double standing=e.factions[p.owner].standing;cargo[Research]=20;require(diplomaticMission(e,p.owner,cargo,message)&&e.factions[p.owner].standing==standing+8&&e.factions[p.owner].warrant==75,"diplomacy improves standing without silently erasing legal debt");
 }
 {Game g;g.ensureEconomy();auto& e=g.s.economy;auto& band=e.pirates[0];auto& route=e.routes[0];route.outbound=true;route.cargo={};route.cargo[Rations]=24;route.escortUntil=24;band.nextRaid=0;auto* haven=findSettlement(e,band.haven);haven->stock[Rations]=1000;
  for(auto& c:e.captains)if(c.band.empty()){c.destroyed=true;c.readyAt=100;}advanceSecurity(e);require(route.cargo[Rations]<24,"a paid escort loses protection when every real patrol is destroyed");
  auto oldId=e.captains[0].id;auto* base=findSettlement(e,e.captains[0].home);base->owner=0;double stock=base->stock[Alloy];e.captains[0].destroyed=false;advanceSecurity(e);require(findCaptain(e,oldId)->destroyed&&base->stock[Alloy]==stock,"a transferred charter cannot keep funding the former authority's patrol from player stock");
 }
 {Game g;g.enterFlight();auto id=pirate(g).id;pirate(g).health=41;pirate(g).ammunition=19;g.s.economy.pirates[0].grievance=31;g.storeSecurity();
  auto& f=g.s.flight;f.docked=false;f.dockedPort=-1;f.savedDock.clear();f.ship.position=f.planets[2].centre+hyperion::flight::V3{0,2500,0};f.recordIds();std::string destination;
  for(int y=-1;y<=1&&destination.empty();y++)for(int x=-1;x<=1&&destination.empty();x++)for(const auto& star:g.universe.cell(x,y))if(star.id!="HOME"&&distance(g.currentStar().pos,star.pos)<g.range()){destination=star.id;break;}
  require(!destination.empty()&&g.travel(destination),"a live campaign can jump away with crew records stored");require(g.s.economy.pirates.size()==2,"new systems receive their own independent pirate organization");require(g.travel("HOME"),"return jump uses the normal campaign travel path");g.ensureFlight();auto* c=findCaptain(g.s.economy,id);
  require(c&&c->hull==41&&c->rounds==19&&pirate(g).health==41&&pirate(g).ammunition==19,"actual interstellar travel cannot reset the original captain's damage or magazines");require(g.s.economy.pirates[0].grievance==31&&pirate(g).hostile,"the original crew's vendetta survives visits to another system");
 }
 std::cout<<"Security integration: "<<checks<<" checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
