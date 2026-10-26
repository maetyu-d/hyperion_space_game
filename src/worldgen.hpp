#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace hyperion::proc {
struct Direction {double x=0,y=0,z=0;};
struct Spec {uint32_t seed=0;int form=0;double tectonic=.5,water=.35,temperature=289,life=.25;};
inline double bound(double v,double a=0,double b=1){return std::clamp(v,a,b);}
inline Direction chart(double x,double y){double a=2*std::atan(x/1800.),b=std::atan(y/1800.);return {std::cos(b)*std::sin(a),std::sin(b),std::cos(b)*std::cos(a)};}
inline uint32_t hash(uint32_t seed,int x,int y,int z){uint32_t h=seed^uint32_t(x)*374761393u^uint32_t(y)*668265263u^uint32_t(z)*2246822519u;h=(h^(h>>13))*1274126177u;return h^(h>>16);}
inline double noise(uint32_t seed,Direction p){
 int x=int(std::floor(p.x)),y=int(std::floor(p.y)),z=int(std::floor(p.z));double a=p.x-x,b=p.y-y,c=p.z-z;
 auto fade=[](double v){return v*v*v*(v*(v*6-15)+10);};a=fade(a);b=fade(b);c=fade(c);double sum=0;
 for(int k=0;k<2;k++)for(int j=0;j<2;j++)for(int i=0;i<2;i++)sum+=double(hash(seed,x+i,y+j,z+k)>>8)/16777216.*(i?a:1-a)*(j?b:1-b)*(k?c:1-c);return sum;
}
inline Direction scaled(Direction n,double scale,double shift=0){return {n.x*scale+shift,n.y*scale+shift*.71,n.z*scale-shift*.39};}
inline double elevation(const Spec& p,Direction n){
 double scale=p.form==1?5.2:p.form==2?2.4:p.form==3?2.8:p.form==6?3.8:2.6;
 auto q=scaled(n,scale,13.7);double warp=noise(p.seed^0x719a35u,scaled(n,3.1,9.));q.x+=warp*.9;q.y+=warp*.5;q.z-=warp*.7;
 double broad=noise(p.seed,q)*.68+noise(p.seed^0x91a23u,scaled(q,2.03,7.1))*.32;
 double detail=noise(p.seed^0x35fa19u,scaled(n,16.3,11.));double ridge=1-std::abs(noise(p.seed^0x156addu,scaled(n,7.7,3.))*2-1);
 double e=.5+(broad-.5)*.85+(detail-.5)*.075+(ridge-.65)*(.07+p.tectonic*.12);
 if(p.form==1)e=.52+(broad-.5)*.62+(detail-.5)*.14+(ridge-.65)*.06;
 if(p.form==2)e=.53+(broad-.5)*.62+(detail-.5)*.028+(ridge-.65)*.025;
 if(p.form==3)e-=std::pow(bound(1-ridge),2)*.17;
 if(p.form==4)e+=(ridge-.65)*.18+(detail-.5)*.06;
 if(p.form==5)e=.54+(broad-.5)*.72+(detail-.5)*.045-std::abs(n.y)*.025;
 if(p.form==6){double t=bound((broad-.3)/.4);t=t*t*(3-2*t);e=.36+t*.3+(detail-.5)*.065+(ridge-.65)*.05;}
 if(p.form==7)e=.48+(broad-.5)*.56+(ridge-.65)*.22-(detail-.5)*.08;
 if(p.water<.15)e=std::max(e,.36+.285*p.water+.025);
 return bound(e,.06,.94);
}
inline const char* formation(int form){static const char* names[]={"Rifted continents","Archipelago chains","Dune seas","Impact basins","Shield volcanoes","Glacial shelves","Karst plateaus","Abyssal rifts"};return names[std::clamp(form,0,7)];}
}
