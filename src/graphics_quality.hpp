#pragma once
#include <algorithm>
#include <cmath>
#include <string_view>
namespace hyperion {
enum class GraphicsQuality { Low, High, Extreme };
inline constexpr const char* graphicsQualityName(GraphicsQuality q){return q==GraphicsQuality::Low?"low":q==GraphicsQuality::Extreme?"extreme":"high";}
inline bool parseGraphicsQuality(std::string_view name,GraphicsQuality& q){
 if(name=="low")q=GraphicsQuality::Low;else if(name=="high")q=GraphicsQuality::High;else if(name=="extreme")q=GraphicsQuality::Extreme;else return false;return true;
}
struct FlightQualityProfile {
 int maximumHeight=1440,maximumWidth=8192;double supersampling=1.;
 int cloudDivisor=2,cloudSamples=24,auroraDivisor=2,atmosphereSamples=8,atmosphereSunSamples=3,auroraSamples=6;
 int surfaceWidth=3072,nearSurfaceWidth=3072,giantSurfaceWidth=1536,geologyWidth=2048,nearGeologyWidth=2048,giantGeologyWidth=1536;
 int materialWidth=2048,detailWidth=2048,shadowWidth=2048,minorWidth=2048;
 int globeLongitude=192,globeLatitude=96,groundSide=97,lifeLimit=256;bool extreme=false;
};
inline constexpr FlightQualityProfile flightQualityProfile(GraphicsQuality quality){
 FlightQualityProfile p;
 if(quality==GraphicsQuality::Low){p.maximumHeight=1080;p.cloudDivisor=3;p.cloudSamples=16;p.surfaceWidth=p.nearSurfaceWidth=2048;p.giantSurfaceWidth=1024;p.geologyWidth=p.nearGeologyWidth=1536;p.giantGeologyWidth=1024;p.detailWidth=p.shadowWidth=1024;p.lifeLimit=150;}
 if(quality==GraphicsQuality::Extreme){
  p.extreme=true;p.maximumHeight=4320;p.maximumWidth=7680;p.supersampling=2.;
  p.cloudDivisor=p.auroraDivisor=1;p.cloudSamples=96;p.atmosphereSamples=24;p.atmosphereSunSamples=8;p.auroraSamples=24;
  // One nearby 8K world; other worlds retain mipmapped orbital fields. Moving
  // between bodies transfers this detail residency without changing geography.
  p.nearSurfaceWidth=8192;p.giantSurfaceWidth=4096;p.nearGeologyWidth=4096;p.giantGeologyWidth=2048;
  p.materialWidth=p.detailWidth=p.shadowWidth=p.minorWidth=4096;
  p.globeLongitude=320;p.globeLatitude=160;p.groundSide=193;p.lifeLimit=768;
 }
 return p;
}
struct FlightRenderSize {int width,height;};
inline FlightRenderSize flightRenderSize(GraphicsQuality quality,bool reduced,int outputWidth,int outputHeight){
 const auto p=flightQualityProfile(quality);outputWidth=std::max(1,outputWidth);outputHeight=std::max(1,outputHeight);
 double height=std::min(double(reduced?810:p.maximumHeight),outputHeight*(reduced?1.:p.supersampling));
 if(quality==GraphicsQuality::Extreme)height=std::min(height,double(p.maximumWidth)*outputHeight/outputWidth);
 const int h=std::max(1,int(height));int width=std::max(1,int(double(outputWidth)*h/outputHeight));if(quality==GraphicsQuality::Extreme)width=std::min(width,p.maximumWidth);return {width,h};
}
}
