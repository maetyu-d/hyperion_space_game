#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace hyperion::flight::architecture {
inline constexpr int buildingTextureSize=2048;
inline double materialNoise(double u,double v,int nx,int ny){
 double x=u*nx,y=v*ny;int ix=int(std::floor(x)),iy=int(std::floor(y));x-=std::floor(x);y-=std::floor(y);
 auto wrap=[](int n,int period){return (n%period+period)%period;};
 auto hash=[&](int a,int b){uint32_t h=uint32_t(wrap(a,nx))*0x9e3779b9u^uint32_t(wrap(b,ny))*0x85ebca6bu^0x71a36f25u;h^=h>>16;h*=0x7feb352du;h^=h>>15;h*=0x846ca68bu;h^=h>>16;return double(h>>8)/16777215.;};
 auto smooth=[](double f){return f*f*f*(f*(f*6-15)+10);};double a=smooth(x),b=smooth(y);
 return (hash(ix,iy)*(1-a)+hash(ix+1,iy)*a)*(1-b)+(hash(ix,iy+1)*(1-a)+hash(ix+1,iy+1)*a)*b;
}
// Four independent, periodic linear-data channels. They are shaded as physical
// materials rather than storing a lit colour or a repeating photograph.
inline std::array<double,4> buildingMaterial(double u,double v){
 auto n=[&](int x,int y){return materialNoise(u,v,x,y)*2-1;};
 double alloy=.5+n(8,192)*.18+n(96,192)*.11+n(192,192)*.045+n(32,768)*.027;
 double enamel=.5+n(64,64)*.23+n(128,128)*.12+n(192,192)*.07+n(512,512)*.033;
 double aggregate=.5+n(32,32)*.19+n(96,96)*.20+n(192,192)*.095+n(384,384)*.043;
 double weather=.5+n(4,4)*.24+n(12,12)*.13+n(32,32)*.07;
 return {std::clamp(alloy,.02,.98),std::clamp(enamel,.02,.98),std::clamp(aggregate,.02,.98),std::clamp(weather,.02,.98)};
}
inline std::vector<unsigned char> bakeBuildingMaterial(int size=buildingTextureSize){
 if(size<8||size>2048)throw std::invalid_argument("Building material size must be 8..2048");
 std::vector<unsigned char> pixels(size_t(size)*size*4);
 for(int y=0;y<size;y++)for(int x=0;x<size;x++){auto value=buildingMaterial((x+.5)/size,(y+.5)/size);for(int c=0;c<4;c++)pixels[(size_t(y)*size+x)*4+c]=static_cast<unsigned char>(std::lround(value[c]*255));}
 return pixels;
}
// Separate masks preserve sharp material identity while the original four
// channels retain their smooth grain and broad colour variation.
inline std::array<double,4> buildingDetail(double u,double v){
 auto smooth=[](double a,double b,double x){double t=std::clamp((x-a)/(b-a),0.,1.);return t*t*(3-2*t);};
 auto cell=[](int x,int y,uint32_t seed){
  auto wrap=[](int n){return (n%16+16)%16;};
  uint32_t h=uint32_t(wrap(x))*0x9e3779b9u^uint32_t(wrap(y))*0x85ebca6bu^seed;
  h^=h>>16;h*=0x7feb352du;h^=h>>15;h*=0x846ca68bu;h^=h>>16;return double(h>>8)/16777215.;
 };
 double x=u*16,y=v*16,fx=x-std::floor(x),fy=y-std::floor(y),first=10,second=10;
 int ix=int(std::floor(x)),iy=int(std::floor(y));
 for(int j=-1;j<=1;j++)for(int i=-1;i<=1;i++){
  double px=i+.22+.56*cell(ix+i,iy+j,0x9e8234b1u)-fx,py=j+.22+.56*cell(ix+i,iy+j,0x7ac19f35u)-fy,d=px*px+py*py;
  if(d<first){second=first;first=d;}else second=std::min(second,d);
 }
 double crack=(1-smooth(.02,.085,std::sqrt(second)-std::sqrt(first)))*(.45+.55*materialNoise(u,v,4,4));
 double tx=u*64,ty=v*64,a=tx-std::floor(tx),b=ty-std::floor(ty),wx=std::sin(a*3.141592653589793),wy=std::sin(b*3.141592653589793);
 bool over=(int(std::floor(tx))+int(std::floor(ty)))%2==0;
 double weave=.5+.36*wx*wx*wy*wy*std::cos((over?b:a)*6.283185307179586);
 double abrasion=smooth(.63,.88,materialNoise(u,v,8,128))*(.3+.7*materialNoise(u,v,8,8));
 // Sparse pores and short abrasive scars occupy different scales. Both
 // remain periodic, and mip averages retain their actual area coverage.
 double pores=smooth(.69,.86,materialNoise(u,v,256,256));
 double pits=std::max(1-smooth(.045,.115,std::sqrt(first)),pores*.78);
 double streak=.5+(materialNoise(u,v,48,3)-.5)*.72+(materialNoise(u,v,96,6)-.5)*.22;
 return {std::clamp(crack,0.,1.),std::clamp(weave,0.,1.),std::max(abrasion,pits),std::clamp(streak,0.,1.)};
}
inline std::vector<unsigned char> bakeBuildingDetail(int size=buildingTextureSize){
 if(size<8||size>2048)throw std::invalid_argument("Building detail size must be 8..2048");
 std::vector<unsigned char> pixels(size_t(size)*size*4);
 for(int y=0;y<size;y++)for(int x=0;x<size;x++){auto value=buildingDetail((x+.5)/size,(y+.5)/size);for(int c=0;c<4;c++)pixels[(size_t(y)*size+x)*4+c]=static_cast<unsigned char>(std::lround(value[c]*255));}
 return pixels;
}
// The distant shader uses the measured box-filter average, rather than an
// assumed neutral grey that would brighten sparse crack and pit masks.
inline std::array<float,4> bakedMaterialMean(const std::vector<unsigned char>& pixels){
 if(pixels.empty()||pixels.size()%4)throw std::invalid_argument("Material mean needs complete RGBA pixels");
 std::array<uint64_t,4> sum{};for(size_t i=0;i<pixels.size();i++)sum[i%4]+=pixels[i];
 std::array<float,4> mean{};for(int c=0;c<4;c++)mean[c]=float(double(sum[c])/(double(pixels.size()/4)*255.));return mean;
}
}
