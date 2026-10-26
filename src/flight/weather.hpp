#pragma once
#include "game.hpp"
namespace hyperion::flight {
struct Weather {double storm=0,lightning=0;V3 wind;};
inline double stormField(V3 n,uint32_t seed,double hours,double atmosphere,double water){
 double phase=(seed%8191)*.013,t=hours*.09;
 double a=.5+.5*std::sin(dot(n,{6.3,2.7,-4.1})+phase+t);
 double b=.5+.5*std::sin(dot(n,{-3.1,8.4,5.7})-phase*.7-t*.63);
 double front=std::clamp((a*.62+b*.38-.48)/.36,0.,1.);front=front*front*(3-2*front);
 return front*std::clamp(atmosphere,0.,1.)*(.4+.6*std::clamp(water,0.,1.));
}
inline Weather weatherAt(const Planet& p,V3 n,double hours){
 Weather w;if(p.giant||p.atmosphere<=0||hours<0)return w;n=unit(n);double phase=(p.worldSeed%8191)*.013;
 w.storm=stormField(n,p.worldSeed,hours,p.atmosphere,p.water);
 auto east=unit(cross({0,1,0},n),{1,0,0}),north=unit(cross(n,east));double angle=phase+hours*.045+n.y*3.;
 double gust=.72+.28*std::sin(hours*55*.29+phase+dot(n,{8,3,7}));
 w.wind=(east*std::cos(angle)+north*std::sin(angle))*(w.storm*.12*gust)+n*(std::sin(hours*55*.17+phase)*w.storm*.018);
 double pulse=std::clamp((std::sin(hours*55*.14+phase)-.996)/.004,0.,1.);
 w.lightning=pulse*pulse*std::pow(w.storm,4.);
 return w;
}
}
// Identical broad fronts in the CPU economy/helm and the cloud shader.
#define HYPERION_WEATHER_GLSL R"GLSL(
float stormField(vec3 n,float phase,float hours,float air,float water){
 float t=hours*.09,a=.5+.5*sin(dot(n,vec3(6.3,2.7,-4.1))+phase+t);
 float b=.5+.5*sin(dot(n,vec3(-3.1,8.4,5.7))-phase*.7-t*.63);
 return smoothstep(.48,.84,a*.62+b*.38)*clamp(air,0.,1.)*(.4+.6*clamp(water,0.,1.));
}
)GLSL"
