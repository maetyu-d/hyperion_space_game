#include "fold_space.hpp"
#include "minor_bodies.hpp"
namespace hyperion {
FoldSpace generateFoldSpace(Universe& u,const std::string& star){
 using namespace flight;FoldSpace out;const auto worlds=u.planets(star);if(worlds.size()<2)return out;
 auto seed=u.star(star).seed;Random random{seed^0xf01d369u};
 auto centre=[&](int body){auto c=systemBodyCentre(body);return V3{c.x,c.y,c.z};};
 auto normal=[&](int body){auto at=u.shoreline(worlds[body]);double lon=2*std::atan(at.x/1800.),lat=std::atan(at.y/1800.);return V3{std::cos(lat)*std::sin(lon),std::sin(lat),std::cos(lat)*std::cos(lon)};};
 bool anomalous=star=="HOME"||(u.star(star).anomaly&&seed%3==0);
 int pairs=star=="HOME"?2:1+int(seed%4==0);
 for(int k=0;k<pairs;k++){
  int a=star=="HOME"?2:int(random()*worlds.size()),b=star=="HOME"?(k==0?1:5):(a+1+int(random()*(worlds.size()-1)))%int(worlds.size());
  std::string id="fold:"+star+":"+std::to_string(k);bool nil=anomalous&&k==pairs-1;
  if(nil&&worlds[b].giant)for(const auto& world:worlds)if(!world.giant&&world.index!=a){b=world.index;break;}
  int authority=star=="HOME"?(k==0?3:4):1+int((seed+k*17)%4);
  for(int side=0;side<2;side++){
   int body=side?b:a;auto n=unit(rotate(normal(body),unit(V3{.3,1,.1}),.55+k*.27));
   double radius=systemBodyRadius(worlds[body]);
   FoldGate g;g.id=id+(side?":B":":A");g.link=id;g.name=(nil?"Archive fold / ":"Trade fold / ")+worlds[side?a:b].name;
   g.position=centre(body)+n*(radius+950+k*180);g.normal=side?-n:n;g.up=unit(cross(n,unit(cross({0,1,0},n),{1,0,0})));g.body=body;g.peer=int(out.gates.size())+(side?-1:1);g.radius=100+seed%31;g.authority=authority;g.nil=nil;
   if(nil&&side){
    // A secluded outer-system pocket keeps ordinary approaches Euclidean.
    g.position=centre(body)+n*(radius*13.6);g.normal=-n;
    g.up=unit(g.up-g.normal*dot(g.up,g.normal));
    NilRegion region;region.id="nil:"+id;region.body=body;region.scale=radius/1.25;region.position=g.position+n*radius*.7;region.axis=-n;region.up=g.up;region.radius=radius*3.2;out.nil.push_back(region);
   }
   out.gates.push_back(g);
  }
 }
 return out;
}
}
