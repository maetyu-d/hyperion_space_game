#pragma once
#include <array>
#include <set>
#include <string>
namespace hyperion {
struct StrategyWorkspace {
 bool panel=false,clean=false;
 int plannedHours=6;
 int tab=0,marketGood=4,buildingChoice=0,routeDestination=0,factionChoice=1,researchPage=0;
 std::string settlement;float scroll=0;std::set<std::string> expanded;std::array<float,8> tabScroll{};
 double surfaceZoom=1.65,oceanZoom=1;
};
struct WorkspaceMemory {
 bool valid=false;StrategyWorkspace strategy;
 void board(StrategyWorkspace& workspace){strategy=workspace;valid=true;workspace.panel=false;workspace.clean=false;workspace.scroll=0;}
 void returnToStrategy(StrategyWorkspace& workspace)const{if(valid)workspace=strategy;}
 void clear(){valid=false;strategy={};}
};
}
