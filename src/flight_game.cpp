#include "game.hpp"
#include "wildlife.hpp"
#include <sstream>
namespace hyperion {
hyperion::flight::V3 Game::surfaceNormal(V2 p)const {
 // A reversible survey chart. The open edge is approached asymptotically;
 // distant expedition coordinates never alias the same landing site.
 double longitude=2*std::atan(p.x/1800.),latitude=std::atan(p.y/1800.);
 return {std::cos(latitude)*std::sin(longitude),std::sin(latitude),std::cos(latitude)*std::cos(longitude)};
}
V2 Game::surfacePosition(hyperion::flight::V3 n)const {
 n=hyperion::flight::unit(n);return {clamp(1800*std::tan(std::atan2(n.x,n.z)*.5),-1.e7,1.e7),clamp(1800*std::tan(std::asin(clamp(n.y,-1.,1.))),-1.e7,1.e7)};
}
int Game::flightPort(const std::string& id,bool orbital){
 for(int i=0;i<int(s.flight.ports.size());i++)if(!s.flight.ports[i].disabled&&s.flight.ports[i].settlement==id&&(s.flight.ports[i].planet<0)==orbital)return i;
 return -1;
}
void Game::ensureFlight(){
 ensureEconomy();auto& f=s.flight;f.ship.vessel=s.economy.vessel;
 std::string key=s.seed+"|"+s.currentStar+"|gen:"+std::to_string(universe.generationFor(s.currentStar));for(auto& p:s.economy.settlements)if(p.star==s.currentStar)key+="|"+p.id+":"+std::to_string(p.owner);
 if(f.worldKey==key&&!f.planets.empty()){f.updateMinorBodies(s.hours);syncWorldDamage();return;}
 bool resume=f.valid&&f.star==s.currentStar;bool resumeNavigator=resume&&f.autopilot;double shield=f.ship.shield;auto equipment=f.ship.combat;
 auto previousTraffic=std::move(f.traffic);
 f.planets.clear();f.minorBodies.clear();f.ports.clear();if(!resume){f.bolts.clear();f.wrecks.clear();f.effects.clear();f.combatTarget.clear();f.lock=0;}f.route.clear();f.events.clear();f.autopilot=false;
 auto bodies=universe.planets(s.currentStar);
 for(auto& p:bodies){
  double r=systemBodyRadius(p);auto at=systemBodyCentre(p.index);hyperion::flight::V3 centre{at.x,at.y,at.z};
  f.planets.push_back({p.name,p.dominant,centre,r,double(p.seed%65536)/1300,surfaceNormal(universe.shoreline(p)),p.giant});
  auto& world=f.planets.back();world.worldSeed=p.seed;world.generation=p.generation;world.landform=p.landform;world.tectonic=p.tectonic;world.water=p.water;world.life=p.life;world.temperature=p.temp;world.atmosphere=p.pressure;
  world.kind=p.giant?5:p.type=="arid"?1:p.type=="frozen"?2:p.type=="volcanic"?3:p.type=="toxic"?4:0;
  auto n=world.portNormal;
  static const char* purposes[]={"spectral observatory","weather watch","xenobiology relay","resonance array","navigation beacon"};auto purpose=purposes[p.seed%5];
  f.ports.push_back({p.name+" "+purpose,std::string("Uncrewed ")+purpose+" / no commodity exchange",centre+n*(r+900),n,-1,"relay:"+p.id,"",p.index});
 }
 for(auto& p:s.economy.settlements)if(p.star==s.currentStar){
  auto& world=f.planets.at(p.planet);auto n=surfaceNormal(p.position);
  std::string desc=s.economy.factions[p.owner].name+" / "+(p.depth>0?"bathylift gateway":"surface settlement");
  f.ports.push_back({p.name+(p.depth>0?" bathylift":""),desc,world.centre+n*(world.radius+hyperion::flight::terrain(world,n)+2),n,p.planet,"surface:"+p.id,p.id,p.planet});
  auto orbital=hyperion::flight::unit(hyperion::flight::rotate(n,{0,1,0},.19));
  f.ports.push_back({p.name+" orbital",s.economy.factions[p.owner].name+" / bonded warehouse shuttle",world.centre+orbital*(world.radius+650),orbital,-1,"orbit:"+p.id,p.id,p.planet});
 }
 for(const auto& band:s.economy.pirates)if(band.star==s.currentStar){auto* haven=findSettlement(s.economy,band.haven);if(!haven)continue;
  const auto& world=f.planets.at(haven->planet);auto n=hyperion::flight::unit(hyperion::flight::rotate(surfaceNormal(haven->position),{0,1,0},.9));
  f.ports.push_back({band.name+" anchorage","Pirate den / independent stores / comms in Factions",world.centre+n*(world.radius+1100),n,-1,"den:"+band.id,"",haven->planet});
 }
 // Append fly-by waypoints after every established port, preserving traffic home indices.
 for(const auto& b:universe.minorBodies(s.currentStar)){
  int index=int(f.minorBodies.size());f.minorBodies.push_back({b,{}});
  auto normal=hyperion::flight::unit(hyperion::flight::V3{.6,.7,-.4});
  f.ports.push_back({b.name,minorDescription(b),{},normal,-1,"minor:"+b.id,"",-1,index});
 }
 f.updateMinorBodies(s.hours);
 f.worldKey=key;f.star=s.currentStar;f.valid=true;f.damageRevision=0;syncWorldDamage();
 auto resolve=[&](const std::string& id,int fallback){for(int i=0;i<int(f.ports.size());i++)if(f.ports[i].id==id)return i;return clamp(fallback,0,int(f.ports.size())-1);};
 f.target=resolve(f.savedTarget,0);f.lastBerth=resolve(f.savedBerth,0);
 if(resume){
  f.dockedPort=f.savedDock.empty()?-1:resolve(f.savedDock,-1);
  if(f.docked&&f.dockedPort>=0){auto saved=f.ship;double contactAge=f.touchdownAge;f.dockAt(f.dockedPort,true);f.touchdownAge=contactAge;const auto& pad=f.ports[f.dockedPort];if(pad.planet>=0&&hyperion::flight::length(saved.position-pad.position)<45&&std::abs(hyperion::flight::dot(saved.position-pad.position,pad.normal)-.25)<.04)f.ship.position=saved.position;f.ship.hull=saved.hull;f.ship.fuel=saved.fuel;f.ship.shield=saved.shield;f.events.clear();}
 }else{
  f.ship=hyperion::flight::Ship{};f.ship.combat=equipment;f.ship.shield=shield;f.deployed=false;f.docked=false;f.dockedPort=-1;
  int body=clamp(s.selectedPlanet,0,int(f.planets.size())-1);const auto& p=f.planets[body];
  f.ship.position=p.centre+p.portNormal*(p.radius+500);f.orient(hyperion::flight::unit(hyperion::flight::cross({1,0,0},p.portNormal))+p.portNormal*.15,p.portNormal);
  f.target=body;f.lastBerth=body;
  if(!s.economy.docked.empty()){int port=flightPort(s.economy.docked,true);if(port>=0){f.dockAt(port);f.target=port;f.events.clear();}}
  else for(int i=int(bodies.size());i<int(f.ports.size());i++)if(f.ports[i].planet>=0){f.target=i;break;}
 }
 // Traffic is recreated for a newly discovered system. A route vessel is the
 // visible representation of the existing freight packet, never another hold.
 f.traffic.clear();
 auto addTraffic=[&](hyperion::flight::Traffic t){
  t.id=f.ports[t.home].id+(t.kind==1?":pirate:"+std::to_string(t.role):":shuttle");
  if(resume)for(const auto& old:previousTraffic)if(old.routeId.empty()&&(old.id==t.id||(old.id.empty()&&old.home==t.home&&old.kind==t.kind&&old.role==t.role))){auto id=t.id;int home=t.home,faction=t.faction;t=old;t.id=id;t.home=home;t.faction=faction;break;}t.genome={1,universe.hash(int32_t(stringHash(t.id)),int32_t(universe.star(s.currentStar).seed),330),false};t.name=hyperion::flight::vesselDesignation(t.genome);f.traffic.push_back(t);
 };
 for(int i=0;i<int(f.ports.size());i++)if(!f.ports[i].settlement.empty()&&f.ports[i].planet<0){
  auto* p=findSettlement(s.economy,f.ports[i].settlement);hyperion::flight::Traffic t;t.phase=i*.73;t.home=i;t.faction=p?p->owner:3;t.position=f.ports[i].position;addTraffic(t);

 }
 if(resume)for(auto& t:previousTraffic)if((t.managed||!t.routeId.empty())&&t.home>=0&&t.home<int(f.ports.size()))f.traffic.push_back(t);
 f.ship.vessel=s.economy.vessel;refreshSecurity();f.ship.fuel=s.fuel/s.maxFuel*100;f.ship.hull=s.hull;f.recordIds();if(resumeNavigator&&!f.docked)f.navigate(true);
}
void Game::parkFlight(int body,V2 position,bool orbital){
 ensureFlight();auto& f=s.flight;const auto& p=f.planets.at(body);auto n=surfaceNormal(position);
 f.cancelNavigation();f.launchAssist=0;f.liftThrust=0;f.ship.position=p.centre+n*(p.radius+(orbital?500:hyperion::flight::terrain(p,n)+2));f.ship.velocity={};f.ship.throttle=0;f.resetDynamics();f.ship.gear=!orbital;
 f.orient(hyperion::flight::unit(hyperion::flight::cross({1,0,0},n))+n*.12,n);f.docked=!orbital;f.dockedPort=-1;f.savedDock.clear();f.deployed=!orbital;
 if(!orbital){int closest=-1;double best=45;
  for(int i=0;i<int(f.ports.size());i++)if(f.ports[i].planet==body){double d=hyperion::flight::length(f.ports[i].position-f.ship.position);if(d<best){best=d;closest=i;}}
  if(closest>=0)f.dockAt(closest);
 }
 f.events.clear();f.recordIds();
}
void Game::enterFlight(){
 if(s.mode==Mode::Flight)return;
 if(hasExpedition()&&expeditionLayer()==Mode::Ocean&&s.depth>80){notify("Recover from 80 m or less / return to the launch buoy first");return;}
 if(s.mode==Mode::Galaxy||s.mode==Mode::System)chartRecall={true,s.mode,s.currentStar,s.viewStar,s.selectedStar,selectedMinor,s.selectedPlanet,s.galaxy,s.galaxyZoom};
 else chartRecall={true,Mode::System,s.currentStar,s.activeStar,s.activeStar,"",s.activePlanet,s.galaxy,s.galaxyZoom};
 const double start=s.hours;if(hasExpedition())rememberSurvey();
 if(hasExpedition())resumeExpedition();
 if(s.mode==Mode::Ocean)surface();
 if(s.mode==Mode::Surface)parkFlight(s.activePlanet,s.surface);
 else ensureFlight();
 s.flight.deployed=false;s.mode=Mode::Flight;s.viewStar=s.currentStar;
 s.flight.ship.fuel=s.fuel/s.maxFuel*100;s.flight.ship.hull=s.hull;
 transferNotice=(s.hours>start?"Submersible recovered / 1 h resolved / ":"")+std::string(s.flight.docked?"Ship landed / Enter launches":"Manual helm retained / real time resumes");
 notify("Flight deck / K or O returns to strategy / Enter launches or docks");
}
bool Game::disembark(){
 ensureFlight();auto& f=s.flight;
 if(!f.docked){notify("Land with gear down before deploying the expedition");return false;}
 int body=f.dockedPort>=0?f.ports[f.dockedPort].planet:f.nearestPlanet();
 if(body<0){notify("This is an orbital berth / navigate to a surface port or land in the wilderness");return false;}
 s.activeStar=s.currentStar;s.activePlanet=body;s.viewStar=s.currentStar;s.selectedPlanet=body;
 s.surface=surfacePosition(f.ship.position-f.planets[body].centre);s.waypoint=s.surface;
 if(f.dockedPort>=0)if(auto* p=findSettlement(s.economy,f.ports[f.dockedPort].settlement))s.surface=s.waypoint=p->position;
 s.locations[universe.planet(s.currentStar,body).id]=s.surface;s.mode=s.expeditionMode=Mode::Surface;f.deployed=true;
 auto p=activePlanet();if(s.visitedPlanets.insert(p.id).second){auto before=shipStock();s.resources[3]+=6;collectStockGains(before);log("Planetfall from NOMA flight deck / "+p.name);}
 restoreSurvey(false);advanceTime(1,true);transferNotice="Expedition deployed / 1 h resolved / K returns to the ship";notify("Expedition deployed at the physical landing site / K returns to the ship");return true;
}
void Game::processFlightEvents(){
 auto& f=s.flight;auto events=std::move(f.events);f.events.clear();auto& e=s.economy;
 for(auto& event:events){
  if(event.kind=="shipLoss"||event.kind=="bounty"||event.kind=="freightLoss"||event.kind=="wreckCreated")registerSpaceWreck(event);if(event.kind=="wreckCreated")continue;
  if(event.kind=="worldHit"||event.kind=="groundHit"||event.kind=="minorGroundHit"){processWorldHit(event);continue;}
  missionEvent(event);
  if(securityEvent(event))continue;
  Settlement* port=event.port>=0&&event.port<int(f.ports.size())?findSettlement(e,f.ports[event.port].settlement):nullptr;
  if(event.kind=="dock"){
   e.docked=port?port->id:"";
   if(f.courier.active&&s.hours<=f.courier.deadline+1e-8&&port&&port->id==f.courier.destination){
    e.credits+=f.courier.escrow;f.deliveries++;e.factions[port->owner].standing=std::min(100.,e.factions[port->owner].standing+2);
    economicEvent(e,port->name,"NOMA delivered the sealed diplomatic courier pouch. Escrow paid: "+std::to_string(f.courier.escrow)+" credits.");
    notify("Courier delivered / +"+std::to_string(f.courier.escrow)+" credits from reserved escrow");f.courier={};
   }
  }else if(event.kind=="bounty"){
   int64_t bounty=port?std::min(int64_t(240),port->treasury):0;if(port)port->treasury-=bounty;e.credits+=bounty;
   if(port)e.factions[port->owner].standing=std::min(100.,e.factions[port->owner].standing+1);
   economicEvent(e,port?port->name:s.currentStar,"Pirate neutralised by NOMA. Local authority bounty "+std::to_string(bounty)+" credits.");notify("Pirate neutralised / authority bounty "+std::to_string(bounty)+" credits");
  }else if(event.kind=="civilianAssault"){
   if(event.faction>0&&event.faction<factionCount){auto& faction=e.factions[event.faction];faction.standing=std::max(-100.,faction.standing-8);faction.warrant=std::min(int64_t(1000000),faction.warrant+75);economicEvent(e,s.currentStar,"NOMA assaulted civilian traffic. "+faction.name+" issued a 75-credit warrant.");notify("Civilian assault / warrant +75 cr",Notice::Important);}
  }else if(event.kind=="civilian"){
   int64_t fine=std::min(int64_t(300),e.credits);e.credits-=fine;if(port)port->treasury+=fine;else e.factions[3].treasury+=fine;
   if(event.faction>0&&event.faction<factionCount){auto& faction=e.factions[event.faction];faction.standing=std::max(-100.,faction.standing-18);faction.warrant=std::min(int64_t(1000000),faction.warrant+300);}
   economicEvent(e,s.currentStar,"NOMA destroyed civilian traffic. Incident fee paid; a 300-credit warrant remains outstanding.");notify("Civilian loss / incident fee "+std::to_string(fine)+" cr / warrant +300 cr",Notice::Important);
  }else if(event.kind=="freightLoss"){
   for(auto& route:e.routes)if(route.id==event.routeId&&route.outbound&&stockTotal(route.cargo)>0){double lost=stockTotal(route.cargo);auto* raider=findCaptain(e,event.actor);auto* band=raider?findPirates(e,raider->band):nullptr;if(band){double room=std::max(0.,600-stockTotal(band->stock));for(int good=0;good<goodCount;good++){double taken=std::min(room,route.cargo[good]);band->stock[good]+=taken;room-=taken;band->stolen+=taken;}band->raids++;}route.cargo={};route.remaining=0;route.outbound=false;route.nextDeparture=e.hour+6;route.status="Convoy destroyed / replacement scheduled";economicEvent(e,s.currentStar,"Freight convoy "+route.id+" destroyed in local combat. "+std::to_string(int(lost))+" cargo lost from the actual shipment.");notify("Freight convoy destroyed / "+std::to_string(int(lost))+" supplies lost",Notice::Important);break;}
  }else if(event.kind=="rearmEnemy"){
   // The event identifies an existing ship. The port outfits its own wing
   // from physical stores; there is no new cargo hold or invented stock.
   auto it=std::find_if(f.traffic.begin(),f.traffic.end(),[&](const hyperion::flight::Traffic& t){return t.id==event.routeId&&!t.dead&&t.kind==1&&t.home==event.port;});
   if(it==f.traffic.end()||f.ports[it->home].disabled||hyperion::flight::length(it->position-f.ports[it->home].position)>210)continue;auto& t=*it;
   auto* band=findPirates(e,t.band);if(!band&&!port)continue;auto& stores=band?band->stock:port->stock;
   int rounds=std::min(180-t.ammunition,int(std::floor(std::max(0.,stores[Alloy])*50)));stores[Alloy]=std::max(0.,stores[Alloy]-rounds/50.);t.ammunition+=rounds;
   int seekers=std::min(2-t.missiles,int(std::floor(std::max(0.,stores[Components]))));stores[Components]-=seekers;t.missiles+=seekers;
   int decoys=std::min(2-t.countermeasures,int(std::floor(std::max(0.,stores[Components])*2)));stores[Components]-=decoys*.5;t.countermeasures+=decoys;
   double work=std::min(stores[Alloy]*5,stores[Components]*10),spent=0;
   double hull=std::min(hyperion::flight::enemyDef(t).hull-t.health,work);t.health+=hull;work=std::max(0.,work-hull);spent+=hull;
   double plating=std::min(100-t.armour,work/.2);t.armour+=plating;work=std::max(0.,work-plating*.2);spent+=plating*.2;
   for(auto& module:t.modules){double repair=std::min(100-module,work/.12);module+=repair;work=std::max(0.,work-repair*.12);spent+=repair*.12;}
   stores[Alloy]=std::max(0.,stores[Alloy]-spent/5);stores[Components]=std::max(0.,stores[Components]-spent/10);
  }else if(event.kind=="rescue"){
   if(auto* raider=findCaptain(e,event.actor)){raider->victories=std::min(1000000,raider->victories+1);if(auto* band=findPirates(e,raider->band)){auto cargo=shipStock();double room=std::max(0.,600-stockTotal(band->stock));for(int good=0;good<goodCount;good++){double taken=std::min(room,cargo[good]);band->stock[good]+=taken;band->stolen+=taken;room-=taken;}economicEvent(e,band->name,"Seized cargo from the defeated NOMA ship; its goods entered the pirate stores.");}}
   storeShipStock({});int64_t fee=std::min(int64_t(250),e.credits);e.credits-=fee;e.factions[3].treasury+=fee;
   if(f.courier.active){if(auto* issuer=findSettlement(e,f.courier.issuer)){issuer->treasury+=f.courier.escrow;e.factions[issuer->owner].standing=std::max(-100.,e.factions[issuer->owner].standing-5);}f.courier={};}
   economicEvent(e,s.currentStar,"NOMA rescued after hull failure. Ship cargo lost; volunteers and settlement warehouses survived.");notify("Rescue to last berth / cargo lost / hull 25% / repair at the exchange",Notice::Critical);
  }
 }
}
void Game::updateFlight(double dt,const hyperion::flight::Input& input){
 ensureFlight();auto& f=s.flight;if(f.deployed)return;
 f.ship.fuel=s.fuel/s.maxFuel*100;f.ship.hull=s.hull;f.strategicCombat=s.mode!=Mode::Flight;
 f.thrustFactor=(1+s.upgrades[3]*.08)*(.65+.35*s.hull/100);
 f.cargoMass=stockTotal(shipStock());for(const auto& [id,d]:s.economy.worldDamage){f.cargoMass+=d.recovery.aboard*.2;if(d.recovery.towed){f.cargoMass+=d.scale*3;f.thrustFactor*=.65;}}f.weatherHours=s.hours;f.updateMinorBodies(s.hours);
 if(dt>0&&f.altitude()<180&&!f.planets.empty()){int body=f.nearestPlanet();auto planet=universe.planet(s.currentStar,body);auto at=surfacePosition(f.ship.position-f.planets[body].centre);registerLifeHabitats(s.economy,universe,planet,at);}
 s.economy.securityWitness=s.currentStar;refreshSecurity();
 // Join visible freighters to the exact economic route and trip. Destroying
 // one removes the packet in transit; arrival cannot unload a duplicate copy.
 f.traffic.erase(std::remove_if(f.traffic.begin(),f.traffic.end(),[&](const hyperion::flight::Traffic& t){if(t.routeId.empty())return false;for(auto& r:s.economy.routes)if(r.id==t.routeId)return !r.outbound||r.trips!=t.trip||stockTotal(r.cargo)<=0;return true;}),f.traffic.end());
 for(auto& r:s.economy.routes)if(r.outbound&&stockTotal(r.cargo)>0){
  auto* from=findSettlement(s.economy,r.from);auto* to=findSettlement(s.economy,r.to);if(!from||!to||from->star!=s.currentStar||to->star!=s.currentStar)continue;
  int departure=flightPort(from->id,true),arrival=flightPort(to->id,true);if(departure<0)departure=flightPort(from->id,false);if(arrival<0)arrival=flightPort(to->id,false);for(int i=0;i<int(f.ports.size());i++)if(f.ports[i].planet<0){if(departure<0&&f.ports[i].settlement==from->id)departure=i;if(arrival<0&&f.ports[i].settlement==to->id)arrival=i;}if(departure<0||arrival<0)continue;
  auto it=std::find_if(f.traffic.begin(),f.traffic.end(),[&](const hyperion::flight::Traffic& t){return t.routeId==r.id;});
  if(it==f.traffic.end()){hyperion::flight::Traffic t;t.kind=2;t.id="freight:"+r.id+":"+std::to_string(r.trips);t.home=departure;t.faction=from->owner;t.routeId=r.id;t.trip=r.trips;t.genome={1,universe.hash(int32_t(stringHash(r.id)),int32_t(universe.star(s.currentStar).seed),330),false};t.name=hyperion::flight::vesselDesignation(t.genome);f.traffic.push_back(t);it=f.traffic.end()-1;}
  double phase=clamp(1-r.remaining/std::max(.1,r.duration),0.,1.);
  std::string pathKey=f.worldKey+"|"+r.id+"|"+std::to_string(r.trips)+"|"+f.ports[departure].id+"|"+f.ports[arrival].id+(f.ports[arrival].disabled?"|holding":"");
  if(it->transitKey!=pathKey){auto start=f.ports[departure].position+f.ports[departure].normal*90;auto destination=f.ports[arrival];if(destination.disabled)destination.position+=destination.normal*180;it->transitPath=hyperion::flight::transferRoute(f.navigationBodies(),start,destination);it->transitPath.insert(it->transitPath.begin(),start);it->transitKey=pathKey;}
  auto pose=hyperion::flight::freightPose(it->transitPath,phase);it->previous=it->positioned?it->position:pose.position;it->velocity=pose.forward*(hyperion::flight::length(pose.position-it->previous)/std::max(.001,dt));it->forward=pose.forward;it->position=pose.position;it->positioned=true;it->band.clear();if(r.owner==0)for(const auto& band:s.economy.pirates)if(band.star==s.currentStar&&band.truceUntil>s.economy.hour&&band.grievance<10)it->band=band.id;it->reason=r.escortUntil>s.economy.hour?"Commissioned escort":"Commercial shipment";
 }
 // A strategic hold damps thrust only while resolving the turn. Opening a
 // chart never edits the pilot's throttle, cruise, assist, route or attitude.
 bool hold=f.strategicCombat&&!f.autopilot&&f.combatOrder==0&&f.launchAssist<=0&&!f.docked;
 double helmThrottle=f.ship.throttle;bool helmAssist=f.ship.assist,helmCruise=f.ship.cruise;
 if(hold){f.ship.throttle=0;f.ship.assist=true;f.ship.cruise=false;f.ship.assistCaptureTime=0;f.ship.assistCapture={};}
 // Flight physics uses bounded substeps even when a render frame is long.
 bool nearbyArmed=false;for(const auto& t:f.traffic)if(!t.dead&&t.kind==1&&(t.hostile||t.charge>0)&&t.fireClock<=.1&&hyperion::flight::length(t.position-f.ship.position)<2000){nearbyArmed=true;break;}
 updateRecovery(dt);bool nearDebris=false;for(const auto& [id,d]:s.economy.worldDamage)if(d.star==s.currentStar&&(d.recovery.valid||(d.structure.localized&&flight::hasDetachedSections(d.structure)))&&flight::length(d.position-f.ship.position)<d.scale*4+150)nearDebris=true;
 bool contactRisk=nearDebris||(f.speed()>1&&f.altitude()<45);if(f.speed()>1){for(const auto& p:f.ports)if(p.minor<0&&hyperion::flight::length(p.position-f.ship.position)<280)contactRisk=true;for(const auto& b:f.minorBodies)if(!b.destroyed&&hyperion::flight::length(b.centre-f.ship.position)<b.source.radius*1.3+f.speed()*.1+30)contactRisk=true;}
 if(input.fire||input.missile||!f.bolts.empty()||nearbyArmed||contactRisk)prepareWorldObjects();else {syncWorldDamage();f.worldObjects.clear();}
 double remaining=clamp(dt,0.,.1);do{double step=std::min(.025,remaining);f.update(step,input);remaining-=step;}while(remaining>1e-8);
 if(hold&&!f.docked){f.ship.throttle=helmThrottle;f.ship.assist=helmAssist;f.ship.cruise=helmCruise;}
 s.fuel=clamp(f.ship.fuel/100*s.maxFuel,0.,s.maxFuel);s.hull=clamp(f.ship.hull,0.,100.);if(f.noticeAge<dt+.001)notify(f.notice);processFlightEvents();
 if(!f.docked)s.economy.docked.clear();else if(f.dockedPort>=0)s.economy.docked=f.ports[f.dockedPort].settlement;
 observeMissions(dt);storeSecurity();expireCourier();
}
void Game::expireCourier(){auto& f=s.flight;
 if(f.courier.active&&s.hours>f.courier.deadline){auto* issuer=findSettlement(s.economy,f.courier.issuer);if(issuer){issuer->treasury+=f.courier.escrow;s.economy.factions[issuer->owner].standing=std::max(-100.,s.economy.factions[issuer->owner].standing-5);}economicEvent(s.economy,s.currentStar,"Courier deadline expired; unpaid escrow returned to issuer.");f.courier={};notify("Courier deadline expired / escrow returned to issuer",Notice::Important);}
}

bool Game::flightAction(const std::string& a,const std::string& arg){
 static const std::set<std::string> actions={"cockpit","flightDock","navigate","flightTarget","flightNext","deploy","service","courier","brake","gear","assist","cruise","weapon","combatTarget","combatOrder","combatFocus","rearm","salvage","surveyMinor"};
 if(!actions.count(a))return false;
 if(a=="cockpit"){enterFlight();return true;}
 if(s.mode!=Mode::Flight&&(a=="navigate"||a=="flightDock"||a=="deploy"||a=="brake"||a=="gear"||a=="assist"||a=="cruise")){notify("Take the helm with K before using flight controls");return true;}
 ensureFlight();auto& f=s.flight;bool completed=false;
 if(a=="weapon"){if(arg=="0"||arg=="1"||arg=="2"||arg=="4")f.selectWeapon(arg[0]-'0');}
 else if(a=="combatTarget")f.cycleCombatTarget();
 else if(a=="combatFocus"){if(arg.empty())f.aimSystem=(f.aimSystem+1)%4;else if(arg=="0"||arg=="1"||arg=="2"||arg=="3")f.aimSystem=arg[0]-'0';f.message(std::string("Precision focus / ")+hyperion::flight::focusName(f.aimSystem)+" / requires sustained target lock");}
 else if(a=="combatOrder"){
  if(arg=="0"||arg=="1"||arg=="2"){f.combatOrder=arg[0]-'0';if(f.combatOrder==1){f.cancelNavigation();if(f.combatTargetIndex()<0)f.cycleCombatTarget();}if(f.combatOrder==2&&!f.docked)f.navigate();f.message(f.combatOrder==1?"Engage orders / End Turn resolves combat outside the cockpit":f.combatOrder==2?"Evade / navigator routing to selected berth":"Hold / damp thrust on turns / cockpit helm retained");}
 }
 else if(a=="salvage"){
  if(f.deployed||f.docked||f.speed()>15)notify("Salvage requires the ship in flight below 15 km/s");
  else{auto cargo=shipStock();double room=std::max(0.,cargoCapacity()-stockTotal(cargo));double recovered=0;const int goods[]={Alloy,Components,Propellant};
   for(auto& wreck:f.wrecks)if(hyperion::flight::length(wreck.position-f.ship.position)<55){
    for(int k=0;k<3;k++){double taken=std::min(wreck.cargo[k],room);if(k==0)taken=std::floor(taken);wreck.cargo[k]-=taken;cargo[goods[k]]+=taken;room-=taken;recovered+=taken;}
    if(wreck.id.rfind("world:",0)==0){auto record=s.economy.worldDamage.find(wreck.id.substr(6));if(record!=s.economy.worldDamage.end()&&!record->second.recovery.valid)for(int good=0;good<goodCount;good++){double taken=std::min(record->second.salvage[good],room);if(good==Alloy||good==Crystal||good==Biomass||good==Research)taken=std::floor(taken);record->second.salvage[good]-=taken;wreck.supplies[good]=record->second.salvage[good];cargo[good]+=taken;room-=taken;recovered+=taken;}}
   }
   for(auto& [id,d]:s.economy.worldDamage)if(d.recovery.valid&&d.star==s.currentStar&&hyperion::flight::length(d.position-f.ship.position)<55)for(int good=0;good<goodCount;good++){double taken=std::min(d.salvage[good],room);if(good==Alloy||good==Crystal||good==Biomass||good==Research)taken=std::floor(taken);d.salvage[good]-=taken;cargo[good]+=taken;room-=taken;recovered+=taken;}
   if(recovered>0){s.economy.damageRevision++;storeShipStock(cargo);completed=true;notify("Wreck salvage / "+std::to_string(int(recovered))+" units recovered into ship cargo");}else notify(room<=0?"Cargo hold full":"No salvage within 55 km");
  }
 }
 else if(a=="rearm"){
  auto* port=findSettlement(s.economy,s.economy.docked);
  if(!port||!canDock(*port))notify("Dock at a staffed exchange to rearm");
  else if(port->owner!=0&&(s.economy.factions[port->owner].standing<=-45||s.economy.factions[port->owner].warrant>0))notify("Authority restrictions / settle warrants and sanctions / ammunition denied");
  else{auto& c=f.ship.combat;int rounds=std::min(600-c.rounds,int(std::floor(std::max(0.,port->stock[Alloy])*50))),missiles=std::min(8-c.missiles,int(std::floor(std::max(0.,port->stock[Components])))),decoys=std::min(8-c.decoys,int(std::floor(std::max(0.,port->stock[Components]-missiles)*2)));
   int64_t price=int64_t(std::ceil(rounds*.1+missiles*35+decoys*8));
   if(rounds+missiles+decoys<=0)notify("Magazine full or local ammunition materials unavailable");else if(price>s.economy.credits)notify("Rearm quote "+std::to_string(price)+" credits / insufficient funds");
   else{port->stock[Alloy]-=rounds/50.;port->stock[Components]-=missiles+decoys*.5;s.economy.credits-=price;port->treasury+=price;c.rounds+=rounds;c.missiles+=missiles;c.decoys+=decoys;completed=true;notify("Rearmed / "+std::to_string(rounds)+" rounds / "+std::to_string(missiles)+" seekers / "+std::to_string(decoys)+" decoys / "+std::to_string(price)+" credits");}
  }
 }
 else if(a=="navigate"){f.ship.fuel=s.fuel/s.maxFuel*100;f.navigate();f.deployed=false;s.economy.docked.clear();}
 else if(a=="flightDock"){f.dock();f.deployed=false;processFlightEvents();if(!f.docked)s.economy.docked.clear();}
 else if(a=="flightTarget"){for(int i=0;i<int(f.ports.size());i++)if(f.ports[i].id==arg){f.selectTarget(i);break;}}
 else if(a=="flightNext")f.selectTarget((f.target+1)%int(f.ports.size()));
 else if(a=="surveyMinor"){
  int index=f.ports[f.target].minor;
  if(f.star!=s.currentStar||f.deployed||index<0||index>=int(f.minorBodies.size()))notify("Select a local moon, asteroid or comet fly-by destination");
  else{const auto& b=f.minorBodies[index];double clearance=hyperion::flight::length(f.ship.position-b.centre)-b.source.radius;
   if(s.scanned.count(b.source.id))notify("Body already surveyed / recorded in the atlas");
   else if(clearance>600||f.speed()>20||!f.lineOfSight(f.ship.position,b.centre+hyperion::flight::unit(f.ship.position-b.centre)*(b.source.radius*1.25+3)))notify("Survey requires a clear approach within 600 km of the surface, below 20 km/s");
   else if(s.battery<6)notify("Survey requires 6 scanner battery");
   else{auto before=shipStock();s.battery-=6;s.resources[3]+=4;collectStockGains(before);s.scanned.insert(b.source.id);
    s.discovered.insert(s.discovered.begin(),{b.source.id,b.source.name,"astronomy",b.source.name,currentStar().name,minorDescription(b.source),0,s.hours});if(s.discovered.size()>900)s.discovered.resize(900);
    log("Fly-by survey / "+b.source.name+" / "+b.source.composition);soundEvent="find";notify("Survey archived / "+b.source.name+" / research data collected");completed=true;
   }
  }
 }

 else if(a=="deploy")disembark();
 else if(a=="brake")f.brake();
 else if(a=="gear")f.ship.gear=!f.ship.gear;
 else if(a=="assist")f.setAssist(!f.ship.assist);
 else if(a=="cruise")f.ship.cruise=!f.ship.cruise;
 else if(a=="service"){
  auto* port=findSettlement(s.economy,s.economy.docked);
  if(!port||!canDock(*port))notify("Dock at a staffed port for ship services");
  else if(port->owner!=0&&(s.economy.factions[port->owner].standing<=-45||s.economy.factions[port->owner].warrant>0))notify("Authority restrictions / settle warrants and sanctions / ship service denied");
  else {
   auto quote=quoteVesselService(currentVessel(),port->stock);double fuel=quote.fuel,hull=quote.hull,work=quote.work;auto repaired=quote.equipment;auto price=quote.price;
   if(price>s.economy.credits)notify("Service quote "+std::to_string(price)+" credits / insufficient funds");
   else{port->stock[Propellant]-=fuel/8;port->stock[Alloy]=std::max(0.,port->stock[Alloy]-work/5);port->stock[Components]=std::max(0.,port->stock[Components]-work/10);s.economy.credits-=price;port->treasury+=price;s.fuel+=fuel;s.hull+=hull;f.ship.fuel=s.fuel/s.maxFuel*100;f.ship.hull=s.hull;f.ship.combat=repaired;f.ship.combat.heat=0;f.ship.combat.overheated=false;f.ship.combat.energy=100;f.ship.shield=hyperion::flight::shieldCapacity(s.economy.vessel)*f.ship.combat.systems[hyperion::flight::ShieldGenerator]/100;completed=true;notify("Service complete / "+std::to_string(price)+" credits / local fuel, alloy and parts consumed");}
  }
 }else if(a=="courier"){
  auto* issuer=findSettlement(s.economy,s.economy.docked);
  if(f.courier.active)notify("Deliver the active sealed pouch before accepting another");
  else if(!issuer||!canDock(*issuer))notify("Dock at a staffed port to accept a courier job");
  else if(issuer->owner!=0&&(s.economy.factions[issuer->owner].standing<=-45||s.economy.factions[issuer->owner].warrant>0))notify("Authority restrictions / settle warrants and sanctions / courier commission denied");
  else {
   Settlement* destination=nullptr;for(auto& p:s.economy.settlements)if(p.id!=issuer->id&&p.star==s.currentStar&&(p.owner==0||s.economy.factions[p.owner].standing> -45)){destination=&p;if(p.planet!=issuer->planet)break;}
   if(!destination)notify("No eligible diplomatic destination in this system");
   else{int64_t reward=std::min(int64_t(180+60*std::abs(destination->planet-issuer->planet)),issuer->treasury/4);
    if(reward<40)notify("The issuing authority cannot fund a courier commission");
    else{completed=true;issuer->treasury-=reward;f.courier={true,issuer->id,destination->id,reward,s.hours+48};int target=flightPort(destination->id,true);if(target>=0)f.selectTarget(target);economicEvent(s.economy,issuer->name,"Sealed diplomatic pouch commissioned to "+destination->name+"; "+std::to_string(reward)+" credits placed in escrow.");notify("Courier accepted / "+destination->name+" / 48 hours / "+std::to_string(reward)+" credits");}
   }
  }
 }
 if(completed&&turnBased())advanceTime(1,true);
 f.recordIds();if(a!="service"&&a!="rearm"&&a!="salvage"&&a!="courier"&&a!="deploy"&&f.noticeAge<.1)notify(f.notice);return true;
}
}
