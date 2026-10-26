#include "ocean_storage.hpp"
#include <stdexcept>
namespace hyperion {
using J=nlohmann::json;
J encodeOceanWrecks(const Economy& e){J j=J::array();for(const auto& w:e.oceanWrecks)j.push_back({{"id",w.contact.id},{"name",w.contact.name},{"description",w.contact.desc},{"seed",w.contact.n},{"position",{w.contact.pos.x,w.contact.pos.y}},{"depth",w.contact.depth},{"star",w.star},{"planet",w.planet},{"home",w.home},{"faction",w.faction},{"initialCrew",w.initialCrew},{"crew",w.crew},{"aboard",w.aboard},{"deliveredCrew",w.deliveredCrew},{"lost",w.lost},{"initial",w.initial},{"remaining",w.remaining},{"recovered",w.recovered},{"delivered",w.delivered},{"created",w.created},{"supportUntil",w.supportUntil},{"surveyed",w.surveyed},{"breached",w.breached}});return j;}
namespace {
double num(const J& v,const char* key,double low,double high){double n=v.at(key).get<double>();if(!std::isfinite(n)||n<low||n>high)throw std::runtime_error("Invalid ocean wreck number");return n;}
int64_t integer(const J& v,const char* key,int64_t low,int64_t high){double n=num(v,key,double(low),double(high));if(std::floor(n)!=n)throw std::runtime_error("Fractional ocean wreck count");return int64_t(n);}
std::string str(const J& v,const char* key,size_t limit=240){auto s=v.at(key).get<std::string>();if(s.size()>limit)throw std::runtime_error("Oversized ocean wreck text");return s;}
Stock stock(const J& v,const char* key){auto a=v.at(key);if(!a.is_array()||a.size()!=goodCount)throw std::runtime_error("Invalid ocean wreck cargo");Stock result{};for(int i=0;i<goodCount;i++){result[i]=a[i].get<double>();if(!std::isfinite(result[i])||result[i]<0||result[i]>32||std::floor(result[i])!=result[i])throw std::runtime_error("Invalid finite wreck allowance");}return result;}
}
void decodeOceanWrecks(Economy& e,const J& j,Universe& u){
 if(!j.is_array()||j.size()>4096)throw std::runtime_error("Invalid ocean wreck collection");std::set<std::string> ids;int passengers=0;
 for(const auto& v:j){OceanWreck w;w.star=str(v,"star");w.planet=int(integer(v,"planet",0,int(u.planets(w.star).size())-1));w.home=str(v,"home");w.faction=int(integer(v,"faction",1,4));w.initialCrew=int(integer(v,"initialCrew",0,4));w.crew=int(integer(v,"crew",0,4));w.aboard=int(integer(v,"aboard",0,4));w.deliveredCrew=int(integer(v,"deliveredCrew",0,4));w.lost=int(integer(v,"lost",0,4));w.initial=stock(v,"initial");w.remaining=stock(v,"remaining");w.recovered=stock(v,"recovered");w.delivered=stock(v,"delivered");
  if(w.crew+w.aboard+w.deliveredCrew+w.lost!=w.initialCrew)throw std::runtime_error("Unbalanced wreck population");passengers+=w.aboard;
  for(int g=0;g<goodCount;g++)if(w.initial[g]!=w.remaining[g]+w.recovered[g]+w.delivered[g])throw std::runtime_error("Unbalanced finite wreck cargo");
  w.created=num(v,"created",0,e.hour+.000001);w.supportUntil=num(v,"supportUntil",w.created,w.created+100);w.surveyed=v.at("surveyed").get<bool>();w.breached=v.at("breached").get<bool>();
  auto& c=w.contact;c.id=str(v,"id");c.name=str(v,"name",500);c.desc=str(v,"description",2400);c.n=uint32_t(integer(v,"seed",0,4294967295LL));c.kind="wreck";c.depth=num(v,"depth",20,15000);const auto& pos=v.at("position");if(!pos.is_array()||pos.size()!=2)throw std::runtime_error("Invalid wreck position");c.pos={pos[0].get<double>(),pos[1].get<double>()};
  if(!std::isfinite(c.pos.x)||!std::isfinite(c.pos.y)||std::abs(c.pos.x)>1e7||std::abs(c.pos.y)>1e7||!e.systems.count(w.star)||!ids.insert(c.id).second)throw std::runtime_error("Invalid wreck identity or coordinates");
  auto* home=findSettlement(e,w.home);auto p=u.planet(w.star,w.planet);int cx=int(std::floor(c.pos.x/390)),cy=int(std::floor(c.pos.y/260));auto seed=u.hash(int32_t(p.seed),cx,cy,355);
  if(!home||home->star!=w.star||c.id!="WRECK:"+p.id+":"+std::to_string(cx)+":"+std::to_string(cy)||c.n!=seed||std::abs(c.pos.x-(cx*390.+80.+double(seed%230)))>.00001||std::abs(c.pos.y-(cy*260+130))>.00001||std::abs(c.depth-std::min(u.floorDepth(p,c.pos.x,c.pos.y)-48,220.+(seed>>8)%430))>.00001)throw std::runtime_error("Wreck does not reference its procedural sector");
  if((w.breached||w.aboard||w.deliveredCrew||stockTotal(w.recovered)>0||stockTotal(w.delivered)>0)&&!w.surveyed)throw std::runtime_error("Unsurveyed wreck has recovered contents");e.oceanWrecks.push_back(std::move(w));
 }
 if(passengers>6)throw std::runtime_error("Too many rescue passengers");
}
}
