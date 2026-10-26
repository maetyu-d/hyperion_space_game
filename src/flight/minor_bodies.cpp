#include "game.hpp"
#include <limits>
namespace hyperion::flight {
void Game::updateMinorBodies(double hours){
 for(auto& b:minorBodies){auto p=minorOrbit(b.source,hours);b.centre={p.x,p.y,p.z};if(b.source.parent>=0&&b.source.parent<int(planets.size()))b.centre+=planets[b.source.parent].centre;}
 for(auto& p:ports)if(p.minor>=0&&p.minor<int(minorBodies.size())){auto& b=minorBodies[p.minor];if(b.source.parent>=0&&b.source.parent<int(planets.size()))p.normal=unit(b.centre-planets[b.source.parent].centre);else{double nearest=1e30;for(const auto& world:planets){double d=length(b.centre-world.centre)-world.radius;if(d<nearest){nearest=d;p.normal=unit(b.centre-world.centre);}}}p.position=b.centre+p.normal*(b.source.radius*1.24+280);}
 if(autopilot&&!route.empty()&&target>=0&&target<int(ports.size())&&ports[target].minor>=0){route.back()=ports[target].position+ports[target].normal*90;if(route.size()==2)route.front()=ports[target].position+ports[target].normal*130;}
}
std::vector<Planet> Game::navigationBodies()const{
 auto result=planets;for(const auto& b:minorBodies){Planet p{};p.centre=b.centre;p.radius=b.source.radius*1.24+4;result.push_back(p);}return result;
}
double Game::minorHit(V3 a,V3 b,double clearance,int* body)const{
 double first=std::numeric_limits<double>::infinity();auto d=b-a;double aa=dot(d,d);
 for(int i=0;i<int(minorBodies.size());i++){const auto& p=minorBodies[i];auto o=a-p.centre;double bound=p.source.radius*1.24+clearance;
  auto field=[&](double t){auto v=o+d*t;auto n=unit(v);return length(v)-minorRadius(p.source,{n.x,n.y,n.z})-clearance;};
  if(dot(o,o)<=bound*bound&&field(0)<=0){first=0;if(body)*body=i;continue;}
  if(aa<1e-14)continue;double bb=dot(o,d),h=bb*bb-aa*(dot(o,o)-bound*bound);if(h<0)continue;
  double lo=std::max(0.,(-bb-std::sqrt(h))/aa),hi=std::min({1.,first,(-bb+std::sqrt(h))/aa});if(lo>hi)continue;
  double previous=lo;for(int k=0;k<=48;k++){double t=lo+(hi-lo)*k/48;if(field(t)<=0){double left=previous,right=t;for(int j=0;j<16;j++){double m=(left+right)*.5;if(field(m)>0)left=m;else right=m;}first=right;if(body)*body=i;break;}previous=t;}
 }
 return first;
}
void Game::minorContacts(V3 previous){
 int index=-1;double hit=minorHit(previous,ship.position,2,&index);if(!std::isfinite(hit)||index<0)return;
 const auto& b=minorBodies[index];auto at=mix(previous,ship.position,hit),n=unit(at-b.centre);
 ship.position=b.centre+n*(minorRadius(b.source,{n.x,n.y,n.z})+2.1);
 double impact=std::max(0.,-dot(ship.velocity,n));ship.velocity+=n*(impact*1.15);ship.velocity=ship.velocity*.94;
 if(impact>2){damageShip((impact-2)*.7,CollisionDamage,b.centre);if(hitFlash>.1)message("Collision / "+b.source.name+" / brake before the surface");}
 cancelNavigation();ship.throttle=0;
}
}
