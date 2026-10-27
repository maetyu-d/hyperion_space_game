#pragma once
#include "universe.hpp"
#include "flight/fold.hpp"
namespace hyperion {
struct FoldSpace {std::vector<flight::FoldGate> gates;std::vector<flight::NilRegion> nil;};
struct FoldSurvey {std::string id,star,name,anchor;int a=0,b=1,authority=3;bool surveyed=false,nil=false;int passages=0;double freight=0;};
FoldSpace generateFoldSpace(Universe&,const std::string&);
}
