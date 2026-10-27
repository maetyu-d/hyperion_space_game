#pragma once
#include "universe.hpp"
namespace hyperion {
struct Economy;
enum MissionKind {ReliefMission,EscortMission,InterdictionMission,SurveyMission,WreckMission,ReconstructionMission,EvacuationMission,BlackBoxMission};
enum MissionState {MissionOffered,MissionActive,MissionReady,MissionComplete,MissionFailed,MissionExpired};
struct Mission {
 std::string id,title,cause,issuer,star,target,result;
 int kind=0,state=0,faction=1,planet=0,good=4,quantity=0,delivered=0,trip=0;
 uint32_t seed=0;Contact contact;
 double created=0,deadline=0,risk=0,coverage=0,initialCargo=0,startProgress=0,lastObserved=0;
 int64_t reward=0,escrow=0;
};
const char* missionKindName(int);const char* missionStateName(int);
bool missionOpen(const Mission&);
void advanceMissions(Economy&,Universe&,const std::string& system,const std::set<std::string>* catalogued=nullptr);
void finishMission(Economy&,Mission&,int state,const std::string& cause);
std::string missionObjective(const Economy&,const Mission&);
}
