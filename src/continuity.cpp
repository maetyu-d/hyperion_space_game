#include "game.hpp"
#include <sstream>
#include <iomanip>
namespace hyperion {
void Game::openStrategy(){
 if(turnBased())return;
 const auto recall=chartRecall;
 if(recall.valid&&recall.star==s.currentStar){
  s.mode=recall.mode;s.viewStar=recall.view;s.selectedStar=recall.selected;s.selectedPlanet=recall.planet;
  s.galaxy=recall.camera;s.galaxyZoom=recall.zoom;selectedMinor=recall.minor;
 }else system(s.currentStar);
 transferNotice="World frozen / your orders resolve time / K resumes the same ship";
 notify("Strategy / world frozen / plans and manual helm retained");
}
void Game::rememberSurvey(){
 if(!hasExpedition())return;auto layer=expeditionLayer();
 if(layer==Mode::Surface&&surveyRecall.valid&&surveyRecall.dive&&surveyRecall.star==s.activeStar&&surveyRecall.planet==s.activePlanet&&distance(s.surface,surveyRecall.anchor)<=2&&distance(s.waypoint,s.surface)<1e-6)return;
 surveyRecall={true,false,layer,s.activeStar,focusedContact,s.activePlanet,layer==Mode::Ocean?V2{s.oceanX,s.oceanAtY}:s.surface,s.waypoint,s.oceanTargetX,s.targetDepth};
}
void Game::restoreSurvey(bool ocean){
 if(!surveyRecall.valid)return;auto& r=surveyRecall;
 if(r.star!=s.activeStar||r.planet!=s.activePlanet||distance(s.surface,r.anchor)>2){r={};focusedContact.clear();return;}
 if(ocean){
  if(r.layer!=Mode::Ocean||!r.dive)return;
  s.oceanTargetX=r.targetX;s.targetDepth=clamp(r.targetDepth,20.,std::min(maxDepth(),universe.floorDepth(activePlanet(),r.targetX,s.oceanAtY)-36));focusedContact=r.focus;r={};
 }else if(r.layer==Mode::Surface){s.waypoint=r.waypoint;focusedContact=r.focus;r={};}
 else{r.dive=true;focusedContact=r.focus;}
}
TransferPlan Game::transferPlan(const std::string& action){
 TransferPlan q;auto reject=[&](const std::string& why){q.reason=why;return q;};
 if(action=="strategy"){
  if(turnBased())return reject("Already in turn-based planning");q.text="Free handoff / freeze the world / restore your chart and panels";
 }else if(action=="cockpit"){
  if(!turnBased())return reject("Already at the helm");if(hasExpedition()&&expeditionLayer()==Mode::Ocean&&s.depth>80)return reject("Return to the launch buoy and ascend to 80 m or less before boarding");q.hours=hasExpedition()&&expeditionLayer()==Mode::Ocean?1:0;
  q.text=q.hours?"Recover submersible / 1 h / ocean route remembered for redeployment":"Free handoff / resume the same ship / K returns to planning";
 }else if(action=="deploy"){
  if(s.mode!=Mode::Flight||!s.flight.docked)return reject("Land with gear down before deploying");
  int body=s.flight.dockedPort>=0&&s.flight.dockedPort<int(s.flight.ports.size())?s.flight.ports[s.flight.dockedPort].planet:s.flight.nearestPlanet();if(body<0)return reject("Orbital berth / land on a solid surface first");q.hours=1;q.text="Deploy at the physical landing site / 1 h / world then waits for orders";
 }else if(action=="dive"){
  if(s.mode!=Mode::Surface||activePlanet().water<.2)return reject("Dive from a world with accessible oceans");q.hours=1;q.text="Deploy submersible / 1 h";
  if(surveyRecall.valid&&surveyRecall.dive&&distance(s.surface,surveyRecall.anchor)<=2)q.text+=" / restore the previous dive route";
 }else if(action=="surface"){
  if(s.mode!=Mode::Ocean)return reject("No submersible is deployed");if(s.depth>80)return reject("Ascend to 80 m or less / E ascends one turn");q.hours=1;q.text="Surface / 1 h / remember the dive route";
 }else if(action=="launch"){
  if(!hasExpedition())return reject("No expedition is deployed");if(expeditionLayer()==Mode::Ocean&&s.depth>80)return reject("Recover the submersible from 80 m or less first");q.hours=1;q.text="Recover expedition to orbit / 1 h";
 }else return reject("Unknown transfer");
 q.available=true;return q;
}
}
