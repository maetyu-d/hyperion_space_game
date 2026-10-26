#include "flight/destruction.hpp"
#include <iostream>
#include <stdexcept>
using namespace hyperion::flight;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
 Traffic source;source.kind=1;source.role=2;source.faction=4;source.forward=unit({.4,.3,-.8});source.genome={1,712345,false};auto hull=hullRemains(source);
 check(hull.valid&&hull.model==craftModel(source)&&hull.seed==source.genome.seed&&hull.faction==4,"Destruction lost vessel identity");
 auto base=debrisPose(hull,0,0);double maxStep=0;
 for(int part=0;part<5;part++){
  auto initial=debrisPose(hull,part,0);check(length(initial.offset)<1e-12&&length(initial.right-base.right)<1e-12,"Fracture does not reassemble at birth");
  auto previous=initial;
  for(int frame=0;frame<1200;frame++){auto pose=debrisPose(hull,part,frame/120.);check(finite(pose.offset)&&finite(pose.right)&&finite(pose.up)&&finite(pose.back),"Nonfinite fragment pose");check(std::abs(length(pose.right)-1)<1e-8&&std::abs(dot(pose.right,pose.up))<1e-8,"Fragment rotation deformed the hull");check(length(pose.offset)<hull.size*2,"Wreckage escaped the salvage envelope");maxStep=std::max(maxStep,length(pose.offset-previous.offset));previous=pose;}
 }
 check(maxStep<hull.size*.025,"Fragment translation has a discontinuity");
 check(hullFragment({-1,0,0})==0&&hullFragment({1,0,0})==1&&hullFragment({0,0,-1})==2&&hullFragment({0,0,1})==3&&hullFragment({0,0,0})==4,"Hull partition changed");
 Game game;game.valid=true;game.docked=false;game.traffic.push_back(source);game.traffic[0].health=1;game.traffic[0].shield=0;game.traffic[0].id="identity";game.traffic[0].velocity={4,2,-3};game.damageTraffic(0,100,Seeker,{0,0,-1},true);
 check(game.wrecks.size()==1&&game.wrecks[0].hull.seed==hull.seed,"Salvage lost its original hull");
 auto explosion=std::find_if(game.effects.begin(),game.effects.end(),[](auto&e){return e.kind==ShipExplosion;});check(explosion!=game.effects.end()&&explosion->hull.model==hull.model&&explosion->actor=="identity","Explosion lost hull snapshot");
 Input input;for(int i=0;i<60;i++)game.prepareCombat(1./60,input);explosion=std::find_if(game.effects.begin(),game.effects.end(),[](auto&e){return e.kind==ShipExplosion;});check(length(explosion->position-game.wrecks[0].position)<1e-8,"Blast and salvage drift separated");
 for(int i=0;i<300;i++)game.prepareCombat(1./60,input);check(game.effects.empty()&&game.wrecks.size()==1,"Fire must expire while salvage persists");
 std::cout<<"PASS: 6000 continuous fragment poses, intact geometry partition, vessel identity, matched drift and finite fire\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
