#pragma once
#include "../world_damage.hpp"
#include "vessel.hpp"
#include "threat.hpp"
#include "fold.hpp"
#include "../minor_bodies.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

namespace hyperion::flight {
// Material properties are derived from the surveyed world; flight rendering
// never invents a second planet or changes its collision/navigation geometry.
struct SurfaceScar {V3 normal;double radius=1,depth=.2;};
struct Planet {std::string name,biome;V3 centre;double radius,seed;V3 portNormal;bool giant=false;
 double water=.35,life=.25,temperature=289,atmosphere=1;int kind=0;uint32_t worldSeed=0;int generation=1,landform=0;double tectonic=.5;std::vector<SurfaceScar> scars{};
};
struct Port {std::string name,description;V3 position,normal;int planet;std::string id,settlement;int body=-1;int minor=-1;bool disabled=false,recovery=false;int fold=-1;};
struct MinorObject {hyperion::MinorBody source;V3 centre;bool destroyed=false;};
std::vector<V3> transferRoute(const std::vector<Planet>& planets,V3 start,const Port& destination);
struct RoutePose {V3 position,forward;};
RoutePose freightPose(const std::vector<V3>& path,double progress);
double terrain(const Planet& p,V3 normal);
V3 terrainNormal(const Planet& p,V3 normal);
double materialElevation(const Planet& p,V3 normal);
enum ShipSystem {Drive,PowerCore,ShieldGenerator,WeaponMounts,Sensors,SystemCount};
enum WeaponKind {Pulse=0,Kinetic=1,Plasma=2,Seeker=3,Resonance=4,CollisionDamage=5};
int primaryWeapon(int slot);const char* focusName(int focus);double interceptTime(V3 offset,V3 velocity,double speed);
struct WeaponDef {const char* name;double damage,shieldFactor,hullFactor,penetration,speed,interval,energy,heat,life;};
const WeaponDef& weaponDef(int type);
const char* systemName(int system);
struct EnemyDef {const char* name;double hull,shield,speed,range,radius;int weapon;double retreat=.22,agility=1,cadence=1,reactor=1,cooling=1,standOff=0,strafe=0,aimJitter=0;bool evades=false;};
const EnemyDef& enemyDef(int role);
EnemyDef enemyDef(int role,const EnemyGenome&);
struct CombatState {
 std::array<double,SystemCount> systems{100,100,100,100,100};
 double armour=100,energy=100,heat=0,shieldDelay=0,missileClock=0,decoyClock=0;
 int weapon=0,rounds=300,missiles=6,decoys=6;bool overheated=false;
 uint32_t hits=0,shots=0;
};
struct Input {double pitch=0,yaw=0,roll=0,throttle=0,lift=0,strafe=0;bool boost=false,fire=false,missile=false,decoy=false,precision=false;};
struct Ship {Vessel vessel;V3 angularVelocity,thrusters,assistCapture;double boostLevel=0,assistCaptureTime=0;V3 position,velocity,previous,forward{0,0,-1},up{0,1,0};double throttle=0,fuel=100,hull=100,shield=100;bool assist=true,gear=true,cruise=false;CombatState combat;};
// Missing weapon metadata denotes a pulse already in flight in a legacy save.
struct Bolt {V3 position,previous,velocity;double life;bool hostile;int weapon=-1;double damage=0;std::string owner,target;int module=0;};
struct Traffic {V3 position,previous,forward;double phase=0,health=100,fireClock=0;int kind=0;bool dead=false;double respawn=0;int home=0,faction=3;std::string routeId;int trip=-1;
 std::vector<V3> transitPath,transitAfter;std::string transitKey;int transitLeg=0;
 V3 velocity;std::string id;int role=0,state=0;double shield=0,armour=100,shieldDelay=0,stateClock=0,hitFlash=0;bool positioned=false;
 std::array<double,3> modules{100,100,100};double energy=100,heat=0,charge=0,decoyClock=0,evadeClock=0,serviceClock=0;
 int ammunition=180,missiles=2,countermeasures=2,chargeWeapon=-1,salvos=0;bool venting=false;
 bool managed=false,pirate=false,hostile=true,assaultReported=false,escortDuty=false,boarding=false,captured=false;
 std::string name,allegiance,band,reason;std::array<bool,5> enemies{};EnemyGenome genome;
};
inline EnemyDef enemyDef(const Traffic& t){return enemyDef(t.role,t.genome);}
const char* enemyStateName(const Traffic&);
enum CombatEffectKind {ShieldImpact=0,HullImpact=1,ShipExplosion=2,DecoyBurst=3,ChainDischarge=4,MuzzleFlash=5,ShieldBreak=6};
struct HullRemains {bool valid=false;int faction=0,model=3;uint32_t seed=0;double size=8;V3 forward{0,0,-1};};
struct CombatEffect {V3 position,velocity;double age=0,duration=1;int kind=0;V3 end;V3 normal{0,0,1};int weapon=Pulse;double strength=1,size=1;HullRemains hull{};std::string actor{};};
// Presentation only: never serialized and never used by aiming or damage rules.
struct CombatFeedback {
 double recoil=0,impact=0,shieldBreak=0;int weapon=Pulse,confirmation=ShieldImpact;
 std::array<uint32_t,5> shots{};uint32_t shieldHits=0,hullHits=0,breaks=0,destructions=0;
};
struct Wreck {V3 position,velocity;double life=180;std::array<double,3> cargo{};std::string id;HullRemains hull{};int debrisKind=0;std::array<double,9> supplies{};};
struct Courier {bool active=false;std::string issuer,destination;int64_t escrow=0;double deadline=0;};
struct Event {std::string kind;int port=-1,faction=0;std::string routeId;std::string actor,victim;double amount=0;int weapon=0;V3 position,normal;
 Event(std::string k,int p=-1,int f=0,std::string r="",std::string a="",std::string v=""):kind(std::move(k)),port(p),faction(f),routeId(std::move(r)),actor(std::move(a)),victim(std::move(v)){}
};
struct LandingInfo {
 int body=-1,port=-1;V3 ground,normal,radial;
 double altitude=0,sink=0,drift=0,tilt=0,slope=0;
 bool available=false,safe=false,water=false;
};
struct Game {
  hyperion::WorldDamageMap worldDamage;std::vector<hyperion::WorldObject> worldObjects;int worldBlastDepth=0;double portCombat=0;V3 scarAnchor{1e10,0,0};uint64_t scarKey=0;uint64_t geometryRevision=0,damageRevision=0;std::string damageStar;
  std::vector<Planet> planets;
  std::vector<MinorObject> minorBodies;
  std::vector<FoldGate> folds;std::vector<NilRegion> nilRegions;
  std::vector<Port> ports;
  Ship ship;
  std::vector<Bolt> bolts;
  std::vector<Traffic> traffic;
  std::vector<CombatEffect> effects;std::vector<Wreck> wrecks;CombatFeedback feedback;
  std::string lastAttacker;
  std::string combatTarget;double lock=0,hitConfirm=0;int combatOrder=0,aimSystem=0;V3 hitDirection;double damageCue=0;
  double weatherHours=-1,impactShield=0,impactHull=0;int impactSystem=-1;
 bool strategicCombat=false,missileHeld=false,decoyHeld=false,foldTransitAllowed=true;
  std::vector<V3> route;
  Courier courier;
  std::vector<Event> events;
  std::string star,worldKey,savedTarget,savedDock,savedBerth;
  bool valid=false,deployed=false;
  int target=0,dockedPort=-1,lastBerth=0,deliveries=0,kills=0;
  bool docked=true,autopilot=false;
  double thrustFactor=1,cargoMass=0; // Cargo units; canonical hold is supplied by Hyperion.
  V3 acceleration;double engineLoad=0;bool precisionActive=false; // Transient cockpit telemetry.
  double time=0,noticeAge=0,fireClock=0,hitFlash=0;
  double launchAssist=0,touchdownAge=10,liftThrust=0;
  std::string notice="NOMA flight deck ready. Select a destination in I; J engages navigation.";
  Game()=default;
  void update(double dt,const Input& input={});
  bool crossFold(int gate);void foldContacts();void foldTraffic(Traffic&);
  void stepHandling(double dt,const Input&);
  double inertialMass()const;
  V3 gravity()const;
  double atmosphereDensity()const;
  double thrustLimit(V3 direction,bool boost=false)const;
  double stoppingDistance()const;
  double manualSpeedLimit(bool precision=false)const;
  void setAssist(bool enabled);
  void brake();
  void resetDynamics(){ship.angularVelocity={};ship.thrusters={};ship.boostLevel=0;ship.assistCapture={};ship.assistCaptureTime=0;acceleration={};engineLoad=0;precisionActive=false;}
  void launch();
  bool dock();
  void dockAt(int port,bool preserveAttitude=false);
  void navigate(bool resume=false);
  void cancelNavigation();
  void selectTarget(int index);
  int nearestPlanet()const;
  double altitude()const;
  LandingInfo landing()const;
  double speed()const{return length(ship.velocity);}
  double targetDistance()const{return length(ports[target].position-ship.position);}
  void message(std::string text);
  void orient(V3 forward,V3 up);
  void recordIds();
  void updateMinorBodies(double hours);std::vector<Planet> navigationBodies()const;
  double minorHit(V3,V3,double clearance=0,int* body=nullptr)const;
  void minorContacts(V3 previous);
  int combatTargetIndex()const;void cycleCombatTarget();bool selectWeapon(int);
  void prepareCombat(double,Input&);void updateCombat(double,const Input&);
  void damageWorld(int,double,int,V3,bool,const std::string& attacker="");
  void damageShip(double,int,V3);void damageTraffic(int,double,int,V3,bool credited,int module=0,const std::string& attacker="");void chainArc(int,double);
  bool enemiesOf(const Traffic&,const Traffic&)const;
  V3 apparentPosition(V3 from,V3 to,int* through=nullptr)const;V3 apparentVelocity(int gate,V3 velocity)const;bool sighted(V3 from,V3 to)const;
  bool lineOfSight(V3,V3)const;double terrainHit(V3,V3)const;bool protectedPort(V3)const;
  void effect(V3,int,V3 velocity={},V3 end={},V3 normal={0,0,1},int weapon=Pulse,double strength=1,double size=1);
};
}
