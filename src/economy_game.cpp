#include "game.hpp"
#include "wildlife.hpp"
#include <sstream>
namespace hyperion {
void Game::ensureEconomy() {
    bool initial=!s.economy.initialized;
    initializeEconomy(s.economy,universe,s.hours);
    if (!s.economy.systems.count(s.currentStar)) discoverEconomy(s.economy,universe,s.currentStar,s.hours);
    discoverFoldSpace(s.economy,universe,s.currentStar);
    for(const auto& star:s.economy.systems)discoverSecurity(s.economy,universe,star);
    initializeActorProfiles(s.economy,universe);s.lifeRevision=1;
    initializeShipyards(s.economy);initializeEcosystems(s.economy,universe);
    if(initial)advanceMissions(s.economy,universe,s.currentStar,&s.scanned);
}
void Game::registerExpeditionHabitat(){
 if(!hasExpedition())return;auto p=activePlanet();bool ocean=expeditionLayer()==Mode::Ocean;V2 centre=ocean?V2{s.oceanX,s.oceanAtY}:s.surface;
 registerLifeHabitats(s.economy,universe,p,centre,ocean?s.depth:0);ensureEcosystem(s.economy,universe,p,centre,ocean?s.depth:0);
 if(ocean){for(int x=int(std::floor((centre.x-150)/130));x<=int(std::floor((centre.x+150)/130));x++)for(const auto& c:universe.oceanObjects(p,x,centre.y))if(c.kind=="life"||c.kind=="flora")ensureContactHabitat(s.economy,universe,p,c);}
 else for(int x=int(std::floor((centre.x-150)/86));x<=int(std::floor((centre.x+150)/86));x++)for(int y=int(std::floor((centre.y-150)/86));y<=int(std::floor((centre.y+150)/86));y++)for(const auto& c:universe.surfaceObjects(p,x,y))if(c.kind=="life"||c.kind=="flora")ensureContactHabitat(s.economy,universe,p,c);
}
void Game::syncEconomy() { ensureEconomy();s.economy.securityWitness=s.flight.valid&&s.flight.star==s.currentStar&&!s.flight.deployed?s.currentStar:"";double before=s.economy.hour;advanceEconomy(s.economy,universe,s.hours);updateRecovery(0);if(!s.flight.events.empty())processFlightEvents();if(s.economy.hour>before+1e-8){advanceRecoverySociety(true);advanceOceanWrecks(s.economy);observeMissions();advanceMissions(s.economy,universe,s.currentStar,&s.scanned);} }
Stock Game::shipStock()const {
    Stock cargo=s.economy.hold;
    cargo[Alloy]=s.resources[0];cargo[Crystal]=s.resources[1];cargo[Biomass]=s.resources[2];cargo[Research]=s.resources[3];
    return cargo;
}
void Game::storeShipStock(const Stock& cargo) {
    s.economy.hold=cargo;for(int good:{Alloy,Crystal,Biomass,Research})s.economy.hold[good]=0;
    s.resources[0]=int(std::floor(cargo[Alloy]+1e-7));s.resources[1]=int(std::floor(cargo[Crystal]+1e-7));
    s.resources[2]=int(std::floor(cargo[Biomass]+1e-7));s.resources[3]=int(std::floor(cargo[Research]+1e-7));
}
bool Game::collectStockGains(const Stock& before) {
    Stock after=shipStock();double room=std::max(0.,cargoCapacity()-stockTotal(before));bool all=true;
    const int priority[]={Research,Crystal,Biomass,Ore,Alloy,Rations,Medicine,Components,Propellant};
    for(int good:priority){double gain=std::max(0.,after[good]-before[good]);double accepted=std::min(gain,room);room-=accepted;after[good]=before[good]+accepted;if(accepted+1e-8<gain)all=false;}
    storeShipStock(after);return all;
}
bool Game::canDock(const Settlement& site)const {
    if(site.star!=s.currentStar)return false;
    if(hasExpedition()){
        if(s.activeStar!=site.star||s.activePlanet!=site.planet)return false;
        if(expeditionLayer()==Mode::Surface)return site.depth==0&&distance(s.surface,site.position)<85;
        return std::hypot(s.oceanX-site.position.x,s.oceanAtY-site.position.y)<85&&std::abs(s.depth-site.depth)<70;
    }
    const auto& f=s.flight;
    if(f.valid&&f.star==s.currentStar){
        if(f.ports.empty())return f.docked&&(f.savedDock=="orbit:"+site.id||f.savedDock=="surface:"+site.id);
        return f.docked&&f.dockedPort>=0&&f.dockedPort<int(f.ports.size())&&f.ports[f.dockedPort].settlement==site.id;
    }
    // A legacy save may have no physical flight extension yet. Looking at a
    // selected planet alone never establishes a cargo connection.
    return s.mode==Mode::System&&s.economy.docked==site.id&&s.viewStar==site.star&&s.selectedPlanet==site.planet;
}
bool Game::dock(const std::string& id) {
    auto* site=findSettlement(s.economy,id);if (!site) return false;
    if (site->star!=s.currentStar) {notify("Travel to this star system before docking");return false;}
    if(s.economy.docked==id&&canDock(*site)){s.economy.docked=id;notify("Docked / "+site->name);return true;}
    if(s.mode==Mode::Flight){
        ensureFlight();int index=flightPort(id,false),orbital=flightPort(id,true);auto& f=s.flight;
        for(int candidate:{index,orbital})if(candidate>=0){double d=hyperion::flight::length(f.ship.position-f.ports[candidate].position);if(d<(f.ports[candidate].planet>=0?55:100)&&f.speed()<55){f.dockAt(candidate);processFlightEvents();return true;}}
        notify("Approach this port with speed below 55 km/s, or select it in I and press J");return false;
    }
    if(hasExpedition()&&canDock(*site)){
        s.economy.docked=id;advanceTime(1,true);notify("Docked / "+site->name);return true;
    }
    if (s.mode==Mode::System||s.mode==Mode::Galaxy) {
        if(hasExpedition()){launch();if(hasExpedition())return false;}
        ensureFlight();int port=flightPort(id,true);if(port<0)return false;
        double d=hyperion::flight::length(s.flight.ship.position-s.flight.ports[port].position);
        s.fuel=std::max(0.,s.fuel-(.25+d*.0001));s.flight.ship.fuel=s.fuel/s.maxFuel*100;s.flight.cancelNavigation();s.economy.docked.clear();advanceTime(.25+d/8000,true,false);s.flight.dockAt(port);processFlightEvents();
        selectedMinor.clear();s.viewStar=s.currentStar;s.selectedPlanet=site->planet;s.mode=Mode::System;
    }
    if (!canDock(*site)) {notify("Approach the settlement within 85 km, or rendezvous from orbit");return false;}
    s.economy.docked=id;if(s.mode==Mode::Surface||s.mode==Mode::Ocean)advanceTime(1,true);notify("Docked / "+site->name);return true;
}
void Game::waitHours(double hours) {
 if(!std::isfinite(hours)||hours<=0)return;
 ensureEconomy();if(s.mode==Mode::Flight&&s.flight.valid&&!s.flight.docked){notify("Dock before fast-forwarding cockpit time");return;}
 if(hasExpedition()&&expeditionLayer()==Mode::Ocean&&s.depth>maxDepth()){notify("Ascend to certified depth before a long shift");return;}
 hours=clamp(hours,0.,24.);
 double start=s.hours;bool strategy=turnBased();advanceTime(hours,strategy,true,true);
 if(s.hours-start+1e-7<hours)notify("Shift interrupted / "+lastTurn.interruption+" / "+lastTurn.summary(),Notice::Important);
 else notify(strategy&&lastTurn.valid?lastTurn.summary():std::to_string(int(hours))+" hour shift completed / review the economic chronicle");
}
namespace {
std::vector<std::string> fields(const std::string& arg) {
    std::vector<std::string> result;std::istringstream in(arg);std::string part;
    while (std::getline(in,part,'|')) result.push_back(part);return result;
}
}
bool Game::economicAction(const std::string& action,const std::string& arg) {
    static const std::set<std::string> actions={"harvestPolicy","harvestTarget","restoreHabitat","found","dock","construct","demolish","repair","rebuildInfrastructure","policy","deposit","withdraw","buy","sell","route","routeToggle","routeRetire","acceptContract","deliverContract","civicResearch","envoy","recruit","freighter","wait","plotSettlement","loadPropellant","settle","mediate","donate","recharter"};
    if (!actions.count(action)) return false;
    syncEconomy();auto& e=s.economy;Stock cargo=shipStock();std::string message;bool success=false;
    auto parts=fields(arg);auto site=[&]()->Settlement* {return parts.empty()?nullptr:findSettlement(e,parts[0]);};
    auto access=[&](Settlement* p){return p&&e.docked==p->id&&canDock(*p);};
    try {
        if (action=="wait") {waitHours(6);return true;}
        if (action=="dock") {dock(arg);return true;}
        if (action=="plotSettlement") {
            if (auto* p=site()) {system(p->star);s.selectedPlanet=p->planet;notify("Settlement orbit selected / "+p->name);}return true;
        }
        if (action=="found") {
            if (s.mode!=Mode::Surface&&s.mode!=Mode::Ocean) message="Land or dive before founding a permanent settlement";
            else if (s.mode==Mode::Ocean&&s.depth>maxDepth()) message="The site exceeds your pressure certification";
            else success=foundSettlement(e,universe,activePlanet(),s.mode==Mode::Surface?s.surface:V2{s.oceanX,s.oceanAtY},s.mode==Mode::Ocean?s.depth:0,cargo,message);
        } else if (action=="freighter") success=buyFreighter(e,cargo,message);
        else if (action=="civicResearch") success=commissionResearch(e,std::stoi(arg),cargo,message);
        else if (action=="envoy") success=diplomaticMission(e,std::stoi(arg),cargo,message);
        else if(action=="mediate"){
            int f=std::stoi(arg);
            if(f<1||f>=factionCount||f==2||e.factions[f].standing<10||e.factions[2].standing<10||e.credits<120||cargo[Research]<12)message="Mediation needs trust of both authorities, twelve data and 120 credits";
            else if(e.factions[f].relations[2]>=20)message="The two authorities have no active diplomatic dispute";
            else{cargo[Research]-=12;e.credits-=120;e.factions[3].treasury+=120;e.factions[f].relations[2]=e.factions[2].relations[f]=std::min(100.,e.factions[f].relations[2]+10);success=true;message="Regional mediation improved inter-faction relations by ten";economicEvent(e,"NOMA",message+". Embargoed freight can resume when relations reach -50.");}
        }
        else if (action=="loadPropellant") {
            if (cargo[Propellant]>=5&&s.fuel<=s.maxFuel-40) {cargo[Propellant]-=5;s.fuel+=40;success=true;message="Five propellant converted into 40 vessel fuel";}else message="Need five propellant and room for 40 fuel";
        } else if (action=="route"&&parts.size()==3) {
            success=commissionRoute(e,parts[0],parts[1],std::stoi(parts[2]),message);
        } else if (action=="routeToggle"||action=="routeRetire") {
            auto it=std::find_if(e.routes.begin(),e.routes.end(),[&](const TradeRoute& r){return r.id==arg&&r.owner==0;});
            if (it!=e.routes.end()) {
                if (action=="routeToggle") {it->enabled=!it->enabled;success=true;message=it->enabled?"Route resumed":"Route suspended / cargo already in transit will arrive";}
                else if (it->outbound||stockTotal(it->cargo)>.001) message="Suspend and unload this route before retiring it";
                else {e.routes.erase(it);success=true;message="Freight shuttle released for a new route";}
            }
        } else if (action=="acceptContract"||action=="deliverContract") {
            auto it=std::find_if(e.contracts.begin(),e.contracts.end(),[&](const Contract& c){return c.id==arg&&!c.closed;});
            if (it!=e.contracts.end()) {
                if (action=="acceptContract") {if(it->accepted)message="Contract already accepted";else{it->accepted=true;success=true;message="Contract accepted / deadline and escrow recorded";}}
                else if (!access(findSettlement(e,it->settlement))) message="Dock at the issuing settlement to deliver this contract";
                else success=fulfillContract(e,*it,cargo,5,message);
            }
        } else if (auto* p=site()) {
            if (!access(p)) message="Dock at the selected settlement before handling cargo or construction";
            else if(action=="harvestPolicy"&&p->owner==0){p->harvestPolicy=(p->harvestPolicy+1)%3;success=true;message=harvestPolicyName(p->harvestPolicy);economicEvent(e,p->name,"Harvest charter changed / "+message);}
            else if(action=="harvestTarget"&&p->owner==0){p->harvestTarget=(p->harvestTarget+1)%4;success=true;message=harvestTargetName(p->harvestTarget);economicEvent(e,p->name,"Harvest focus / "+message);}
            else if(action=="restoreHabitat"&&p->owner==0){auto* h=settlementHabitat(e,*p);if(!h||h->capacity<=0)message="No indigenous biosphere to restore";else if(h->quality>=.999&&h->pollution<.001&&h->disease<.001)message="Habitat does not need intervention";else if(cargo[Medicine]<2||cargo[Biomass]<8)message="Restoration needs two medicine and eight biomass";else{cargo[Medicine]-=2;cargo[Biomass]-=8;h->nutrients+=8;h->input+=8;restoreEcosystem(*h,5);success=true;message="Habitat treated / native propagules released / food-web recovery takes time";economicEvent(e,p->name,message);}}
            else if(action=="demolish"&&parts.size()==2&&p->owner==0){int kind=std::stoi(parts[1]);if(kind<0||kind>=buildingCount)message="Choose a building";else{int slot=-1;for(int i=0;i<p->built[kind];i++)if(facilityIntegrity(*p,kind,i)>0){slot=kind*24+i;break;}if(slot<0)message="No standing facility of this type";else{ensureFlight();WorldObject object;object.id=facilityObjectId(p->id,kind,slot%24);object.owner=p->id;object.kind=FacilityObject;object.body=p->planet;object.slot=slot;object.up=surfaceNormal(p->position);object.position=s.flight.planets[p->planet].centre+object.up*s.flight.planets[p->planet].radius;object.capacity=1;int index=int(s.flight.worldObjects.size());s.flight.worldObjects.push_back(object);flight::Event hit{"worldHit",index,0,object.id,"NOMA",object.id};hit.amount=1;processWorldHit(hit);success=true;message=std::string(buildings[kind].name)+" demolished / remaining capacity removed";}}}
            else if (action=="construct"&&parts.size()==2) success=construct(e,*p,std::stoi(parts[1]),cargo,message);
            else if(action=="rebuildInfrastructure")success=rebuildInfrastructure(e,*p,cargo,message);
            else if (action=="repair") success=repairSettlement(e,*p,cargo,message);
            else if (action=="policy"&&p->owner==0) {p->policy=(p->policy+1)%4;success=true;message=std::string("Charter policy / ")+policyName(p->policy);economicEvent(e,p->name,message);}
            else if(action=="recharter"){
                if(p->id.rfind("NOMA-",0)==0&&p->owner!=0&&e.factions[p->owner].standing>=25&&p->morale>=40&&p->residents>0&&e.credits>=500){e.credits-=500;e.factions[p->owner].treasury+=500;p->owner=0;p->loyalty=65;success=true;message="Settlement charter restored after compensation and public recovery";economicEvent(e,p->name,message);}
                else message="Restoration needs a former NOMA site, 500 credits, 25 standing and recovered public morale";
            }
            else if (action=="recruit") success=recruitColonists(e,*p,message);
            else if (action=="settle"&&p->owner==0) {
                if(e.colonists>=4&&p->residents+4<=workingBuildings(*p,Habitat)*12){e.colonists-=4;p->residents+=4;success=true;message="Four expedition volunteers settled here";economicEvent(e,p->name,message);}
                else message="Assigning volunteers needs four passengers and four available beds";
            }
            else if (parts.size()==2) {
                int good=std::stoi(parts[1]);
                if(action=="donate"){
                    if(good<0||good>=goodCount||cargo[good]<5||stockTotal(p->stock)+5>storageCapacity(*p))message="Relief delivery needs five units and free warehouse space";
                    else{double deficit=std::max(0.,reserveFor(*p,good)-p->stock[good]);cargo[good]-=5;p->stock[good]+=5;if(p->owner)e.factions[p->owner].standing=std::min(100.,e.factions[p->owner].standing+std::min(2.,deficit*.4));success=true;message="Relief cargo delivered; supplying an unmet need improves standing";economicEvent(e,p->name,std::string("NOMA donated ")+goods[good].name+" to the local warehouse.");}
                }
                else if (action=="buy"||action=="sell") success=trade(e,*p,cargo,good,5,action=="buy",message);
                else if (action=="deposit"||action=="withdraw") success=transferCargo(e,*p,cargo,good,5,action=="deposit",message);
            }
        }
    } catch (const std::exception&) {message="Invalid economic order";}
    if (success) {storeShipStock(cargo);soundEvent="find";if(turnBased())advanceTime(1,true);}
    notify(message.empty()?"Economic order unavailable":message);return true;
}
}
