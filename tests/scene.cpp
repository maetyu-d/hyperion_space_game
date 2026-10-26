#include "game.hpp"
#include "flight_scene.hpp"
#include "economy_storage.hpp"
#include <iostream>
#include <set>
#include <stdexcept>
using namespace hyperion;
namespace {
int checks=0;
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
bool close(double a,double b){return std::abs(a-b)<1e-7;}
Economy production(Universe& u){
 Economy e;initializeEconomy(e,u,0);e.settlements.resize(1);e.routes.clear();e.contracts.clear();
 auto& s=e.settlements.front();s.owner=0;s.planet=2;s.residents=24;s.condition=1;s.health=s.morale=100;s.built={};
 s.built[Habitat]=2;s.built[Warehouse]=4;s.built[Reactor]=1;s.built[Refinery]=1;s.stock={};s.stock[Rations]=120;s.stock[Propellant]=20;s.stock[Ore]=40;s.nextMigration=1e6;return e;
}
}
int main(){try{
 Game g;g.enterFlight();g.s.hours=.25;g.syncEconomy();auto before=encodeEconomy(g.s.economy);auto position=g.s.flight.ship.position;auto hours=g.s.hours;
 auto scene=flightScene(g);for(int i=0;i<50;i++)flightScene(g);
 check(encodeEconomy(g.s.economy)==before&&g.s.hours==hours&&hyperion::flight::length(position-g.s.flight.ship.position)==0,"presentation snapshots must never move the clock, ship, money or goods");
 check(scene.settlements.size()==g.s.economy.settlements.size(),"every real local settlement is represented");
 for(auto& site:g.s.economy.settlements){auto* v=scene.settlement(site.id);check(v&&v->facilities==site.built&&v->stock==site.stock&&v->activity==site.activity,"presentation uses authoritative facilities, goods and shift activity");}
 auto v=scene.settlements.front();auto layout=hyperion::flight::layoutSettlement(v);size_t total=0;for(int n:v.facilities)total+=n;
 check(layout.facilities.size()==total,"one near-view site per actual facility");
 auto oldKey=v.geometryKey();v.activity.fill(.9);v.condition=.2;v.pollution=.8;v.ecology=.1;
 check(v.geometryKey()==oldKey,"shift changes update uniforms without rebuilding geometry");
 v.facilities[hyperion::flight::Fabricator]++;auto grown=hyperion::flight::layoutSettlement(v);check(v.geometryKey()!=oldKey,"construction invalidates settlement geometry");
 for(auto& a:layout.facilities){auto it=std::find_if(grown.facilities.begin(),grown.facilities.end(),[&](const hyperion::flight::FacilitySite& b){return a.kind==b.kind&&a.ordinal==b.ordinal;});check(it!=grown.facilities.end()&&hyperion::flight::length(it->position-a.position)<1e-8,"existing buildings keep their sites as new districts grow");}
 v.facilities.fill(24);auto maximum=hyperion::flight::layoutSettlement(v);check(maximum.facilities.size()==336,"all supported facility counts have stable bounded sites");
 for(auto& a:maximum.facilities)check(hyperion::flight::finite(a.position)&&hyperion::flight::length(a.position)>60&&hyperion::flight::length(a.position)<maximum.radius,"buildings clear the landing pad and fit the settlement bound");
 std::set<int> shapes;std::set<std::array<unsigned char,3>> accents;for(int i=0;i<5;i++){shapes.insert(hyperion::flight::factionStyle(i).silhouette);accents.insert(hyperion::flight::factionStyle(i).accent);}
 check(shapes.size()==5&&accents.size()==5,"all five factions have distinct silhouettes and palettes");
 v.stock.fill(0);auto empty=v.geometryKey();v.stock[Rations]=.01;check(v.geometryKey()!=empty,"warehouse empty-to-occupied transition changes stacks");auto first=v.geometryKey();v.stock[Rations]=19;check(v.geometryKey()==first,"consumption inside a stack bucket does not rebuild geometry");v.stock[Rations]=21;check(v.geometryKey()!=first,"additional real stock adds a visual stack");

 Universe u;auto active=production(u);advanceEconomy(active,u,.25);check(active.settlements[0].activity[Refinery]>0&&active.settlements[0].progress[Refinery]>0,"lit refinery records an actual productive shift");
 auto missing=production(u);missing.settlements[0].stock[Ore]=0;advanceEconomy(missing,u,.25);check(missing.settlements[0].activity[Refinery]==0&&missing.settlements[0].progress[Refinery]==0,"material shortages extinguish factory activity");
 auto powerless=production(u);powerless.settlements[0].built[Reactor]=0;advanceEconomy(powerless,u,.25);check(powerless.settlements[0].activity[Refinery]==0&&powerless.settlements[0].activity[Reactor]==0,"brownout extinguishes factories and reactor");
 auto exhausted=production(u);exhausted.settlements[0].built[Refinery]=0;exhausted.settlements[0].built[Mine]=1;exhausted.settlements[0].oreDeposit=0;advanceEconomy(exhausted,u,.25);check(exhausted.settlements[0].activity[Mine]==0,"exhausted deposits stop extractor activity");
 auto emptyTown=production(u);emptyTown.settlements[0].activity.fill(1);emptyTown.settlements[0].residents=0;advanceEconomy(emptyTown,u,.25);for(double a:emptyTown.settlements[0].activity)check(a==0,"abandoned facilities clear previous shift activity");
 // Visual metadata must be causally inert, even when imported values differ.
 auto a=production(u),b=a;a.settlements[0].activity.fill(0);b.settlements[0].activity.fill(1);advanceEconomy(a,u,4);advanceEconomy(b,u,4);check(encodeEconomy(a)==encodeEconomy(b),"presentation metadata cannot affect the simulation");
 auto encoded=encodeEconomy(active);auto restored=decodeEconomy(encoded,u,.25);check(restored.settlements[0].activity==active.settlements[0].activity,"last shift survives save/reopen");
 for(auto& s:encoded["settlements"])s.erase("activity");restored=decodeEconomy(encoded,u,.25);check(restored.settlements[0].activity==std::array<double,buildingCount>{},"older saves without activity remain valid");
 encoded["settlements"][0]["activity"]=std::array<double,buildingCount>{};encoded["settlements"][0]["activity"][0]=1.01;bool rejected=false;try{decodeEconomy(encoded,u,.25);}catch(...){rejected=true;}check(rejected,"malformed activity is rejected");

 auto& route=g.s.economy.routes.front();route.outbound=true;route.cargo={};route.cargo[route.good]=24;route.remaining=route.duration;g.updateFlight(0);scene=flightScene(g);
 auto* packet=scene.shipment(route.id);check(packet&&packet->loading&&!packet->holding&&packet->amount==24,"visible loading is tied to a real outbound packet");
 auto economic=encodeEconomy(g.s.economy);g.updateFlight(0);check(encodeEconomy(g.s.economy)==economic,"visual route placement cannot load goods twice");
 auto convoy=std::find_if(g.s.flight.traffic.begin(),g.s.flight.traffic.end(),[&](const hyperion::flight::Traffic& t){return t.routeId==route.id;});check(convoy!=g.s.flight.traffic.end()&&!convoy->transitPath.empty(),"actual freight has a cached physical transfer route");
 auto path=convoy->transitPath;check(path.size()>2,"transfer route includes safe approach waypoints");
 for(int i=0;i<=1000;i++){auto pose=hyperion::flight::freightPose(path,i/1000.);check(hyperion::flight::finite(pose.position)&&close(hyperion::flight::length(pose.forward),1),"freight poses and headings stay finite");for(auto& planet:g.s.flight.planets)check(hyperion::flight::length(pose.position-planet.centre)>planet.radius,"freight routes avoid the interiors of actual planets");}
 auto start=hyperion::flight::freightPose(path,0),loading=hyperion::flight::freightPose(path,.04),finish=hyperion::flight::freightPose(path,1),unloading=hyperion::flight::freightPose(path,.97);
 check(hyperion::flight::length(start.position-loading.position)<1e-8&&hyperion::flight::length(finish.position-unloading.position)<1e-8,"loading and unloading hold at the actual berths");
 route.remaining=route.duration*.5;g.updateFlight(0);convoy=std::find_if(g.s.flight.traffic.begin(),g.s.flight.traffic.end(),[&](const hyperion::flight::Traffic& t){return t.routeId==route.id;});auto expected=hyperion::flight::freightPose(convoy->transitPath,.5);check(hyperion::flight::length(expected.forward-convoy->forward)<1e-8,"physics preserves the heading of economy-driven freight");
 route.remaining=0;g.updateFlight(0);scene=flightScene(g);packet=scene.shipment(route.id);check(packet&&packet->holding&&!packet->unloading,"blocked arrival remains one visible holding packet");
 auto inTransit=route.cargo;g.system("HOME");auto clock=g.s.hours;auto seconds=g.s.flight.time;for(int i=0;i<100;i++)g.simulate(.1);check(g.s.hours==clock&&g.s.flight.time==seconds&&route.cargo==inTransit,"strategy freezes freight and port activity until a committed order");
 route.outbound=false;route.cargo={};g.updateFlight(0);scene=flightScene(g);check(!scene.shipment(route.id)&&std::none_of(g.s.flight.traffic.begin(),g.s.flight.traffic.end(),[&](const hyperion::flight::Traffic& t){return t.routeId==route.id;}),"resolved packets remove their ship and handling animation");
 std::cout<<"PASS: "<<checks<<" living-world checks; real production activity, stable growth, distinct factions, safe freight, save compatibility and turn-based freeze\n";return 0;
}catch(const std::exception& e){std::cerr<<"Living-world regression: "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
