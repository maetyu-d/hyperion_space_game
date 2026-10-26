#include "scene.hpp"

namespace hyperion::flight {
const FactionStyle& factionStyle(int faction){
 static const std::array<FactionStyle,5> styles{{
  {{57,90,110},{135,255,214},{125,175,255},1.,1.,0},
  {{91,65,56},{255,167,83},{255,84,166},1.18,1.12,1},
  {{43,91,83},{97,255,192},{117,197,255},.83,1.18,2},
  {{63,75,111},{255,220,126},{154,133,255},1.28,.90,3},
  {{71,57,105},{191,147,255},{107,241,255},1.42,.82,4}
 }};return styles[std::clamp(faction,0,4)];
}
uint32_t sceneHash(const std::string& text){uint32_t h=2166136261u;for(unsigned char c:text){h^=c;h*=16777619u;}return h;}
uint64_t SettlementView::geometryKey()const{
 uint64_t h=sceneHash(id);auto add=[&](int v){h^=uint64_t(v)+0x9e3779b97f4a7c15ull+(h<<6)+(h>>2);};
 add(faction);add(residents/4);add(submerged);
 for(int n:facilities)add(n);for(const auto& [slot,hp]:facilityIntegrity){add(slot);add(int(hp*4));}
 // A cargo stack represents up to twenty real units; tiny consumption changes
 // update shader uniforms without rebuilding the settlement's static mesh.
 for(double n:stock)add(std::min(8,int(std::ceil(std::max(0.,n)/20.))));
 return h;
}
SettlementLayout layoutSettlement(const SettlementView& view){
 SettlementLayout result;double rotation=(sceneHash(view.id)%65536)/65536.*2*pi;
 for(int kind=0;kind<facilityKinds;kind++)for(int i=0;i<std::clamp(view.facilities[kind],0,24);i++){
  double angle=rotation+2*pi*kind/facilityKinds,radius=84+(i/2)*27.;V3 radial{std::cos(angle),0,std::sin(angle)},across{-radial.z,0,radial.x};
  V3 p=radial*radius+across*(i%2?9.5:-9.5);
  int occupants=kind==Habitat?std::clamp(view.residents-i*12,0,12):0;
  result.facilities.push_back({kind,i,occupants,p,angle});result.radius=std::max(result.radius,length(p)+22);
 }
 return result;
}
bool plantingClear(const SettlementLayout& layout,V3 p){
 if(std::hypot(p.x,p.z)<49||(std::abs(p.x)<29&&p.z>32&&p.z<85))return false;
 for(const auto& site:layout.facilities){if(length(p-site.position)<19)return false;if(site.ordinal==0){auto direction=unit(site.position);double along=dot(p,direction);if(along>40&&along<length(site.position)+5&&length(p-direction*along)<8)return false;}}
 return true;
}
const SettlementView* Scene::settlement(const std::string& id)const{for(auto& s:settlements)if(s.id==id)return &s;return nullptr;}
const ShipmentView* Scene::shipment(const std::string& id)const{for(auto& s:shipments)if(s.id==id)return &s;return nullptr;}
}
