#include "world_objects.hpp"
#include "ground_objects.hpp"
namespace hyperion::flight {
std::vector<hyperion::WorldObject> worldObjects(const Game& game,const Scene& scene){
 std::vector<hyperion::WorldObject> out;
 auto append=[&](WorldObject object){object.integrity=objectIntegrity(game.worldDamage,object.id);out.push_back(std::move(object));};
 for(const auto& port:game.ports){if(port.minor>=0||length(port.position-game.ship.position)>2800)continue;auto* site=scene.settlement(port.settlement);auto n=port.normal,a=unit(cross({1,0,0},n),{0,0,1}),b=unit(cross(a,n));
  auto point=[&](V3 local){return port.position+a*local.x+n*local.y+b*local.z;};
  auto prop=[&](std::string id,int kind,V3 local,V3 half,double hp,int slot=-1,int good=-1){WorldObject o;o.id=id;o.owner=port.settlement.empty()?port.id:port.settlement;o.kind=kind;o.body=port.body;o.position=point(local);o.half=half;o.right=a;o.up=n;o.back=b;o.capacity=hp;o.slot=slot;o.good=good;o.faction=site?site->faction:port.id.rfind("den:",0)==0?0:3;o.scale=std::max({half.x,half.y,half.z});append(o);};
  prop(portObjectId(port.id),PortObject,port.planet>=0?V3{0,-.65,0}:V3{},port.planet>=0?V3{38,1.3,38}:V3{85,85,80},port.planet>=0?800:4200);
  if(port.planet<0){for(int side:{-1,1})prop(propObjectId(port.id,"panel",side+1),PanelObject,{side*131.,0,0},{34,2,18},160,side+1);continue;}
  prop(propObjectId(port.id,"gantry",0),PanelObject,{0,13,53},{21,14,4},220);
  prop(propObjectId(port.id,"terminal",0),PanelObject,{-17,2.8,67},{2.5,2.3,1.7},65);
  prop(propObjectId(port.id,"mast",0),PanelObject,{-43,17,14},{8,17,1},80);
  if(!site)continue;const auto& style=factionStyle(site->faction);auto layout=layoutSettlement(*site);
  for(const auto& facility:layout.facilities){auto local=facility.position;auto radial=unit(point(local)-game.planets[port.planet].centre);auto floor=game.planets[port.planet].centre+radial*(game.planets[port.planet].radius+terrain(game.planets[port.planet],radial)+2.12);auto x=unit(a-radial*dot(a,radial)),z=unit(cross(x,radial));double angle=facility.angle+(facility.ordinal%2?pi:0);x=unit(x*std::cos(angle)+z*std::sin(angle));z=unit(cross(x,radial));
   WorldObject o;o.id=facilityObjectId(site->id,facility.kind,facility.ordinal);o.owner=site->id;o.kind=FacilityObject;o.body=port.planet;o.slot=facility.kind*24+facility.ordinal;o.half=facilityHalf(facility.kind,style,facility.occupants);o.position=floor+radial*o.half.y;o.right=x;o.up=radial;o.back=z;o.capacity=facility.kind==Reactor?620:facility.kind==Solar?100:300;o.scale=std::max(o.half.x,o.half.y);o.faction=site->faction;
   auto hp=site->facilityIntegrity.find(o.slot);o.integrity=hp==site->facilityIntegrity.end()?1:hp->second;out.push_back(o);
  }
  for(int good=0;good<9;good++)for(int c=0;c<cargoSlots(game.worldDamage,port.id,good,site->stock[good]);c++)prop(propObjectId(port.id,"cargo",good*8+c),CargoObject,{-11+(good%3)*11.,3.42+(c/2)*2.44,48+(good/3)*6.+(c%2)*2.6},{3.5,1.2,1.15},45,good*8+c,good);
  for(int i=0;i<14;i++){double angle=i*2*pi/14+.1,rad=62+(i%3)*6;V3 at{std::cos(angle)*rad,0,std::sin(angle)*rad};bool clear=plantingClear(layout,at);if(clear){double height=9.5+(i%4);prop(propObjectId(port.id,"tree",i),PlantObject,at+V3{0,height*.5,0},{2.8,height*.5,2.8},30,i);auto& o=out.back();const auto& planet=game.planets[port.planet];auto radial=unit(point(at)-planet.centre);o.position=planet.centre+radial*(planet.radius+terrain(planet,radial)+2.12+height*.5);o.up=radial;o.right=unit(a-radial*dot(a,radial));o.back=unit(cross(o.right,radial));}}
  for(int i=0;i<10;i++)for(int side:{-1,1})prop(propObjectId(port.id,"beacon",i*2+(side>0)),PanelObject,{side*13.,.35,-45-i*7.},{.3,.35,.3},8,i*2+(side>0));
 }
 for(const auto& organism:scene.organisms){if(length(organism.position-game.ship.position)>1000)continue;WorldObject o;o.id=organism.id;o.habitat=organism.habitat;o.habitatAt=organism.habitatAt;o.habitatBiome=organism.habitatBiome;o.good=organism.flora?3:4;o.population=organism.population;o.mass=organism.mass;o.kind=LifeObject;o.body=game.nearestPlanet();o.position=organism.position+organism.up*organism.size*.8;o.half={organism.size*.85,organism.size,organism.size*.9};o.up=organism.up;o.back=-organism.forward;o.right=unit(cross(o.up,o.back));o.capacity=12+organism.size*14;o.scale=organism.size;o.flora=organism.flora;append(o);}
 if(!game.planets.empty()&&game.altitude()<350){int body=game.nearestPlanet();for(const auto& rock:groundRocks(game,body)){WorldObject o;o.id=rock.id;o.kind=RockObject;o.body=body;o.position=game.planets[body].centre+rock.at+rock.normal*rock.height*.5;o.half={rock.size,rock.height*.6,rock.size};o.right=rock.a;o.up=rock.normal;o.back=rock.b;o.capacity=20+rock.size*rock.height*25;o.scale=std::max(rock.size,rock.height);o.good=rock.type==2?2:0;append(o);}}
 for(int i=0;i<int(game.minorBodies.size());i++){const auto& body=game.minorBodies[i];if(body.destroyed||body.source.kind==MinorKind::Moon||length(body.centre-game.ship.position)>5000)continue;WorldObject o;o.id="body:"+body.source.id;o.kind=MinorObjectKind;o.body=body.source.parent;o.slot=i;o.position=body.centre;o.half={body.source.radius,body.source.radius,body.source.radius};o.capacity=400+body.source.radius*body.source.radius*8;o.scale=body.source.radius;append(o);}
 return out;
}
}
