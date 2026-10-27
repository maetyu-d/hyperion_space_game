#include "flight/space.hpp"
#include "game.hpp"
#include <iostream>
#include <stdexcept>
using namespace hyperion;namespace hf=hyperion::flight;
int main(){int checks=0;auto require=[&](bool b,const char* why){checks++;if(!b)throw std::runtime_error(why);};try{
 for(int i=0;i<1200;i++){
  double a=i*.013,t=.1+(i%60)*.05;auto v=hf::unit(hf::V3{std::sin(a),std::cos(a*1.7),std::sin(a*.9)});auto q=hf::nilGeodesic(v,t);auto inverse=hf::nilLog(q);require(hf::length(inverse-v*t)<.00005,"Nil optical inverse disagrees with exact geodesics");
  auto view=hf::spaceView({std::sin(a)*18000,std::cos(a*.3)*800,std::cos(a)*18000},1000+i%7);auto point=view.eye+v*(i%100*390.);auto p=hf::spacePoint(view,point);require(hf::finite(p),"Ordinary-space projection became non-finite");
  require(std::abs(hf::length(hf::spaceSky(view,v))-1)<1e-8,"Curved sky directions are not normalized");
  auto near=view.eye+v*200;require(hf::length(hf::spacePoint(view,near)-near)<1e-8,"Close flight is not an exact flat near field");view.strength=0;require(hf::length(hf::spacePoint(view,point)-point)<1e-8,"Flat audit mode changed an object position");
 }
 auto view=hf::spaceView({4000,1000,9000},42);auto point=view.eye+hf::V3{13000,7000,11000};require(hf::length(hf::spacePoint(view,point)-point)>1000,"Ordinary flight lacks visible curvature away from rifts");
 auto left=hf::spacePoint(view,point-hf::V3{.01,0,0}),right=hf::spacePoint(view,point+hf::V3{.01,0,0});require(hf::length(left-right)<.1,"Optical motion is discontinuous");

 for(int i=0;i<200;i++){
  double w=(i-100)*.00001;auto v=hf::unit(hf::V3{.6,.8,w});auto a=hf::nilGeodesic(v,3.8),b=hf::nilGeodesic(hf::unit(hf::V3{.6,.8,w+.0000001}),3.8);
  require(hf::finite(a)&&hf::length(a-b)<.00001,"Axis-crossing light rays jump or become non-finite");
  auto s=hf::spaceView({std::sin(i*.2)*18000,1000,std::cos(i*.2)*18000},42+i%4);s.strength=(i%4)*.25+.25;
  auto ray=hf::unit(hf::V3{std::cos(i*.31),std::sin(i*.17),std::sin(i*.31)}),source=hf::spaceSky(s,ray);auto image=hf::spaceSkyImage(s,source,hf::unit(ray+hf::V3{.02,-.01,.01}));
  require(image.error<.00001&&hf::length(hf::spaceSky(s,image.ray)-source)<.00001,"Sky-source image inversion misses the visible light source");
 }
 Game game;game.enterFlight();auto position=game.s.flight.ship.position,velocity=game.s.flight.ship.velocity;auto fuel=game.s.fuel,hour=game.s.hours;for(int i=0;i<100;i++)hf::spacePoint(view,point+hf::V3{double(i),0,0});require(hf::length(game.s.flight.ship.position-position)==0&&hf::length(game.s.flight.ship.velocity-velocity)==0&&game.s.fuel==fuel&&game.s.hours==hour,"View calculations changed flight handling or time");
 std::cout<<checks<<" curved-space checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
