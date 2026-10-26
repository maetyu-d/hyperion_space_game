#pragma once
#include "worldgen.hpp"
#include <array>
#include <string>
#include <vector>
namespace hyperion {
enum class MinorKind {Moon,Asteroid,Comet};
struct ImpactBasin {proc::Direction axis;double width=0,depth=0;};
// A separate catalogue keeps established planet, settlement and traffic indices stable.
// Distances use the same compressed kilometre scale as cockpit flight; periods are hours.
struct MinorBody {
 std::string id,name,composition;MinorKind kind=MinorKind::Asteroid;
 uint32_t seed=0;int parent=-1,belt=-1;double radius=0,orbit=0,eccentricity=0,inclination=0,node=0,argument=0,phase=0,period=1,chartSlot=0;
 double ice=0,metal=0,hue=0;std::array<ImpactBasin,12> basins{};
};
const char* minorKindName(MinorKind);
proc::Direction minorOrbit(const MinorBody&,double hours);
double minorRadius(const MinorBody&,proc::Direction normal);
std::string minorDescription(const MinorBody&);
class Universe;struct Star;struct Planet;
proc::Direction systemBodyCentre(int index);
double systemBodyRadius(const Planet&);
std::vector<MinorBody> generateMinorBodies(const Universe&,const Star&,const std::vector<Planet>&);
}
