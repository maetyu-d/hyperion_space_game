#include "flight/building_material.hpp"
#include <iostream>
#include <stdexcept>
using namespace hyperion::flight::architecture;
int main(){try{
 int checks=0;auto require=[&](bool ok,const char* message){checks++;if(!ok)throw std::runtime_error(message);};
 for(int y=-5;y<=5;y++)for(int x=-5;x<=5;x++){
  double u=x*.137+.021,v=y*.183+.037;auto a=buildingMaterial(u,v),b=buildingMaterial(u+1,v-2);
  for(int c=0;c<4;c++){require(std::isfinite(a[c])&&a[c]>=0&&a[c]<=1,"Material channel is outside its linear-data range");require(std::abs(a[c]-b[c])<1e-10,"Texture does not repeat continuously");}
 }
 for(int i=0;i<32;i++){
  double t=(i+.5)/32,epsilon=1e-5;auto a=buildingMaterial(epsilon,t),b=buildingMaterial(1+epsilon,t),c=buildingMaterial(-epsilon,t),d=buildingMaterial(1-epsilon,t);
  auto e=buildingMaterial(t,epsilon),f=buildingMaterial(t,1+epsilon),g=buildingMaterial(t,-epsilon),h=buildingMaterial(t,1-epsilon);
  for(int k=0;k<4;k++){require(std::abs((a[k]-c[k])-(b[k]-d[k]))<1e-10,"Horizontal seam has a normal discontinuity");require(std::abs((e[k]-g[k])-(f[k]-h[k]))<1e-10,"Vertical seam has a normal discontinuity");}
 }
 auto pixels=bakeBuildingMaterial(64);require(pixels.size()==64*64*4,"Baked channel layout is incomplete");require(pixels==bakeBuildingMaterial(64),"Texture generation is not deterministic");
 for(int c=0;c<4;c++){int lo=255,hi=0;double sum=0;for(size_t i=c;i<pixels.size();i+=4){lo=std::min(lo,int(pixels[i]));hi=std::max(hi,int(pixels[i]));sum+=pixels[i];}require(hi-lo>60,"A material channel has lost its useful detail");require(sum/(pixels.size()/4)>80&&sum/(pixels.size()/4)<180,"A material channel has a biased mip average");}
 bool rejected=false;try{bakeBuildingMaterial(0);}catch(const std::invalid_argument&){rejected=true;}require(rejected,"Invalid texture size was accepted");
 for(int y=-5;y<=5;y++)for(int x=-5;x<=5;x++){
  double u=x*.137+.021,v=y*.183+.037;auto a=buildingDetail(u,v),b=buildingDetail(u+1,v-2);
  for(int c=0;c<4;c++){require(std::isfinite(a[c])&&a[c]>=0&&a[c]<=1,"Detail mask is outside its linear-data range");require(std::abs(a[c]-b[c])<1e-10,"Detail mask does not repeat continuously");}
 }
 for(int i=0;i<32;i++){
  double t=(i+.5)/32,epsilon=1e-5;auto a=buildingDetail(epsilon,t),b=buildingDetail(1+epsilon,t),c=buildingDetail(-epsilon,t),d=buildingDetail(1-epsilon,t);
  auto e=buildingDetail(t,epsilon),f=buildingDetail(t,1+epsilon),g=buildingDetail(t,-epsilon),h=buildingDetail(t,1-epsilon);
  for(int k=0;k<4;k++){require(std::abs((a[k]-c[k])-(b[k]-d[k]))<1e-10,"Detail horizontal seam has a normal discontinuity");require(std::abs((e[k]-g[k])-(f[k]-h[k]))<1e-10,"Detail vertical seam has a normal discontinuity");}
 }
 auto masks=bakeBuildingDetail(192);require(masks.size()==192*192*4,"Detail mask packing is incomplete");require(masks==bakeBuildingDetail(192),"Detail generation is not deterministic");
 for(int c=0;c<4;c++){int lo=255,hi=0;double sum=0;for(size_t i=c;i<masks.size();i+=4){lo=std::min(lo,int(masks[i]));hi=std::max(hi,int(masks[i]));sum+=masks[i];}require(hi-lo>60,"A detail channel has lost its useful variation");require(sum/(masks.size()/4)>4&&sum/(masks.size()/4)<250,"A detail mask has lost its useful coverage");}
 rejected=false;try{bakeBuildingDetail(0);}catch(const std::invalid_argument&){rejected=true;}require(rejected,"Invalid detail size was accepted");
 auto mean=bakedMaterialMean({0,0,255,255,255,0,255,255,0,255,255,255,255,0,0,255});
 for(int c=0;c<4;c++)require(std::abs(mean[c]-std::array<float,4>{.5f,.25f,.75f,1.f}[c])<1e-7,"Distant material mean does not match box-filtered channel coverage");
 for(auto bad:{std::vector<unsigned char>{},std::vector<unsigned char>{1,2,3}}){rejected=false;try{bakedMaterialMean(bad);}catch(const std::invalid_argument&){rejected=true;}require(rejected,"Incomplete channel data was accepted for material averaging");}
 std::cout<<"PASS: "<<checks<<" material checks; periodic values and derivatives, deterministic grain/detail packing, useful mask coverage and exact distant averages\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
