#include "economy_storage.hpp"
#include "world_damage_storage.hpp"
#include "ecosystem_storage.hpp"
#include "security_storage.hpp"
#include "shipyard_storage.hpp"
#include "missions_storage.hpp"
#include "ocean_storage.hpp"
#include "recovery_society_storage.hpp"
#include <stdexcept>
namespace hyperion {
using J=nlohmann::json;
J encodeEconomy(const Economy& e) {
    J j={{"schema",1},{"initialized",e.initialized},{"hour",e.hour},{"credits",e.credits},
         {"colonists",e.colonists},{"ships",e.ships},{"nextId",e.nextId},{"hold",e.hold},
         {"systems",e.systems},{"researched",e.researched},{"research",e.research},
         {"researchProgress",e.researchProgress},{"docked",e.docked}};
    if(!e.recoveryCrews.empty()||!e.boardings.empty()||!e.prizes.empty())j["recoverySociety"]=encodeRecoverySociety(e);
    if(!e.oceanWrecks.empty())j["oceanWrecks"]=encodeOceanWrecks(e);
    if(!e.missions.empty()||!e.missionClock.empty())j["missions"]=encodeMissions(e);
    j["ecosystems"]=encodeEcosystems(e);j["security"]=encodeSecurity(e);j["shipyard"]=encodeShipyard(e);
    j["factions"]=J::array();for (const auto& f:e.factions) j["factions"].push_back({
        {"name",f.name},{"doctrine",f.doctrine},{"standing",f.standing},{"relations",f.relations},
        {"treasury",f.treasury},{"reservePopulation",f.reservePopulation},{"warrant",f.warrant}});
    j["settlements"]=J::array();for (const auto& s:e.settlements) j["settlements"].push_back({
        {"habitat",s.habitat},{"harvestPolicy",s.harvestPolicy},{"harvestTarget",s.harvestTarget},{"id",s.id},{"name",s.name},{"star",s.star},{"planetName",s.planetName},{"governor",s.governor},
        {"planet",s.planet},{"owner",s.owner},{"authority",s.authority},{"residents",s.residents},{"policy",s.policy},
        {"position",{s.position.x,s.position.y}},{"depth",s.depth},{"stock",s.stock},{"built",s.built},
        {"progress",s.progress},{"activity",s.activity},{"treasury",s.treasury},{"health",s.health},{"morale",s.morale},
        {"loyalty",s.loyalty},{"condition",s.condition},{"pollution",s.pollution},{"ecosystem",s.ecosystem},
        {"oreDeposit",s.oreDeposit},{"crystalDeposit",s.crystalDeposit},{"power",s.power},{"demand",s.demand},
        {"labor",s.labor},{"productivity",s.productivity},{"researchRate",s.researchRate},{"nextContract",s.nextContract},{"nextMigration",s.nextMigration},
        {"medicineUsed",s.medicineUsed},{"reactorUse",s.reactorUse},{"upkeep",s.upkeep},
        {"diplomaticClock",s.diplomaticClock},{"mortality",s.mortality},{"alerts",s.alerts},{"bottleneck",s.bottleneck}});
    for(size_t i=0;i<e.settlements.size();i++)if(!e.settlements[i].facilityIntegrity.empty()){auto damaged=J::array();for(const auto& [slot,hp]:e.settlements[i].facilityIntegrity)damaged.push_back({slot,hp});j["settlements"][i]["facilityDamage"]=damaged;}
    if(!e.worldDamage.empty())j["destruction"]=encodeWorldDamage(e.worldDamage);
    j["routes"]=J::array();for (const auto& r:e.routes) j["routes"].push_back({
        {"id",r.id},{"from",r.from},{"to",r.to},{"status",r.status},{"good",r.good},{"owner",r.owner},
        {"capacity",r.capacity},{"enabled",r.enabled},{"outbound",r.outbound},{"commercial",r.commercial},
        {"remaining",r.remaining},{"duration",r.duration},{"nextDeparture",r.nextDeparture},{"cargo",r.cargo},{"trips",r.trips},{"escortUntil",r.escortUntil}});
    j["contracts"]=J::array();for (const auto& c:e.contracts) j["contracts"].push_back({
        {"id",c.id},{"settlement",c.settlement},{"good",c.good},{"requested",c.requested},{"delivered",c.delivered},
        {"rewardPerUnit",c.rewardPerUnit},{"escrow",c.escrow},{"deadline",c.deadline},{"accepted",c.accepted},{"closed",c.closed}});
    j["history"]=J::array();for (const auto& h:e.history) j["history"].push_back({{"hour",h.hour},{"place",h.place},{"cause",h.cause}});
    return j;
}
namespace {
double n(const J& j,const char* key,double def,double low,double high) {
    double v=j.value(key,def);if (!std::isfinite(v)||v<low||v>high) throw std::runtime_error(std::string("Invalid economy field: ")+key);return v;
}
std::string t(const J& j,const char* key,const std::string& def="",size_t max=4096) {
    std::string value=j.value(key,def);if (value.size()>max) throw std::runtime_error("Oversized economy text");return value;
}
template<class T,size_t N> std::array<T,N> a(const J& j,const char* key,double low,double high) {
    const auto& values=j.at(key);if (!values.is_array()||values.size()!=N) throw std::runtime_error("Invalid economy array");
    std::array<T,N> result{};for (size_t i=0;i<N;i++) {double value=values[i].get<double>();if (!std::isfinite(value)||value<low||value>high) throw std::runtime_error("Invalid economy array value");if constexpr(std::is_integral_v<T>) if (std::floor(value)!=value) throw std::runtime_error("Non-integral economy count");result[i]=T(value);}return result;
}
void array(const J& j,size_t max) {if (!j.is_array()||j.size()>max) throw std::runtime_error("Oversized economy collection");}
}
Economy decodeEconomy(const J& j,Universe& u,double hour) {
    if (j.at("schema")!=1) throw std::runtime_error("Unsupported economy schema");Economy e;e.ecosystemRevision=0;
    e.initialized=j.value("initialized",false);if (!e.initialized) return e;
    e.hour=n(j,"hour",hour,0,hour+.000001);
    if (hour-e.hour>.251) throw std::runtime_error("Economy clock is out of step with the expedition");
    e.credits=int64_t(n(j,"credits",0,0,1e12));e.colonists=int(n(j,"colonists",24,0,120));e.ships=int(n(j,"ships",2,1,24));e.nextId=int(n(j,"nextId",1,1,1e9));
    e.hold=a<double,goodCount>(j,"hold",0,1e8);e.research=int(n(j,"research",-1,-1,techCount-1));e.researchProgress=n(j,"researchProgress",0,0,1e6);e.docked=t(j,"docked");
    const auto& research=j.at("researched");array(research,techCount);if (research.size()!=techCount) throw std::runtime_error("Invalid research table");for (int i=0;i<techCount;i++) e.researched[i]=research[i].get<bool>();
    const auto& factions=j.at("factions");array(factions,factionCount);if (factions.size()!=factionCount) throw std::runtime_error("Invalid faction table");
    for (int i=0;i<factionCount;i++) {const auto& f=factions[i];auto& dest=e.factions[i];dest.name=t(f,"name");dest.doctrine=t(f,"doctrine");dest.standing=n(f,"standing",0,-100,100);dest.relations=a<double,factionCount>(f,"relations",-100,100);dest.treasury=int64_t(n(f,"treasury",0,0,1e12));dest.reservePopulation=int(n(f,"reservePopulation",0,0,100000));double warrant=n(f,"warrant",0,0,1000000);if(std::floor(warrant)!=warrant)throw std::runtime_error("Fractional faction warrant");dest.warrant=int64_t(warrant);}
    const auto& systems=j.at("systems");array(systems,20000);for (auto& id:systems) {auto s=id.get<std::string>();if (s.size()>100||u.star(s).id!=s) throw std::runtime_error("Invalid economic system");e.systems.insert(s);}
    const auto& settlements=j.at("settlements");array(settlements,80000);std::set<std::string> ids;
    for (const auto& v:settlements) {
        Settlement s;s.id=t(v,"id");s.name=t(v,"name");s.star=t(v,"star");s.planetName=t(v,"planetName");s.governor=t(v,"governor");
        if (s.id.empty()||!ids.insert(s.id).second||u.star(s.star).id!=s.star) throw std::runtime_error("Invalid settlement identity");
        s.planet=int(n(v,"planet",0,0,int(u.planets(s.star).size())-1));s.owner=int(n(v,"owner",0,0,4));s.habitat=t(v,"habitat");double hp=n(v,"harvestPolicy",0,0,2),ht=n(v,"harvestTarget",0,0,3);if(std::floor(hp)!=hp||std::floor(ht)!=ht)throw std::runtime_error("Fractional harvest policy");s.harvestPolicy=int(hp);s.harvestTarget=int(ht);s.authority=int(n(v,"authority",1,1,4));s.residents=int(n(v,"residents",0,0,10000));s.policy=int(n(v,"policy",0,0,3));
        auto point=a<double,2>(v,"position",-1e7,1e7);s.position={point[0],point[1]};s.depth=n(v,"depth",0,0,15000);s.stock=a<double,goodCount>(v,"stock",0,1e8);s.built=a<int,buildingCount>(v,"built",0,24);s.progress=a<double,buildingCount>(v,"progress",0,1000);if(v.contains("activity"))s.activity=a<double,buildingCount>(v,"activity",0,1);
        if(v.contains("facilityDamage")){const auto& damage=v.at("facilityDamage");array(damage,336);for(const auto& item:damage){if(!item.is_array()||item.size()!=2||!item[0].is_number_integer())throw std::runtime_error("Invalid damaged building");int slot=item[0].get<int>();double hp=item[1].get<double>();if(slot<0||slot>=336||slot%24>=s.built[slot/24]||!std::isfinite(hp)||hp<0||hp>=1||!s.facilityIntegrity.emplace(slot,hp).second)throw std::runtime_error("Invalid building integrity");}}
        s.treasury=int64_t(n(v,"treasury",0,0,1e12));s.health=n(v,"health",90,0,100);s.morale=n(v,"morale",75,0,100);s.loyalty=n(v,"loyalty",80,0,100);s.condition=n(v,"condition",1,0,1);s.pollution=n(v,"pollution",0,0,1);s.ecosystem=n(v,"ecosystem",150,0,10000);s.oreDeposit=n(v,"oreDeposit",18000,0,1e9);s.crystalDeposit=n(v,"crystalDeposit",6000,0,1e9);
        s.power=n(v,"power",0,0,1e6);s.demand=n(v,"demand",0,0,1e6);s.labor=n(v,"labor",0,0,1e6);s.productivity=n(v,"productivity",1,0,2);s.researchRate=n(v,"researchRate",0,0,1000);
        s.nextContract=n(v,"nextContract",hour,0,1e12);s.nextMigration=n(v,"nextMigration",hour,0,1e12);s.medicineUsed=n(v,"medicineUsed",0,0,1e12);s.reactorUse=n(v,"reactorUse",0,0,1e12);s.upkeep=n(v,"upkeep",0,0,1e12);s.diplomaticClock=n(v,"diplomaticClock",0,0,24);s.mortality=n(v,"mortality",0,0,24);s.alerts=int(n(v,"alerts",0,0,31));s.bottleneck=t(v,"bottleneck");e.settlements.push_back(s);
    }
    const auto& routes=j.at("routes");array(routes,160000);std::set<std::string> routeIds;
    for (const auto& v:routes) {
        TradeRoute r;r.id=t(v,"id");r.from=t(v,"from");r.to=t(v,"to");r.status=t(v,"status");
        if (!routeIds.insert(r.id).second||!ids.count(r.from)||!ids.count(r.to)||r.from==r.to) throw std::runtime_error("Invalid freight endpoint");
        r.good=int(n(v,"good",0,0,goodCount-1));r.owner=int(n(v,"owner",0,0,4));r.capacity=int(n(v,"capacity",20,1,1000));r.enabled=v.value("enabled",true);r.outbound=v.value("outbound",false);r.commercial=v.value("commercial",false);
        r.remaining=n(v,"remaining",0,0,1e8);r.duration=n(v,"duration",6,.25,1e8);r.nextDeparture=n(v,"nextDeparture",0,0,1e12);r.cargo=a<double,goodCount>(v,"cargo",0,1000);r.trips=int(n(v,"trips",0,0,1e9));r.escortUntil=n(v,"escortUntil",0,0,1e12);e.routes.push_back(r);
    }
    const auto& contracts=j.at("contracts");array(contracts,100000);std::set<std::string> contractIds;
    for (const auto& v:contracts) {
        Contract c;c.id=t(v,"id");c.settlement=t(v,"settlement");if (!contractIds.insert(c.id).second||!ids.count(c.settlement)) throw std::runtime_error("Invalid supply contract");
        c.good=int(n(v,"good",0,0,goodCount-1));c.requested=int(n(v,"requested",20,1,10000));c.delivered=int(n(v,"delivered",0,0,c.requested));c.rewardPerUnit=int64_t(n(v,"rewardPerUnit",10,1,1e6));c.escrow=int64_t(n(v,"escrow",0,0,1e12));c.deadline=n(v,"deadline",0,0,1e12);c.accepted=v.value("accepted",false);c.closed=v.value("closed",false);
        if (c.escrow!=int64_t(c.requested-c.delivered)*c.rewardPerUnit&&!c.closed) throw std::runtime_error("Invalid contract escrow");e.contracts.push_back(c);
    }
    const auto& history=j.at("history");array(history,2000);for (const auto& v:history) e.history.push_back({n(v,"hour",0,0,hour+.000001),t(v,"place"),t(v,"cause")});
    if (!e.docked.empty()&&!ids.count(e.docked)) throw std::runtime_error("Invalid docked settlement");
    if(j.contains("destruction"))e.worldDamage=decodeWorldDamage(j.at("destruction"),u,hour);
    for(const auto& [id,d]:e.worldDamage)if(d.kind==FacilityObject){auto* site=findSettlement(e,d.owner);if(!site||site->star!=d.star||site->planet!=d.body||std::abs(facilityIntegrity(*site,d.slot/24,d.slot%24)-d.integrity)>1e-7)throw std::runtime_error("Structural ruin disagrees with facility capacity");}
    if(j.contains("ecosystems"))decodeEcosystems(e,j.at("ecosystems"),u);else for(const auto& s:e.settlements)if(!s.habitat.empty())throw std::runtime_error("Habitat reference has no ecosystem state");
    if(j.contains("security"))decodeSecurity(e,j.at("security"));
    if(j.contains("shipyard"))decodeShipyard(e,j.at("shipyard"));
    if(j.contains("oceanWrecks"))decodeOceanWrecks(e,j.at("oceanWrecks"),u);
    if(j.contains("recoverySociety"))decodeRecoverySociety(e,j.at("recoverySociety"),hour);
    validateRecoverySociety(e);
    if(j.contains("missions"))decodeMissions(e,j.at("missions"),u);
    return e;
}
}
