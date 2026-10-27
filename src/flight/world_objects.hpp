#pragma once
#include "scene.hpp"
namespace hyperion::flight {
std::vector<hyperion::WorldObject> portObjects(const Game&,const Scene&,const Port&);
std::vector<hyperion::WorldObject> worldObjects(const Game&,const Scene&);
inline uint64_t portGeometryKey(const Port& port,const SettlementView* view,const Game& game){SettlementView relay;relay.id=port.id;uint64_t key=view?view->geometryKey():relay.geometryKey();for(const auto& [id,d]:game.worldDamage)if((d.integrity<=0||modularStructure(game.worldDamage,id))&&(id==portObjectId(port.id)||id.rfind("prop:"+port.id+":",0)==0||(d.kind==ImpactObject&&d.body==port.planet&&length(d.position-port.position)<800)))key^=uint64_t(sceneHash(id))*0x9e3779b97f4a7c15ull;return key;}
inline V3 facilityHalf(int kind,const FactionStyle& style,int occupants){double heights[]={25,28,28,9,10,15,17,15,25,8,22,20,13,8};double h=(kind==Habitat?8+(occupants/4)*4:heights[kind])*style.height;return {kind==Solar?9:11*style.width,h*.5,kind==Warehouse?11.:kind==Garden?12.:10.};}
}
