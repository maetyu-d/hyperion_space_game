#pragma once
#include "game.hpp"
#include "scene.hpp"
#include "raylib.h"
#include <unordered_map>
#include <map>
#include <tuple>
namespace hyperion::flight {
struct GeometryStats {int models=0,closeTriangles=0,closePorts=0,detailedTriangles=0,distantTriangles=0,degenerateTriangles=0,invalidVertices=0,invalidNormals=0;};
struct CloudOcclusionStats {int testedPixels=0,leakingPixels=0;float maxLeakAlpha=0,maxCloudAlpha=0;};
class Renderer {
 int minorDrawn=0;Texture2D cometGlow{};Shader minorShader{},minorBake{};std::unordered_map<std::string,Model> minorModels;
 struct MinorField {RenderTexture2D base{},detail{};float blend=0;};std::unordered_map<std::string,MinorField> minorFields;
 RenderTexture2D bakeMinorField(const MinorBody&,int);void prepareMinorBodies(const Game&);
 void clearMinorBodies();void drawMinorBodies(const Game&,bool depth=false);void drawCometTails(const Game&);
 Shader lifeShader{};std::array<Model,24> lifeModels{};std::array<int,13> lifeUniforms{};int lifeDrawn=0;
 void drawLife(const Scene&);void clearLife();
 bool profiling=false;std::array<double,6> timings{};
 struct PortAsset {Model close{},detailed{},distant{};std::vector<V3> detailCentres;V3 at{},up{};uint64_t key=0;double radius=60;int detail=-1;};
 std::unordered_map<std::string,PortAsset> portAssets;std::map<std::tuple<int,int,int,int>,Model> craftAssets;
 const Scene* presentation=nullptr;Shader structure{},surfaceBake{},geologyBake{},detailBake{},planetDepth{},sunDepth{};
 std::vector<RenderTexture2D> surfaceMaps,geologyMaps;std::string bodiesKey;
 Shader sky{},world{},groundShader{},atmo{},cloud{},cloudResolve{},ringShader{},aurora{},blur{},post{},cockpitShader{};
 RenderTexture2D opaqueScene{},sunScene{},detailMap{},scene{},composite{},auroraScene{},cloudScene{},bloom[2]{};
 Texture2D opaqueDepth{},noiseTexture{},buildingTexture{},buildingDetailTexture{};
 std::vector<Model> planets,distantPlanets,atmospheres,clouds;std::vector<int> detailedBodies;
 Model ringModel{},cockpitModel{};Camera3D camera{};
 Model groundModel{},rockModel{};int groundBody=-1;V3 groundAnchor;Shader dust{};float groundBlend=0;
 Matrix sunMatrix{};V3 shadowAnchor{},shadowEye{};float shadowBlend=-1;int shadowBody=-1;uint64_t shadowCasters=0;unsigned shadowUpdates=0,shadowFrames=0;bool shadowActive=false;
 void drawSunShadows(const Game&,V3);
 void updateGround(const Game&,int);void drawGroundEffects(const Game&);
 std::string worldKey;int cockpitWidth=0,cockpitHeight=0,cockpitKind=-1;bool cockpitDocked=false,hdr=false;
 bool reduced=false,showCockpit=true;double visualTime=0,speed=0,fixedVisualTime=-1;V3 sunDirection{.6,.7,-.4};
 void drawRings(const Planet&,double);
 void clearPortAssets();PortAsset buildPort(const Port&,const SettlementView*,const Game&);void drawSettlement(const Port&,const SettlementView*,const Game&);void drawPortActivity(const Port&,const Scene&);Model buildCraft(int,int,int,int);void drawFactionCraft(const Traffic&,const Scene&);
 void rebuild(const Game&);void rebuildCockpit(int,int,bool,int);void drawCockpit(const Game&);void drawMotion(const Game&);
 void drawPlanetDepth(const Game&,const std::vector<int>&,V3);void drawAuroras(const Game&,const std::vector<int>&,V3);void drawClouds(const Game&,const std::vector<int>&,V3);
 bool auditClouds=false;CloudOcclusionStats cloudStats{},auroraStats{};void measureCloudOcclusion(const Game&,int,V3,bool emission=false);void resolveWeather(const Game&,V3,Texture2D,bool emission=false);
public:
 int visibleMinorBodies()const{return minorDrawn;}
 int minorDetailFields()const;int minorMaterialWidth(const Game&,int body)const;
 bool exportMinorMaterial(const Game&,int body,const std::string&)const;
 int visibleOrganisms()const{return lifeDrawn;}
 void auditCloudOcclusion(bool enabled){auditClouds=enabled;}CloudOcclusionStats cloudOcclusionStats(bool emission=false)const{return emission?auroraStats:cloudStats;}
 struct GenerationAudit {int samples=0;double maximumError=0;};
 GenerationAudit auditGeneration(const Game&)const;
 GeometryStats architectureStats()const;
 int planetDetail(int body)const{return body>=0&&body<int(detailedBodies.size())?detailedBodies[body]:-1;}
 void freezeVisualTime(double seconds){fixedVisualTime=seconds;}
 void profile(bool enabled){profiling=enabled;}const std::array<double,6>& profileTimes()const{return timings;}
 bool exportMaterial(int body,const std::string& path)const;
 bool exportDetail(const std::string& path)const;
 bool exportShadow(const std::string& path)const;
 bool init();void shutdown();void prepare(const Game&,const Scene&,bool reducedEffects=false,bool cockpitVisible=true);void present(float width,float height);
 Vector2 project(V3 p,float width,float height)const;
};
}
