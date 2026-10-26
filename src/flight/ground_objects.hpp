#pragma once
#include "game.hpp"
namespace hyperion::flight {
inline uint32_t rockHash(int x,int y,int face,double seed){uint32_t h=uint32_t(x)*73856093u^uint32_t(y)*19349663u^uint32_t(face)*83492791u^uint32_t(seed*1300);h^=h>>16;h*=2246822519u;return h^(h>>13);}
inline V3 rockCube(int face,double u,double v){switch(face){case 0:return unit({1,v,-u});case 1:return unit({-1,v,u});case 2:return unit({u,1,-v});case 3:return unit({u,-1,v});case 4:return unit({u,v,1});default:return unit({-u,v,-1});}}
struct GroundRock {std::string id;V3 at,normal,a,b;double size,height,j,k;int type,detail;};
inline std::vector<GroundRock> groundRocks(const Game& g,int body){
 const auto& p=g.planets[body];auto n=unit(g.ship.position-p.centre);std::vector<GroundRock> out;
 for(int face=0;face<6;face++){
  double den=dot(n,rockCube(face,0,0));if(den<.35)continue;double u=0,v=0;
  switch(face){case 0:u=-n.z/den;v=n.y/den;break;case 1:u=n.z/den;v=n.y/den;break;case 2:u=n.x/den;v=-n.z/den;break;case 3:u=n.x/den;v=n.z/den;break;case 4:u=n.x/den;v=n.y/den;break;default:u=-n.x/den;v=n.y/den;break;}
  int cx=std::clamp(int((u+1)*64),0,127),cy=std::clamp(int((v+1)*64),0,127),reach=std::min(128,int(std::ceil(175*64/(p.radius*den*den))));
  for(int y=std::max(0,cy-reach);y<=std::min(127,cy+reach);y++)for(int x=std::max(0,cx-reach);x<=std::min(127,cx+reach);x++){
   auto h=rockHash(x,y,face,p.seed);double j=(h&65535)/65535.,k=(h>>16)/65535.;auto d=rockCube(face,(x+.2+j*.6)/64-1,(y+.2+k*.6)/64-1);
   if(length(d-n)*p.radius>145||materialElevation(p,d)<.28+.44*p.water)continue;auto at=d*(p.radius+terrain(p,d)+2);
   if(std::any_of(g.ports.begin(),g.ports.end(),[&](const auto& port){return port.planet==body&&length(p.centre+at-port.position)<65;}))continue;
   auto a=unit(cross({0,1,0},d),{1,0,0}),b=unit(cross(d,a));double size=.3+j*1.2,height=.25+k*.9;
   bool formation=h%11==0,crystal=p.kind==2&&h%5==0,column=p.kind==3&&h%7==0;
   if(formation){size*=2.5;height*=1.7;}if(p.kind==1){size*=1.35;height*=.7;}if(crystal){size*=.7;height=1.8+k*3.2;}if(column){size*=.8;height=1.4+k*2.6;}
   auto id="rock:"+g.star+":"+std::to_string(body)+":"+std::to_string(face)+":"+std::to_string(x)+":"+std::to_string(y);
   out.push_back({id,at,d,a,b,size,height,j,k,crystal?2:column?3:p.kind==1?1:0,formation||crystal||column?2:1});
   if(h%4==0){double angle=j*13.7;auto smallD=unit(at+(a*std::cos(angle)+b*std::sin(angle))*(size*1.6+.45)),smallAt=smallD*(p.radius+terrain(p,smallD)+2);
    bool clear=std::none_of(g.ports.begin(),g.ports.end(),[&](const auto& port){return port.planet==body&&length(p.centre+smallAt-port.position)<60;});
    if(clear&&materialElevation(p,smallD)>.28+.44*p.water)out.push_back({id+":small",smallAt,smallD,a,b,.16+j*.24,.11+k*.22,k,j,0,0});
   }
  }
 }return out;
}
}
