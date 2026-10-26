#include "game.hpp"
#include "economy_storage.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace hyperion;
void require(bool value,const char* reason) {if (!value) throw std::runtime_error(reason);}
bool close(double a,double b) {return std::abs(a-b)<1.e-7;}
int64_t money(const Economy& e) {
    int64_t total=e.credits;for (auto& f:e.factions)total+=f.treasury;
    for (auto& s:e.settlements)total+=s.treasury;for (auto& c:e.contracts)total+=c.escrow;return total;
}
Economy sandbox(Universe& u) {
    Economy e;initializeEconomy(e,u,0);e.settlements.clear();e.routes.clear();e.contracts.clear();e.history.clear();return e;
}
Settlement site(const std::string& id="A") {
    Settlement s;s.id=id;s.name=id;s.star="HOME";s.planet=2;s.planetName="NACRE";s.owner=0;s.authority=2;
    s.residents=20;s.built[Habitat]=2;s.built[Warehouse]=1;s.condition=1;s.stock[Rations]=200;s.stock[Propellant]=60;s.stock[Medicine]=10;s.nextMigration=1.e6;return s;
}
int main() {try {
    Universe u;Economy a,b;initializeEconomy(a,u,0);initializeEconomy(b,u,0);
    require(a.settlements.size()==4&&a.routes.size()>=8,"Generated faction ports and interconnected freight");
    require(encodeEconomy(a)==encodeEconomy(b),"Seeded political and economic reproducibility");
    int64_t originalMoney=money(a);advanceEconomy(a,u,96);
    for (int i=0;i<384;i++) advanceEconomy(b,u,(i+1)*.25);
    require(encodeEconomy(a)==encodeEconomy(b),"Economy independent of frame / update partition");
    require(money(a)==originalMoney,"NPC trade, escrow and production conserve credits");
    require(!a.contracts.empty(),"Actual shortages create funded supply contracts");
    bool traffic=false;for (auto& r:a.routes)traffic|=r.trips>0;require(traffic,"NPC supply chains move physical cargo");
    double oreLeft=0;for (auto& s:a.settlements) {oreLeft+=s.oreDeposit;for (double g:s.stock)require(g>=0&&std::isfinite(g),"Non-negative finite inventories");require(s.health>=0&&s.health<=100,"Bounded health");}
    require(oreLeft>0,"Finite deposits survive ordinary operation");

    auto production=sandbox(u);Settlement factory=site();factory.built[Refinery]=1;factory.built[Reactor]=1;factory.stock[Ore]=30;
    production.settlements.push_back(factory);advanceEconomy(production,u,12);auto& f=production.settlements.front();
    require(f.stock[Alloy]>0,"Powered and staffed refinery produces alloy");
    require(close(30-f.stock[Ore],f.stock[Alloy]*1.5),"Smelter transforms the exact ore recipe");
    auto powerless=sandbox(u);factory.built[Reactor]=0;powerless.settlements.push_back(factory);advanceEconomy(powerless,u,12);
    require(powerless.settlements.front().stock[Alloy]==0,"No power means no industrial output");
    auto noInput=sandbox(u);factory.built[Reactor]=1;factory.stock[Ore]=0;noInput.settlements.push_back(factory);advanceEconomy(noInput,u,12);
    require(noInput.settlements.front().stock[Alloy]==0,"No input means no industrial output");
    auto starving=sandbox(u);Settlement hungry=site();hungry.stock[Rations]=hungry.stock[Medicine]=0;starving.settlements.push_back(hungry);advanceEconomy(starving,u,96);
    require(starving.settlements[0].health<45&&starving.settlements[0].morale<25,"Food failure cascades into illness and work stoppage");
    require(starving.settlements[0].alerts&1,"Food crisis is observable");

    auto transport=sandbox(u);Settlement origin=site("ORIGIN"),destination=site("DEST");origin.residents=destination.residents=0;
    origin.stock={};destination.stock={};origin.built[Warehouse]=destination.built[Warehouse]=0;origin.stock[Alloy]=100;origin.stock[Propellant]=50;
    transport.settlements={origin,destination};std::string msg;
    require(commissionRoute(transport,"ORIGIN","DEST",Alloy,msg),"Assign a finite freight shuttle");
    int64_t credits=money(transport);advanceEconomy(transport,u,.25);
    require(transport.routes[0].outbound&&transport.routes[0].cargo[Alloy]==20,"Departure removes cargo into transit");
    require(close(transport.settlements[0].stock[Alloy]+transport.routes[0].cargo[Alloy],100),"Departure conserves traded goods");
    transport.settlements[1].stock[Rations]=220;advanceEconomy(transport,u,4);
    require(transport.routes[0].outbound&&transport.routes[0].cargo[Alloy]==20,"Full warehouses keep incoming cargo in orbit");
    transport.settlements[1].stock[Rations]=0;advanceEconomy(transport,u,4.25);
    require(transport.settlements[1].stock[Alloy]==20&&!transport.routes[0].outbound,"Cargo unloads when space becomes available");
    require(money(transport)==credits&&close(transport.settlements[0].stock[Propellant],49),"Freight fees conserve money; departure consumes one fuel");

    auto markets=sandbox(u);Settlement port=site();port.owner=3;port.stock[Components]=50;port.treasury=5000;Stock cargo{};cargo[Rations]=50;
    int64_t cash=markets.credits,portCash=port.treasury;double parts=port.stock[Components];
    require(trade(markets,port,cargo,Components,5,true,msg),"Finite stock purchase");
    require(close(port.stock[Components]+cargo[Components],parts)&&markets.credits+port.treasury==cash+portCash,"Market transfers conserve goods and money");
    require(trade(markets,port,cargo,Components,5,false,msg)&&markets.credits<cash,"Bid-ask spread prevents cost-free roundtrip profit");
    for(int g=0;g<goodCount;g++)for(int available:{5,6,9,24,100}){
        Economy roundtrip=sandbox(u);roundtrip.credits=100000;Settlement store=site();store.owner=3;store.treasury=100000;store.stock={};store.stock[g]=available;Stock manifest{};int64_t opening=roundtrip.credits;
        require(trade(roundtrip,store,manifest,g,5,true,msg)&&trade(roundtrip,store,manifest,g,5,false,msg)&&roundtrip.credits<opening,"Marginal pricing prevents a roundtrip exploit at every stock level");
    }
    markets.factions[3].standing=-50;require(!trade(markets,port,cargo,Components,1,true,msg),"Faction sanctions block strategic goods");
    cargo[Research]=8;require(diplomaticMission(markets,3,cargo,msg)&&markets.factions[3].standing==-42,"Diplomacy spends evidence and changes standing");
    require(trade(markets,port,cargo,Components,1,true,msg),"Restored trust reopens strategic trade");

    auto colony=sandbox(u);Stock supplies{};supplies[Alloy]=200;supplies[Crystal]=100;supplies[Biomass]=80;supplies[Rations]=100;supplies[Medicine]=20;supplies[Components]=80;supplies[Propellant]=60;
    require(foundSettlement(colony,u,u.planet("HOME",2),{4000,4000},100,supplies,msg),"Found an underwater colony with real provisions");
    require(colony.colonists==18&&colony.settlements[0].residents==6&&colony.credits==1250,"Founding transfers volunteers and pays charter fees");
    double metal=supplies[Alloy];require(construct(colony,colony.settlements[0],Harvester,supplies,msg),"Construct an actual productive facility");
    require(supplies[Alloy]==metal-buildings[Harvester].construction[Alloy],"Construction consumes its bill of materials");
    require(!foundSettlement(colony,u,u.planet("HOME",2),{4001,4001},100,supplies,msg),"Settlement leases cannot overlap");
    auto ecology=sandbox(u);auto dirty=site("DIRTY"),neighbor=site("NEIGHBOR");dirty.residents=neighbor.residents=0;dirty.pollution=1;neighbor.pollution=0;neighbor.position={40,0};ecology.settlements={dirty,neighbor};advanceEconomy(ecology,u,24);
    require(ecology.settlements[1].pollution>.01,"Runoff affects neighboring settlements");

    auto research=sandbox(u);Stock evidence{};evidence[Research]=200;evidence[Alloy]=50;evidence[Crystal]=100;evidence[Components]=20;evidence[Medicine]=20;
    require(!commissionResearch(research,3,evidence,msg),"Civic research has prerequisites");
    require(commissionResearch(research,1,evidence,msg),"Commission research with material evidence");advanceEconomy(research,u,48);
    require(research.researched[1]&&research.research==-1,"Research progresses on the simulation clock");
    require(commissionResearch(research,3,evidence,msg),"Completed metallurgy unlocks freight coordination");

    auto contracts=sandbox(u);Settlement issuer=site("ISSUER");issuer.owner=2;issuer.treasury=1000;contracts.settlements.push_back(issuer);
    Contract contract;contract.id="DELIVERY";contract.settlement="ISSUER";contract.good=Medicine;contract.requested=5;contract.rewardPerUnit=30;contract.escrow=150;contract.accepted=true;contracts.settlements[0].treasury-=150;contracts.contracts.push_back(contract);Stock relief{};relief[Medicine]=5;
    credits=money(contracts);require(fulfillContract(contracts,contracts.contracts[0],relief,5,msg),"Accepted aid contract pays from escrow");
    require(money(contracts)==credits&&contracts.contracts[0].closed&&relief[Medicine]==0,"Contract payout conserves credits and consumes delivered cargo");
    require(!fulfillContract(contracts,contracts.contracts[0],relief,5,msg),"No duplicate contract reward");

    auto politics=sandbox(u);auto polluter=site("POLLUTER"),steward=site("STEWARD");polluter.owner=1;polluter.authority=1;polluter.pollution=.95;polluter.diplomaticClock=23.75;steward.owner=2;steward.stock[Rations]=0;
    politics.settlements={polluter,steward};politics.factions[1].relations[2]=politics.factions[2].relations[1]=-49;
    TradeRoute sanctioned;sanctioned.id="SANCTIONED";sanctioned.from=polluter.id;sanctioned.to=steward.id;sanctioned.good=Rations;sanctioned.owner=3;sanctioned.commercial=true;politics.routes.push_back(sanctioned);
    advanceEconomy(politics,u,.25);require(politics.factions[1].relations[2]<-50&&politics.routes[0].status=="Faction embargo","Industrial runoff can cause inter-faction sanctions and stop freight");
    politics.settlements[0].pollution=0;politics.settlements[0].diplomaticClock=23.75;politics.factions[1].relations[2]=politics.factions[2].relations[1]=-50.5;advanceEconomy(politics,u,.5);
    require(politics.factions[1].relations[2]>=-50,"Ecological recovery can reopen diplomatic relations");
    auto unrest=sandbox(u);auto failed=site("NOMA-FAILED");failed.loyalty=0;failed.morale=0;unrest.settlements.push_back(failed);advanceEconomy(unrest,u,.25);require(unrest.settlements[0].owner==failed.authority,"Unrest can transfer a settlement charter");
    auto emptyFuel=sandbox(u);auto underfueled=site();underfueled.built[Reactor]=1;underfueled.built[Refinery]=1;underfueled.stock[Ore]=50;underfueled.stock[Propellant]=.001;emptyFuel.settlements.push_back(underfueled);advanceEconomy(emptyFuel,u,12);require(emptyFuel.settlements[0].stock[Alloy]==0&&emptyFuel.settlements[0].stock[Propellant]>=0,"A final trace of reactor fuel cannot power free production");

    Game game;game.ensureEconomy();auto id=game.s.economy.settlements[0].id;int64_t account=game.s.economy.credits;
    game.action("buy",id+"|4");require(game.s.economy.credits==account,"Remote orders cannot move goods without docking");
    require(game.dock(id),"Orbital rendezvous selects the correct planet");
    game.action("buy",id+"|4");require(game.s.economy.credits<account,"Docked trade uses the native game economy");
    double shiftStart=game.s.hours;game.waitHours(6);require(close(game.s.hours,shiftStart+6)&&game.s.economy.hour>=game.s.hours-.251,"Survey shifts advance the same clock after a timed rendezvous");
    game.s.economy.factions[1].standing=game.s.economy.factions[2].standing=30;game.s.economy.factions[1].relations[2]=game.s.economy.factions[2].relations[1]=-55;game.s.resources[3]=30;
    game.action("mediate","1");require(game.s.economy.factions[1].relations[2]==-45&&game.s.resources[3]==18,"Survey diplomacy can resolve an actual faction embargo");
    Stock full=game.shipStock();full[Rations]+=std::max(0.,480-stockTotal(full));game.storeShipStock(full);Stock beforeSample=game.shipStock();game.s.economy.hold[Ore]+=14;require(!game.collectStockGains(beforeSample)&&close(stockTotal(game.shipStock()),480),"A full ship leaves additional samples in situ");
    std::string error;auto path=(std::filesystem::temp_directory_path()/"hyperion-economy-roundtrip.json").string();
    require(saveState(game.s,path,error),"Save expanded simulation");State reopened;require(loadState(reopened,path,error),"Load expanded simulation");
    require(encodeEconomy(reopened.economy)==encodeEconomy(game.s.economy),"Full political, economic and historical state survives reopening");
    std::ifstream input(path);nlohmann::json malformed;input>>malformed;malformed["economy"]["routes"][0]["from"]="MISSING";{std::ofstream output(path);output<<malformed.dump();}
    auto stable=reopened.economy.credits;require(!loadState(reopened,path,error)&&reopened.economy.credits==stable,"Invalid freight graphs reject transactionally");std::filesystem::remove(path);
    std::cout<<"PASS: seeded societies; fixed-clock determinism; conservation of money, goods and freight; real recipes, staffing and power; starvation cascade; runoff; finite markets; diplomacy and sanctions; settlement construction; technology prerequisites; contract escrow; docking; complete save/reopen; invalid-state rollback\n";
    return 0;
} catch (const std::exception& e) {std::cerr<<"FAIL economy: "<<e.what()<<'\n';return 1;}}
