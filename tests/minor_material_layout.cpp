#include "flight/minor_material_layout.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace hyperion::flight;
int main(){int checks=0;auto require=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);};try{
 require(minorDetailSelection({})==std::array<int,2>{-1,-1},"empty view allocates no detail fields");
 require(minorDetailSelection({{0,119,false},{1,89,true}})==std::array<int,2>{-1,-1},"entry and exit bounds reject unresolved fields");
 require(minorDetailSelection({{0,120,false},{1,90,true}})==std::array<int,2>{0,1},"a resolved new body and a retained body can coexist");
 require(minorDetailSelection({{0,200,true},{1,201,false},{2,199,false}})==std::array<int,2>{0,1},"retention priority prevents tiny rank changes from rebaking textures");
 require(minorDetailSelection({{0,200,true},{1,250,false},{2,240,false}})==std::array<int,2>{1,2},"larger approaching bodies replace smaller detail residents");
 require(minorDetailSelection({{3,180,false},{2,180,false},{1,180,false}})==std::array<int,2>{1,2},"equal-size candidates use stable identity order");
 for(int count=0;count<150;count++){
  std::vector<MinorDetailCandidate> list;for(int i=0;i<count;i++)list.push_back({i,40+std::sin(i*.73+count*.91)*160,i%3==0});
  auto result=minorDetailSelection(list);require(result[0]==-1||result[0]!=result[1],"detail slots never duplicate a body");
  for(int index:result)if(index>=0){require(index<count,"selected body belongs to the current catalogue");auto c=list[index];require(c.pixels>=(c.resident?90:120),"a retained texture is visually resolvable");}
 }
 for(int i=0;i<1000;i++){double pixels=105+std::sin(i*.17)*14;auto r=minorDetailSelection({{7,pixels,true},{8,15,false}});require(r[0]==7&&r[1]==-1,"hovering in the retention band does not churn the texture cache");}
 std::cout<<"Minor material layout: "<<checks<<" checks / two-field budget, hysteresis, stable priority and replacement passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
