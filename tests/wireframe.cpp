#include "display_preferences.hpp"
#include "flight/wire_geometry.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
using namespace hyperion;using namespace hyperion::flight;
int main(){auto folder=std::filesystem::temp_directory_path()/("hyperion-wireframe-"+std::to_string(getpid()));std::filesystem::create_directories(folder);int checks=0;
 auto check=[&](bool pass,const char* message){++checks;if(!pass)throw std::runtime_error(message);};
 try{
  float quad[]={0,0,0,1,0,0,1,1,0,0,0,0,1,1,0,0,1,0};auto masks=wireFeatures(quad,6,nullptr,2);int count=0;for(auto mask:masks)for(float f:mask)count+=int(f);check(count==4,"Coplanar quad reveals its triangulation diagonal");check(masks[0][1]==0&&masks[1][2]==0,"Shared diagonal has wrong barycentric edge mask");
  float indexed[]={0,0,0,1,0,0,1,1,0,0,1,0};unsigned short ids[]={0,1,2,0,2,3};check(wireFeatures(indexed,4,ids,2)==masks,"Indexed and split-vertex geometry produce different edges");
  float folded[]={0,0,0,1,0,0,1,1,0,0,0,0,1,1,0,0,0,1};auto sharp=wireFeatures(folded,6,nullptr,2);check(sharp[0][1]==1&&sharp[1][2]==1,"Architectural crease disappears");
  float degenerate[]={0,0,0,0,0,0,0,0,0};auto empty=wireFeatures(degenerate,3,nullptr,1);check(empty[0]==std::array<float,3>{0,0,0},"Degenerate geometry emits edges");
  std::vector<float> cube;const V3 points[]={{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}};const int faces[][4]={{0,1,2,3},{5,4,7,6},{4,0,3,7},{1,5,6,2},{3,2,6,7},{4,5,1,0}};
  for(auto face:faces)for(int k:{0,1,2,0,2,3}){auto p=points[face[k]];for(double n:{p.x,p.y,p.z})cube.push_back(float(n));}auto edges=wireFeatures(cube.data(),36,nullptr,12);count=0;for(auto e:edges)for(float f:e)count+=int(f);check(count==24,"Cube must retain twelve edges and hide six face diagonals");
  auto look=folder/"look.txt",quality=folder/"graphics.txt",hud=folder/"display.txt";check(loadFlightLook(look)==FlightLook::Standard,"Missing preference changes default look");
  for(auto style:{FlightLook::Standard,FlightLook::Psychedelic,FlightLook::Wireframe}){FlightLook parsed;check(parseFlightLook(flightLookName(style),parsed)&&parsed==style,"Look names do not roundtrip");check(saveFlightLook(look,style)&&loadFlightLook(look)==style,"Look does not persist");}
  check(saveGraphicsQuality(quality,GraphicsQuality::Extreme)&&saveEnhancedHUD(hud,true)&&saveFlightLook(look,FlightLook::Wireframe)&&loadGraphicsQuality(quality)==GraphicsQuality::Extreme&&loadEnhancedHUD(hud),"Wireframe changes quality or HUD");
  check(savePsychedelicLook(look,true)&&loadFlightLook(look)==FlightLook::Psychedelic,"Legacy preference writer is incompatible");
  for(auto invalid:{"HYPERION LOOK 2\nlook=wireframe\n","HYPERION LOOK 1\nlook=wireframe\nextra","HYPERION LOOK 1\nlook=other\n"}){std::ofstream(look)<<invalid;check(loadFlightLook(look)==FlightLook::Standard,"Malformed look accepted");}
  std::filesystem::remove(look);auto protectedFile=folder/"protected.txt";std::ofstream(protectedFile)<<"untouched";std::filesystem::create_symlink(protectedFile,look);check(!saveFlightLook(look,FlightLook::Wireframe),"Look follows destination symlink");std::filesystem::remove(look);std::filesystem::create_symlink(protectedFile,look.string()+".pending");check(!saveFlightLook(look,FlightLook::Wireframe),"Look follows pending-file symlink");std::string text;std::ifstream(protectedFile)>>text;check(text=="untouched","Protected file was changed");
  std::filesystem::remove_all(folder);std::cout<<"PASS: "<<checks<<" vector edge, geometry and independent preference checks\n";return 0;
 }catch(const std::exception& e){std::filesystem::remove_all(folder);std::cerr<<e.what()<<'\n';return 1;}}
