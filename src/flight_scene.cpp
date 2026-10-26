#include "flight_scene.hpp"
#include "game.hpp"
#include "wildlife.hpp"
#include "flight/life_visibility.hpp"

namespace hyperion {
hyperion::flight::Scene flightScene(const Game& game){
 static_assert(buildingCount==hyperion::flight::facilityKinds&&goodCount==hyperion::flight::commodityKinds);
 hyperion::flight::Scene result;result.hours=game.s.hours;result.seconds=game.s.flight.time;
 for(const auto& s:game.s.economy.settlements)if(s.star==game.s.currentStar){
  hyperion::flight::SettlementView view;view.id=s.id;view.name=s.name;view.faction=s.owner;view.factionName=game.s.economy.factions[s.owner].name;
  view.residents=s.residents;view.alerts=s.alerts;view.submerged=s.depth>0;view.status=settlementStatus(s)+" / "+s.bottleneck;
  view.facilities=s.built;view.facilityIntegrity=s.facilityIntegrity;view.activity=s.activity;view.stock=s.stock;view.condition=s.condition;view.pollution=s.pollution;
    double life=s.planet>=0&&s.planet<int(game.s.flight.planets.size())?game.s.flight.planets[s.planet].life:.25;
    if(const auto* habitat=settlementHabitat(game.s.economy,s))view.ecology=habitat->quality*biodiversity(*habitat);else view.ecology=clamp(s.ecosystem/std::max(1.,80+life*520),0.,1.);
  view.power=s.power;view.demand=s.demand;view.storage=stockTotal(s.stock);view.capacity=storageCapacity(s);
  result.settlements.push_back(std::move(view));
 }
 for(const auto& r:game.s.economy.routes)if(r.outbound&&stockTotal(r.cargo)>.001){
  auto* from=findSettlement(game.s.economy,r.from);auto* to=findSettlement(game.s.economy,r.to);
  if(!from||!to||from->star!=game.s.currentStar||to->star!=game.s.currentStar)continue;
  hyperion::flight::ShipmentView v;v.id=r.id;v.from=r.from;v.to=r.to;v.faction=from->owner;v.good=r.good;v.trip=r.trips;v.amount=stockTotal(r.cargo);
  v.progress=clamp(1-r.remaining/std::max(.1,r.duration),0.,1.);v.holding=r.remaining<=1e-8;v.loading=v.progress<.05;v.unloading=v.progress>.95&&!v.holding;v.status=r.status;
  result.shipments.push_back(std::move(v));
 }
 if(!game.s.flight.planets.empty()&&game.s.flight.altitude()<420){
  int body=game.s.flight.nearestPlanet();const auto& world=game.s.flight.planets[body];auto& universe=const_cast<Universe&>(game.universe);auto planet=universe.planet(game.s.currentStar,body);LifeView view;view.centre=game.surfacePosition(game.s.flight.ship.position-world.centre);view.observer=view.centre;view.alert=clamp((90-game.s.flight.altitude())/80.,0.,1.);view.radius=hyperion::flight::lifeCandidateRadius;view.hours=game.s.hours;view.limit=hyperion::flight::lifeCandidateLimit;
  for(const auto& agent:visibleLife(game.s.economy,game.universe,planet,view)){
   double coverage=hyperion::flight::lifeChartCoverage(distance(agent.position,view.centre));if(coverage<=.001)continue;
   auto n=game.surfaceNormal(agent.position);hyperion::flight::V3 at=world.centre+n*(world.radius+hyperion::flight::terrain(world,n)+2.12);
   // Use the real settlement layout to keep bodies out of pads and structures.
   for(const auto& port:game.s.flight.ports)if(port.planet==body&&!port.settlement.empty()&&hyperion::flight::length(at-port.position)<260){auto* settlement=result.settlement(port.settlement);if(!settlement)continue;auto layout=hyperion::flight::layoutSettlement(*settlement);auto east=hyperion::flight::unit(hyperion::flight::cross({1,0,0},port.normal),{0,0,1}),north=hyperion::flight::unit(hyperion::flight::cross(east,port.normal));
    auto avoid=[&](hyperion::flight::V3 centre,double radius){auto offset=at-centre;offset=offset-port.normal*hyperion::flight::dot(offset,port.normal);double length=hyperion::flight::length(offset);if(length<radius){at+=hyperion::flight::unit(offset,east)*(radius-length);n=hyperion::flight::unit(at-world.centre);at=world.centre+n*(world.radius+hyperion::flight::terrain(world,n)+2.12);}};
    for(int pass=0;pass<3;pass++){avoid(port.position,48+agent.size);for(const auto& site:layout.facilities)avoid(port.position+east*site.position.x+north*site.position.z,15+agent.size);}
   }
   auto east=hyperion::flight::unit(hyperion::flight::cross({0,1,0},n),{1,0,0}),north=hyperion::flight::unit(hyperion::flight::cross(n,east));auto bio=phenotype(agent.contact);hyperion::flight::OrganismView v;v.id=agent.id;v.habitat=ecosystemKey(planet,agent.contact.habitatPos,agent.contact.habitatDepth,agent.contact.habitatBiome);v.habitatAt={agent.contact.habitatPos.x,agent.contact.habitatDepth,agent.contact.habitatPos.y};v.habitatBiome=agent.contact.habitatBiome;v.population=agent.contact.n;v.mass=std::clamp(agent.size*.08,.05,.8);v.position=at;v.up=hyperion::flight::terrainNormal(world,n);v.forward=hyperion::flight::unit(east*std::cos(agent.heading)+north*std::sin(agent.heading));v.size=agent.size*(agent.flora?1.8:1.4);v.hue=bio.hue;v.phase=agent.phase;v.gait=bio.gait;v.activity=agent.activity;v.plan=bio.plan;v.behaviour=agent.behaviour;v.guild=agent.guild;v.flora=agent.flora;v.marine=agent.marine;
   v.visibility=coverage;
   v.genes=double(agent.contact.n%65521)/65521.;v.shape={.78+.065*bio.limbs,.82+.06*(agent.contact.n%7),.80+.05*bio.segments};v.resting=agent.behaviour==Resting;v.feeding=agent.behaviour==Grazing||agent.behaviour==GardenForaging||agent.behaviour==Filtering||agent.behaviour==Recycling;auto foodNormal=game.surfaceNormal(agent.food);v.food=world.centre+foodNormal*(world.radius+hyperion::flight::terrain(world,foodNormal)+2.5);if(agent.marine&&!agent.flora)v.position+=n*(.65+std::sin(game.s.hours*55+agent.phase)*.24);result.organisms.push_back(v);
  }
 }
 return result;
}
}
