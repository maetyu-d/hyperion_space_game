#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <set>
#include "worldgen.hpp"
#include "minor_bodies.hpp"
#include <vector>
namespace hyperion {
constexpr double pi=3.14159265358979323846,tau=pi*2;
template<class T> T clamp(T v,T lo,T hi){return std::max(lo,std::min(hi,v));}
struct V2 {double x=0,y=0;};
inline double distance(V2 a,V2 b){return std::hypot(a.x-b.x,a.y-b.y);}
uint32_t stringHash(const std::string& s);
struct Random {uint32_t x;double operator()();};
struct Star {
 std::string id,name,type; V2 pos; int cx=0,cy=0,numPlanets=0; uint32_t seed=0;
 double size=1,age=0,metallicity=0,luminosity=1; bool anomaly=false;
};
struct Planet {
 std::string id,starId,name,type,bio,dominant,formation; int index=0,temp=0,metal=0,wind=0,rad=0,axialTilt=0,period=0,moonCount=0,oceanMax=0;
 uint32_t seed=0; double orbit=0,size=0,gravity=0,water=0,pressure=0,life=0,tectonic=0,hazards=0,cloud=0,salinity=0,phase=0;
 bool giant=false,anomaly=false;int generation=1,landform=0;
};
struct Region {std::string name,biome;double elevation=0,temperature=0,moisture=0,ore=1,crystal=1,geothermal=0,biomass=0;};
struct SystemProfile {std::string layout,frontier;int architecture=0,settlements=0;};
struct Contact {
 std::string id,kind,name,desc; V2 pos; double depth=0; uint32_t n=0;int feature=0;V2 habitatPos;double habitatDepth=-1;std::string habitatBiome;
};
class Universe {
 uint32_t seed_=0;int generation_=2;std::set<std::string> legacy_;
 std::map<std::pair<int,int>,std::vector<Star>> stars_;
 std::map<std::string,std::vector<Planet>> systems_;
 std::map<std::string,std::vector<MinorBody>> minorSystems_;
public:
 explicit Universe(const std::string& seed="MERIDIAN-0001",int generation=2,const std::set<std::string>& legacy={}){reset(seed,generation,legacy);}
 void reset(const std::string& seed,int generation=2,const std::set<std::string>& legacy={});
 int generationFor(const std::string& star)const{return generation_==1||legacy_.count(star)?1:2;}
 SystemProfile profile(const std::string& star)const;
 Region region(const Planet&,V2,double depth=0)const;
 proc::Spec spec(const Planet& p)const{return {p.seed,p.landform,p.tectonic,p.water,double(p.temp),p.life};}
 uint32_t seed()const{return seed_;}
 uint32_t hash(int32_t a,int32_t b=0,int32_t c=0,int32_t d=0)const;
 double rnd(int32_t a,int32_t b=0,int32_t c=0,int32_t d=0)const{return hash(a,b,c,d)/4294967296.;}
 double noise(double x,double y,int32_t s=0)const;
 double fbm(double x,double y,int32_t s=0,int oct=4)const;
 const std::vector<Star>& cell(int cx,int cy);
 Star star(const std::string& id);
 const std::vector<Planet>& planets(const std::string& id);
 const std::vector<MinorBody>& minorBodies(const std::string& id);
 Planet planet(const std::string& id,int index);
 double elevation(const Planet&,double x,double y)const;
 double seaLevel(const Planet& p)const{return .36+p.water*.285;}
 double floorDepth(const Planet&,double x,double y=0)const;
 V2 shoreline(const Planet&)const;
 std::string terrain(const Planet&,V2)const;
 std::vector<Contact> surfaceObjects(const Planet&,int cx,int cy)const;
 std::vector<Contact> oceanObjects(const Planet&,int cx,double y=0)const;
};
std::vector<Planet> generatePlanets(const Star&,uint32_t,const SystemProfile&);
std::vector<Contact> generateSurface(const Universe&,const Planet&,int,int);
std::vector<Contact> generateOcean(const Universe&,const Planet&,int,double);
std::string atmosphere(const Planet&);
std::string oceanZone(double depth);
std::string description(const Contact&,const Planet&);
std::string objectName(const std::string&,uint32_t);
}
