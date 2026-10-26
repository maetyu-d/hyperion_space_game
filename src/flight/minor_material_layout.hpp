#pragma once
#include <algorithm>
#include <array>
#include <vector>
namespace hyperion::flight {
struct MinorDetailCandidate {int index=0;double pixels=0;bool resident=false;};
// Two close fields bound the extra texture memory. The retention band avoids
// rebaking a body repeatedly while hovering around the detail boundary.
inline std::array<int,2> minorDetailSelection(std::vector<MinorDetailCandidate> candidates){
 candidates.erase(std::remove_if(candidates.begin(),candidates.end(),[](const auto& c){return c.pixels<(c.resident?90:120);}),candidates.end());
 std::stable_sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b){double x=a.pixels+(a.resident?20:0),y=b.pixels+(b.resident?20:0);return x==y?a.index<b.index:x>y;});
 std::array<int,2> result{-1,-1};for(size_t i=0;i<std::min(size_t(2),candidates.size());i++)result[i]=candidates[i].index;return result;
}
}
