#pragma once
#include "game.hpp"
#include <limits>
namespace hyperion::flight {
inline double worldObjectHit(V3 a,V3 b,const hyperion::WorldObject& object){
 if(object.integrity<=0)return std::numeric_limits<double>::infinity();double enter=0,leave=1;
 auto relative=a-object.position,delta=b-a;
 const V3 axes[]={object.right,object.up,object.back};const double extent[]={object.half.x,object.half.y,object.half.z};
 for(int i=0;i<3;i++){double at=dot(relative,axes[i]),speed=dot(delta,axes[i]);if(std::abs(speed)<1e-10){if(std::abs(at)>extent[i])return std::numeric_limits<double>::infinity();continue;}double lo=(-extent[i]-at)/speed,hi=(extent[i]-at)/speed;if(lo>hi)std::swap(lo,hi);enter=std::max(enter,lo);leave=std::min(leave,hi);if(enter>leave)return std::numeric_limits<double>::infinity();}
 return enter;
}
inline void worldContacts(Game& game){
 if(game.docked||game.launchAssist>0||game.autopilot||game.speed()<1)return;
 int selected=-1;double first=2;for(int i=0;i<int(game.worldObjects.size());i++){auto object=game.worldObjects[i];if(object.kind==hyperion::MinorObjectKind||(object.kind==hyperion::PortObject&&object.body>=0&&object.half.y<3))continue;object.half+=V3{2,2,2};double hit=worldObjectHit(game.ship.previous,game.ship.position,object);if(hit<first){first=hit;selected=i;}}
 if(selected<0)return;const auto object=game.worldObjects[selected];auto at=mix(game.ship.previous,game.ship.position,first),relative=at-object.position;const V3 axes[]={object.right,object.up,object.back};double extent[]={object.half.x+2,object.half.y+2,object.half.z+2};int face=0;for(int i=1;i<3;i++)if(std::abs(dot(relative,axes[i]))/extent[i]>std::abs(dot(relative,axes[face]))/extent[face])face=i;auto normal=axes[face]*(dot(relative,axes[face])<0?-1:1);double impact=std::max(0.,-dot(game.ship.velocity,normal));if(impact<1)return;
 bool soft=object.kind==hyperion::LifeObject||object.kind==hyperion::PlantObject;
 game.damageWorld(selected,std::max(2.,impact*impact*(soft?.4:.075)),CollisionDamage,at,true);
 if(soft){game.ship.velocity=game.ship.velocity*.98;game.damageShip(std::min(1.,impact*.005),CollisionDamage,at);}
 else{game.ship.position=at+normal*.15;game.ship.velocity+=normal*(impact*1.12);game.ship.velocity=game.ship.velocity*.92;game.damageShip(std::max(0.,impact-2)*.65,CollisionDamage,at);game.cancelNavigation();game.ship.throttle=0;game.message("Collision / physical object damaged");}
}
}
