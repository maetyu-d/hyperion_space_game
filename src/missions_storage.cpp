#include "missions_storage.hpp"
#include <stdexcept>
namespace hyperion {
using J=nlohmann::json;
J encodeMissions(const Economy& e){
 J j={{"schema",1},{"clocks",e.missionClock},{"records",J::array()}};
 for(const auto& m:e.missions)j["records"].push_back({{"id",m.id},{"title",m.title},{"cause",m.cause},{"issuer",m.issuer},{"star",m.star},{"target",m.target},{"result",m.result},{"kind",m.kind},{"state",m.state},{"faction",m.faction},{"planet",m.planet},{"good",m.good},{"quantity",m.quantity},{"delivered",m.delivered},{"trip",m.trip},{"seed",m.seed},{"created",m.created},{"deadline",m.deadline},{"risk",m.risk},{"coverage",m.coverage},{"initialCargo",m.initialCargo},{"startProgress",m.startProgress},{"lastObserved",m.lastObserved},{"reward",m.reward},{"escrow",m.escrow},{"contact",{{"id",m.contact.id},{"kind",m.contact.kind},{"name",m.contact.name},{"desc",m.contact.desc},{"position",{m.contact.pos.x,m.contact.pos.y}},{"depth",m.contact.depth},{"seed",m.contact.n}}}});
 return j;
}
namespace {
double number(const J& j,const char* k,double lo,double hi){double v=j.at(k).get<double>();if(!std::isfinite(v)||v<lo||v>hi)throw std::runtime_error(std::string("Invalid mission ")+k);return v;}
int64_t integer(const J& j,const char* k,int64_t lo,int64_t hi){double v=number(j,k,double(lo),double(hi));if(std::floor(v)!=v)throw std::runtime_error("Fractional mission count");return int64_t(v);}
std::string text(const J& j,const char* k,size_t max=240){auto s=j.at(k).get<std::string>();if(s.size()>max)throw std::runtime_error("Oversized mission text");return s;}
}
void decodeMissions(Economy& e,const J& j,Universe& u){
 if(!j.is_object()||j.at("schema")!=1||!j.at("clocks").is_object()||j.at("clocks").size()>20000||!j.at("records").is_array()||j.at("records").size()>256)throw std::runtime_error("Invalid mission collection");
 for(auto it=j.at("clocks").begin();it!=j.at("clocks").end();++it){double v=it.value().get<double>();if(!e.systems.count(it.key())||!std::isfinite(v)||v<0||v>1e12)throw std::runtime_error("Invalid mission system clock");e.missionClock[it.key()]=v;}
 std::set<std::string> ids;
 for(const auto& v:j.at("records")){
  Mission m;m.id=text(v,"id");m.title=text(v,"title",500);m.cause=text(v,"cause",2400);m.issuer=text(v,"issuer");m.star=text(v,"star");m.target=text(v,"target");m.result=text(v,"result",1000);
  m.kind=int(integer(v,"kind",0,7));m.state=int(integer(v,"state",0,5));m.faction=int(integer(v,"faction",1,4));m.planet=int(integer(v,"planet",0,int(u.planets(m.star).size())-1));m.good=int(integer(v,"good",0,goodCount-1));m.quantity=int(integer(v,"quantity",0,10000));m.delivered=int(integer(v,"delivered",0,m.quantity));m.trip=int(integer(v,"trip",0,1000000000));m.seed=uint32_t(integer(v,"seed",0,4294967295LL));
  m.created=number(v,"created",0,e.hour+.000001);m.deadline=number(v,"deadline",m.created,1e12);m.risk=number(v,"risk",0,1);m.coverage=number(v,"coverage",0,1);m.initialCargo=number(v,"initialCargo",0,1000);m.startProgress=number(v,"startProgress",0,1);m.lastObserved=number(v,"lastObserved",0,e.hour+.251);m.reward=integer(v,"reward",0,1000000000);m.escrow=integer(v,"escrow",0,1000000000);
  auto* site=findSettlement(e,m.issuer);if(m.id.empty()||!ids.insert(m.id).second||!site||site->star!=m.star||!e.systems.count(m.star)||(missionOpen(m)?m.escrow!=m.reward:m.escrow!=0))throw std::runtime_error("Invalid mission issuer, identity or escrow");
  if(m.kind>=ReconstructionMission&&m.state<=MissionActive){auto target=e.worldDamage.find(m.target);if(target==e.worldDamage.end()||!target->second.recovery.valid||target->second.star!=m.star||target->second.recovery.faction!=m.faction||target->second.recovery.culprit=="NOMA"||m.quantity<1||(m.kind==BlackBoxMission&&(m.quantity!=1||m.good!=Research))||(m.kind==EvacuationMission&&m.quantity!=target->second.recovery.initialCrew))throw std::runtime_error("Invalid physical recovery commission");}
  if(m.kind==ReliefMission&&(m.target!=m.issuer||m.quantity<1))throw std::runtime_error("Invalid relief mission");
  if(m.kind==InterdictionMission){auto* c=findCaptain(e,m.target);if(!c||c->star!=m.star||c->band.empty())throw std::runtime_error("Unknown commissioned pirate captain");}
  if(m.kind==EscortMission&&m.state<=MissionReady&&std::none_of(e.routes.begin(),e.routes.end(),[&](const auto& r){return r.id==m.target;}))throw std::runtime_error("Unknown commissioned route");
  const auto& c=v.at("contact");m.contact.id=text(c,"id");m.contact.kind=text(c,"kind");m.contact.name=text(c,"name",500);m.contact.desc=text(c,"desc",2400);m.contact.depth=number(c,"depth",0,15000);m.contact.n=uint32_t(integer(c,"seed",0,4294967295LL));auto pos=c.at("position");if(!pos.is_array()||pos.size()!=2)throw std::runtime_error("Invalid mission point");m.contact.pos={pos[0].get<double>(),pos[1].get<double>()};
  if(!std::isfinite(m.contact.pos.x)||!std::isfinite(m.contact.pos.y)||std::abs(m.contact.pos.x)>1e7||std::abs(m.contact.pos.y)>1e7)throw std::runtime_error("Invalid mission coordinates");
  if(m.kind==SurveyMission){auto world=u.planet(m.star,m.planet);auto generated=m.contact.depth>0?u.oceanObjects(world,int(std::floor(m.contact.pos.x/130)),m.contact.pos.y):u.surfaceObjects(world,int(std::floor(m.contact.pos.x/86)),int(std::floor(m.contact.pos.y/86)));bool valid=false;for(const auto& actual:generated)if(actual.id==m.target&&actual.id==m.contact.id&&actual.n==m.contact.n&&actual.kind==m.contact.kind&&distance(actual.pos,m.contact.pos)<1e-5&&std::abs(actual.depth-m.contact.depth)<1e-5)valid=true;if(!valid||m.quantity<1||m.good!=Research)throw std::runtime_error("Mission does not reference a generated field feature");}
  if(m.kind==WreckMission){auto* w=findOceanWreck(e,m.target);if(!w||m.issuer!=w->home||m.faction!=w->faction||m.planet!=w->planet||m.contact.id!=w->contact.id||m.contact.kind!="wreck"||m.contact.n!=w->contact.n||distance(m.contact.pos,w->contact.pos)>1e-5||std::abs(m.contact.depth-w->contact.depth)>1e-5||m.quantity<1||m.trip>1||(m.trip==1?m.quantity>w->initialCrew:m.quantity>w->initial[m.good]))throw std::runtime_error("Invalid commissioned ocean wreck");}
  e.missions.push_back(std::move(m));
 }
}
}
