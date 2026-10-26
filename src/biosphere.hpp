#pragma once
#include "universe.hpp"
namespace hyperion {
struct Phenotype {int plan=0,limbs=4,segments=4,samples=10;double size=1,hue=0,gait=1;std::string body,niche;};
Phenotype phenotype(const Contact&);
std::string biologicalTraits(const Contact&);
Contact habitatFeature(const Universe&,const Planet&,int cx,int cy,bool ocean,double actualY=0);
}
