#include "minor_bodies.hpp"
#include "universe.hpp"
#include <sstream>
#include <iomanip>
namespace hyperion {
proc::Direction systemBodyCentre(int i){double a=i*1.31+.45;return i==0?proc::Direction{}:proc::Direction{std::cos(a)*(4500+i*3000),std::sin(a*2)*400,std::sin(a)*(4500+i*3000)};}
double systemBodyRadius(const Planet& p){return p.giant?1700.:std::clamp(650+p.size*120,650.,1150.);}
const char* minorKindName(MinorKind k){return k==MinorKind::Moon?"Moon":k==MinorKind::Comet?"Comet":"Asteroid";}
proc::Direction minorOrbit(const MinorBody& b,double hours){
 double mean=std::remainder(b.phase+tau*std::remainder(hours,b.period)/b.period,tau),e=mean;
 // Bounded eccentricity and Newton iterations give continuous, time-based Kepler orbits.
 for(int i=0;i<10;i++)e-=(e-b.eccentricity*std::sin(e)-mean)/(1-b.eccentricity*std::cos(e));
 double x=b.orbit*(std::cos(e)-b.eccentricity),z=b.orbit*std::sqrt(1-b.eccentricity*b.eccentricity)*std::sin(e);
 double a=x*std::cos(b.argument)-z*std::sin(b.argument),c=x*std::sin(b.argument)+z*std::cos(b.argument);
 return {a*std::cos(b.node)-c*std::cos(b.inclination)*std::sin(b.node),c*std::sin(b.inclination),a*std::sin(b.node)+c*std::cos(b.inclination)*std::cos(b.node)};
}
double minorRadius(const MinorBody& b,proc::Direction n){
 double stretch=b.kind==MinorKind::Moon?.025:.17;
 double shape=1+stretch*(n.x*n.x-n.z*n.z)+stretch*.5*n.y*n.x;
 shape+=(proc::noise(b.seed,proc::scaled(n,3.7,2.))-.5)*(b.kind==MinorKind::Moon?.065:.19);
 shape+=(proc::noise(b.seed^0xadc1u,proc::scaled(n,17.1,6.))-.5)*.012;
 for(const auto& c:b.basins){double u=(1-(n.x*c.axis.x+n.y*c.axis.y+n.z*c.axis.z))/c.width;
  if(u<1.6){double bowl=std::max(0.,1-u);shape-=c.depth*bowl*bowl;shape+=c.depth*.23*std::exp(-std::pow((u-.94)*8,2));}
 }
 return b.radius*std::clamp(shape,.65,1.24);
}
std::vector<MinorBody> generateMinorBodies(const Universe& u,const Star& star,const std::vector<Planet>& planets){
 std::vector<MinorBody> out;
 auto finish=[&](MinorBody b){Random r{b.seed^0x983a6bu};b.hue=r();b.node=r()*tau;b.argument=r()*tau;b.phase=r()*tau;
  b.composition=b.ice>.6?"Water ice / silicate dust":b.metal>.65?"Nickel-iron / prismatic inclusions":b.ice>.25?"Frosted silicates / carbonaceous seams":"Basalt / carbonaceous regolith";
  for(auto& c:b.basins){double y=r()*2-1,a=r()*tau;c.axis={std::sqrt(1-y*y)*std::cos(a),y,std::sqrt(1-y*y)*std::sin(a)};c.width=.004+r()*.075;c.depth=.025+r()*.055;}
  out.push_back(std::move(b));
 };
 for(const auto& p:planets)for(int m=0;m<p.moonCount;m++){
  MinorBody b;b.kind=MinorKind::Moon;b.parent=p.index;b.seed=u.hash(int32_t(p.seed),m,4401);Random r{b.seed};b.id=p.id+"/moon/"+std::to_string(m);b.name=p.name+" "+char('a'+m);
  double parentRadius=systemBodyRadius(p);
  b.radius=55+r()*110+(p.giant?50:0);b.orbit=parentRadius+650+m*450;b.eccentricity=r()*.045;b.inclination=(r()-.5)*.34;
  b.period=1800*std::pow(b.orbit/(parentRadius+650),1.5);b.ice=std::clamp((p.temp<190?.65:.08)+r()*.3,0.,1.);b.metal=r();b.chartSlot=m;finish(b);
 }
 Random belts{u.hash(int32_t(star.seed),4402)};int beltCount=1+int(belts()*2);std::vector<int> gaps;
 for(int i=1;i+1<int(planets.size());i++)if(!planets[i].giant&&!planets[i+1].giant)gaps.push_back(i);
 for(int k=0;k<beltCount;k++){
  int gap=gaps.empty()?int(planets.size()):gaps[size_t(belts()*gaps.size())];int count=14+int(belts()*15);
  for(int i=0;i<count;i++){
   MinorBody b;b.seed=u.hash(int32_t(star.seed),k,i,4403);Random r{b.seed};b.belt=k;b.chartSlot=gap+.5+(r()-.5)*.04;
   b.id=star.id+"/asteroid/"+std::to_string(k)+"/"+std::to_string(i);b.name=star.name+" "+std::to_string(100+k*100+i);b.radius=7+std::pow(r(),2)*38;
   b.orbit=4500+b.chartSlot*3000;b.eccentricity=.001+r()*.003;b.inclination=(r()-.5)*.075;b.period=15000*std::pow(b.orbit/9000,1.5);
   b.ice=gap>int(planets.size()/2)?.25+r()*.5:r()*.18;b.metal=r();finish(b);
  }
 }
 int comets=1+int(belts()*3);
 for(int i=0;i<comets;i++){
  MinorBody b;b.kind=MinorKind::Comet;b.seed=u.hash(int32_t(star.seed),i,4404);Random r{b.seed};b.id=star.id+"/comet/"+std::to_string(i);b.name=star.name+" C/"+std::to_string(1+i);
  b.radius=10+r()*18;b.orbit=12000+r()*9000;b.eccentricity=.42+r()*.24;b.inclination=.3+r()*.7;b.period=42000*std::pow(b.orbit/12000,1.5);b.ice=.75+r()*.24;b.metal=r()*.15;b.chartSlot=double(planets.size())-.5;finish(b);
  auto& comet=out.back();auto clear=[&](){double margin=comet.orbit*std::sqrt((1+comet.eccentricity)/(1-comet.eccentricity))*pi/256+50;
   for(int sample=0;sample<256;sample++){auto at=minorOrbit(comet,comet.period*sample/256);for(const auto& p:planets){auto c=systemBodyCentre(p.index);if(std::hypot(std::hypot(at.x-c.x,at.y-c.y),at.z-c.z)<systemBodyRadius(p)+comet.radius*1.24+margin)return false;}}return true;
  };
  bool safe=false;for(int attempt=0;attempt<16;attempt++){if(clear()){safe=true;break;}comet.node+=2.3999632297;comet.argument+=1.117;}
  if(!safe){double extent=0;for(const auto& p:planets){auto c=systemBodyCentre(p.index);extent=std::max(extent,std::hypot(std::hypot(c.x,c.y),c.z)+systemBodyRadius(p));}comet.orbit=(extent+2500)/(1-comet.eccentricity);comet.period=42000*std::pow(comet.orbit/12000,1.5);}

 }
 return out;
}
std::string minorDescription(const MinorBody& b){std::ostringstream s;s<<minorKindName(b.kind)<<" / "<<b.composition<<". Diameter "<<std::fixed<<std::setprecision(0)<<b.radius*2<<" km; orbital period "<<b.period<<" h. ";
 s<<(b.kind==MinorKind::Comet?"An eccentric, inclined orbit carries an icy nucleus and spectral dust and ion tails.":b.kind==MinorKind::Moon?"A cratered, airless satellite bound to its parent planet.":"An irregular remnant in a seeded debris belt.");s<<" Fly-by survey available; no surface expedition or docking facilities.";return s.str();}
}
