#pragma once
#include "universe.hpp"
#include "economy.hpp"
#include "flight/game.hpp"
#include <set>
namespace hyperion {
enum class Mode {Galaxy,System,Surface,Ocean,Flight};
enum class Notice {Status,Important,Critical};
std::string modeName(Mode);
struct Discovery {std::string id,title,kind,planet,star,description;double depth=0,hours=0;};
struct LogEntry {double time=0;std::string msg;};
struct State {
 std::string seed="MERIDIAN-0001",currentStar="HOME",selectedStar="HOME",viewStar="HOME",activeStar="HOME";
 Mode mode=Mode::Galaxy,expeditionMode=Mode::Surface;int selectedPlanet=2,activePlanet=2;
 V2 galaxy{65,28},surface{},waypoint{};double galaxyZoom=.74;
 double oceanX=0,depth=75,oceanAtY=0,oceanTargetX=0,targetDepth=75;
 double fuel=190,maxFuel=240,hull=100,battery=100,hours=0,totalDistance=0;
 std::array<int,4> resources{60,22,24,8},upgrades{};
 std::set<std::string> visitedStars{"HOME"},visitedPlanets,scanned;
 std::map<std::string,V2> locations;std::vector<Discovery> discovered;std::vector<LogEntry> log;
 Economy economy;hyperion::flight::Game flight;
 int signals=0,relics=0,turns=0;int lifeRevision=1;int generation=2;std::set<std::string> legacySystems;
};
struct Upgrade {const char* key;const char* name;const char* desc;std::array<int,4> cost;int max;};
extern const std::array<Upgrade,4> upgradeDefs;
struct MovePlan {V2 position{};double depth=0;int turns=0;bool valid=false,impact=false;double power=0,wear=0;};
struct FieldQuote {bool available=false;int battery=0;double wear=0,repair=0;Stock gain{};MovePlan move;std::string reason,text;};
// Session recall changes views and plans, never the physical ship or save schema.
struct ChartRecall {bool valid=false;Mode mode=Mode::System;std::string star,view,selected,minor;int planet=2;V2 camera{};double zoom=.74;};
struct SurveyRecall {bool valid=false,dive=false;Mode layer=Mode::Surface;std::string star,focus;int planet=2;V2 anchor{},waypoint{};double targetX=0,targetDepth=75;};
struct DiveState {bool active=false,returning=false,finished=false;std::string star;int planet=2,studies=0,collections=0;V2 origin{};Contact goal;std::set<std::string> revealed;Stock initial{};double started=0,initialHull=100,lastPing=-1000,pingRange=0;};
struct DiveBrief {Contact goal;bool hasGoal=false,known=false,studied=false,atBuoy=false,nearSurface=false,custom=false;double distance=0,range=260;int homeTurns=0,phase=0;std::string instruction;};
struct DiveOrder {bool available=false;std::string action,arg,label,text;int hours=1;};
struct OceanWater {double current=0,shelter=0,vent=0;std::string biome;};
struct OceanRoute {V2 waypoint;int turns=0;double power=0,wear=0;bool detour=false;};
struct DiveSiteInfo {std::string role,purpose;int approach=0,home=0,shell=0;};
struct DiveReport {bool valid=false,complete=false,emergency=false;std::string star,goal;int planet=2,studies=0,collections=0;double hours=0,ended=0,hull=0;Stock delta{};};
struct TransferPlan {bool available=false;double hours=0;std::string text,reason;};
struct TurnAdvisory {Notice priority=Notice::Status;std::string text,settlement;};
struct TurnOutlook {double hours=1,battery=0,pressureLoss=0;int warnings=0;std::vector<TurnAdvisory> items;};
struct TurnEvent {double hours=0;Notice priority=Notice::Status;bool estimate=false;std::string text,settlement;};
struct TurnReport {
 bool valid=false,strategic=true;int first=0,last=0;double startHour=0,endHour=0,requested=0,hours=0,hull=0,shield=0,battery=0,fuel=0,rations=0,alloy=0;int64_t credits=0;
 std::vector<std::string> events,colonies;std::string interruption;
 std::string summary()const;
};
class Game {
public:
 State s;Universe universe;
 std::string toast;double toastTime=0,radar=-1,heading=0,elapsed=0;std::string soundEvent;
 Notice toastPriority=Notice::Status;unsigned criticalSerial=0;TurnReport lastTurn;std::vector<TurnReport> recentTurns;
 std::string trackedMission;
 int wreckGood=Components;
 std::string selectedMinor,focusedContact,transferNotice;ChartRecall chartRecall;SurveyRecall surveyRecall;DiveState diveState;DiveReport diveReport;std::string diveDebrief;
 const MinorBody* minorSelection(){for(const auto& b:universe.minorBodies(s.viewStar))if(b.id==selectedMinor)return &b;return nullptr;}
 void observeMissions(double dt=0);void missionEvent(const hyperion::flight::Event&);bool missionAction(const std::string&,const std::string&);
 explicit Game(const std::string& seed="MERIDIAN-0001"){reset(seed);}
 void reset(const std::string& seed);void adoptGeneration();
 void notify(const std::string& text,Notice priority=Notice::Status);
 void log(const std::string& text);
 Star currentStar(){return universe.star(s.currentStar);}
 Star selectedStar(){return universe.star(s.selectedStar);}
 Planet planet(){return universe.planet(s.viewStar,s.selectedPlanet);}
 Planet activePlanet(){return universe.planet(s.activeStar,s.activePlanet);}
 double cargoCapacity()const{return hyperion::flight::cargoCapacity(s.economy.vessel);}
 double range()const{return (310+s.upgrades[0]*185)*hyperion::flight::vesselDef(s.economy.vessel.model).jump;}
 double maxDepth()const{return 900+s.upgrades[2]*1800;}
 int fuelCost(double d)const{return int(std::ceil((d*.16+10)*hyperion::flight::vesselDef(s.economy.vessel.model).burn));}
 bool travel(const std::string& id);
 bool hasExpedition()const{return s.flight.deployed||s.mode==Mode::Surface||s.mode==Mode::Ocean;}
 Mode expeditionLayer()const{return s.mode==Mode::Surface||s.mode==Mode::Ocean?s.mode:s.expeditionMode;}
 bool inOrbit();void resumeExpedition();
 void beginDive(bool plot=true);void finishDive(bool safe=true);std::vector<Contact> diveSites();std::vector<Contact> oceanContacts();bool oceanKnown(const Contact&)const;DiveBrief diveBrief();DiveOrder nextDiveOrder();Contact nextDiveContact();double diveAlert()const;DiveSiteInfo diveSiteInfo(const Contact&)const;bool diveReturnNeeded()const;int oceanSonar(bool quiet=false);bool oceanAction(const std::string&,const std::string&);
 void discoverOceanWreck();std::vector<Contact> wreckSites()const;FieldQuote wreckQuote(const std::string&,const std::string&);bool wreckAction(const std::string&,const std::string&);int wreckCargoGood(const OceanWreck&)const;int rescuedCrew()const;
 OceanWater oceanWater(V2,double)const;OceanRoute oceanRoute(bool shelter)const;
 void openStrategy();void rememberSurvey();void restoreSurvey(bool ocean);TransferPlan transferPlan(const std::string&);
 void galaxy();void system(const std::string& id);void refuel();bool orbitScan();bool descend();void launch();bool dive();void surface(bool consumeTurn=true);
 std::vector<Contact> targets();bool catalogue(const Contact&,bool collect=true,int bonus=0);int scan();
 std::array<int,4> upgradeCost(int i)const;bool canUpgrade(int i)const;bool purchase(int i);
 void waypoint(const Contact&);
 void simulate(double dt,V2 movement={},const hyperion::flight::Input& flightInput={});
 bool turnBased()const{return s.mode!=Mode::Flight;}
 TurnOutlook outlook(double hours=1)const;std::vector<TurnEvent> upcoming(double hours=24)const;
 void waitPlanned(const std::string& duration);std::string orderHint(const std::string& action,const std::string& arg="");
 MovePlan planMove(V2 input={},bool follow=true,double stride=1);
 std::vector<Contact> fieldSites();FieldQuote fieldQuote(const std::string&,const std::string& id="");bool fieldAction(const std::string&,const std::string&);
 void advanceTime(double hours,bool strategy,bool resolveFlight=true,bool interruptible=false);void endTurn();bool moveTurn(V2 movement={},bool follow=false);void expireCourier();
 void action(const std::string&,const std::string& arg="");
 static bool planningAction(const std::string&);bool issueOrder(const std::string&,const std::string& arg="",bool blocked=false);
 void registerExpeditionHabitat();void ensureEconomy();void syncEconomy();void waitHours(double);
 Stock shipStock()const;void storeShipStock(const Stock&);bool collectStockGains(const Stock&);
 bool canDock(const Settlement&)const;bool dock(const std::string&);
 bool economicAction(const std::string&,const std::string&);
 bool shipyardAction(const std::string&,const std::string&);bool canUseShipyard(const Settlement&)const;ShipOffer currentVessel()const;
 void refreshSecurity();void storeSecurity();bool securityEvent(const hyperion::flight::Event&);bool securityAction(const std::string&,const std::string&);
 void ensureFlight();void enterFlight();void updateFlight(double,const hyperion::flight::Input& = {});
 void parkFlight(int,V2,bool orbital=false);bool disembark();void processFlightEvents();
 bool flightAction(const std::string&,const std::string&);int flightPort(const std::string&,bool orbital=false);
 hyperion::flight::V3 surfaceNormal(V2)const;V2 surfacePosition(hyperion::flight::V3)const;
private:
 bool commitMove(const MovePlan&,bool follow,double battery=0,double wear=0);void followFieldRoute(const std::string& approach="");
};
bool saveState(const State&,const std::string& path,std::string& error);
bool loadState(State&,const std::string& path,std::string& error);
std::string defaultSavePath();
std::string legacySavePath();
}
