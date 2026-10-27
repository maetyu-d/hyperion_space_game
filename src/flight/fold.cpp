#include "game.hpp"
namespace hyperion::flight {
bool Game::crossFold(int i){
 if(i<0||i>=int(folds.size())||docked||deployed)return false;
 if(!foldTransitAllowed){ship.position=ship.previous;ship.velocity={};cancelNavigation();message("Fold crossing held / recover boarding party and release wreck tow first");return false;}
 const auto& a=folds[i];if(a.peer<0||a.peer>=int(folds.size()))return false;const auto& b=folds[a.peer];
 ship.position=foldPoint(a,b,ship.position);ship.previous=ship.position;
 ship.velocity=foldVector(a,b,ship.velocity);ship.forward=unit(foldVector(a,b,ship.forward));ship.up=unit(foldVector(a,b,ship.up));
 ship.angularVelocity=foldVector(a,b,ship.angularVelocity);ship.assistCapture=foldVector(a,b,ship.assistCapture);acceleration=foldVector(a,b,acceleration);
 // Navigation ends at a mouth; it never draws a straight chord across a fold.
 cancelNavigation();launchAssist=0;ship.gear=false;dockedPort=-1;savedDock.clear();combatTarget.clear();lock=0;
 int destination=0;for(int p=0;p<int(ports.size());p++)if(ports[p].fold==a.peer){destination=p;break;}target=destination;
 Event event{"foldTransit",i,a.authority,a.link,"NOMA"};event.position=ship.position;events.push_back(event);
 message("Fold transit / "+b.name+" / momentum and attitude retained");return true;
}
void Game::foldContacts(){
 double t;int gate=firstFold(folds,ship.previous,ship.position,t,3);
 if(gate>=0&&!docked&&!deployed)crossFold(gate);
}
void Game::foldTraffic(Traffic& t){
 double fraction;int i=firstFold(folds,t.previous,t.position,fraction,t.kind==1?enemyDef(t).radius:5);
 if(i<0)return;const auto& a=folds[i];const auto& b=folds[a.peer];
 t.position=foldPoint(a,b,t.position);t.previous=t.position;t.velocity=foldVector(a,b,t.velocity);t.forward=unit(foldVector(a,b,t.forward));
}
V3 Game::apparentPosition(V3 from,V3 to,int* through)const{
 V3 result=to;double best=length(to-from);if(through)*through=-1;
 for(int i=0;i<int(folds.size());i++){const auto& gate=folds[i];if(gate.peer<0||gate.peer>=int(folds.size()))continue;
  const auto& exit=folds[gate.peer];auto image=foldPoint(exit,gate,to);double range=length(image-from);if(range>=best)continue;
  double fraction=foldIntersection(gate,from,image);if(!std::isfinite(fraction))continue;auto at=mix(from,image,fraction);
  if(!lineOfSight(from,at)||!lineOfSight(foldPoint(gate,exit,at)+exit.normal*.001,to))continue;
  best=range;result=image;if(through)*through=i;
 }return result;
}
V3 Game::apparentVelocity(int gate,V3 velocity)const{
 return gate>=0&&gate<int(folds.size())?foldVector(folds[folds[gate].peer],folds[gate],velocity):velocity;
}
bool Game::sighted(V3 from,V3 to)const{int gate;apparentPosition(from,to,&gate);return gate>=0||lineOfSight(from,to);}
}
