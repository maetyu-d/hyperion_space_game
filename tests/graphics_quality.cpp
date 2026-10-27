#include "graphics_quality.hpp"
#include "display_preferences.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace hyperion;
int main(){int checks=0;auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);};
 auto folder=std::filesystem::temp_directory_path()/("hyperion-quality-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 try{
  auto file=folder/"graphics.txt";check(loadGraphicsQuality(file)==GraphicsQuality::High,"Missing preference changes the established default");
  std::filesystem::create_directories(folder);auto look=folder/"look.txt";check(savePsychedelicLook(look,true),"Look fixture failed");
  for(auto q:{GraphicsQuality::Low,GraphicsQuality::High,GraphicsQuality::Extreme}){
   GraphicsQuality parsed;check(parseGraphicsQuality(graphicsQualityName(q),parsed)&&parsed==q,"Quality name does not round-trip");
   check(saveGraphicsQuality(file,q)&&loadGraphicsQuality(file)==q,"Quality does not persist");check(loadPsychedelicLook(look),"Quality changed the independent visual look");
   auto p=flightQualityProfile(q);check((p.globeLongitude+1)*(p.globeLatitude+1)<=65535&&p.groundSide*p.groundSide<=65535,"Quality overflows terrain/globe index buffers");
   for(int width:{1,720,1920,2560,3440,7680,16000})for(int height:{1,450,1200,1600,2160,4320})for(bool reduced:{false,true}){
    auto size=flightRenderSize(q,reduced,width,height);check(size.width>=1&&size.height>=1,"Zero-sized render target");
    check(size.height<=(reduced?810:p.maximumHeight),"Render target exceeds its height budget");
    if(q==GraphicsQuality::Extreme)check(size.width<=p.maximumWidth,"Ultrawide supersampling exceeds texture budget");
   }
  }
  for(auto legacy:{"high","low"}){std::ofstream(file)<<"HYPERION GRAPHICS 1\ngraphics="<<legacy<<'\n';check(std::string(graphicsQualityName(loadGraphicsQuality(file)))==legacy,"Legacy quality was not preserved");}
  for(auto invalid:{"","HYPERION GRAPHICS 2\ngraphics=extreme\n","HYPERION GRAPHICS 1\ngraphics=other\n","HYPERION GRAPHICS 1\ngraphics=extreme\nextra"}){std::ofstream(file)<<invalid;check(loadGraphicsQuality(file)==GraphicsQuality::High,"Invalid quality preference must default to High");}
  GraphicsQuality previous=GraphicsQuality::Extreme;check(!parseGraphicsQuality("ultra",previous)&&previous==GraphicsQuality::Extreme,"Unknown CLI quality silently changes the choice");
  auto high=flightRenderSize(GraphicsQuality::High,false,2560,1600),extreme=flightRenderSize(GraphicsQuality::Extreme,false,2560,1600);
  check(high.width==2304&&high.height==1440&&extreme.width==5120&&extreme.height==3200,"High/default or supersampling dimensions regressed");
  auto protectedFile=folder/"protected.txt";std::ofstream(protectedFile)<<"untouched";std::filesystem::remove(file);std::filesystem::create_symlink(protectedFile,file);
  check(!saveGraphicsQuality(file,GraphicsQuality::Extreme),"Quality writer follows destination symlinks");std::filesystem::remove(file);auto pending=file;pending+=".pending";std::filesystem::create_symlink(protectedFile,pending);check(!saveGraphicsQuality(file,GraphicsQuality::Extreme),"Quality writer follows staging symlinks");
  std::string unchanged;std::ifstream(protectedFile)>>unchanged;check(unchanged=="untouched","Preference writer changed a linked file");
  std::filesystem::remove_all(folder);std::cout<<"PASS: "<<checks<<" quality checks / legacy preferences, independent looks, supersampling budgets and safe mesh indices\n";return 0;
 }catch(const std::exception& e){std::filesystem::remove_all(folder);std::cerr<<"FAIL after "<<checks<<" quality checks: "<<e.what()<<'\n';return 1;}}
