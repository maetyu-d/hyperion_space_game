#pragma once
#include "ecosystem.hpp"
#include "biosphere.hpp"
namespace hyperion {
struct Economy;
enum LifeBehaviour {Rooted,Roaming,Grazing,Hunting,Fleeing,Filtering,Recycling,GardenForaging,Avoiding,Resting,Following,Startled};
struct LifeAgent {
 Contact contact;std::string id,target,settlement;V2 home,position,velocity,food;
 double depth=0,foodDepth=0,size=1,heading=0,phase=0,presence=1,activity=0;
 int guild=0,behaviour=Rooted;bool marine=false,flora=false;
};
struct LifeView {V2 centre,observer;double depth=0,radius=200,vertical=650,hours=0,observerDepth=0,alert=0;bool ocean=false;size_t limit=192;};
const char* lifeBehaviourName(int);
// Read-only representatives of existing regional populations. No extra prey,
// biomass or rewards are created by a camera, a frame, or an individual mesh.
std::vector<LifeAgent> visibleLife(const Economy&,const Universe&,const Planet&,const LifeView&);
void registerLifeHabitats(Economy&,const Universe&,const Planet&,V2,double depth=0);
}
