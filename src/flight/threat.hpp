#pragma once
#include <cstdint>
#include <string>
namespace hyperion::flight {
// Immutable identity; condition, ammunition and political history live elsewhere.
struct EnemyGenome {int revision=0;uint32_t seed=0;bool legacyEquipment=false;};
inline bool operator==(const EnemyGenome& a,const EnemyGenome& b){return a.revision==b.revision&&a.seed==b.seed&&a.legacyEquipment==b.legacyEquipment;}
inline bool operator!=(const EnemyGenome& a,const EnemyGenome& b){return !(a==b);}
uint32_t actorHash(uint32_t seed,uint32_t channel);
double actorTrait(const EnemyGenome&,uint32_t channel);
int hullVariant(const EnemyGenome&);
const char* pilotTemperament(const EnemyGenome&);
std::string vesselDesignation(const EnemyGenome&);
}
