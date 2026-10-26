#pragma once
#include "scene.hpp"
namespace hyperion::flight {
std::vector<hyperion::WorldObject> worldObjects(const Game&,const Scene&);
inline V3 facilityHalf(int kind,const FactionStyle& style,int occupants){double heights[]={25,28,28,9,10,15,17,15,25,8,22,20,13,8};double h=(kind==Habitat?8+(occupants/4)*4:heights[kind])*style.height;return {kind==Solar?9:11*style.width,h*.5,kind==Warehouse?11.:kind==Garden?12.:10.};}
}
