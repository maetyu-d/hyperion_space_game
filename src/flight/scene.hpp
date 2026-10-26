#pragma once
#include "game.hpp"

namespace hyperion::flight {
constexpr int facilityKinds=14,commodityKinds=9;
enum FacilityKind { Extractor,Smelter,Bore,Harvester,Garden,Medical,Fabricator,
                   Fuelworks,Observatory,Solar,Reactor,Habitat,Warehouse,Restoration };
struct FactionStyle {
 std::array<unsigned char,3> hull,accent,secondary;double height,width;int silhouette;
};
const FactionStyle& factionStyle(int faction);
uint32_t sceneHash(const std::string& text);
struct SettlementView {
 std::string id,name,factionName,status;int faction=0,residents=0,alerts=0;bool submerged=false;
 std::array<int,facilityKinds> facilities{};std::array<double,facilityKinds> activity{};
 std::array<double,commodityKinds> stock{};
 double condition=1,pollution=0,ecology=1,power=0,demand=0,storage=0,capacity=0;
 uint64_t geometryKey()const;
};
struct FacilitySite {int kind=0,ordinal=0,occupants=0;V3 position;double angle=0;};
struct SettlementLayout {std::vector<FacilitySite> facilities;double radius=52;};
SettlementLayout layoutSettlement(const SettlementView&);
struct ShipmentView {
 std::string id,from,to,status;int faction=0,good=0,trip=0;double amount=0,progress=0;
 bool holding=false,loading=false,unloading=false;
};
struct OrganismView {std::string id;V3 position,forward,up,food,shape{1,1,1};double size=1,hue=0,phase=0,gait=1,activity=0,genes=0;int plan=0,behaviour=0,guild=0;bool flora=false,marine=false,feeding=false,resting=false;};
struct Scene {
 double hours=0,seconds=0;std::vector<OrganismView> organisms;std::vector<SettlementView> settlements;std::vector<ShipmentView> shipments;
 const SettlementView* settlement(const std::string& id)const;
 const ShipmentView* shipment(const std::string& id)const;
};
}
