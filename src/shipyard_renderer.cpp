#include "renderer.hpp"
#include <cstdio>
namespace hyperion {
namespace {
constexpr Color pearl{234,244,231,255},mint{153,255,218,255},muted{132,163,170,255},warning{255,178,125,255};
std::string n(double value){char buffer[64];std::snprintf(buffer,sizeof(buffer),"%.0f",value);return buffer;}
std::string materials(const Stock& cost){std::string result;for(int g=0;g<goodCount;g++)if(cost[g]>0){if(!result.empty())result+=" / ";result+=n(cost[g])+" "+goods[g].name;}return result;}
}
void Renderer::shipyardPanel(const Settlement& port,float x,float& y,float width){
 auto& e=game->s.economy;const auto& current=e.vessel;
 if(!disclosure("Shipyard / "+std::string(hyperion::flight::vesselDef(current.model).name),"shipyard",x,y,width))return;
 auto para=[&](const std::string& value,Color colour=muted){wrapped(value,x,y,width,12,colour);y+=12;};
 auto full=[&](const std::string& label,const std::string& command,const std::string& arg,bool enabled){button(label,{x,y,width,30},command,arg,enabled);y+=40;};
 bool access=game->canUseShipyard(port),restricted=port.owner>0&&(e.factions[port.owner].warrant>0||e.factions[port.owner].standing<=-45);
 para(std::string(hyperion::flight::vesselDef(current.model).purpose),pearl);
 row("Hold / tank",n(game->cargoCapacity())+" / "+n(game->s.maxFuel),x,y,width);row("Hull strength / shields",n(hyperion::flight::hullCapacity(current))+" / "+n(hyperion::flight::shieldCapacity(current)),x,y,width);
 row("Drive / weapons",n(hyperion::flight::speedFactor(current)*100)+"% / "+n(hyperion::flight::weaponFactor(current)*100)+"%",x,y,width);
 auto allowance=vesselValue(game->currentVessel(),false);row("Trade allowance",std::to_string(allowance)+" cr",x,y,width);
 if(!access)para("Return to the ship and dock here. Ship work needs four residents, power and an operating fabricator.",warning);
 if(restricted)para("Authority warrants or sanctions restrict the yard.",warning);
 int prizes=0;for(const auto& p:e.prizes)prizes+=p.ship.port==port.id;
 if(prizes&&disclosure("Owned and captured hulls / "+std::to_string(prizes),"prize-hulls",x,y,width))for(const auto& p:e.prizes)if(p.ship.port==port.id){const auto& ship=p.ship;para(std::string(hyperion::flight::vesselDef(ship.vessel.model).name)+" / "+n(ship.hull)+"% hull",pearl);para(p.disputed?"Disputed title / "+e.factions[p.faction].name:"Clear title / owned hull",p.disputed?warning:mint);
  if(p.disputed){int price=std::max(180,hyperion::flight::vesselDef(ship.vessel.model).price/5);full("Settle title / "+std::to_string(price)+" cr","prizeTitle",p.id,access&&!restricted&&e.credits>=price);}
  else{Stock cost{};cost[Alloy]=3+std::ceil((100-ship.hull)/8);cost[Components]=3+std::ceil((100-ship.equipment.systems[hyperion::flight::Drive])/20);cost[Crystal]=2;cost[Propellant]=ship.tank*.2/8;bool supplies=true;for(int g=0;g<goodCount;g++)supplies&=port.stock[g]+1e-8>=cost[g];para("Commission / 200 cr / "+materials(cost));para("Repairs hull and systems to at least 70%; magazines stay finite. Previous ship is stored here.");full("Commission this hull / 1 h","prizeCommission",p.id,access&&!restricted&&supplies&&e.credits>=200&&stockTotal(game->shipStock())<=hyperion::flight::cargoCapacity(ship.vessel));auto bid=vesselValue(ship,false);full("Sell owned hull / "+std::to_string(bid)+" cr","prizeSell",p.id,access&&!restricted&&port.treasury>=bid);}
 }
 if(disclosure("Fit ship upgrades","ship-refits",x,y,width)){
  const char* descriptions[]={"+80 cargo capacity per mark","+8% speed and +5% handling per mark","+15% reactor recharge per mark","+15% shield capacity per mark","+15% hull strength per mark","+8% weapon damage and +10% cooling per mark","+15% lock range and tracking per mark"};
  for(int i=0;i<hyperion::flight::vesselUpgradeCount;i++){int level=current.fitted[i];para(std::string(hyperion::flight::vesselUpgradeName(i))+" / mark "+std::to_string(level)+" of 3",pearl);para(descriptions[i]);
   if(level>=3){para("Fully fitted",mint);continue;}auto cost=refitMaterials(i,level+1);auto price=hyperion::flight::refitPrice(level+1);bool stock=true;for(int g=0;g<goodCount;g++)if(port.stock[g]+1e-8<cost[g])stock=false;
   para(materials(cost));full("Install mark "+std::to_string(level+1)+" / "+std::to_string(price)+" cr","refitShip",std::to_string(i),access&&!restricted&&stock&&e.credits>=price);
  }
 }
 if(disclosure("Trade hulls / dealer stock","ship-stock",x,y,width)){
  para("Cargo and expedition progress transfer. Fitted modules, fuel, damage and magazines stay with each hull. The quoted difference is the full transaction.");
  int count=0;for(const auto& offer:e.shipOffers)if(offer.port==port.id){count++;const auto& v=offer.vessel;const auto& def=hyperion::flight::vesselDef(v.model);auto ask=vesselValue(offer,true),net=ask-allowance;
   fitted(def.name,x,y,width,19,pearl);y+=29;para(def.purpose);para(v.serial,muted);
   row("Hold / tank",n(hyperion::flight::cargoCapacity(v))+" / "+n(offer.tank),x,y,width);row("Hull strength / shields",n(hyperion::flight::hullCapacity(v))+" / "+n(hyperion::flight::shieldCapacity(v)),x,y,width);
   row("Drive / weapons",n(hyperion::flight::speedFactor(v)*100)+"% / "+n(hyperion::flight::weaponFactor(v)*100)+"%",x,y,width);row("Jump range",n((310+game->s.upgrades[0]*185)*def.jump)+" ly",x,y,width);
   row("Condition / fuel",n(offer.hull)+"% / "+n(offer.fuel),x,y,width);row("Rounds / seekers / decoys",std::to_string(offer.equipment.rounds)+" / "+std::to_string(offer.equipment.missiles)+" / "+std::to_string(offer.equipment.decoys),x,y,width);
   int marks=0;double systems=0;for(int tier:v.fitted)marks+=tier;for(double condition:offer.equipment.systems)systems+=condition/5;row("Fitted marks / systems",std::to_string(marks)+" / "+n(systems)+"%",x,y,width);row("Asking price",std::to_string(ask)+" cr",x,y,width);
   bool fits=stockTotal(game->shipStock())<=hyperion::flight::cargoCapacity(v)+1e-8;if(!fits)para("Unload cargo to fit this hold before trading.",warning);
   full(net>=0?"Trade / pay "+std::to_string(net)+" cr":"Trade / receive "+std::to_string(-net)+" cr","tradeShip",v.serial,access&&!restricted&&fits&&net<=e.credits&&(net>=0||port.treasury>=-net));
  }
  if(!count)para("No hulls available here. NPC fabricators replenish up to two slots using actual supplies and faction funds; shortages can delay the daily build.");
 }
}
}
