#include "biosphere.hpp"
#include "flight/threat.hpp"
namespace hyperion {
Phenotype phenotype(const Contact& c){
 auto h=[&](uint32_t channel){return hyperion::flight::actorHash(c.n,channel);};Phenotype b;b.plan=int(h(1)%6);b.limbs=3+int(h(2)%6);b.segments=4+int(h(3)%5);b.samples=8+int(h(4)%7);b.size=.78+double(h(5)%1000)/2000.;b.hue=double(h(6)%1000)/1000.;b.gait=.65+double(h(7)%1000)/1000.;
 static const char* animals[]={"radial medusoid","ribbon swimmer","frond grazer","plated crawler","colonial shoal","branching filterer"};
 static const char* plants[]={"fan colony","spiral frond","cup colony","jointed spire","puff colony","branching lattice"};
 static const char* niches[]={"filter feeder","grazing symbiont","ambush predator","detritivore","chemosynthetic symbiont"};
 b.body=c.kind=="flora"?plants[b.plan]:animals[b.plan];b.niche=c.feature==6?"chemosynthetic symbiont":c.feature==5?"nursery filter feeder":c.kind=="flora"?"light-harvesting symbiont":niches[c.n%5];return b;
}
std::string biologicalTraits(const Contact& c){auto b=phenotype(c);return " Morphology: "+b.body+" / "+std::to_string(b.limbs)+" primary lobes / "+std::to_string(b.segments)+" segments. Ecological role: "+b.niche+". Pigment and body plan are shared by this regional species.";}
Contact habitatFeature(const Universe& u,const Planet& p,int cx,int cy,bool ocean,double actualY){
 Contact c;if(p.generation<2||p.giant||(ocean&&p.water<.2))return c;
 auto seed=u.hash(cx,cy,int32_t(p.seed),ocean?0x46d2:0x46a2);if(seed%100>=19)return c;
 Random r{seed};double anchorY=ocean?(cy+.5)*130:cy*86+8+r()*70;
 c.pos={cx*(ocean?130.:86.)+8+r()*(ocean?115:70),ocean?actualY:anchorY};
 double referenceDepth=ocean?std::max(55.,u.floorDepth(p,c.pos.x,anchorY)-70):0;
 auto region=u.region(p,{c.pos.x,anchorY},referenceDepth);bool water=region.elevation<u.seaLevel(p);int feature=0;
 double ancient=proc::noise(p.seed^0x51943u,proc::scaled(proc::chart(c.pos.x,anchorY),9.1,8.));
 if(seed%17==0&&ancient>.64)feature=seed%2?7:8;
 else if(ocean){feature=region.geothermal>.34?(region.biomass>.01?6:3):referenceDepth<200&&region.biomass>.02?5:2;}
 else if(region.geothermal>.36)feature=3;
 else if(region.biomass>.1)feature=water?5:4;
 else feature=p.type=="frozen"||region.temperature<235?2:1;
 static const char* names[]={"","Prismatic mineral arches","Cryogenic vault","Fumarole field","Spore grove","Nursery reef","Chemotroph garden","Resonant monolith","Buried derelict"};
 static const char* kinds[]={"","mineral","mineral","vent","flora","life","life","relic","signal"};
 // Underwater vaults are mineral chimneys; ice requires an icy world.
 c.habitatPos={c.pos.x,anchorY};c.habitatDepth=referenceDepth;c.habitatBiome=region.biome;c.feature=feature;c.kind=kinds[feature];c.n=u.hash(int32_t(seed),int32_t(p.seed),feature,331);c.name=names[feature];
 if(ocean&&feature==2)c.name=p.type=="frozen"?"Subglacial mineral vault":"Abyssal mineral chimneys";
 if(ocean)c.depth=std::max(20.,u.floorDepth(p,c.pos.x,actualY)-70);
 c.id=std::string(ocean?"D2:":"F2:")+p.id+":"+std::to_string(cx)+":"+std::to_string(cy);
 c.desc=region.name+" / "+region.biome+". "+c.name+" shaped by "+(feature==3||feature==6?"fault chemistry and geothermal circulation.":feature==4||feature==5?"local moisture, light and substrate.":feature>=7?"an ancient regional anomaly complex.":"erosion, deposition and mineral growth.");
 if(c.kind=="life"||c.kind=="flora")c.desc+=biologicalTraits(c);
 return c;
}
}
