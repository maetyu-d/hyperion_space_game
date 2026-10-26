#include "game.hpp"
namespace hyperion {
ShipOffer Game::currentVessel()const{ShipOffer ship;ship.port=s.economy.docked;ship.vessel=s.economy.vessel;ship.hull=s.hull;ship.fuel=s.fuel;ship.tank=s.maxFuel;ship.shield=s.flight.ship.shield;ship.equipment=s.flight.ship.combat;return ship;}
bool Game::canUseShipyard(const Settlement& p)const{
 const auto& f=s.flight;return canDock(p)&&s.economy.docked==p.id&&f.valid&&!f.deployed&&f.docked&&f.dockedPort>=0&&f.dockedPort<int(f.ports.size())&&f.ports[f.dockedPort].settlement==p.id&&p.residents>=4&&p.condition>=.25&&workingBuildings(p,Fabricator)>0&&shipyardPowered(s.economy,p);
}
bool Game::shipyardAction(const std::string& action,const std::string& arg){
 if(action!="tradeShip"&&action!="refitShip")return false;
 ensureFlight();auto& e=s.economy;auto& f=s.flight;auto* port=findSettlement(e,e.docked);bool success=false;std::string message;
 if(!port||!canUseShipyard(*port))message="Return to the ship and dock at a staffed port with power and an operating fabricator";
 else if(port->owner>0&&(e.factions[port->owner].standing<=-45||e.factions[port->owner].warrant>0))message="Shipyard restricted / settle authority warrants and sanctions";
 else if(action=="tradeShip"){
  auto it=std::find_if(e.shipOffers.begin(),e.shipOffers.end(),[&](const auto& offer){return offer.port==port->id&&offer.vessel.serial==arg;});
  if(it==e.shipOffers.end())message="This ship is no longer in the current port's inventory";
  else if(stockTotal(shipStock())>hyperion::flight::cargoCapacity(it->vessel)+1e-8)message="Unload cargo first / replacement hold capacity "+std::to_string(int(hyperion::flight::cargoCapacity(it->vessel)));
  else{auto old=currentVessel();int64_t ask=vesselValue(*it,true),bid=vesselValue(old,false),net=ask-bid;
   if(net>e.credits)message="Trade needs "+std::to_string(net)+" credits after the "+std::to_string(bid)+" credit allowance";
   else if(net<0&&port->treasury< -net)message="The dealer cannot fund the cash difference";
   else{ShipOffer bought=*it;*it=old;it->port=port->id;e.credits-=net;port->treasury+=net;e.vessel=bought.vessel;
    s.hull=bought.hull;s.fuel=bought.fuel;s.maxFuel=bought.tank;f.ship.vessel=e.vessel;f.ship.hull=s.hull;f.ship.fuel=s.fuel/s.maxFuel*100;f.ship.shield=bought.shield;f.ship.combat=bought.equipment;
    f.combatTarget.clear();f.lock=0;f.cancelNavigation();f.launchAssist=0;f.ship.throttle=0;f.ship.velocity={};f.resetDynamics();f.recordIds();
    message=std::string("Now aboard ")+hyperion::flight::vesselDef(e.vessel.model).name+" / "+(net>=0?"paid ":"received ")+std::to_string(std::abs(net))+" cr / cargo transferred; old hull and fittings remain with the dealer";
    economicEvent(e,port->name,message);success=true;}
  }
 }else{
  int slot=-1;try{size_t used=0;slot=std::stoi(arg,&used);if(used!=arg.size())slot=-1;}catch(...){}
  if(slot<0||slot>=hyperion::flight::vesselUpgradeCount)message="Unknown ship refit";
  else if(e.vessel.fitted[slot]>=3)message="This fitting is already at mark III";
  else{int tier=e.vessel.fitted[slot]+1;auto cost=refitMaterials(slot,tier);auto price=hyperion::flight::refitPrice(tier);bool supplies=true;for(int good=0;good<goodCount;good++)if(port->stock[good]+1e-8<cost[good])supplies=false;
   if(!supplies)message="The fabricator lacks alloy, components or crystal for this fitting";
   else if(e.credits<price)message="Refit requires "+std::to_string(price)+" credits";
   else{for(int good=0;good<goodCount;good++)port->stock[good]-=cost[good];e.credits-=price;port->treasury+=price;e.vessel.fitted[slot]=tier;f.ship.vessel=e.vessel;
    message=std::string(hyperion::flight::vesselUpgradeName(slot))+" / mark "+std::to_string(tier)+" installed / "+std::to_string(price)+" cr / local materials consumed";economicEvent(e,port->name,message);success=true;}
  }
 }
 notify(message);if(success){soundEvent="find";if(turnBased())advanceTime(1,true);}return true;
}
}
