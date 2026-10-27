#include "game.hpp"
namespace hyperion {
void discoverFoldSpace(Economy& e,Universe& u,const std::string& star){
 if(e.folds.count("fold:"+star+":0")){for(auto& [id,fold]:e.folds)if(fold.star==star)if(auto* anchor=findSettlement(e,fold.anchor))fold.authority=anchor->owner;return;}
 auto space=generateFoldSpace(u,star);
 for(size_t i=0;i+1<space.gates.size();i+=2){const auto& gate=space.gates[i];const auto& other=space.gates[i+1];
  FoldSurvey f;f.id=gate.link;f.star=star;f.name=u.planet(star,gate.body).name+" / "+u.planet(star,other.body).name;f.a=gate.body;f.b=other.body;f.authority=gate.authority;f.nil=gate.nil;
  for(const auto& site:e.settlements)if(site.star==star&&(site.planet==f.a||site.planet==f.b)&&site.owner==f.authority){f.anchor=site.id;break;}
  if(f.anchor.empty())for(const auto& site:e.settlements)if(site.star==star&&(site.planet==f.a||site.planet==f.b)){f.anchor=site.id;f.authority=site.owner;break;}
  e.folds.emplace(f.id,std::move(f));
 }
}
bool foldAccess(const Economy& e,const FoldSurvey& fold,int faction){
 if(faction<0||faction>=factionCount)return false;
 int authority=fold.authority;if(auto* site=findSettlement(e,fold.anchor))authority=site->owner;
 return authority==faction||(faction==0?e.factions[authority].standing>-45&&e.factions[authority].warrant==0:e.factions[authority].relations[faction]>-50);
}
const FoldSurvey* freightFold(const Economy& e,const Settlement& a,const Settlement& b,int owner){
 if(a.star!=b.star||a.planet==b.planet)return nullptr;
 for(const auto& [id,f]:e.folds)if(!f.nil&&f.star==a.star&&((f.a==a.planet&&f.b==b.planet)||(f.b==a.planet&&f.a==b.planet))&&
   (owner!=0||f.surveyed)&&foldAccess(e,f,owner==0?0:a.owner))return &f;
 return nullptr;
}
void Game::configureFoldFlight(){
 auto& f=s.flight;if(f.folds.empty()){auto space=generateFoldSpace(universe,s.currentStar);f.folds=std::move(space.gates);f.nilRegions=std::move(space.nil);}
 for(int i=0;i<int(f.folds.size());i++){auto& gate=f.folds[i];auto record=s.economy.folds.find(gate.link);
  if(record!=s.economy.folds.end()){auto& r=record->second;if(auto* site=findSettlement(s.economy,r.anchor))r.authority=site->owner;gate.authority=r.authority;gate.surveyed=r.surveyed;}
  auto it=std::find_if(f.ports.begin(),f.ports.end(),[&](const auto& port){return port.id==gate.id;});
  if(it==f.ports.end()){flight::Port port;port.id=gate.id;port.name=gate.name;port.position=gate.position;port.normal=gate.normal;port.planet=-1;port.body=gate.body;port.fold=i;port.description="Spatial fold / fly through the luminous opening / survey unlocks freight charts";f.ports.push_back(port);}
  else{it->fold=i;it->position=gate.position;it->normal=gate.normal;}
 }
}
void Game::foldEvent(const flight::Event& event){
 auto it=s.economy.folds.find(event.routeId);if(it==s.economy.folds.end())return;auto& f=it->second;
 if(event.actor!="NOMA")return;
 f.passages=std::min(1000000000,f.passages+1);
 if(!f.surveyed){f.surveyed=true;s.scanned.insert(f.id);log("Fold connection charted / "+f.name+" / both mouths identify the same spatial connection.");}
 if(f.authority>0){
  if(foldAccess(s.economy,f,0)&&s.economy.credits>=12){s.economy.credits-=12;s.economy.factions[f.authority].treasury+=12;}
  else{auto& authority=s.economy.factions[f.authority];authority.warrant=std::min(int64_t(1000000),authority.warrant+60);authority.standing=std::max(-100.,authority.standing-2);notify("Fold authority / unauthorized passage / 60 cr restitution",Notice::Important);}
 }
 economicEvent(s.economy,f.name,"NOMA crossed a spatial fold; momentum and orientation followed the same mapping as the aperture view.");
 soundEvent="jump";
}
bool Game::foldAction(const std::string& action,const std::string& arg){
 if(action!="foldSurvey"&&action!="foldTraverse")return false;
 ensureFlight();auto& f=s.flight;int index=-1;
 for(int i=0;i<int(f.folds.size());i++)if(f.folds[i].id==arg||f.folds[i].link==arg){index=i;break;}
 if(index<0){notify("Select a fold mouth in the local destinations");return true;}
 const auto gate=f.folds[index];auto& record=s.economy.folds.at(gate.link);
 double range=flight::length(f.ship.position-gate.position);
 if(f.deployed||f.docked||range>600||f.speed()>40){notify("Fold approach requires free flight within 600 km, below 40 km/s");return true;}
 if(action=="foldSurvey"){
  if(record.surveyed){notify("Both mouths are already charted");return true;}
  if(s.battery<6){notify("Fold survey requires 6 scanner battery");return true;}
  s.battery-=6;record.surveyed=true;s.scanned.insert(record.id);auto cargo=shipStock();cargo[Research]+=std::min(record.nil?8.:4.,std::max(0.,std::floor(cargoCapacity()-stockTotal(cargo))));storeShipStock(cargo);
  log("Spatial connection surveyed / "+record.name+(record.nil?" / Nil optical pocket at the far mouth":" / freight charter may use this shortcut"));soundEvent="find";notify(record.nil?"Archive fold charted / Nil optical pocket identified":"Fold charted / compatible freight routes can use the shortcut");
 }else{
  if(!turnBased()){f.selectTarget(int(std::find_if(f.ports.begin(),f.ports.end(),[&](const auto& p){return p.fold==index;})-f.ports.begin()));f.navigate();notify("Fly through the aperture / navigator approach engaged");return true;}
  if(!record.surveyed){notify("Survey the connection before committing a strategic transit");return true;}
  if(boardingActive()){notify("Withdraw the boarding party before traversing a fold");return true;}
  for(const auto& [id,d]:s.economy.worldDamage)if(d.recovery.towed){notify("Release the wreck tow before traversing a fold");return true;}
  // The committed hour covers approach, crossing and clearing the exit.
  f.foldTransitAllowed=true;f.ship.position=gate.position-gate.normal*8;if(!f.crossFold(index))return true;processFlightEvents();
 }
 configureFoldFlight();if(turnBased())advanceTime(1,true);return true;
}
}
