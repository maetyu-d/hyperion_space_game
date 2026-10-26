#include "economy.hpp"
namespace hyperion {
bool shipyardPowered(const Economy& e,const Settlement& p){return e.hour<.25||p.power>.01;}
VesselService quoteVesselService(const ShipOffer& ship,const Stock& stock){
 VesselService q;q.equipment=ship.equipment;q.fuel=std::min(ship.tank-ship.fuel,stock[Propellant]*8);
 double scale=hyperion::flight::hullCapacity(ship.vessel)/100,available=std::min(stock[Alloy]*5,stock[Components]*10);
 q.hull=std::min(100-ship.hull,available/scale);available=std::max(0.,available-q.hull*scale);q.work=q.hull*scale;
 double armour=std::min(100-q.equipment.armour,available/(.2*scale));q.equipment.armour+=armour;available=std::max(0.,available-armour*.2*scale);q.work+=armour*.2*scale;
 for(auto& system:q.equipment.systems){double points=std::min(100-system,available/(.12*scale));system+=points;available=std::max(0.,available-points*.12*scale);q.work+=points*.12*scale;}
 q.price=int64_t(std::ceil(q.fuel*.65+q.work*3));return q;
}
Stock refitMaterials(int slot,int tier){Stock cost{};tier=clamp(tier,1,3);cost[Alloy]=2+tier;cost[Components]=1+tier;if(slot==hyperion::flight::ShieldCoils||slot==hyperion::flight::ReactorTuning||slot==hyperion::flight::WeaponCapacitors||slot==hyperion::flight::SensorSuite)cost[Crystal]=tier;return cost;}
int64_t vesselValue(const ShipOffer& ship,bool purchase){
 double systems=0;for(auto v:ship.equipment.systems)systems+=v/500.;
 double condition=(.25+.75*clamp(ship.hull/100,0.,1.))*(.5+.5*systems)*(.85+.15*clamp(ship.equipment.armour/100,0.,1.));
 double value=hyperion::flight::vesselDef(ship.vessel.model).price*condition;
 for(int level:ship.vessel.fitted)for(int tier=1;tier<=level;tier++)value+=hyperion::flight::refitPrice(tier)*condition;
 value+=ship.fuel*.65+ship.equipment.rounds*.1+ship.equipment.missiles*35+ship.equipment.decoys*8;
 return int64_t(purchase?std::ceil(value):std::floor(value*.65));
}
namespace {
bool buildStock(Economy& e,Settlement& p){
 int count=0;for(const auto& offer:e.shipOffers)if(offer.port==p.id)count++;
 if(count>=2||p.owner==0||p.residents<4||p.condition<.25||workingBuildings(p,Fabricator)<1||!shipyardPowered(e,p))return false;
 int model=p.owner==1?(count?3:2):(count?2:1);ShipOffer offer;offer.port=p.id;offer.vessel.model=model;
 offer.tank=hyperion::flight::vesselDef(model).tank;offer.fuel=offer.tank*.4;offer.shield=hyperion::flight::shieldCapacity(offer.vessel);
 offer.equipment.rounds=180;offer.equipment.missiles=offer.equipment.decoys=2;
 Stock cost{};double scale=hyperion::flight::vesselDef(model).hull/100;cost[Alloy]=12*scale+180/50.;cost[Components]=6*scale+3;cost[Crystal]=model==3?4:2;cost[Propellant]=4*scale+offer.fuel/8;
 int64_t fee=hyperion::flight::vesselDef(model).price/5;
 if(e.factions[p.owner].treasury<fee)return false;for(int i=0;i<goodCount;i++)if(p.stock[i]+1e-8<cost[i])return false;
 for(int i=0;i<goodCount;i++)p.stock[i]-=cost[i];e.factions[p.owner].treasury-=fee;p.treasury+=fee;
 offer.vessel.serial="HULL:"+p.id+":"+std::to_string(e.nextId++);e.shipOffers.push_back(offer);
 economicEvent(e,p.name,std::string(hyperion::flight::vesselDef(model).name)+" completed for the shipyard. Alloy, components, crystal, propellant and a funded yard fee supplied the hull, fuel and ammunition.");return true;
}
}
void initializeShipyards(Economy& e){
 e.shipyardsInitialized=true;
 for(auto& p:e.settlements)if(p.owner>0&&workingBuildings(p,Fabricator)>0&&!e.shipyardClock.count(p.id)){
  e.shipyardClock[p.id]=e.hour+24;for(int i=0;i<2;i++)if(!buildStock(e,p))break;
 }
}
void advanceShipyards(Economy& e){
 for(auto& [id,next]:e.shipyardClock)if(e.hour+1e-8>=next){next=e.hour+24;if(auto* p=findSettlement(e,id))buildStock(e,*p);}
}
}
