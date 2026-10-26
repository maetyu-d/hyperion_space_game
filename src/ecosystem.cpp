#include "ecosystem.hpp"
#include "economy.hpp"
#include "biosphere.hpp"
#include "flight/threat.hpp"
#include <sstream>
#include <iomanip>
#include <stdexcept>
namespace hyperion {
namespace {
constexpr double cellSize=640;
int layerAt(double depth){return depth<=0?0:1+int(depth/1000);}
std::string key(const Planet& p,int x,int y,int layer,const std::string& biome){return p.id+"@"+std::to_string(x)+","+std::to_string(y)+","+std::to_string(layer)+":"+biome;}
V2 address(const Contact& c){return c.habitatDepth>=0?c.habitatPos:c.pos;}
double depthOf(const Contact& c){return c.habitatDepth>=0?c.habitatDepth:c.depth;}
double ratio(double a,double b){return b>1e-9?clamp(a/b,0.,1.):0.;}
bool target(const Population& p,int t){return t==1?(p.guild==Producer||p.guild==Chemotroph):t==2?(p.guild==Grazer||p.guild==FilterFeeder):t==3?p.guild==Predator:p.guild!=Predator&&p.guild!=Decomposer;}
double floorMass(const Population& p,int policy){return p.capacity*(policy==2?.015:.38);}
bool eats(int predator,int prey){return predator==Grazer?(prey==Producer||prey==Chemotroph):predator==FilterFeeder?(prey==Producer||prey==Chemotroph):predator==Predator?(prey==Grazer||prey==FilterFeeder||prey==Decomposer):false;}
void ecologyStep(Ecosystem& h,const Planet& planet,double hour,double dt){
 if(h.capacity<=0)return;
 double phase=double(planet.seed%1000)*.01,day=.5+.5*std::sin(hour*2*pi/std::max(12,planet.period)+phase+h.cx*1.28);
 double light=h.layer?std::exp(-((h.layer-.5)*1000)*.0018):.35+.65*day;
 double season=.8+.2*std::sin(hour*.012+phase),stress=h.pollution*.025+h.disease*.005;
 h.quality=clamp(h.quality+dt*(.0012*(1-h.quality)-h.pollution*.004),0.,1.);
 double added=std::min(h.capacity*.0015*dt,std::max(0.,h.capacity*2-h.nutrients));h.nutrients+=added;h.input+=added;
 double recycled=std::min(h.detritus,h.detritus*(.004+.035*ratio(guildMass(h,Decomposer),guildCapacity(h,Decomposer)*.55))*dt);h.detritus-=recycled;h.nutrients+=recycled;
 // Producers fix available nutrients using light or vent energy. Dormant
 // propagules are finite biomass; neither recolonization nor restoration spawns it.
 for(auto& p:h.populations){
  double viable=p.capacity*h.quality,headroom=clamp(1-p.mass/std::max(.001,viable),0.,1.);
  if(p.guild==Producer||p.guild==Chemotroph){double energy=p.guild==Producer?light:(h.layer?.85:.32);
   double growth=std::min(h.nutrients,p.mass*.055*headroom*energy*season*(1-h.pollution)*dt);h.nutrients-=growth;p.mass+=growth;}
  if(h.pollution<.4&&h.quality>.3&&p.dormant>0&&headroom>.1){bool food=p.guild==Producer||p.guild==Chemotroph||(p.guild==Decomposer&&h.detritus>1);for(const auto& prey:h.populations)if(eats(p.guild,prey.guild)&&prey.mass>prey.capacity*.1)food=true;
   if(food){double hatch=std::min(p.dormant,p.dormant*.008*dt);p.dormant-=hatch;p.mass+=hatch;}}
 }
 // All feeding demand is computed from the same snapshot, then scaled by
 // actual prey stock, avoiding iteration-order starvation and negative prey.
 size_t n=h.populations.size();std::vector<double> demand(n*n,0),preyDemand(n,0),food(n,0),delta(n,0);
 for(size_t i=0;i<n;i++){const auto& a=h.populations[i];if(a.guild==Producer||a.guild==Chemotroph)continue;
  double budget=a.mass*.075*dt,edible=0;for(const auto& b:h.populations)if(eats(a.guild,b.guild))edible+=b.mass;
  if(a.guild==Decomposer||a.guild==FilterFeeder)edible+=h.detritus*(a.guild==FilterFeeder?.3:1);
  if(edible<=1e-9)continue;
  for(size_t j=0;j<n;j++)if(eats(a.guild,h.populations[j].guild)){double portion=budget*h.populations[j].mass/edible;demand[i*n+j]=portion;preyDemand[j]+=portion;}
  if(a.guild==Decomposer||a.guild==FilterFeeder){double portion=std::min(h.detritus,budget*h.detritus*(a.guild==FilterFeeder?.3:1)/edible);h.detritus-=portion;food[i]+=portion;}
 }
 for(size_t j=0;j<n;j++){double scale=preyDemand[j]>0?std::min(1.,h.populations[j].mass/preyDemand[j]):0;for(size_t i=0;i<n;i++){double consumed=demand[i*n+j]*scale;delta[j]-=consumed;food[i]+=consumed;}}
 for(size_t i=0;i<n;i++){auto& p=h.populations[i];double headroom=clamp(1-p.mass/std::max(.001,p.capacity*h.quality),0.,1.);double gain=food[i]*.34*headroom;delta[i]+=gain;h.detritus+=food[i]-gain;}
 for(size_t i=0;i<n;i++){auto& p=h.populations[i];p.mass=std::max(0.,p.mass+delta[i]);bool producer=p.guild==Producer||p.guild==Chemotroph;double hunger=producer?0:1-ratio(food[i],p.mass*.04*dt);
  double mortality=std::min(p.mass,p.mass*(.003+stress+hunger*.012+std::max(0.,p.mass-p.capacity*h.quality)/std::max(1.,p.capacity)*.03)*dt);p.mass-=mortality;h.detritus+=mortality;
  double seedLoss=std::min(p.dormant,p.dormant*h.pollution*.007*dt);p.dormant-=seedLoss;h.detritus+=seedLoss;
  if(p.mass<.00001){h.detritus+=p.mass;p.mass=0;}
 }
 double filtering=ratio(guildMass(h,FilterFeeder),guildCapacity(h,FilterFeeder)*.55),recycling=ratio(guildMass(h,Decomposer),guildCapacity(h,Decomposer)*.55);
 double risk=clamp(h.pollution*.55+ratio(h.detritus,h.capacity)*.4-filtering*.2-recycling*.1,0.,1.);h.disease=clamp(h.disease+dt*(risk-h.disease)*.035,0.,1.);
 h.pollution=std::max(0.,h.pollution-dt*(.0008+filtering*.002));
}
}
const char* guildName(int g){static const char* n[]={"Producers","Grazers","Filter feeders","Predators","Decomposers","Chemotrophs"};return n[clamp(g,0,GuildCount-1)];}
const char* harvestPolicyName(int p){static const char* n[]={"Managed harvest","Sanctuary","Extraction"};return n[clamp(p,0,2)];}
const char* harvestTargetName(int p){static const char* n[]={"Mixed biomass","Vegetation","Grazers and filters","Predators"};return n[clamp(p,0,3)];}
int contactGuild(const Contact& c){static const int g[]={FilterFeeder,Grazer,Predator,Decomposer,Chemotroph};return c.feature==6?Chemotroph:c.feature==5?FilterFeeder:c.kind=="flora"?Producer:g[c.n%5];}
std::string ecosystemKey(const Planet& p,V2 at,double depth,const std::string& biome){return key(p,int(std::floor(at.x/cellSize)),int(std::floor(at.y/cellSize)),layerAt(depth),biome);}
Ecosystem generateEcosystem(const Universe& u,const Planet& p,int cx,int cy,int layer,const std::string& biome){
 Ecosystem h;h.id=key(p,cx,cy,layer,biome);h.star=p.starId;h.planet=p.index;h.cx=cx;h.cy=cy;h.layer=layer;h.biome=biome;
 double depth=layer?(layer-.5)*1000:0;auto region=u.region(p,{(cx+.5)*cellSize,(cy+.5)*cellSize},depth);
 h.capacity=p.life>0&&!p.giant?std::max(12.,region.biomass*950+p.life*80):0;h.nutrients=h.capacity*1.6;h.detritus=h.capacity*.2;
 std::array<int,GuildCount> counts{};
 for(int i=0;i<5;i++){uint32_t seed=proc::hash(p.seed,int(stringHash(biome)),layer?layer-1:0,i);h.populations.push_back({seed,true,Producer});counts[Producer]++;Contact c;c.n=seed;c.kind="life";int g=contactGuild(c);h.populations.push_back({seed,false,g});counts[g]++;}
 for(int g=1;g<GuildCount;g++)if(!counts[g]){uint32_t seed=u.hash(int32_t(p.seed),int32_t(stringHash(biome)),layer,g+337);while(seed%5!=uint32_t(g==FilterFeeder?0:g==Grazer?1:g==Predator?2:g==Decomposer?3:4))seed=hyperion::flight::actorHash(seed,17);h.populations.push_back({seed,false,g});counts[g]++;}
 const double shares[]={.58,.16,.12,.045,.12,.16};for(auto& pop:h.populations){pop.capacity=h.capacity*shares[pop.guild]/counts[pop.guild];pop.mass=pop.capacity*.55;pop.dormant=pop.capacity*.025;}
 return h;
}
Ecosystem& ensureEcosystem(Economy& e,const Universe& u,const Planet& p,V2 at,double depth,const std::string& biome){
 auto name=biome.empty()?u.region(p,at,depth).biome:biome;auto id=ecosystemKey(p,at,depth,name);auto it=e.ecosystems.find(id);if(it!=e.ecosystems.end())return it->second;
 if(e.ecosystems.size()>=32000)throw std::runtime_error("Regional ecosystem catalogue is full");auto h=generateEcosystem(u,p,int(std::floor(at.x/cellSize)),int(std::floor(at.y/cellSize)),layerAt(depth),name);return e.ecosystems.emplace(id,std::move(h)).first->second;
}
Ecosystem& ensureContactHabitat(Economy& e,const Universe& u,const Planet& p,const Contact& c){return ensureEcosystem(e,u,p,address(c),depthOf(c),c.habitatBiome);}
const Ecosystem* contactHabitat(const Economy& e,const Universe& u,const Planet& p,const Contact& c){auto at=address(c);auto depth=depthOf(c);auto biome=c.habitatBiome.empty()?u.region(p,at,depth).biome:c.habitatBiome;auto it=e.ecosystems.find(ecosystemKey(p,at,depth,biome));return it==e.ecosystems.end()?nullptr:&it->second;}
Ecosystem* settlementHabitat(Economy& e,const Settlement& s){auto it=e.ecosystems.find(s.habitat);return it==e.ecosystems.end()?nullptr:&it->second;}
const Ecosystem* settlementHabitat(const Economy& e,const Settlement& s){auto it=e.ecosystems.find(s.habitat);return it==e.ecosystems.end()?nullptr:&it->second;}
void initializeEcosystems(Economy& e,Universe& u){
 bool legacy=e.ecosystemRevision==0;std::map<std::string,double> oldHealth;
 for(auto& s:e.settlements)if(s.habitat.empty()){
  auto p=u.planet(s.star,s.planet);auto& h=ensureEcosystem(e,u,p,s.position,s.depth);s.habitat=h.id;
  if(legacy){double health=clamp(s.ecosystem/std::max(1.,80+p.life*520),0.,1.);auto it=oldHealth.find(h.id);if(it==oldHealth.end())oldHealth.emplace(h.id,health);else it->second=std::min(it->second,health);h.pollution=std::max(h.pollution,s.pollution);}
 }
 for(const auto& [id,health]:oldHealth){auto& h=e.ecosystems.at(id);h.quality=health;for(auto& pop:h.populations){double removed=pop.mass*(1-health);pop.mass-=removed;h.detritus+=removed;}h.status=h.capacity<=0?0:h.quality<.25||biodiversity(h)<.18?2:h.pollution>.35||biodiversity(h)<.5?1:0;}
 e.ecosystemRevision=1;
}
double guildMass(const Ecosystem& h,int guild){double total=0;for(const auto& p:h.populations)if(p.guild==guild)total+=p.mass;return total;}
double guildCapacity(const Ecosystem& h,int guild){double total=0;for(const auto& p:h.populations)if(p.guild==guild)total+=p.capacity;return total;}
double ecosystemMass(const Ecosystem& h){double total=h.nutrients+h.detritus;for(const auto& p:h.populations)total+=p.mass+p.dormant;return total;}
double biodiversity(const Ecosystem& h){if(h.capacity<=0)return 0;double total=0;for(const auto& p:h.populations)total+=ratio(p.mass,p.capacity*.45);return total/h.populations.size();}
double ecosystemServices(const Ecosystem& h){return h.quality*(.45*ratio(guildMass(h,Producer)+guildMass(h,Chemotroph),(guildCapacity(h,Producer)+guildCapacity(h,Chemotroph))*.5)+.3*ratio(guildMass(h,Predator),guildCapacity(h,Predator)*.45)+.25*ratio(guildMass(h,Decomposer),guildCapacity(h,Decomposer)*.5));}
double harvestAvailable(const Ecosystem& h,int policy,int focus){if(policy==1)return 0;double total=0;for(const auto& p:h.populations)if(target(p,focus))total+=std::max(0.,p.mass-floorMass(p,policy));return total;}
double harvestEcosystem(Ecosystem& h,double wanted,int policy,int focus){double available=harvestAvailable(h,policy,focus),taken=std::min(std::max(0.,wanted),available);if(available>0)for(auto& p:h.populations)if(target(p,focus))p.mass-=taken*std::max(0.,p.mass-floorMass(p,policy))/available;h.harvested+=taken;return taken;}
double populationPresence(const Ecosystem& h,const Contact& c){int guild=contactGuild(c);for(const auto& p:h.populations)if(p.seed==c.n&&p.flora==(c.kind=="flora"))return ratio(p.mass,p.capacity*.5);return ratio(guildMass(h,guild),guildCapacity(h,guild)*.5);}
double samplePopulation(Ecosystem& h,const Contact& c,double wanted){
 if(h.quality<.2)return 0;double taken=0;for(auto& p:h.populations)if(p.seed==c.n&&p.flora==(c.kind=="flora")){taken=std::min(std::max(0.,wanted),std::floor(std::max(0.,p.mass-p.capacity*.38)));p.mass-=taken;h.sampled+=taken;return taken;}
 // A landmark is an assemblage of local taxa, sharing the finite guild stock.
 int guild=contactGuild(c);double available=0;for(const auto& p:h.populations)if(p.guild==guild)available+=std::max(0.,p.mass-p.capacity*.38);taken=std::min(std::max(0.,wanted),std::floor(available));if(available>0)for(auto& p:h.populations)if(p.guild==guild)p.mass-=taken*std::max(0.,p.mass-p.capacity*.38)/available;h.sampled+=taken;return taken;
}
void restoreEcosystem(Ecosystem& h,double effort){h.quality=std::min(1.,h.quality+effort*.02);h.pollution=std::max(0.,h.pollution-effort*.04);h.disease=std::max(0.,h.disease-effort*.015);h.restored+=effort;for(auto& p:h.populations){double seed=std::min(p.dormant,std::max(0.,p.capacity*.02-p.mass));p.dormant-=seed;p.mass+=seed;}}
void advanceEcosystems(Economy& e,Universe& u,double dt){
 std::map<std::string,std::pair<double,double>> exposureByHabitat;for(const auto& s:e.settlements){auto& v=exposureByHabitat[s.habitat];v.first=std::max(v.first,s.pollution);if(s.residents)v.second+=s.built[Mine]+s.built[Refinery]+s.built[FuelPlant];}
 for(auto& [id,h]:e.ecosystems){auto exposureRecord=exposureByHabitat.find(id);double exposure=exposureRecord==exposureByHabitat.end()?0:exposureRecord->second.first,footprint=exposureRecord==exposureByHabitat.end()?0:exposureRecord->second.second;h.quality=std::max(0.,h.quality-dt*.000008*footprint);h.pollution=clamp(h.pollution+dt*.08*(exposure-h.pollution),0.,1.);ecologyStep(h,u.planet(h.star,h.planet),e.hour,dt);}
 // Dispersal is a transfer between already registered neighboring habitats,
 // never a fresh population rolled by revisiting a region.
 std::map<std::string,std::vector<double>> flux;for(const auto& [id,h]:e.ecosystems){auto p=u.planet(h.star,h.planet);for(auto offset:{V2{1,0},V2{0,1}}){auto it=e.ecosystems.find(key(p,h.cx+int(offset.x),h.cy+int(offset.y),h.layer,h.biome));if(it==e.ecosystems.end())continue;const auto& b=it->second;auto& aFlux=flux[id];auto& bFlux=flux[b.id];aFlux.resize(h.populations.size()+1);bFlux.resize(b.populations.size()+1);
  for(size_t i=0;i<h.populations.size();i++){double amount=(h.populations[i].mass-b.populations[i].mass)*.002*dt;aFlux[i]-=amount;bFlux[i]+=amount;}double flow=(h.pollution-b.pollution)*.015*dt;aFlux.back()-=flow;bFlux.back()+=flow;}}
 for(const auto& [id,f]:flux){auto& h=e.ecosystems.at(id);for(size_t i=0;i<h.populations.size();i++){h.populations[i].mass+=f[i];h.transport+=f[i];}h.pollution=clamp(h.pollution+f.back(),0.,1.);}
 for(auto& [id,h]:e.ecosystems){int status=h.capacity<=0?0:h.quality<.25||biodiversity(h)<.18?2:h.pollution>.35||biodiversity(h)<.5?1:0;if(status!=h.status){h.status=status;economicEvent(e,h.biome,status==2?"Regional food web collapsed: wild harvests, ecological crop support and medicine production are impaired.":status==1?"Regional populations are under stress; reduce harvest pressure and runoff before dependent industries fail.":"Regional populations recovered; ecosystem services and managed harvests are returning.");}}
}
std::string ecosystemSummary(const Ecosystem& h){if(h.capacity<=0)return "No indigenous biosphere / imports required";return h.status==2?"Food web collapse":h.status==1?"Populations under pressure":"Functioning food web";}
std::string populationReport(const Ecosystem& h,const Contact& c){std::ostringstream out;out<<" Regional population: "<<guildName(contactGuild(c))<<" / "<<std::fixed<<std::setprecision(0)<<populationPresence(h,c)*100<<"% of reference abundance. "<<ecosystemSummary(h)<<". Harvests and survey samples remove actual living biomass.";return out.str();}
}
