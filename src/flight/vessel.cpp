#include "vessel.hpp"
#include <algorithm>
namespace hyperion::flight {
const VesselDef& vesselDef(int model){static const std::array<VesselDef,vesselModelCount> frames{{
 {"NOMA surveyor","Balanced expedition ship",1600,480,240,100,100,1,1,1,1,1,1,1},
 {"Kestrel skimmer","Fast, agile light ship / smaller hold",2200,280,200,80,90,1.32,1.3,1.05,.95,1.15,1.1,.85},
 {"Hyperion mule","Heavy freight carrier / slower handling",3200,900,360,160,120,.72,.74,.9,.95,.9,.85,1.15},
 {"Prism warden","Combat escort / strong shields and reactor",4200,360,280,130,160,1.05,.95,1.2,1.15,1.1,.95,1.1}
 }};return frames[std::clamp(model,0,vesselModelCount-1)];}
const char* vesselUpgradeName(int i){static const char* names[]={"Cargo racks","Flight thrusters","Reactor tuning","Shield coils","Hull reinforcement","Weapon capacitors","Sensor suite"};return names[std::clamp(i,0,vesselUpgradeCount-1)];}
namespace {int tier(const Vessel& v,int i){return std::clamp(v.fitted[i],0,3);}}
double cargoCapacity(const Vessel& v){return vesselDef(v.model).cargo+80*tier(v,CargoRacks);}
double hullCapacity(const Vessel& v){return vesselDef(v.model).hull*(1+.15*tier(v,ReinforcedHull));}
double shieldCapacity(const Vessel& v){return vesselDef(v.model).shield*(1+.15*tier(v,ShieldCoils));}
double speedFactor(const Vessel& v){return vesselDef(v.model).speed*(1+.08*tier(v,FlightDrive));}
double agilityFactor(const Vessel& v){return vesselDef(v.model).agility*(1+.05*tier(v,FlightDrive));}
double reactorFactor(const Vessel& v){return vesselDef(v.model).reactor*(1+.15*tier(v,ReactorTuning));}
double weaponFactor(const Vessel& v){return vesselDef(v.model).weapons*(1+.08*tier(v,WeaponCapacitors));}
double sensorFactor(const Vessel& v){return vesselDef(v.model).sensors*(1+.15*tier(v,SensorSuite));}
int64_t refitPrice(int tier){return 300*std::clamp(tier,1,3);}
}
