#pragma once
#include "game.hpp"
#include "wildlife.hpp"
#include "raylib.h"
#include "flight/renderer.hpp"
#include "ui_layout.hpp"
#include "mode_handoff.hpp"
#include "strategy_workspace.hpp"
#include <functional>
namespace hyperion {
inline UIViewport uiViewport(){return UIViewport(float(GetScreenWidth()),float(GetScreenHeight()));}
inline Vector2 uiPoint(Vector2 windowPoint){auto view=uiViewport();return {view.logical(windowPoint.x),view.logical(windowPoint.y)};}
inline Vector2 uiMouse(){return uiPoint(GetMousePosition());}

struct UI : StrategyWorkspace {
 WorkspaceMemory workspaceMemory;
 ModeHandoff handoff;
 bool enhancedHUD=false,lowGraphics=false;
 bool help=false,settings=false,paused=false,sound=false,reduced=false;
 bool menu=false,sections=false;int destinationFilter=0,divePage=0;
 bool auditInterface=false;std::map<std::string,Rectangle> auditButtons;
 float panelHeight=0;int reportIndex=0;double reportHour=-1;std::string orderHint;
 std::string action,arg,seedInput,savePath,saveWarning;bool clickConsumed=false,pointerCaptured=false,pointerPressed=false,pointerReleased=false;Vector2 pressPosition{},releasePosition{};float noticeBottom=70;
 bool observeMode(int mode,double dt,bool held){
  int previous=handoff.mode;bool changed=handoff.observe(mode,dt,held);
  if(changed&&mode==int(Mode::Flight)){workspaceMemory.board(*this);menu=sections=false;}
  else if(changed&&previous==int(Mode::Flight)){workspaceMemory.returnToStrategy(*this);menu=sections=false;}
  return changed;
 }
 void toggleInterface(){clean=!clean;menu=sections=panel=false;}
 void toggleFlightHUD(){enhancedHUD=!enhancedHUD;clean=false;}
 void openPanel(int target,bool toggle=false){target=std::clamp(target,0,7);menu=sections=false;clean=false;if(panel&&tab==target&&toggle){panel=false;return;}tabScroll[tab]=scroll;if(tab!=target)scroll=tabScroll[target];tab=target;panel=true;}
};
struct Hotspot {Vector2 p;float radius;std::string type,id;int index=0;Contact object;};
class Renderer {
 hyperion::flight::Renderer flightGraphics;
 hyperion::flight::Scene flightPresentation;
 Font sans{},mono{};Shader scene{},orb{},blur{},post{};RenderTexture2D history[2]{},bloom[2]{},composite{},handoffFrame{};int presentedMode=-1;Texture2D terrain{};
 int current=0,width=0,height=0;std::string terrainKey;V2 terrainOrigin{};float terrainSpan=1536;
 double lifeHours=0,lifeFrom=0,lifeTarget=0,lifeBlend=1;std::string lifeKey;
 float w=0,h=0,clock=0;Game* game=nullptr;UI* ui=nullptr;
 TurnOutlook turnOutlook;bool auditHideWildlife=false;
 bool panelDrawing=false,modalDrawing=false,panelContent=false;
 std::vector<Hotspot> hotspots;
 void targets();void updateTerrain();void background(double);void galaxy();void system();void surface();void ocean();
 void drawWildlife(bool);void lifeBody(const LifeAgent&,Vector2,float);
 void craft(Vector2,float,float,bool);void contact(const Contact&,Vector2,float,bool);
 void planet(const Planet&,Vector2,float,bool=false);void orbDraw(Vector2,float,uint32_t,float,bool);
 void text(const std::string&,float,float,float,Color,bool=false);void right(const std::string&,float,float,float,Color,bool=false);
 std::string elide(std::string,float,float,bool=false)const;
 void fitted(const std::string&,float,float,float,float,Color,bool=false);
 bool disclosure(const std::string&,const std::string&,float,float&,float);
 void wrapped(const std::string&,float,float&,float,float,Color,bool=false);
 bool button(const std::string&,Rectangle,const std::string&,const std::string& = "",bool=true,bool=false);
 void nextMoves(float,float&,float);void oceanOverlay();void oceanDrawer();void oceanOperations(float,float&,float);void diveReportDetails(float,float&,float);void wreckDeliveries(float,float&,float,bool);
 void turnPlanner(float,float&,float);void turnReport(float,float&,float);
 void missionPanel(float,float&,float);void economyPanel(float,float&,float);void civicResearchPanel(float,float&,float);void shipyardPanel(const Settlement&,float,float&,float);void colonyMarkers();
 void flightDirector(const hyperion::flight::LandingInfo&,double);void flightHUD();void flightPanel(float,float&,float);void combatPanel(float,float&,float);
 void overlay();void panel();void modal();void notification();void menus();void heading(const std::string&,float,float);void row(const std::string&,const std::string&,float,float&,float);
public:
 hyperion::flight::Renderer::BreakupStats flightBreakupStats()const{return flightGraphics.breakupStats();}
 hyperion::flight::Renderer::QualityStats flightQualityStats()const{return flightGraphics.qualityStats();}
 void hideWildlifeForAudit(bool hidden){auditHideWildlife=hidden;}
 int flightMinorDetailFields()const{return flightGraphics.minorDetailFields();}
 int flightMinorMaterialWidth(const hyperion::flight::Game& g,int index)const{return flightGraphics.minorMaterialWidth(g,index);}
 bool exportFlightMinorMaterial(const hyperion::flight::Game& g,int index,const std::string& path)const{return flightGraphics.exportMinorMaterial(g,index,path);}
 int flightVisibleMinorBodies()const{return flightGraphics.visibleMinorBodies();}
 int flightVisibleOrganisms()const{return flightGraphics.visibleOrganisms();}
 void auditCloudOcclusion(bool enabled){flightGraphics.auditCloudOcclusion(enabled);}hyperion::flight::CloudOcclusionStats cloudOcclusionStats(bool emission=false)const{return flightGraphics.cloudOcclusionStats(emission);}
 hyperion::flight::Renderer::GenerationAudit auditGeneration(const hyperion::flight::Game& g)const{return flightGraphics.auditGeneration(g);}
 int flightPlanetDetail(int body)const{return flightGraphics.planetDetail(body);}
 void freezeFlightVisualTime(double seconds){flightGraphics.freezeVisualTime(seconds);}
 void profileFlight(bool enabled){flightGraphics.profile(enabled);}const std::array<double,6>& flightProfile()const{return flightGraphics.profileTimes();}
 bool exportFlightMaterial(int body,const std::string& path)const{return flightGraphics.exportMaterial(body,path);}
 bool exportFlightDetail(const std::string& path)const{return flightGraphics.exportDetail(path);}
 hyperion::flight::GeometryStats flightArchitectureStats()const{return flightGraphics.architectureStats();}
 bool exportFlightShadow(const std::string& path)const{return flightGraphics.exportShadow(path);}
 bool init();void close();bool ready()const;
 void draw(Game&,UI&,double dt);bool screenshot(const std::string& path,bool centreCrop=false);bool makeIcon(const std::string& path);void pointer(Game&,UI&,Vector2,bool doubleClick);
 float oceanX()const{return UILayout::worldRight(w,ui->panel)*.48f;}float oceanY()const{return std::min(h*.46f,110.f+float(game->s.depth*.35*ui->oceanZoom));}
};
}
