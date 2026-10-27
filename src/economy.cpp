#include "economy.hpp"
#include "flight/weather.hpp"
#include <numeric>
#include <sstream>
#include <unordered_map>

namespace hyperion {
namespace {
Stock stock(std::initializer_list<std::pair<int,double>> values) {
    Stock result{}; for (auto [good, amount] : values) result.at(good) = amount; return result;
}
bool has(const Stock& available, const Stock& cost) {
    for (int i=0;i<goodCount;i++) if (available[i]+1.e-8<cost[i]) return false;
    return true;
}
void subtract(Stock& available, const Stock& cost) {
    for (int i=0;i<goodCount;i++) available[i]=std::max(0.,available[i]-cost[i]);
}
std::string deficits(const Stock& available, const Stock& cost) {
    std::string text;
    for (int i=0;i<goodCount;i++) if (available[i]+1.e-8<cost[i]) {
        if (!text.empty()) text+=", "; text+=std::to_string(int(std::ceil(cost[i]-available[i])))+" "+goods[i].name;
    }
    return text;
}
std::string identifier(Economy& e,const char* prefix) { return std::string(prefix)+std::to_string(e.nextId++); }
int ownedCount(const Economy& e) {
    return int(std::count_if(e.settlements.begin(),e.settlements.end(),[](const Settlement& s){return s.owner==0;}));
}
}
const std::array<GoodDef,goodCount> goods={{{"Ore",4},{"Alloy",12},{"Crystal",17},{"Biomass",7},
    {"Rations",10},{"Medicine",34},{"Components",42},{"Propellant",15},{"Research",22}}};
const std::array<BuildingDef,buildingCount> buildings={{
 {"Ore extractor","Depletes the local ore seam; feeds refineries.",{},stock({{Ore,5}}),stock({{Alloy,12},{Components,2}}),2,2,2,.005,120},
 {"Smelter","Converts ore and propellant into structural alloy.",stock({{Ore,3},{Propellant,1}}),stock({{Alloy,2}}),stock({{Alloy,16},{Crystal,3},{Components,3}}),3,3,3,.012,170},
 {"Crystal bore","Extracts finite crystalline deposits.",{},stock({{Crystal,2}}),stock({{Alloy,14},{Components,3}}),2,3,3,.008,160},
 {"Biomass harvester","Harvests the living ecosystem; overuse exhausts it.",{},stock({{Biomass,3}}),stock({{Alloy,8},{Components,2}}),2,1,2,0,90},
 {"Nutrient garden","Turns biomass into food for residents.",stock({{Biomass,1}}),stock({{Rations,4}}),stock({{Alloy,10},{Crystal,2},{Components,2}}),2,2,2,0,110},
 {"Medical laboratory","Produces medicine from biomass and crystal.",stock({{Biomass,2},{Crystal,1}}),stock({{Medicine,1}}),stock({{Alloy,12},{Crystal,4},{Components,3}}),2,3,3,.001,180},
 {"Component fabricator","Builds machinery from alloy and crystal.",stock({{Alloy,2},{Crystal,1}}),stock({{Components,1}}),stock({{Alloy,18},{Crystal,5},{Components,4}}),3,3,3,.003,220},
 {"Catalytic fuelworks","Refines biomass into transport and reactor fuel.",stock({{Biomass,3}}),stock({{Propellant,2}}),stock({{Alloy,14},{Crystal,3},{Components,3}}),2,2,3,.004,170},
 {"Observatory","Consumes crystals to produce research and accelerate civic projects.",stock({{Crystal,1}}),stock({{Research,2}}),stock({{Alloy,12},{Crystal,6},{Components,3}}),2,2,4,0,210},
 {"Solar canopy","Generates power according to local weather, daylight and depth.",{},{},stock({{Alloy,7},{Crystal,4},{Components,1}}),0,0,1,0,80},
 {"Closed reactor","Stable power, consuming propellant while operating.",{},{},stock({{Alloy,18},{Crystal,4},{Components,4}}),0,0,1,0,240},
 {"Habitat","Houses twelve residents; overcrowding lowers morale.",{},{},stock({{Alloy,14},{Crystal,2},{Components,2}}),0,0,1,0,130},
 {"Warehouse","Adds 260 units of shared storage capacity.",{},{},stock({{Alloy,10},{Components,2}}),0,0,1,0,100},
 {"Restoration clinic","Uses medicine to remove pollution and restore the local ecosystem.",stock({{Medicine,1}}),{},stock({{Alloy,10},{Crystal,3},{Components,3}}),2,2,4,-.055,170}
}};
const std::array<TechDef,techCount> technologies={{
 {"Closed nutrient loop","Gardens use half as much biomass.",-1,12,stock({{Research,24},{Crystal,6}})},
 {"Catalytic metallurgy","Smelters need two ore instead of three.",-1,16,stock({{Research,32},{Alloy,12}})},
 {"Local microgrids","Solar generation improves by 35 percent.",-1,14,stock({{Research,28},{Crystal,10}})},
 {"Freight coordination","Player routes carry 32 units instead of 20.",1,20,stock({{Research,48},{Components,8}})},
 {"Field epidemiology","Medicine heals twice as effectively; pollution harms workers less.",0,22,stock({{Research,42},{Medicine,8}})},
 {"Frontier charter","New settlements receive twelve colonists and two solar canopies.",2,24,stock({{Research,55},{Components,6}})}
}};
const char* policyName(int p) { static const char* names[]={"Balanced","Industrial priority","Stewardship","Food and medical relief"};return names[clamp(p,0,3)]; }
Settlement* findSettlement(Economy& e,const std::string& id) {
    auto it=std::find_if(e.settlements.begin(),e.settlements.end(),[&](const Settlement& s){return s.id==id;});
    return it==e.settlements.end()?nullptr:&*it;
}
const Settlement* findSettlement(const Economy& e,const std::string& id) {
    auto it=std::find_if(e.settlements.begin(),e.settlements.end(),[&](const Settlement& s){return s.id==id;});
    return it==e.settlements.end()?nullptr:&*it;
}
double stockTotal(const Stock& s) { return std::accumulate(s.begin(),s.end(),0.); }
double storageCapacity(const Settlement& s) { return 220+workingBuildings(s,Warehouse)*260.; }
double reserveFor(const Settlement& s,int good) {
    double use=good==Rations?s.residents*.18:good==Medicine?s.residents*.025:good==Propellant?2.:0.;
    for (int i=0;i<buildingCount;i++) use+=buildings[i].inputs[good]*workingBuildings(s,i)/buildings[i].hours;
    return std::max(8.,use*12);
}
double marketPrice(const Settlement& s,int good) {
    double ratio=(reserveFor(s,good)+4)/(s.stock[good]+4);
    return goods[good].value*clamp(std::sqrt(ratio),.42,3.6);
}
int64_t tradeOrderPrice(const Economy& e,const Settlement& s,int good,int quantity,bool buy) {
    if(good<0||good>=goodCount||quantity<1||quantity>10000)return 0;
    double spread=clamp(.16-e.factions[s.owner].standing*.0007,.07,.25);int64_t total=0;
    for(int unit=0;unit<quantity;unit++){
        // Pair each purchased unit with its returned inventory level. Charging
        // the whole order at the opening spot price would create a buy/resell
        // loophole when the last few units make a good scarce.
        double level=std::max(0.,s.stock[good]+(buy?-unit:unit+1));
        double spot=goods[good].value*clamp(std::sqrt((reserveFor(s,good)+4)/(level+4)),.42,3.6);
        double quote=buy?std::ceil(spot*(1+spread)):std::floor(spot/(1+spread));
        total+=std::max(int64_t(1),int64_t(quote));
    }
    return total;
}
int64_t tradeUnitPrice(const Economy& e,const Settlement& s,int good,bool buy) {return tradeOrderPrice(e,s,good,1,buy);}
LocalConditions conditions(const Universe& u,const Planet& p,const Settlement& s,double hours) {
    LocalConditions c; double phase=(p.seed%10000)*.003;
    double daylight=.5+.5*std::sin(hours*2*pi/std::max(12,p.period)+phase+s.position.x*.002);
    double lon=2*std::atan(s.position.x/1800.),lat=std::atan(s.position.y/1800.);
    c.storm=hyperion::flight::stormField({std::cos(lat)*std::sin(lon),std::sin(lat),std::cos(lat)*std::cos(lon)},p.seed,hours,p.pressure,p.water);
    c.daylight=(.12+.88*daylight)*(1-c.storm*.65)*std::exp(-s.depth*.004);
    c.temperature=p.temp+std::sin(hours*.025+phase)*p.axialTilt*.22+daylight*6;
    c.mineralYield=.55+p.metal*.09+p.tectonic*.4;
    c.fertility=clamp(p.life*1.3*(.3+p.water)*std::exp(-std::abs(c.temperature-290)/90),.07,1.6);
    c.toxicity=clamp((p.type=="toxic"?.45:p.type=="volcanic"?.25:.02)+p.rad*.0002+s.pollution*.65,0.,1.);
    if(p.generation>=2){auto local=u.region(p,s.position,s.depth);c.temperature=local.temperature+(c.temperature-p.temp);c.mineralYield*=.45+local.ore*.55;c.fertility*=.4+local.biomass/std::max(.05,p.life)*.75;}
    c.pressure=p.pressure+s.depth/10;
    c.current=std::sin(hours*.14+s.position.x*.002+phase)*(.3+p.wind*.012);
    return c;
}
std::string settlementStatus(const Settlement& s) {
    if (!s.residents) return "Evacuated";
    if (s.alerts&1) return "Food shortage";
    if (s.alerts&4) return "Medical emergency";
    if (s.alerts&8) return "Work stoppage";
    if (s.alerts&2) return "Power deficit";
    if (s.alerts&16) return "Ecological damage";
    return "Operating";
}
void economicEvent(Economy& e,const std::string& place,const std::string& cause) {
    e.history.push_back({e.hour,place,cause});
    if (e.history.size()>2000) e.history.erase(e.history.begin(),e.history.begin()+200);
}
void initializeEconomy(Economy& e,Universe& u,double hour) {
    if (e.initialized) return;
    e=Economy{};e.initialized=true;e.hour=hour;
    const char* names[]={"NOMA expedition","Helix Combine","Tidal Compact","Meridian Exchange","Quiet Archive"};
    const char* doctrines[]={"Independent survey and settlement","Industrial growth and extraction","Ecological stewardship and mutual aid","Open markets and freight contracts","Scientific archives and technical autonomy"};
    for (int i=0;i<factionCount;i++) {
        auto& f=e.factions[i];f.name=names[i];f.doctrine=doctrines[i];
        f.standing=i==0?100:double(int(u.hash(i,991)%31)-5);
        for (int j=0;j<factionCount;j++) f.relations[j]=i==j?100:20;
    }
    e.factions[1].relations[2]=e.factions[2].relations[1]=-35;
    e.factions[3].relations[4]=e.factions[4].relations[3]=35;
    discoverEconomy(e,u,"HOME",hour);
    economicEvent(e,"NOMA","A frontier charter grants 24 volunteers, two freight shuttles and 1,600 credits. Markets and colonies use finite inventories.");
}
void discoverEconomy(Economy& e,Universe& u,const std::string& star,double hour) {
    if (e.systems.count(star)) return;
    e.systems.insert(star);if(u.generationFor(star)>=2&&star!="HOME"){discoverProceduralEconomy(e,u,star,hour);return;}auto planets=u.planets(star);Random r{u.star(star).seed^0x71e67a2du};
    std::vector<int> solid;for (auto& p:planets) if (!p.giant) solid.push_back(p.index);
    if (solid.empty()) return;
    std::vector<std::string> ids;
    const char* prefixes[]={"","Helix","Tidehouse","Exchange","Archive"};
    const char* names[]={"Iona","Marek","Sela","Tarin","Osei","Neri","Vale","Rin","Asha","Daro"};
    for (int i=1;i<factionCount;i++) {
        int preferred=solid[size_t((i-1)%solid.size())];if (star=="HOME") {const int home[]={0,1,2,2,5};if (!planets[size_t(home[i])].giant) preferred=home[i];}
        const Planet& p=planets[size_t(preferred)];Settlement s;
        s.id="PORT:"+star+":"+std::to_string(i);s.star=star;s.planet=p.index;s.planetName=p.name;
        s.owner=s.authority=i;s.name=std::string(prefixes[i])+" / "+p.name;
        s.governor=names[size_t(r()*10)];s.position=u.shoreline(p);s.position.x+=(r()-.5)*500;s.position.y+=(r()-.5)*500;
        s.residents=24+int(r()*13);s.built[Habitat]=4;s.built[Warehouse]=2;s.built[Solar]=3;s.built[Reactor]=1;
        s.stock={55,55,35,80,90,14,22,80,20};s.treasury=3800+int64_t(r()*2400);
        s.oreDeposit=9000+p.metal*2500;s.crystalDeposit=3000+p.tectonic*6000;
        s.ecosystem=120+p.life*500;s.condition=.84+r()*.14;s.nextContract=hour+3+i;s.nextMigration=hour+24;
        if (i==1) {s.built[Mine]=2;s.built[Refinery]=2;s.built[CrystalBore]=1;s.built[Fabricator]=1;s.stock[Ore]=140;}
        if (i==2) {s.built[Harvester]=3;s.built[Growhouse]=3;s.built[MedLab]=1;s.built[Restoration]=1;s.policy=Stewardship;s.stock[Rations]=130;}
        if (i==3) {s.built[Harvester]=1;s.built[FuelPlant]=3;s.built[Growhouse]=1;s.built[Fabricator]=1;s.stock[Propellant]=140;}
        if (i==4) {s.built[CrystalBore]=1;s.built[Observatory]=2;s.built[MedLab]=2;s.built[Growhouse]=1;s.stock[Research]=45;}
        e.settlements.push_back(s);ids.push_back(s.id);
        economicEvent(e,s.name,"A chartered settlement under "+e.factions[i].name+" is administered by "+s.governor+". Its industries depend on imported supplies.");
    }
    auto connect=[&](int from,int to,int good) {
        TradeRoute t;t.id=identifier(e,"NPC-");t.from=ids[size_t(from-1)];t.to=ids[size_t(to-1)];t.good=good;
        t.owner=3;t.commercial=true;t.capacity=24;t.duration=4+std::abs(from-to)*2;t.nextDeparture=hour+2;e.routes.push_back(t);
    };
    connect(2,1,Rations);connect(3,2,Propellant);connect(3,1,Propellant);connect(1,3,Alloy);connect(4,2,Medicine);
    connect(2,3,Biomass);connect(2,4,Biomass);connect(1,4,Crystal);connect(1,2,Crystal);connect(3,4,Propellant);
}
namespace {
void requestSupplies(Economy& e,Settlement& s) {
    if (e.hour+1e-8<s.nextContract||s.owner==0||!s.residents) return;
    s.nextContract=e.hour+24;
    for (auto& c:e.contracts) if (!c.closed&&c.settlement==s.id) return;
    int selected=-1;double urgency=1;
    for (int g=0;g<goodCount;g++) {
        double deficit=reserveFor(s,g)/(s.stock[g]+1);
        if (deficit>urgency) {selected=g;urgency=deficit;}
    }
    if (selected<0) return;
    int quantity=clamp(int(std::ceil(reserveFor(s,selected)-s.stock[selected])),8,40);
    int64_t price=int64_t(std::ceil(marketPrice(s,selected)*1.35));
    quantity=std::min(quantity,int(s.treasury/std::max(int64_t(1),price)));
    if (quantity<4) return;
    Contract c;c.id=identifier(e,"CONTRACT-");c.settlement=s.id;c.good=selected;c.requested=quantity;
    c.rewardPerUnit=price;c.escrow=price*quantity;c.deadline=e.hour+48;
    s.treasury-=c.escrow;e.contracts.push_back(c);
    economicEvent(e,s.name,std::string(goods[selected].name)+" shortage generated a supply contract. "+std::to_string(c.escrow)+" credits reserved in escrow.");
}
void updateSettlement(Economy& e,Universe& u,Settlement& s,double dt) {
    Planet p=u.planet(s.star,s.planet);LocalConditions c=conditions(u,p,s,e.hour);
    if(const auto* h=settlementHabitat(e,s))c.toxicity=clamp(c.toxicity+h->disease*.3,0.,1.);
    s.activity={};
    if (!s.residents) {s.pollution=std::max(0.,s.pollution-dt*.002);s.power=s.demand=s.labor=s.researchRate=0;return;}
    bool fed=s.stock[Rations]>=s.residents*.035*dt;
    double rationNeed=s.residents*.035*dt,used=std::min(s.stock[Rations],rationNeed);s.stock[Rations]-=used;
    double foodFraction=rationNeed>0?used/rationNeed:1;
    double overcrowding=std::max(0.,s.residents-workingBuildings(s,Habitat)*12.)/12;
    s.health=clamp(s.health+dt*((foodFraction-.85)*.9-c.toxicity*.32*(e.researched[4]?.5:1)),0.,100.);
    s.morale=clamp(s.morale+dt*((foodFraction-.75)*1.2-overcrowding*.35-s.pollution*.18+(s.policy==Relief?.15:0)),0.,100.);
    if (s.health<80&&s.stock[Medicine]>0) {
        double dose=std::min(s.stock[Medicine],s.residents*.002*dt);s.stock[Medicine]-=dose;s.medicineUsed+=dose;
        s.health=std::min(100.,s.health+dose*(e.researched[4]?9:4.5));
    }
    auto* habitat=settlementHabitat(e,s);
    if(habitat)s.ecosystem=harvestAvailable(*habitat,s.harvestPolicy,s.harvestTarget);
    s.pollution=std::max(0.,s.pollution-dt*(.0018+c.storm*.001));
    double solar=workingBuildings(s,Solar)*6*c.daylight*s.condition*(e.researched[2]?1.35:1);
    bool reactor=workingBuildings(s,Reactor)>0&&s.stock[Propellant]>0;
    double potentialDemand=0;for(int i=0;i<buildingCount;i++)potentialDemand+=workingBuildings(s,i)*buildings[i].power;
    double nominalReactor=workingBuildings(s,Reactor)*16*s.condition;
    double reserve=reactor?std::min(s.stock[Propellant],dt*.13*workingBuildings(s,Reactor)*clamp((potentialDemand-solar)/std::max(.001,nominalReactor),0.,1.)):0.;
    s.stock[Propellant]-=reserve;
    double reactorPower=reserve/dt/.13*16*s.condition;
    double power=solar+reactorPower;s.power=power;s.demand=0;
    double able=s.residents*clamp(s.health/100.,.12,1.)*clamp(s.morale/70.,.12,1.);
    if (!fed) able*=.35;
    s.productivity=clamp(s.condition*(.3+s.morale*.007),.05,1.3);
    std::array<int,9> order={Growhouse,Harvester,FuelPlant,Refinery,Mine,CrystalBore,MedLab,Fabricator,Observatory};
    if (s.policy==Industry) order={FuelPlant,Refinery,Mine,CrystalBore,Fabricator,Growhouse,Harvester,MedLab,Observatory};
    if (s.policy==Relief) order={Growhouse,MedLab,Harvester,FuelPlant,Mine,Refinery,CrystalBore,Observatory,Fabricator};
    s.activity={};s.activity[Solar]=workingBuildings(s,Solar)>0?clamp(c.daylight*s.condition,0.,1.):0.;
    s.activity[Habitat]=workingBuildings(s,Habitat)>0&&s.residents>0?s.condition*clamp(s.health/100.,0.,1.):0.;
    s.activity[Warehouse]=workingBuildings(s,Warehouse)>0&&power>0?s.condition*.6:0.;
    s.researchRate=0;double workers=able,availablePower=power;std::string bottleneck;bool made=false;
    auto produce=[&](int b) {
        double count=workingBuildings(s,b);if (!count) return;
        const auto& def=buildings[b];double workforce=std::min(workers,double(count*def.workers));
        if (workforce<.05) {if (bottleneck.empty()) bottleneck="No workers for "+std::string(def.name);return;}
        Stock inputs=def.inputs;
        if (b==Growhouse&&e.researched[0]) inputs[Biomass]*=.5;
        if (b==Refinery&&e.researched[1]) inputs[Ore]=2;
        if (!has(s.stock,inputs)) {if (bottleneck.empty()) bottleneck=std::string(def.name)+" needs "+deficits(s.stock,inputs);return;}
        if (b==Mine&&s.oreDeposit<5) {bottleneck="Ore seam exhausted";return;}
        if (b==CrystalBore&&s.crystalDeposit<2) {bottleneck="Crystal seam exhausted";return;}
        if (b==Harvester&&(!habitat||harvestAvailable(*habitat,s.harvestPolicy,s.harvestTarget)<3)) {bottleneck="Regional harvest limit / change target, restore habitat or import biomass";return;}
        double required=def.power*workforce/def.workers;s.demand+=required;
        double fraction=required>0?std::min(1.,availablePower/required):1;
        if (fraction<.05) {if (bottleneck.empty()) bottleneck="Power deficit stops "+std::string(def.name);return;}
        workers-=workforce;availablePower=std::max(0.,availablePower-required*fraction);
        double rate=workforce/def.workers*fraction*s.productivity;
        if (s.policy==Stewardship&&(b==Mine||b==Refinery||b==CrystalBore||b==Harvester)) rate*=.55;
        if (b==Mine||b==CrystalBore) rate*=c.mineralYield;
        if (b==Harvester) rate*=c.fertility*(habitat?(.25+.75*habitat->quality):0);
        if(b==Growhouse&&habitat&&habitat->capacity>0)rate*=.55+.45*ecosystemServices(*habitat);
        if(b==MedLab&&habitat&&habitat->capacity>0)rate*=.45+.55*biodiversity(*habitat);
        s.activity[b]=clamp(rate/std::max(1.,count),0.,1.);
        if(b==Observatory)s.researchRate=rate;
        s.progress[b]+=dt*rate;
        for (int n=0;s.progress[b]+1e-8>=def.hours&&n<24;n++) {
            if (!has(s.stock,inputs)) break;
            if (stockTotal(s.stock)-stockTotal(inputs)+stockTotal(def.outputs)>storageCapacity(s)) {bottleneck="Warehouse full";s.activity[b]=0;break;}
            if (b==Mine&&s.oreDeposit<5) break;
            if (b==CrystalBore&&s.crystalDeposit<2) break;
            if (b==Harvester&&(!habitat||harvestAvailable(*habitat,s.harvestPolicy,s.harvestTarget)<3)) break;
            subtract(s.stock,inputs);for (int g=0;g<goodCount;g++) s.stock[g]+=def.outputs[g];
            s.progress[b]-=def.hours;made=true;
            if (b==Mine) s.oreDeposit-=5;
            if (b==CrystalBore) s.crystalDeposit-=2;
            if (b==Harvester){harvestEcosystem(*habitat,3,s.harvestPolicy,s.harvestTarget);s.ecosystem=harvestAvailable(*habitat,s.harvestPolicy,s.harvestTarget);}
            if (b==Restoration) {if(habitat)restoreEcosystem(*habitat,1);s.health=std::min(100.,s.health+1);}
            s.pollution=clamp(s.pollution+def.pollution*(s.policy==Stewardship?.35:1),0.,1.);
        }
        s.progress[b]=std::min(s.progress[b],def.hours*2);
    };
    if (s.policy==Stewardship||s.pollution>.5) produce(Restoration);
    for (int b:order) produce(b);
    if (s.policy!=Stewardship&&s.pollution<=.5) produce(Restoration);
    s.labor=able-workers;s.bottleneck=bottleneck.empty()?(made?"Production completing":"Shifts in progress"):bottleneck;
    double load=clamp((power-availablePower-solar)/std::max(.001,nominalReactor),0.,1.);
    double actualBurn=std::min(reserve,dt*.13*workingBuildings(s,Reactor)*load);
    s.activity[Reactor]=workingBuildings(s,Reactor)>0?clamp(actualBurn/(dt*.13*workingBuildings(s,Reactor)),0.,1.):0.;
    s.stock[Propellant]+=reserve-actualBurn;s.reactorUse+=actualBurn;

    s.condition=clamp(s.condition-dt*(.00008+c.storm*.00015+s.pollution*.00015),.08,1.);
    // Maintenance is a real recipe, so breakdowns create demand for parts.
    if (s.condition<.88&&s.stock[Components]>=1&&s.stock[Alloy]>=1) {
        s.upkeep+=dt;
        if (s.upkeep>=12) {s.upkeep-=12;s.stock[Components]-=1;s.stock[Alloy]-=1;s.condition=std::min(1.,s.condition+.06);}
    }
    if (c.storm>.8&&s.condition<.45) {s.condition=std::max(.08,s.condition-dt*.002);s.bottleneck="Storm damage / repair machinery";}
    int alerts=(foodFraction<.95?1:0)|(s.demand>s.power+.1?2:0)|(s.health<45?4:0)|(s.morale<25?8:0)|((s.pollution>.55||(habitat&&habitat->capacity>0&&(habitat->quality<.25||biodiversity(*habitat)<.18)))?16:0);
    static const char* messages[]={"Rations ran short: workers lose health and productive shifts.","Power demand exceeds generation: lower-priority industries are throttled.","Worker health fell below 45 percent: illness reduces labor and increases medicine demand.","Morale fell below 25 percent: residents stop work and question the charter.","Regional populations or habitat condition collapsed: wild harvests, crop support and medical production are threatened."};
    for (int bit=0;bit<5;bit++) if ((alerts&(1<<bit))&&!(s.alerts&(1<<bit))) economicEvent(e,s.name,messages[bit]);
    if (s.alerts&&!alerts) economicEvent(e,s.name,"Supplies, power and public health recovered; normal shifts resumed.");
    s.alerts=alerts;
    if (s.owner==0) {
        s.loyalty=clamp(s.loyalty+dt*((s.morale-45)*.008),0.,100.);
        s.diplomaticClock+=dt;
        if (s.diplomaticClock>=24) {
            s.diplomaticClock-=24;
            if (s.pollution>.5) {e.factions[2].standing=std::max(-100.,e.factions[2].standing-5);economicEvent(e,s.name,"The Tidal Compact censured persistent runoff; its trade terms deteriorated.");}
            else if (s.policy==Stewardship) e.factions[2].standing=std::min(100.,e.factions[2].standing+1);
            if (s.policy==Industry&&made) e.factions[1].standing=std::min(100.,e.factions[1].standing+1);
        }
        if (s.loyalty<8&&s.morale<20) {s.owner=s.authority;economicEvent(e,s.name,"After prolonged shortages and unrest, residents transferred their charter to "+e.factions[s.owner].name+". Player freight permissions ended.");}
    }
    if (s.owner!=0) {
        s.diplomaticClock+=dt;
        if (s.diplomaticClock>=24) {
            s.diplomaticClock-=24;
            if (s.pollution>.5&&s.owner!=2) {
                int owner=s.owner;double before=e.factions[owner].relations[2];
                e.factions[owner].relations[2]=e.factions[2].relations[owner]=std::max(-100.,before-3);
                if (before>=-50&&e.factions[owner].relations[2]<-50) economicEvent(e,s.name,"Persistent runoff triggered a Tidal Compact embargo. Commercial freight between the two authorities is suspended.");
            }
            else if(s.pollution<.2&&s.owner!=2){int owner=s.owner;double baseline=owner==1?-35.:20.;double before=e.factions[owner].relations[2];
                if(before<baseline){e.factions[owner].relations[2]=e.factions[2].relations[owner]=std::min(baseline,before+1);
                    if(before<-50&&e.factions[owner].relations[2]>=-50)economicEvent(e,s.name,"Sustained ecological recovery eased the Tidal dispute; commercial freight may resume.");}}
        }
    }
    if (s.health<8) {s.mortality+=dt;if (s.mortality>=24&&s.residents>0) {s.mortality-=24;s.residents--;economicEvent(e,s.name,"A resident died after a day of critical illness. Unfilled jobs further weaken the settlement.");}}
    if (e.hour>=s.nextMigration) {
        s.nextMigration=e.hour+24;
        int sponsor=s.owner==0?s.authority:s.owner;
        if (s.health>80&&s.morale>75&&s.stock[Rations]>s.residents*4&&s.residents<workingBuildings(s,Habitat)*12&&e.factions[sponsor].reservePopulation>0) {
            s.residents++;e.factions[sponsor].reservePopulation--;economicEvent(e,s.name,"Reliable food and housing attracted a migrant from "+e.factions[sponsor].name+".");
        }
    }
    requestSupplies(e,s);
}
void updateRoute(Economy& e,TradeRoute& route,double dt,const std::unordered_map<std::string,Settlement*>& index) {
    auto fromIt=index.find(route.from),toIt=index.find(route.to);
    Settlement* from=fromIt==index.end()?nullptr:fromIt->second;Settlement* to=toIt==index.end()?nullptr:toIt->second;
    if (!from||!to) {route.status="Endpoint unavailable";return;}
    if (route.outbound) {
        route.remaining=std::max(0.,route.remaining-dt);
        if (route.remaining>1e-8) {route.status="In transit / "+std::to_string(int(std::ceil(route.remaining)))+" h";return;}
                if(objectIntegrity(e.worldDamage,"port:orbit:"+to->id)<=0&&objectIntegrity(e.worldDamage,"port:surface:"+to->id)<=0){route.status="Holding cargo / destination berths destroyed";return;}
        double room=std::max(0.,storageCapacity(*to)-stockTotal(to->stock));
        for (int g=0;g<goodCount;g++) {double amount=std::min(route.cargo[g],room);route.cargo[g]-=amount;to->stock[g]+=amount;room-=amount;}
        if (stockTotal(route.cargo)>.001) {route.status="Holding cargo / destination warehouse full";return;}
        route.outbound=false;route.trips++;route.nextDeparture=e.hour+4;
        economicEvent(e,to->name,std::string(goods[route.good].name)+" shipment arrived from "+from->name+"; warehouse stock increased.");
    }
    if (!route.enabled) {route.status="Suspended";return;}
    if (e.hour<route.nextDeparture) {route.status="Turnaround";return;}
    if (route.owner==0&&(from->owner!=0||to->owner!=0)) {route.status="Charter changed / route blocked";return;}
    if (route.commercial&&e.factions[from->owner].relations[to->owner]<-50) {route.status="Faction embargo";return;}
    if(objectIntegrity(e.worldDamage,"port:orbit:"+from->id)<=0&&objectIntegrity(e.worldDamage,"port:surface:"+from->id)<=0){route.status="Departure berths destroyed / rebuild required";return;}
    double available=std::max(0.,from->stock[route.good]-reserveFor(*from,route.good));
    double need=route.commercial?std::max(0.,reserveFor(*to,route.good)*1.6-to->stock[route.good]):route.capacity;
    int capacity=route.owner==0&&e.researched[3]?32:route.capacity;
    int amount=int(std::floor(std::min({available,need,double(capacity),std::max(0.,storageCapacity(*to)-stockTotal(to->stock))})));
    if (amount<=0) {route.status=available<1?"Source lacks export surplus":"No demand or warehouse space";return;}
    if (from->stock[Propellant]<1+(route.good==Propellant?amount:0)) {route.status="No departure propellant";return;}
    if (route.commercial) {
        int64_t payment=int64_t(std::ceil(marketPrice(*from,route.good)*amount));
        if (to->treasury<payment) {route.status="Buyer cannot finance shipment";return;}
        to->treasury-=payment;from->treasury+=payment;
    } else {
        if (e.credits<2) {route.status="Freight fees unpaid";return;}
        e.credits-=2;e.factions[3].treasury+=2;
    }
    from->stock[Propellant]-=1;from->stock[route.good]-=amount;route.cargo[route.good]=amount;
    route.outbound=true;route.remaining=route.duration;route.status="In transit";
}
}
void advanceEconomy(Economy& e,Universe& u,double targetHour) {
    if (!e.initialized) initializeEconomy(e,u,targetHour);
    // A fixed quarter-hour clock makes results independent of drawing frame rate.
    // Long imports are bounded; ordinary travel and waiting use the same clock.
    initializeEcosystems(e,u);targetHour=std::max(targetHour,e.hour);
    int steps=0;
    if(e.hour+.25>targetHour+1e-8)return;
    std::unordered_map<std::string,Settlement*> index;
    std::map<std::pair<std::string,int>,std::vector<size_t>> regions;
    for(size_t i=0;i<e.settlements.size();i++){auto& s=e.settlements[i];index.emplace(s.id,&s);regions[{s.star,s.planet}].push_back(i);}
    while (e.hour+.25<=targetHour+1e-8&&steps++<8192) {
        e.hour+=.25;advanceEcosystems(e,u,.25);
        for (auto& s:e.settlements) updateSettlement(e,u,s,.25);
        if(std::abs(e.hour/6-std::round(e.hour/6))<1e-8)for(auto& site:e.settlements)if(site.owner!=0){
            bool rebuilt=false;for(auto it=site.facilityIntegrity.begin();it!=site.facilityIntegrity.end();++it){const auto& def=buildings[it->first/24];auto damaged=e.worldDamage.find(facilityObjectId(site.id,it->first/24,it->first%24));if(damaged!=e.worldDamage.end()&&(damaged->second.recovery.crew||damaged->second.recovery.aboard||damaged->second.recovery.npcAboard))continue;if(site.treasury>=def.price&&has(site.stock,def.construction)){subtract(site.stock,def.construction);site.treasury-=def.price;e.factions[site.authority].treasury+=def.price;e.worldDamage.erase(facilityObjectId(site.id,it->first/24,it->first%24));site.facilityIntegrity.erase(it);e.damageRevision++;e.geometryRevision++;economicEvent(e,site.name,"Local crews rebuilt damaged infrastructure using warehouse supplies.");rebuilt=true;break;}}
            if(!rebuilt)for(auto it=e.worldDamage.begin();it!=e.worldDamage.end();++it){const auto& d=it->second;if(d.recovery.crew||d.recovery.aboard||d.recovery.npcAboard||d.owner!=site.id||d.integrity>=1||(d.kind!=PortObject&&d.kind!=PanelObject))continue;bool core=d.kind==PortObject;Stock cost=stock({{Alloy,core?32.:8.},{Components,core?12.:3.}});int price=core?800:120;if(site.treasury>=price&&has(site.stock,cost)){subtract(site.stock,cost);site.treasury-=price;e.factions[site.authority].treasury+=price;e.worldDamage.erase(it);e.damageRevision++;e.geometryRevision++;economicEvent(e,site.name,"Local crews reconstructed port equipment; physical supplies and treasury funded the work.");break;}}
        }
        // Runoff travels between nearby sites instead of remaining a private penalty.
        std::vector<double> runoff(e.settlements.size(),0);
        for(const auto& region:regions)for(size_t a=0;a<region.second.size();a++)for(size_t b=a+1;b<region.second.size();b++){
            size_t i=region.second[a],j=region.second[b];auto& left=e.settlements[i];auto& right=e.settlements[j];
            double dist=distance(left.position,right.position)+std::abs(left.depth-right.depth)*.05;
            if(dist>650)continue;double flow=(left.pollution-right.pollution)*.0015*(1-dist/650.);
            runoff[i]-=flow;runoff[j]+=flow;
        }
        for (size_t i=0;i<runoff.size();i++) e.settlements[i].pollution=clamp(e.settlements[i].pollution+runoff[i],0.,1.);
        for (auto& r:e.routes) updateRoute(e,r,.25,index);
        advanceSecurity(e);advanceShipyards(e);
        for (auto& c:e.contracts) if (!c.closed&&e.hour>c.deadline) {
            if (auto* s=findSettlement(e,c.settlement)) {
                s->treasury+=c.escrow;c.escrow=0;
                if (c.accepted) {e.factions[s->owner].standing=std::max(-100.,e.factions[s->owner].standing-4);economicEvent(e,s->name,"An accepted supply contract expired; unused escrow returned and trust fell.");}
            }
            c.closed=true;
        }
        if (e.contracts.size()>600) {
            auto end=std::remove_if(e.contracts.begin(),e.contracts.end(),[&](const Contract& c){return c.closed&&c.deadline<e.hour-240;});e.contracts.erase(end,e.contracts.end());
        }
        if (e.research>=0) {
            double labs=0;for (auto& s:e.settlements) if (s.owner==0&&s.residents&&s.stock[Rations]>0) labs+=s.researchRate;
            e.researchProgress+=.25*(.35+std::min(3.,labs*.4));
            if (e.researchProgress>=technologies[e.research].hours) {
                e.researched[e.research]=true;economicEvent(e,"NOMA",std::string(technologies[e.research].name)+" completed. "+technologies[e.research].description);e.research=-1;e.researchProgress=0;
            }
        }
    }
}
bool foundSettlement(Economy& e,Universe& u,const Planet& p,V2 at,double depth,Stock& ship,std::string& message) {
    Stock cost=stock({{Alloy,24},{Crystal,8},{Biomass,12},{Rations,18},{Medicine,2},{Components,6},{Propellant,8}});
    int volunteers=e.researched[5]?12:6;
    if (p.giant) {message="A settlement needs a solid surface or a sheltered ocean site";return false;}
    if (ownedCount(e)>=24) {message="Frontier administration currently supports 24 player settlements";return false;}
    if (e.colonists<volunteers) {message="Recruit volunteers at a chartered port";return false;}
    if (e.credits<350||!has(ship,cost)) {message="Foundation needs 350 credits and "+deficits(ship,cost);return false;}
    int authority=1+int(p.seed%4);double closest=1e30;
    for (auto& site:e.settlements) if (site.star==p.starId&&site.planet==p.index) {
        double dist=distance(site.position,at)+std::abs(site.depth-depth)*.1;
        if (dist<120) {message="This lease overlaps another settlement; move at least 120 km away";return false;}
        if (dist<closest) {closest=dist;authority=site.authority;}
    }
    if ((e.factions[authority].standing<=-45||e.factions[authority].warrant>0)) {message="The regional authority refuses a charter; settle warrants and restore diplomatic standing";return false;}
    Settlement s;s.id=identifier(e,"NOMA-");s.name=p.name+" / "+s.id;s.star=p.starId;s.planet=p.index;s.planetName=p.name;
    s.position=at;s.depth=depth;s.owner=0;s.authority=authority;s.governor="Expedition council";s.residents=volunteers;
    s.built[Habitat]=1;s.built[Solar]=e.researched[5]?2:1;s.built[Growhouse]=1;
    s.stock[Biomass]=12;s.stock[Rations]=18;s.stock[Medicine]=2;s.stock[Propellant]=8;
    s.treasury=0;s.condition=1;s.ecosystem=80+p.life*520;s.oreDeposit=9000+p.metal*2500;s.crystalDeposit=3000+p.tectonic*6000;
    if(p.generation>=2){auto local=u.region(p,at,depth);s.oreDeposit*=local.ore;s.crystalDeposit*=local.crystal;s.ecosystem=80+local.biomass*520;}
    s.nextMigration=e.hour+24;s.nextContract=e.hour+24;
    subtract(ship,cost);e.credits-=350;e.factions[authority].treasury+=350;e.colonists-=volunteers;
    auto& habitat=ensureEcosystem(e,u,p,at,depth);s.habitat=habitat.id;habitat.quality=std::max(0.,habitat.quality-.025);
    e.settlements.push_back(s);e.docked=s.id;
    economicEvent(e,s.name,"A new charter moved "+std::to_string(volunteers)+" volunteers and expedition provisions to a permanent "+(depth>0?"underwater":"surface")+" outpost. Food, power and imported machinery now determine its future.");
    message="Founded "+s.name+" / build a supply chain before provisions run out";return true;
}
bool construct(Economy& e,Settlement& s,int building,Stock& ship,std::string& message) {
    if (building<0||building>=buildingCount||s.owner!=0) {message="Only player-chartered settlements accept construction orders";return false;}
    auto& def=buildings[building];
    int restore=-1;for(const auto& [slot,hp]:s.facilityIntegrity)if(slot/24==building&&hp<1){restore=slot;break;}
    if(restore>=0){auto d=e.worldDamage.find(facilityObjectId(s.id,restore/24,restore%24));if(d!=e.worldDamage.end()&&(d->second.recovery.crew||d->second.recovery.aboard||d->second.recovery.npcAboard)){message="Evacuate and deliver survivors before rebuilding";return false;}}
    if (s.built[building]>=24&&restore<0) {message="Local construction limit reached";return false;}
    if (e.credits<def.price||!has(ship,def.construction)) {message="Construction needs "+std::to_string(def.price)+" credits; missing "+deficits(ship,def.construction);return false;}
    subtract(ship,def.construction);e.credits-=def.price;e.factions[s.authority].treasury+=def.price;if(restore>=0){s.facilityIntegrity.erase(restore);e.worldDamage.erase(facilityObjectId(s.id,restore/24,restore%24));}else s.built[building]++;e.damageRevision++;e.geometryRevision++;
    economicEvent(e,s.name,std::string(def.name)+" commissioned. "+def.purpose);message=std::string(def.name)+" constructed";return true;
}
bool rebuildInfrastructure(Economy& e,Settlement& site,Stock& cargo,std::string& message){
 if(site.owner!=0){message="Only player-chartered infrastructure accepts reconstruction orders";return false;}
 auto chosen=e.worldDamage.end();for(auto it=e.worldDamage.begin();it!=e.worldDamage.end();++it)if(it->second.owner==site.id&&it->second.integrity<1&&(it->second.kind==PortObject||it->second.kind==PanelObject))if(chosen==e.worldDamage.end()||it->second.kind<chosen->second.kind)chosen=it;
 if(chosen!=e.worldDamage.end()&&(chosen->second.recovery.crew||chosen->second.recovery.aboard||chosen->second.recovery.npcAboard)){message="Evacuate and deliver survivors before rebuilding";return false;}
 if(chosen==e.worldDamage.end()){message="No port equipment needs reconstruction";return false;}
 bool core=chosen->second.kind==PortObject;Stock cost=stock({{Alloy,core?32.:8.},{Components,core?12.:3.}});int price=core?800:120;
 if(e.credits<price||!has(cargo,cost)){message=std::string(core?"Berth: 32 alloy / 12 components / 800 cr":"Equipment: 8 alloy / 3 components / 120 cr");return false;}
 subtract(cargo,cost);e.credits-=price;e.factions[site.authority].treasury+=price;e.worldDamage.erase(chosen);e.damageRevision++;e.geometryRevision++;message=core?"Berth reconstructed / docking restored":"Port equipment reconstructed";economicEvent(e,site.name,message);return true;
}
bool repairSettlement(Economy& e,Settlement& s,Stock& ship,std::string& message) {
    Stock cost=stock({{Alloy,4},{Components,2}});
    if (s.owner!=0||s.condition>=.999) {message="No player machinery needs repair here";return false;}
    if (!has(ship,cost)) {message="Repairs need four alloy and two components";return false;}
    subtract(ship,cost);s.condition=std::min(1.,s.condition+.30);economicEvent(e,s.name,"Replacement parts restored machinery. Labor and electricity can now support more productive shifts.");message="Machinery repaired";return true;
}
bool transferCargo(Economy& e,Settlement& s,Stock& ship,int good,int quantity,bool deposit,std::string& message) {
    if (s.owner!=0||good<0||good>=goodCount||quantity<=0) {message="Cargo transfer requires a player settlement";return false;}
    Stock& source=deposit?ship:s.stock;Stock& destination=deposit?s.stock:ship;
    double capacity=deposit?storageCapacity(s):hyperion::flight::cargoCapacity(e.vessel);
    if (source[good]+1e-8<quantity||stockTotal(destination)+quantity>capacity+1e-8) {message="Insufficient goods or storage space";return false;}
    source[good]-=quantity;destination[good]+=quantity;message=std::string(deposit?"Delivered ":"Loaded ")+std::to_string(quantity)+" "+goods[good].name;return true;
}
bool trade(Economy& e,Settlement& s,Stock& ship,int good,int quantity,bool buy,std::string& message) {
    if (good<0||good>=goodCount||quantity<=0||quantity>10000) {message="Invalid trade order";return false;}
    if (s.owner!=0&&(e.factions[s.owner].standing<=-45||e.factions[s.owner].warrant>0)&&(good==Propellant||good==Components||good==Research)) {message="Authority restrictions block strategic goods; settle warrants and restore trust";return false;}
    int64_t payment=tradeOrderPrice(e,s,good,quantity,buy);
    if (buy) {
        if (s.stock[good]+1e-8<quantity||e.credits<payment||stockTotal(ship)+quantity>hyperion::flight::cargoCapacity(e.vessel)) {message="Purchase needs available port stock, credits and cargo space";return false;}
        s.stock[good]-=quantity;ship[good]+=quantity;e.credits-=payment;s.treasury+=payment;
    } else {
        if (ship[good]+1e-8<quantity||s.treasury<payment||stockTotal(s.stock)+quantity>storageCapacity(s)) {message="Sale needs cargo, a solvent buyer and warehouse space";return false;}
        ship[good]-=quantity;s.stock[good]+=quantity;e.credits+=payment;s.treasury-=payment;
    }
    message=std::string(buy?"Purchased ":"Sold ")+std::to_string(quantity)+" "+goods[good].name+" / "+std::to_string(payment)+" credits";return true;
}
bool commissionRoute(Economy& e,const std::string& fromId,const std::string& toId,int good,std::string& message) {
    auto* from=findSettlement(e,fromId);auto* to=findSettlement(e,toId);
    if (!from||!to||fromId==toId||from->owner!=0||to->owner!=0||good<0||good>=goodCount) {message="Freight routes connect two different player settlements";return false;}
    int used=int(std::count_if(e.routes.begin(),e.routes.end(),[](const TradeRoute& r){return r.owner==0;}));
    if (used>=e.ships) {message="All freight shuttles are assigned; commission another ship or retire a route";return false;}
    for (auto& r:e.routes) if (r.from==fromId&&r.to==toId&&r.good==good&&r.owner==0) {message="This supply route already exists";return false;}
    TradeRoute r;r.id=identifier(e,"FREIGHT-");r.from=fromId;r.to=toId;r.good=good;r.owner=0;r.capacity=e.researched[3]?32:20;
    r.duration=from->star==to->star?(from->planet==to->planet?2+distance(from->position,to->position)/120:6+std::abs(from->planet-to->planet)*2):36.;
    r.nextDeparture=e.hour;e.routes.push_back(r);economicEvent(e,from->name,std::string(goods[good].name)+" freight route opened to "+to->name+". It reserves local needs, then loads surplus and pays departure fuel and handling fees.");message="Freight shuttle assigned";return true;
}
bool fulfillContract(Economy& e,Contract& c,Stock& ship,int quantity,std::string& message) {
    auto* site=findSettlement(e,c.settlement);
    if (!site||!c.accepted||c.closed||e.hour>c.deadline||quantity<=0) {message="Accept an active contract before delivering";return false;}
    quantity=std::min(quantity,c.requested-c.delivered);int64_t pay=c.rewardPerUnit*quantity;
    if (ship[c.good]+1e-8<quantity||c.escrow<pay||stockTotal(site->stock)+quantity>storageCapacity(*site)) {message="Delivery needs the requested cargo and free port storage";return false;}
    ship[c.good]-=quantity;site->stock[c.good]+=quantity;e.credits+=pay;c.escrow-=pay;c.delivered+=quantity;
    e.factions[site->owner].standing=std::min(100.,e.factions[site->owner].standing+8.*quantity/c.requested);
    if (c.delivered==c.requested) {c.closed=true;economicEvent(e,site->name,std::string(goods[c.good].name)+" supply contract fulfilled by NOMA. Supplies increased and diplomatic trust improved.");}
    message="Contract cargo delivered / "+std::to_string(pay)+" credits from escrow";return true;
}
bool commissionResearch(Economy& e,int tech,Stock& ship,std::string& message) {
    if (tech<0||tech>=techCount) {message="Unknown civic project";return false;}
    auto& def=technologies[tech];
    if (e.research>=0||e.researched[tech]) {message="A project is already running, or this technology is complete";return false;}
    if (def.prerequisite>=0&&!e.researched[def.prerequisite]) {message="Complete "+std::string(technologies[def.prerequisite].name)+" first";return false;}
    if (!has(ship,def.cost)) {message="Research needs "+deficits(ship,def.cost);return false;}
    subtract(ship,def.cost);e.research=tech;e.researchProgress=0;economicEvent(e,"NOMA",std::string(def.name)+" funded. Settlement observatories accelerate the project; shift reports show progress.");message="Civic research commissioned";return true;
}
bool diplomaticMission(Economy& e,int faction,Stock& ship,std::string& message) {
    if (faction<1||faction>=factionCount||ship[Research]<8||e.credits<80) {message="An envoy needs eight research and 80 credits";return false;}
    ship[Research]-=8;e.credits-=80;e.factions[faction].treasury+=80;e.factions[faction].standing=std::min(100.,e.factions[faction].standing+8);
    economicEvent(e,e.factions[faction].name,"NOMA shared survey evidence and funded a diplomatic mission. Standing improved; market terms follow the new relationship.");message="Survey envoy received / standing +8";return true;
}
bool recruitColonists(Economy& e,Settlement& s,std::string& message) {
    if (!s.owner||e.factions[s.owner].standing<-20||e.factions[s.owner].warrant>0||e.factions[s.owner].reservePopulation<4||e.credits<150||e.colonists>116) {message="Recruitment needs a friendly chartered port, four volunteers and 150 credits";return false;}
    e.factions[s.owner].reservePopulation-=4;e.colonists+=4;e.credits-=150;e.factions[s.owner].treasury+=150;message="Four volunteers joined the expedition";return true;
}
bool buyFreighter(Economy& e,Stock& ship,std::string& message) {
    Stock cost=stock({{Alloy,14},{Components,10}});
    if(e.factions[3].warrant>0||e.factions[3].standing<=-45){message="Meridian refuses a ship commission until warrants and sanctions are resolved";return false;}
    if (e.ships>=24||e.credits<450||!has(ship,cost)) {message="A freighter needs 450 credits, 14 alloy and 10 components";return false;}
    subtract(ship,cost);e.credits-=450;e.factions[3].treasury+=450;e.ships++;message="Freight shuttle commissioned";return true;
}
}
