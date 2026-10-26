#pragma once
#include "universe.hpp"
namespace hyperion {
struct Economy;struct Settlement;
enum Guild {Producer,Grazer,FilterFeeder,Predator,Decomposer,Chemotroph,GuildCount};
struct Population {uint32_t seed=0;bool flora=false;int guild=0;double mass=0,dormant=0,capacity=0;};
struct Ecosystem {
 std::string id,star,biome;int planet=0,cx=0,cy=0,layer=0,status=0;
 double capacity=0,quality=1,pollution=0,disease=0,nutrients=0,detritus=0;
 // Carbon budgets: input from environmental fixation, exports to the economy,
 // and signed dispersal between registered neighboring habitats.
 double input=0,harvested=0,sampled=0,transport=0,restored=0;
 std::vector<Population> populations;
};
const char* guildName(int);const char* harvestPolicyName(int);const char* harvestTargetName(int);
int contactGuild(const Contact&);
std::string ecosystemKey(const Planet&,V2,double,const std::string& biome);
Ecosystem generateEcosystem(const Universe&,const Planet&,int,int,int,const std::string&);
Ecosystem& ensureEcosystem(Economy&,const Universe&,const Planet&,V2,double,const std::string& biome="");
Ecosystem& ensureContactHabitat(Economy&,const Universe&,const Planet&,const Contact&);
const Ecosystem* contactHabitat(const Economy&,const Universe&,const Planet&,const Contact&);
Ecosystem* settlementHabitat(Economy&,const Settlement&);
const Ecosystem* settlementHabitat(const Economy&,const Settlement&);
void initializeEcosystems(Economy&,Universe&);
void advanceEcosystems(Economy&,Universe&,double);
void restoreEcosystem(Ecosystem&,double);
double ecosystemMass(const Ecosystem&);double guildMass(const Ecosystem&,int);double guildCapacity(const Ecosystem&,int);
double biodiversity(const Ecosystem&);double ecosystemServices(const Ecosystem&);
double harvestAvailable(const Ecosystem&,int policy,int target=0);
double harvestEcosystem(Ecosystem&,double,int policy,int target=0);
double samplePopulation(Ecosystem&,const Contact&,double);
double populationPresence(const Ecosystem&,const Contact&);
std::string ecosystemSummary(const Ecosystem&);
std::string populationReport(const Ecosystem&,const Contact&);
}
