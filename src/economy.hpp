#pragma once
#include "universe.hpp"
#include "ecosystem.hpp"
#include "missions.hpp"
#include "flight/game.hpp"
#include <set>

namespace hyperion {
constexpr int goodCount = 9, buildingCount = 14, factionCount = 5, techCount = 6;
enum Good { Ore, Alloy, Crystal, Biomass, Rations, Medicine, Components, Propellant, Research };
enum Building { Mine, Refinery, CrystalBore, Harvester, Growhouse, MedLab, Fabricator,
                FuelPlant, Observatory, Solar, Reactor, Habitat, Warehouse, Restoration };
enum Policy { Balanced, Industry, Stewardship, Relief };
using Stock = std::array<double, goodCount>;
struct GoodDef { const char* name; double value; };
struct BuildingDef {
    const char* name;
    const char* purpose;
    Stock inputs, outputs, construction;
    int workers;
    double power, hours, pollution;
    int price;
};
struct TechDef {
    const char* name;
    const char* description;
    int prerequisite;
    double hours;
    Stock cost;
};
extern const std::array<GoodDef, goodCount> goods;
extern const std::array<BuildingDef, buildingCount> buildings;
extern const std::array<TechDef, techCount> technologies;
const char* policyName(int policy);

struct OceanWreck {
 Contact contact;std::string star,home;int planet=0,faction=1,initialCrew=0,crew=0,aboard=0,deliveredCrew=0,lost=0;
 Stock initial{},remaining{},recovered{},delivered{};double created=0,supportUntil=0;bool surveyed=false,breached=false;
};
OceanWreck* findOceanWreck(struct Economy&,const std::string&);
const OceanWreck* findOceanWreck(const struct Economy&,const std::string&);
void advanceOceanWrecks(struct Economy&);
struct Faction {
    std::string name, doctrine;
    double standing = 0;
    std::array<double, factionCount> relations{};
    int64_t treasury = 5000;
    int reservePopulation = 100;
    int64_t warrant = 0; // Outstanding restitution; diplomacy cannot erase a crime.
};
struct LocalConditions {
    double daylight = 1, storm = 0, mineralYield = 1, fertility = 1, toxicity = 0;
    double temperature = 289, pressure = 1, current = 0;
};
struct Settlement {
    std::string habitat;int harvestPolicy=0,harvestTarget=0;
    std::string id, name, star, planetName, governor;
    int planet = 0, owner = 0, authority = 0, residents = 6, policy = Balanced;
    V2 position;
    double depth = 0;
    Stock stock{};
    std::array<int, buildingCount> built{};
    std::array<double, buildingCount> progress{};
    // Last resolved shift, used by the visual presentation; never drives output.
    std::array<double, buildingCount> activity{};
    int64_t treasury = 2500;
    double health = 90, morale = 75, loyalty = 80, condition = 1;
    double pollution = 0, ecosystem = 150, oreDeposit = 18000, crystalDeposit = 6000;
    double power = 0, demand = 0, labor = 0, productivity = 1, researchRate = 0;
    double nextContract = 6, nextMigration = 24, medicineUsed = 0;
    double reactorUse = 0, upkeep = 0, diplomaticClock = 0, mortality = 0;
    int alerts = 0;
    std::string bottleneck = "Establishing production";
};
struct TradeRoute {
    std::string id, from, to, status = "Awaiting departure";
    int good = Rations, owner = 0, capacity = 20;
    bool enabled = true, outbound = false, commercial = false;
    double remaining = 0, duration = 6, nextDeparture = 0;
    Stock cargo{};
    int trips = 0;
    double escortUntil = 0;
};
struct Contract {
    std::string id, settlement;
    int good = Rations, requested = 20, delivered = 0;
    int64_t rewardPerUnit = 10, escrow = 200;
    double deadline = 48;
    bool accepted = false, closed = false;
};
// These records outlive the local flight scene, including destroyed ships.
struct PirateBand {
    std::string id,name,star,haven;Stock stock{};int64_t treasury=0;
    double grievance=0,truceUntil=0,nextRaid=0,stolen=0;int raids=0;
};
struct Captain {
    hyperion::flight::EnemyGenome genome;
    std::string id,name,star,home,band;int faction=1,role=0,defeats=0,victories=0;
    bool destroyed=false;double readyAt=0,grudge=0;
    double hull=100,shield=30,armour=100,energy=100,heat=0;
    std::array<double,3> modules{100,100,100};int rounds=180,missiles=2,decoys=2;
};
struct ShipOffer {std::string port;hyperion::flight::Vessel vessel;hyperion::flight::CombatState equipment;double hull=100,fuel=0,tank=240,shield=100;};
struct VesselService {double fuel=0,hull=0,work=0;int64_t price=0;hyperion::flight::CombatState equipment;};
VesselService quoteVesselService(const ShipOffer&,const Stock&);
struct EconomicEvent { double hour = 0; std::string place, cause; };
struct Economy {
    std::vector<OceanWreck> oceanWrecks;
    std::vector<Mission> missions;std::map<std::string,double> missionClock;
    int ecosystemRevision=1;std::map<std::string,Ecosystem> ecosystems;
    bool initialized = false;
    double hour = 0;
    int64_t credits = 1600;
    int colonists = 24, ships = 2, nextId = 1;
    Stock hold{0, 0, 0, 0, 40, 8, 18, 40, 0};
    std::array<Faction, factionCount> factions;
    std::set<std::string> systems;
    std::vector<Settlement> settlements;
    std::vector<TradeRoute> routes;
    std::vector<Contract> contracts;
    std::vector<EconomicEvent> history;
    std::array<bool, techCount> researched{};
    int research = -1;
    double researchProgress = 0;
    std::string docked;
    std::set<std::string> securitySystems;
    std::vector<PirateBand> pirates;std::vector<Captain> captains;
    // Runtime witness: a live scene resolves local attacks using physical ships.
    std::string securityWitness;
    hyperion::flight::Vessel vessel;bool shipyardsInitialized=false;
    std::map<std::string,double> shipyardClock;std::vector<ShipOffer> shipOffers;
};

bool shipyardPowered(const Economy&,const Settlement&);
void initializeShipyards(Economy&);void advanceShipyards(Economy&);
Stock refitMaterials(int slot,int tier);int64_t vesselValue(const ShipOffer&,bool purchase);
Captain* findCaptain(Economy&,const std::string&);
PirateBand* findPirates(Economy&,const std::string&);
void discoverSecurity(Economy&,Universe&,const std::string&);
void initializeActorProfiles(Economy&,Universe&);
void advanceSecurity(Economy&);
bool factionsAtWar(const Economy&,int,int);
int64_t pirateTrucePrice(const PirateBand&);
Settlement* findSettlement(Economy&, const std::string&);
const Settlement* findSettlement(const Economy&, const std::string&);
LocalConditions conditions(const Universe&, const Planet&, const Settlement&, double hours);
double storageCapacity(const Settlement&);
double stockTotal(const Stock&);
double reserveFor(const Settlement&, int good);
double marketPrice(const Settlement&, int good);
int64_t tradeUnitPrice(const Economy&,const Settlement&,int good,bool buy);
int64_t tradeOrderPrice(const Economy&,const Settlement&,int good,int quantity,bool buy);
std::string settlementStatus(const Settlement&);
void economicEvent(Economy&, const std::string& place, const std::string& cause);
void initializeEconomy(Economy&, Universe&, double hour);
void discoverProceduralEconomy(Economy&,Universe&,const std::string&,double);
void discoverEconomy(Economy&, Universe&, const std::string& star, double hour);
void advanceEconomy(Economy&, Universe&, double hour);
bool foundSettlement(Economy&, Universe&, const Planet&, V2 position, double depth,
                     Stock& ship, std::string& message);
bool construct(Economy&, Settlement&, int building, Stock& ship, std::string& message);
bool repairSettlement(Economy&, Settlement&, Stock& ship, std::string& message);
bool transferCargo(Economy&, Settlement&, Stock& ship, int good, int quantity,
                   bool deposit, std::string& message);
bool trade(Economy&, Settlement&, Stock& ship, int good, int quantity,
           bool buy, std::string& message);
bool commissionRoute(Economy&, const std::string& from, const std::string& to,
                     int good, std::string& message);
bool fulfillContract(Economy&, Contract&, Stock& ship, int quantity, std::string& message);
bool commissionResearch(Economy&, int tech, Stock& ship, std::string& message);
bool diplomaticMission(Economy&, int faction, Stock& ship, std::string& message);
bool recruitColonists(Economy&, Settlement&, std::string& message);
bool buyFreighter(Economy&, Stock& ship, std::string& message);
}
