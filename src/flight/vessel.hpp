#pragma once
#include <array>
#include <cstdint>
#include <string>
namespace hyperion::flight {
constexpr int vesselModelCount=4,vesselUpgradeCount=7;
enum VesselUpgrade {CargoRacks,FlightDrive,ReactorTuning,ShieldCoils,ReinforcedHull,WeaponCapacitors,SensorSuite};
struct Vessel {std::string serial="NOMA-01";int model=0;std::array<int,vesselUpgradeCount> fitted{};};
struct VesselDef {const char* name;const char* purpose;int price,cargo;double tank,hull,shield,speed,agility,reactor,weapons,sensors,jump,burn;};
const VesselDef& vesselDef(int model);
const char* vesselUpgradeName(int);
double cargoCapacity(const Vessel&);double hullCapacity(const Vessel&);double shieldCapacity(const Vessel&);
double speedFactor(const Vessel&);double agilityFactor(const Vessel&);double reactorFactor(const Vessel&);
double weaponFactor(const Vessel&);double sensorFactor(const Vessel&);
int64_t refitPrice(int tier);
}
