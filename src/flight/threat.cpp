#include "game.hpp"
namespace hyperion::flight {
uint32_t actorHash(uint32_t seed,uint32_t channel){uint32_t x=seed+channel*0x9e3779b9u+0x85ebca6bu;x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);}
double actorTrait(const EnemyGenome& g,uint32_t channel){return double(actorHash(g.seed,channel))/4294967296.;}
int hullVariant(const EnemyGenome& g){return g.revision?1+int(actorHash(g.seed,11)%8):0;}
const char* pilotTemperament(const EnemyGenome& g){static const char* names[]={"Methodical","Skirmisher","Resolute","Opportunist"};return g.revision?names[actorHash(g.seed,12)%4]:"Standard";}
std::string vesselDesignation(const EnemyGenome& g){
 if(!g.revision)return "Standard frame";
 static const char* a[]={"Glass","Violet","Nacre","Cinder","Prism","Moth","Opal","Choir","Cobalt","Echo","Sable","Halo"};
 static const char* b[]={"Kestrel","Needle","Manta","Wraith","Thorn","Lantern","Tern","Reaver","Wisp","Scarab","Crown","Siren"};
 return std::string(a[actorHash(g.seed,21)%12])+" "+b[actorHash(g.seed,22)%12]+" / "+std::to_string(100+actorHash(g.seed,23)%900);
}
EnemyDef enemyDef(int role,const EnemyGenome& g){
 EnemyDef d=enemyDef(role);if(!g.revision)return d;
 int temperament=int(actorHash(g.seed,12)%4);
 d.retreat=temperament==2?.14:temperament==3?.34:.23;
 d.agility=.85+actorTrait(g,2)*.35+(temperament==1?.18:0);
 d.cadence=.88+actorTrait(g,3)*.26;d.reactor=.9+actorTrait(g,4)*.2;d.cooling=.9+actorTrait(g,5)*.25;
 d.standOff=temperament==0?.88:temperament==1?.55:temperament==2?.68:.80;
 d.strafe=temperament==1?.95:temperament==2?.24:.45;d.evades=temperament==1||temperament==3;
 d.aimJitter=.004+actorTrait(g,6)*.014;
 if(!g.legacyEquipment){
  // Frame kits share a finite economic recipe; mass trades speed for armour.
  double mass=.86+actorTrait(g,7)*.28;
  d.hull=std::round(d.hull*mass);d.shield=std::round(d.shield*(.86+actorTrait(g,8)*.28));
  d.speed*=1.08-(mass-.86)*.55;d.radius*=std::cbrt(mass);d.range*=.9+actorTrait(g,9)*.2;
  static const int mounts[4][4]={{Pulse,Kinetic,Pulse,Resonance},{Kinetic,Pulse,Kinetic,Resonance},{Plasma,Plasma,Pulse,Kinetic},{Resonance,Pulse,Resonance,Plasma}};
  d.weapon=mounts[std::clamp(role,0,3)][actorHash(g.seed,10)%4];
 }
 return d;
}
}
