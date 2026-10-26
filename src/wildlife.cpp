#include "wildlife.hpp"
#include "economy.hpp"
#include "flight/threat.hpp"
#include <array>
namespace hyperion {
namespace {
constexpr double tile=64;
double random(uint32_t seed,int channel){return double(hyperion::flight::actorHash(seed,uint32_t(channel)))/4294967296.;}
V2 add(V2 a,V2 b){return {a.x+b.x,a.y+b.y};}V2 sub(V2 a,V2 b){return {a.x-b.x,a.y-b.y};}V2 mul(V2 a,double s){return {a.x*s,a.y*s};}
V2 unit2(V2 v){double l=std::hypot(v.x,v.y);return l>1e-8?mul(v,1/l):V2{1,0};}
V2 blend(V2 a,V2 b,double t){return add(mul(a,1-t),mul(b,t));}
double smooth(double t){t=clamp(t,0.,1.);return t*t*(3-2*t);}
V2 loop(const std::vector<V2>& points,double t){
 double f=t*points.size(),floor=std::floor(f),u=f-floor;int i=int(floor)%int(points.size()),n=int(points.size());auto a=points[(i+n-1)%n],b=points[i],c=points[(i+1)%n],d=points[(i+2)%n];
 auto component=[&](double w,double x,double y,double z){return .5*((2*x)+(-w+y)*u+(2*w-5*x+4*y-z)*u*u+(-w+3*x-3*y+z)*u*u*u);};return {component(a.x,b.x,c.x,d.x),component(a.y,b.y,c.y,d.y)};
}
struct Group {
 std::array<LifeAgent,7> agents;std::array<bool,7> alive{};V2 centre;double phase=0,pollution=0,disturbance=0;int prey=-1;bool ocean=false,hasFood=true,garden=false;const Settlement* port=nullptr;
};
double cycle(const Group& g,double hours){double t=std::fmod(hours*.43+g.phase,1.);return t<0?t+1:t;}
double rest(double t){return smooth((t-.81)/.06)*(1-smooth((t-.96)/.04));}
double chase(double t){return smooth((t-.24)/.12)*(1-smooth((t-.68)/.12));}
V2 disturbed(const Group& g,int slot,V2 p){if(g.disturbance>0){V2 source=g.port?(g.ocean?V2{g.port->position.x,g.port->depth}:g.port->position):g.centre;p=add(p,mul(unit2(sub(g.agents[slot].home,source)),12*g.disturbance));}return p;}
V2 travel(const Group& g,int slot,double t){
 const auto& a=g.agents[slot];V2 home=a.home;
 V2 leaf=g.agents[slot%2].home,away=unit2(sub(leaf,g.agents[4].home));
 if(g.garden&&slot!=4)leaf=blend(leaf,g.ocean?V2{g.port->position.x,g.port->depth}:g.port->position,.35);
 auto escape=add(leaf,mul(away,24)),side=V2{-away.y,away.x};bool predator=g.alive[4]&&slot!=4;
 if(!g.hasFood&&!g.garden)return loop({home,add(home,V2{9,12}),add(home,V2{-9,10})},t);
 auto far=add(home,V2{std::sin(a.phase)*15,std::cos(a.phase)*12});
 V2 p=predator?loop({home,leaf,leaf,add(leaf,mul(side,3)),escape,add(escape,mul(side,9)),far,home},t):loop({home,leaf,leaf,add(leaf,V2{4,3}),far,add(home,V2{-10,8}),home,home},t);
 // A second grazer follows the same travelling group, with a stable formation
 // offset. This is evaluated locally, independent of camera bounds or ordering.
 if(slot==3&&g.alive[2])p=blend(p,add(travel(g,2,t),V2{6+std::sin(a.phase)*2,4+std::cos(a.phase)*2}),.8);
 return p;
}
V2 pose(const Group& g,int slot,double hours){
 const auto& a=g.agents[slot];double t=cycle(g,hours);if(a.flora)return a.home;
 V2 p;if(slot==4){p=loop({a.home,add(a.home,V2{15,8}),add(a.home,V2{0,16}),add(a.home,V2{-14,5})},t);
  if(g.prey>=0){V2 prey=pose(g,g.prey,hours),previous=pose(g,g.prey,hours-.002);auto trailing=mul(unit2(sub(prey,previous)),-7);p=blend(p,add(prey,trailing),chase(t)*.88);}
 }else p=travel(g,slot,t);
 return disturbed(g,slot,blend(p,a.home,rest(t)));
}
V2 reaction(const LifeView& view,const LifeAgent& a,V2 p,double& alarm){
 if(a.flora||view.alert<=0)return p;double vertical=view.ocean?.2:1;V2 observer{view.observer.x,view.ocean?view.observerDepth:view.observer.y},offset=sub(p,observer);offset.y*=vertical;
 double radius=22+random(a.contact.n,91)*18,pressure=smooth(1-std::hypot(offset.x,offset.y)/radius)*clamp(view.alert,0.,1.);alarm=std::max(alarm,pressure);auto direction=unit2(offset);direction.y/=vertical;return add(p,mul(direction,pressure*(10+random(a.contact.n,92)*8)));
}
}
const char* lifeBehaviourName(int b){static const char* names[]={"Rooted colony","Roaming","Grazing","Hunting","Fleeing","Filter feeding","Recycling detritus","Foraging by gardens","Avoiding disturbance","Resting","Moving with group","Evading expedition"};return names[clamp(b,0,11)];}
void registerLifeHabitats(Economy& e,const Universe& u,const Planet& p,V2 centre,double depth){
 if(p.giant)return;int cx=int(std::floor(centre.x/64)),cy=int(std::floor((depth>0?depth:centre.y)/(depth>0?160:64)));
 for(int x=cx-2;x<=cx+2;x++)for(int y=cy-2;y<=cy+2;y++){
  V2 at{(x+.5)*64,depth>0?(std::floor(centre.y/130)+.5)*130:(y+.5)*64};double d=depth>0?std::max(20.,(y+.5)*160):0;
  if(depth>0&&d>=u.floorDepth(p,at.x,at.y)-30)continue;ensureEcosystem(e,u,p,at,d);
 }
}
std::vector<LifeAgent> visibleLife(const Economy& e,const Universe& u,const Planet& p,const LifeView& view){
 std::vector<LifeAgent> result;if(p.giant||p.life<=0||!std::isfinite(view.centre.x)||!std::isfinite(view.centre.y)||std::abs(view.centre.x)>1e7||std::abs(view.centre.y)>1e7||!std::isfinite(view.depth)||!std::isfinite(view.vertical)||!std::isfinite(view.hours)||!std::isfinite(view.radius)||!std::isfinite(view.alert)||!std::isfinite(view.observer.x)||!std::isfinite(view.observer.y)||!std::isfinite(view.observerDepth)||view.radius<=0||view.limit==0)return result;
 double radius=clamp(view.radius,20.,480.);int cx=int(std::floor(view.centre.x/tile)),cy=int(std::floor((view.ocean?view.depth:view.centre.y)/(view.ocean?160:tile))),rx=int(std::ceil(radius/tile))+1,ry=view.ocean?int(std::ceil(std::min(1600.,view.vertical)/160))+1:rx;
 std::map<std::string,Ecosystem> baseline;
 for(int x=cx-rx;x<=cx+rx;x++)for(int y=cy-ry;y<=cy+ry;y++){
  Group g;g.ocean=view.ocean;g.centre={(x+.5)*tile,(y+.5)*(view.ocean?160:tile)};V2 chart{g.centre.x,view.ocean?(std::floor(view.centre.y/130)+.5)*130:g.centre.y};double depth=view.ocean?g.centre.y:0;
  if(view.ocean&&(depth<20||depth>u.floorDepth(p,chart.x,chart.y)-30))continue;
  if(std::abs(g.centre.x-view.centre.x)>radius+48||(!view.ocean&&std::abs(g.centre.y-view.centre.y)>radius+48))continue;
  auto region=u.region(p,chart,depth);bool wet=view.ocean||region.elevation<u.seaLevel(p);if(region.biomass<(view.ocean?.005:.018))continue;
  auto id=ecosystemKey(p,chart,depth,region.biome);const Ecosystem* h=nullptr;auto it=e.ecosystems.find(id);
  if(it!=e.ecosystems.end())h=&it->second;else{auto base=baseline.find(id);if(base==baseline.end())base=baseline.emplace(id,generateEcosystem(u,p,int(std::floor(chart.x/640)),int(std::floor(chart.y/640)),depth>0?1+int(depth/1000):0,region.biome)).first;h=&base->second;}
  if(h->capacity<=0)continue;g.hasFood=guildMass(*h,Producer)+guildMass(*h,Chemotroph)>.01;g.pollution=h->pollution;uint32_t seed=u.hash(x,y,int32_t(p.seed),view.ocean?0x331b:0x331a);g.phase=random(seed,1);
  double closest=180;for(const auto& port:e.settlements)if(port.star==p.starId&&port.planet==p.index&&port.residents>0&&(port.depth>0)==view.ocean){double d=std::hypot(port.position.x-chart.x,port.position.y-chart.y)+std::abs(port.depth-depth)*.12;if(d<closest){closest=d;g.port=&port;g.pollution=std::max(h->pollution,port.pollution);}}
  bool harvesting=g.port&&g.port->activity[Harvester]>.25&&g.port->harvestPolicy==2;g.disturbance=std::max(g.pollution>.5?smooth((g.pollution-.4)*2):0.,harvesting?.85:0.);g.garden=g.port&&workingBuildings(*g.port,Growhouse)>0&&g.port->stock[Rations]>0&&g.pollution<.45&&!harvesting;
  for(int slot=0;slot<7;slot++){
   auto& a=g.agents[slot];a.flora=slot<2||slot==6;a.guild=a.flora?(slot==6?Chemotroph:Producer):slot==4?Predator:slot==5?(wet?FilterFeeder:Decomposer):Grazer;
   if(slot==6&&region.geothermal<.25)continue;
   std::vector<const Population*> candidates;for(const auto& pop:h->populations)if(pop.guild==a.guild)candidates.push_back(&pop);if(candidates.empty())continue;
   const auto& pop=*candidates[hyperion::flight::actorHash(seed,uint32_t(slot+3))%candidates.size()];a.presence=clamp(pop.mass/std::max(.001,pop.capacity*.5),0.,1.);
   if(pop.mass<.01||a.presence<random(seed,slot+21)*.94||h->quality<.02)continue;
   a.phase=random(seed,slot+41)*tau;a.home={g.centre.x+(random(seed,slot+51)-.5)*28,g.centre.y+(random(seed,slot+61)-.5)*(view.ocean?70:28)};
   V2 homeChart{a.home.x,view.ocean?view.centre.y:a.home.y};if(!view.ocean&&(u.elevation(p,homeChart.x,homeChart.y)<u.seaLevel(p))!=wet)continue;
   a.marine=wet;a.contact.kind=pop.flora?"flora":"life";a.contact.n=pop.seed;a.contact.name=objectName(a.contact.kind,pop.seed);a.contact.habitatPos=chart;a.contact.habitatDepth=depth;a.contact.habitatBiome=region.biome;
   a.contact.id="BIO:"+h->id+":"+std::to_string(pop.seed)+":"+(pop.flora?"P":"A");a.contact.desc=region.biome+". "+biologicalTraits(a.contact)+" This moving cohort belongs to the persistent regional "+guildName(a.guild)+" population.";
   a.id="L1:"+p.id+":"+std::to_string(x)+":"+std::to_string(y)+":"+std::to_string(slot)+(view.ocean?":O:"+std::to_string(int(std::floor(view.centre.y/130))):":S");if(objectIntegrity(e.worldDamage,a.id)<=0)continue;auto bio=phenotype(a.contact);a.size=bio.size*(a.flora?1.5:slot==4?1.65:1.0);if(g.port)a.settlement=g.port->id;g.alive[slot]=true;
  }
  // Missing visible plants may still exist below the render density threshold.
  for(int slot=0;slot<2;slot++)if(!g.alive[slot])g.agents[slot].home=add(g.centre,V2{slot?9.:-9.,-5});
  if(!g.alive[4])g.agents[4].home=add(g.centre,V2{17,14});
  g.prey=g.alive[2]?2:g.alive[3]?3:-1;double t=cycle(g,view.hours);
  std::array<V2,7> positions,nextPositions;std::array<double,7> alarms{};
  auto constrain=[&](int slot,V2 at){if(view.ocean){at.y=clamp(at.y,20.,u.floorDepth(p,at.x,view.centre.y)-30);return at;}auto allowed=[&](V2 q){return (u.elevation(p,q.x,q.y)<u.seaLevel(p))==wet;};if(!allowed(at)){V2 lo=g.agents[slot].home,hi=at;for(int n=0;n<7;n++){auto mid=blend(lo,hi,.5);if(allowed(mid))lo=mid;else hi=mid;}at=lo;}return at;};
  // Resolve the whole local group before assigning targets. Hunters then see
  // the prey's actual shoreline/observer-adjusted pose, not an unclipped path.
  for(int slot=0;slot<7;slot++)if(g.alive[slot]){positions[slot]=constrain(slot,reaction(view,g.agents[slot],pose(g,slot,view.hours),alarms[slot]));nextPositions[slot]=constrain(slot,reaction(view,g.agents[slot],pose(g,slot,view.hours+.0005),alarms[slot]));}
  for(int slot=0;slot<7;slot++)if(g.alive[slot]){
   auto a=g.agents[slot];a.position=positions[slot];a.velocity=mul(sub(nextPositions[slot],a.position),2000);a.heading=std::atan2(a.velocity.y,a.velocity.x);
   int foodSlot=slot==4?g.prey:slot%2;a.food=foodSlot>=0?g.agents[foodSlot].home:a.home;a.target=foodSlot>=0?g.agents[foodSlot].id:std::string{};
   if(a.flora){a.behaviour=Rooted;a.activity=.15;}
   else if(alarms[slot]>.08){a.behaviour=Startled;a.activity=1;a.target="observer";V2 observer{view.observer.x,view.ocean?view.observerDepth:view.observer.y};a.heading=std::atan2(a.position.y-observer.y,a.position.x-observer.x);}
   else if(g.disturbance>0){a.behaviour=Avoiding;a.activity=.9;}
   else if(rest(t)>.999){a.behaviour=Resting;a.activity=.04;}
   else if(slot==4){a.behaviour=g.prey>=0&&chase(t)>.45?Hunting:Roaming;a.activity=a.behaviour==Hunting?1:.35;if(g.prey>=0){a.food=positions[g.prey];if(a.behaviour==Hunting)a.heading=std::atan2(a.food.y-a.position.y,a.food.x-a.position.x);}}
   else if(g.alive[4]&&chase(t)>.3&&distance(a.position,positions[4])<30){a.behaviour=Fleeing;a.activity=1;}
   else if(slot==5){a.behaviour=wet?Filtering:Recycling;a.activity=.5;}
   else{bool feeding=(g.hasFood||g.garden)&&t>.12&&t<.42;a.behaviour=feeding?(g.garden?GardenForaging:Grazing):slot==3&&g.alive[2]?Following:Roaming;a.activity=feeding?.75:.4;if(a.behaviour==Following){a.target=g.agents[2].id;a.food=positions[2];}}
   if(a.behaviour==GardenForaging&&g.port){a.food=blend(a.food,g.ocean?V2{g.port->position.x,g.port->depth}:g.port->position,.35);a.target="garden:"+g.port->id;}
   if(a.behaviour==Grazing||a.behaviour==GardenForaging)a.heading=std::atan2(a.food.y-a.position.y,a.food.x-a.position.x);if(a.flora)a.heading=a.phase;
   if(view.ocean){a.depth=a.position.y;a.foodDepth=clamp(a.food.y,20.,u.floorDepth(p,a.food.x,view.centre.y)-30);a.contact.pos={a.position.x,view.centre.y};a.contact.depth=a.depth;}
   else{a.contact.pos=a.position;a.contact.depth=0;}
   bool near=false;double range=view.ocean?std::hypot(a.position.x-view.centre.x,(a.depth-view.depth)*.2):distance(a.position,view.centre);near=range<=radius;
   if(near)result.push_back(std::move(a));
  }
 }
 std::sort(result.begin(),result.end(),[&](const auto& a,const auto& b){auto dist=[&](const auto& n){return view.ocean?std::hypot(n.position.x-view.centre.x,(n.depth-view.depth)*.2):distance(n.position,view.centre);};double da=dist(a),db=dist(b);return da==db?a.id<b.id:da<db;});
 if(result.size()>view.limit)result.resize(view.limit);return result;
}
}
