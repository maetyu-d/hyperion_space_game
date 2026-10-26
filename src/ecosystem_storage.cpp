#include "ecosystem_storage.hpp"
#include <stdexcept>
namespace hyperion {
using J=nlohmann::json;
namespace {
double n(const J& j,const char* key,double lo,double hi){if(!j.at(key).is_number())throw std::runtime_error("Non-numeric ecosystem field");double v=j.at(key).get<double>();if(!std::isfinite(v)||v<lo||v>hi)throw std::runtime_error(std::string("Invalid ecosystem field: ")+key);return v;}
int integer(const J& j,const char* key,int lo,int hi){double v=n(j,key,lo,hi);if(v!=std::floor(v))throw std::runtime_error("Fractional ecosystem coordinate");return int(v);}
std::string text(const J& j,const char* key){auto s=j.at(key).get<std::string>();if(s.size()>240)throw std::runtime_error("Oversized ecosystem identity");return s;}
}
J encodeEcosystems(const Economy& e){
 J j={{"schema",1},{"revision",e.ecosystemRevision},{"habitats",J::array()}};
 for(const auto& [id,h]:e.ecosystems){J pops=J::array();for(const auto& pop:h.populations)pops.push_back({{"seed",pop.seed},{"flora",pop.flora},{"guild",pop.guild},{"mass",pop.mass},{"dormant",pop.dormant}});
  j["habitats"].push_back({{"id",id},{"star",h.star},{"planet",h.planet},{"cx",h.cx},{"cy",h.cy},{"layer",h.layer},{"biome",h.biome},{"quality",h.quality},{"pollution",h.pollution},{"disease",h.disease},{"nutrients",h.nutrients},{"detritus",h.detritus},{"input",h.input},{"harvested",h.harvested},{"sampled",h.sampled},{"transport",h.transport},{"restored",h.restored},{"status",h.status},{"populations",pops}});
 }
 return j;
}
void decodeEcosystems(Economy& e,const J& j,Universe& u){
 if(!j.is_object()||j.at("schema")!=1||!j.at("revision").is_number_integer()||(j.at("revision")!=0&&j.at("revision")!=1))throw std::runtime_error("Unsupported ecosystem schema");
 e.ecosystemRevision=j.at("revision").get<int>();const auto& list=j.at("habitats");if(!list.is_array()||list.size()>32000)throw std::runtime_error("Oversized ecosystem collection");
 static const std::set<std::string> biomes={"Sunlit shelf","Twilight water","Vent oasis","Rift trench","Abyssal plain","Glacial shelf","Open ocean","Reef shallows","Tidal flats","Fault highlands","Basalt terraces","Chemical marsh","Impact regolith","Dune basin","Luminous forest","Spore grassland","Mineral steppe"};
 for(const auto& v:list){auto star=text(v,"star");if(!e.systems.count(star)||u.star(star).id!=star)throw std::runtime_error("Unknown ecosystem star");int planet=integer(v,"planet",0,int(u.planets(star).size())-1),cx=integer(v,"cx",-15625,15625),cy=integer(v,"cy",-15625,15625),layer=integer(v,"layer",0,16);auto biome=text(v,"biome");if(!biomes.count(biome))throw std::runtime_error("Unknown ecosystem biome");
  auto p=u.planet(star,planet);auto h=generateEcosystem(u,p,cx,cy,layer,biome);if(text(v,"id")!=h.id||e.ecosystems.count(h.id)||p.giant)throw std::runtime_error("Invalid ecosystem identity");
  h.quality=n(v,"quality",0,1);h.pollution=n(v,"pollution",0,1);h.disease=n(v,"disease",0,1);h.nutrients=n(v,"nutrients",0,1e7);h.detritus=n(v,"detritus",0,1e7);h.input=n(v,"input",0,1e12);h.harvested=n(v,"harvested",0,1e12);h.sampled=n(v,"sampled",0,1e12);h.transport=n(v,"transport",-1e12,1e12);h.restored=n(v,"restored",0,1e12);h.status=integer(v,"status",0,2);
  const auto& pops=v.at("populations");if(!pops.is_array()||pops.size()!=h.populations.size())throw std::runtime_error("Invalid food web topology");
  for(size_t i=0;i<pops.size();i++){auto& pop=h.populations[i];const auto& x=pops[i];double seed=n(x,"seed",0,4294967295.);if(seed!=pop.seed||!x.at("seed").is_number_integer()||x.at("flora").get<bool>()!=pop.flora||integer(x,"guild",0,GuildCount-1)!=pop.guild)throw std::runtime_error("Food web disagrees with procedural species");pop.mass=n(x,"mass",0,1e7);pop.dormant=n(x,"dormant",0,1e7);if(h.capacity<=0&&(pop.mass>0||pop.dormant>0))throw std::runtime_error("Lifeless habitat contains an invented population");}
  double initial=ecosystemMass(generateEcosystem(u,p,cx,cy,layer,biome)),expected=initial+h.input+h.transport-h.harvested-h.sampled;
  if(std::abs(ecosystemMass(h)-expected)>std::max(.0001,std::abs(expected)*1e-7))throw std::runtime_error("Ecosystem biomass budget is inconsistent");
  e.ecosystems.emplace(h.id,std::move(h));
 }
 for(const auto& s:e.settlements)if(!s.habitat.empty()){auto* h=settlementHabitat(e,s);auto p=u.planet(s.star,s.planet);if(!h||h->star!=s.star||h->planet!=s.planet||ecosystemKey(p,s.position,s.depth,u.region(p,s.position,s.depth).biome)!=s.habitat)throw std::runtime_error("Settlement is attached to the wrong habitat");}
}
}
