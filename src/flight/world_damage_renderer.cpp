#include "renderer.hpp"
#include "rlgl.h"
namespace hyperion::flight {
namespace {Vector3 vec(V3 p){return {float(p.x),float(p.y),float(p.z)};}double random(uint32_t seed,int n){uint32_t h=seed^uint32_t(n+1)*0x9e3779b9u;h^=h>>16;h*=0x7feb352du;h^=h>>15;return double(h>>8)/16777215.;}}
void Renderer::drawWorldDamage(const Game& game){
 drawStructuralRemains(game);
 int chunks=reduced?48:lowGraphics?96:180;
 for(const auto& [id,d]:game.worldDamage){if(d.integrity>0)continue;auto at=d.position;
  if(d.kind==MinorObjectKind||d.kind==ImpactObject)for(const auto& body:game.minorBodies)if("body:"+body.source.id==id||(d.kind==ImpactObject&&d.owner==body.source.id))at=body.centre+unit(d.normal)*body.source.radius;
  double distance=length(at-game.ship.position);if(distance>900||dot(at-game.ship.position,game.ship.forward)<-d.scale*3)continue;
  auto up=unit(d.normal),right=unit(cross(up,{0,0,1}),{1,0,0}),back=unit(cross(right,up));uint32_t seed=sceneHash(id);
  if(d.kind==ImpactObject&&!game.planets.empty()){
   const auto& planet=game.planets[std::clamp(d.body,0,int(game.planets.size())-1)];double radius=d.scale;const MinorObject* minor=nullptr;if(d.owner.rfind("planet:",0)!=0)for(const auto& body:game.minorBodies)if(body.source.id==d.owner&&!body.destroyed){minor=&body;break;}
   auto point=[&,normal=d.normal](double angle,double ring){double bodyRadius=minor?minor->source.radius:planet.radius;auto n=unit(normal+(right*std::cos(angle)+back*std::sin(angle))*(radius*ring/bodyRadius));return minor?minor->centre+n*(minorRadius(minor->source,{n.x,n.y,n.z})+.075):planet.centre+n*(planet.radius+terrain(planet,n)+2.075);};
   rlDisableDepthMask();rlBegin(RL_TRIANGLES);rlColor4ub(23,16,31,180);int segments=reduced?12:28;
   auto centre=point(0,0);for(int i=0;i<segments;i++){auto a=point(i*2*pi/segments,.85+.15*random(seed,i)),b=point((i+1)*2*pi/segments,.85+.15*random(seed,i+1));for(auto p:{centre,a,b,centre,b,a})rlVertex3f(float(p.x),float(p.y),float(p.z));}rlEnd();rlEnableDepthMask();continue;
  }
  if(d.structure.valid||std::any_of(structuralInstances.begin(),structuralInstances.end(),[&id=id](const auto& s){return s.id==id;}))continue;
  if(d.kind==FacilityObject||d.kind==PortObject||d.kind==SalvageObject)continue;
  if(objectIntegrity(game.worldDamage,"wreck:world:"+id)<=0)continue;
  if(d.kind==LifeObject||d.kind==PlantObject){
   if(chunks<=0)continue;chunks--;auto ground=at-up*d.scale*.6;Color bark{53,46,65,255};
   if(d.good==3||d.kind==PlantObject){DrawCylinderEx(vec(ground),vec(ground+right*d.scale*.9+up*.35),float(d.scale*.12+.06),.035f,6,bark);for(int i=0;i<3;i++)DrawSphereEx(vec(ground+right*d.scale*(.2+i*.25)),float(d.scale*.12),4,5,Color{39,65,65,255});}
   else {DrawSphereEx(vec(ground),float(d.scale*.42),5,8,Color{45,38,64,255});DrawCylinderEx(vec(ground),vec(ground-right*d.scale*.75),float(d.scale*.16),.03f,5,bark);}
   continue;
  }
  int count=d.kind==MinorObjectKind?12:d.kind==RockObject?5:4;
  for(int i=0;i<count&&chunks>0;i++,chunks--){double angle=i*2.399963,spread=d.kind==MinorObjectKind?d.scale*1.3:d.scale*.8;auto centre=at+(right*std::cos(angle)+back*std::sin(angle))*spread*(.35+random(seed,i));
   double size=d.kind==MinorObjectKind?d.scale*(.09+.13*random(seed,i+20)):std::clamp(d.scale*.17,.12,2.8);if(d.kind!=MinorObjectKind)centre=centre-up*(d.scale*.4);
   Color tint=d.kind==RockObject||d.kind==MinorObjectKind?Color{74,63,88,255}:Color{37,43,57,255};DrawCubeV(vec(centre),{float(size*1.4),float(size*.6),float(size)},tint);
   if(!reduced&&i%3==0)DrawLine3D(vec(centre+up*size*.3-right*size*.5),vec(centre+up*size*.3+right*size*.5),Color{105,76,133,210});
  }
 }
}
}
