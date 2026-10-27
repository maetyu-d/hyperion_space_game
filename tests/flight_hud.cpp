#include "flight/hud.hpp"
#include "display_preferences.hpp"
#include <iostream>
#include <stdexcept>
#include <chrono>
using namespace hyperion::flight;
int main(){int checks=0;auto check=[&](bool v,const char* why){++checks;if(!v)throw std::runtime_error(why);};
 std::filesystem::path folder=std::filesystem::temp_directory_path()/("hyperion-hud-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 try{
  for(int width:{320,640,720,800,960,1280,1920,3440})for(int height:{240,400,450,600,720,800,1080}){
   hyperion::UIViewport v{float(width),float(height)};HUDLayout l(v.width,v.height);
   double inset=std::clamp(std::min(double(v.width),double(v.height))*.04,24.,40.);
   for(auto r:{l.attitude,l.target,l.bearing}){
    check(r.x>=20&&r.y>=60&&r.x+r.width<=v.width-20&&r.y+r.height<=v.height-inset-148,"Advanced HUD intrudes on the header, deck or edge");
   }
   check(l.target.y>l.attitude.y+l.attitude.height,"Target detail overlaps the attitude reference");
   check(!HUDLayout(v.width,v.height,true).visible&&!HUDLayout(v.width,v.height,false,true).visible,"Panel/docked mode retains crowded full instruments");
   for(int a=0;a<360;a+=3){double x=std::cos(a*pi/180),y=std::sin(a*pi/180);auto p=hudBearing(l.bearing,{l.cx,l.cy},x,y);check(hudContains(l.bearing,p),"Bearing arrow escapes its safe frame");}
  }
  HUDRect box{10,20,100,80};HUDPoint a{-20,60},b{180,60};check(hudClip(box,a,b)&&std::abs(a.x-10)<1e-9&&std::abs(b.x-110)<1e-9&&std::abs(a.y-60)<1e-9,"Crossing rung was not clipped to the frame");
  a={0,0};b={0,100};check(!hudClip(box,a,b),"Invisible rung was retained");
  a={25,35};b=a;check(hudClip(box,a,b),"Zero-length internal rung failed");
  auto fallback=hudBearing(box,{60,60},0,0);check(std::isfinite(fallback.x)&&hudContains(box,fallback),"Directly aft/zero bearing is undefined");
  Game f;f.ship.forward={0,0,-1};f.ship.up={1,0,0};f.ship.velocity={0,0,-12};f.ports.push_back({"Berth","",{0,0,-200},{0,0,1},-1});f.target=0;
  LandingInfo g;g.available=true;g.altitude=.3;g.radial={1,0,0};g.sink=2;
  auto m=hudTelemetry(f,g,130);check(m.surface&&m.headingAvailable&&std::abs(m.heading-90)<1e-8,"Local east bearing is wrong");check(std::abs(m.navRange-110)<1e-8&&m.navAlong==12,"Navigation does not use the real docking stand-off");check(m.brake,"Stopping estimate fails to flag an exhausted brake margin");check(m.vertical==-2&&m.speed==12&&m.drift==0,"Flight telemetry changed signs or reference frames");
  f.ship.velocity={0,0,5};m=hudTelemetry(f,g,4);check(m.navAlong==-5&&!m.brake&&std::abs(m.drift-180)<1e-8,"Retreating/backwards velocity is shown as approach");
  g.radial={0,1,0};m=hudTelemetry(f,g,0);check(m.polar&&!m.headingAvailable&&std::isfinite(m.pitch)&&std::isfinite(m.roll),"Polar heading fabricates north or becomes non-finite");
  g.radial={1,0,0};f.ship.forward={1,0,0};m=hudTelemetry(f,g,0);check(!m.headingAvailable&&std::abs(m.pitch-90)<1e-8,"Vertical flight has a false compass heading");
  g.available=false;m=hudTelemetry(f,g,0);check(!m.surface,"Gas/space view displays terrain instruments");
  f.ports[0].minor=0;f.ports[0].position={100,0,0};m=hudTelemetry(f,g,0);check(m.navRange==100,"Minor fly-by incorrectly received a berth offset");
  check(hudClosing({100,0,0},{10,0,0},{3,0,0})==7,"Target closure ignores target motion");check(hudClosing({100,0,0},{3,0,0},{10,0,0})==-7,"Receding contact has the wrong closure sign");check(hudClosing({},{},{})==0,"Coincident contact divides by zero");
  HUDTelemetry cruise;cruise.speed=60;LandingInfo terrain;terrain.altitude=20;
  auto cues=hudCues(cruise,terrain,true,false,false,false);check(!cues.horizon&&!cues.vector&&!cues.braking,"Settled cruise retains redundant guidance");
  cruise.drift=20;cues=hudCues(cruise,terrain,true,false,false,false);check(cues.vector,"Significant drift loses its flight vector");
  cruise.drift=0;cues=hudCues(cruise,terrain,false,false,false,false);check(cues.vector,"Inertial flight loses its velocity reference");
  cruise.speed=0;cues=hudCues(cruise,terrain,false,false,false,false);check(!cues.vector,"Stationary ship displays a false velocity reference");
  cruise.surface=true;cues=hudCues(cruise,terrain,true,false,false,false);check(cues.horizon,"Close surface flight loses its horizon");
  terrain.altitude=160;cues=hudCues(cruise,terrain,true,false,false,false);check(!cues.horizon,"High flight retains landing-only guidance");
  cues=hudCues(cruise,terrain,true,false,true,false);check(cues.horizon,"Launch loses its horizon reference");
  cruise.brake=true;cues=hudCues(cruise,terrain,true,false,false,false);check(cues.braking,"Manual approach hides exhausted braking margin");
  cues=hudCues(cruise,terrain,true,true,false,false);check(!cues.braking,"Navigator duplicates its braking instructions");
  cues=hudCues(cruise,terrain,true,false,true,true);check(!cues.horizon&&!cues.braking,"Combat guidance is crowded by landing/navigation cues");
  auto graphics=folder/"graphics.txt";check(!hyperion::loadLowGraphics(graphics),"Missing graphics preference must default to High");check(hyperion::saveLowGraphics(graphics,true)&&hyperion::loadLowGraphics(graphics),"Low graphics preference must persist");check(hyperion::saveLowGraphics(graphics,false)&&!hyperion::loadLowGraphics(graphics),"High graphics preference must replace Low atomically");
  for(auto invalid:{"","HYPERION GRAPHICS 2\ngraphics=low\n","HYPERION GRAPHICS 1\ngraphics=other\n","HYPERION GRAPHICS 1\ngraphics=low\nextra"}){std::ofstream(graphics)<<invalid;check(!hyperion::loadLowGraphics(graphics),"Malformed graphics preference must default to High");}
  auto look=folder/"look.txt";check(!hyperion::loadPsychedelicLook(look),"Missing flight look must preserve Standard");
  check(hyperion::savePsychedelicLook(look,true)&&hyperion::loadPsychedelicLook(look),"Psychedelic look must persist");
  check(hyperion::saveLowGraphics(graphics,true)&&hyperion::loadPsychedelicLook(look),"Quality changes must retain the chosen look");
  check(hyperion::savePsychedelicLook(look,false)&&!hyperion::loadPsychedelicLook(look)&&hyperion::loadLowGraphics(graphics),"Standard look must replace Psychedelic without changing quality");
  check(!std::filesystem::exists(look.string()+".pending"),"Completed look update leaves a pending file");
  for(auto invalid:{"","HYPERION LOOK 2\nlook=psychedelic\n","HYPERION LOOK 1\nlook=other\n","HYPERION LOOK 1\nlook=psychedelic\nextra"}){std::ofstream(look)<<invalid;check(!hyperion::loadPsychedelicLook(look),"Malformed/newer flight look must preserve Standard");}
  auto lookProtected=folder/"look-protected.txt";std::ofstream(lookProtected)<<"untouched";std::filesystem::remove(look);std::filesystem::create_symlink(lookProtected,look);
  check(!hyperion::savePsychedelicLook(look,true),"Look writer follows a destination symlink");std::string lookValue;std::ifstream(lookProtected)>>lookValue;check(lookValue=="untouched","Look writer modified the symlink target");
  std::filesystem::remove(look);auto pendingLook=look;pendingLook+=".pending";std::filesystem::create_symlink(lookProtected,pendingLook);check(!hyperion::savePsychedelicLook(look,true),"Look writer follows a pending-file symlink");std::filesystem::remove(pendingLook);
  auto pref=folder/"display.txt";check(!hyperion::loadEnhancedHUD(pref),"Missing display preferences do not default to minimal");check(hyperion::saveEnhancedHUD(pref,true)&&hyperion::loadEnhancedHUD(pref),"Enhanced preference does not persist");check(hyperion::saveEnhancedHUD(pref,false)&&!hyperion::loadEnhancedHUD(pref),"Minimal preference does not replace enhanced atomically");check(!std::filesystem::exists(pref.string()+".pending"),"Completed preference update leaves a pending file");
  for(auto invalid:{"","HYPERION DISPLAY 2\nhud=enhanced\n","HYPERION DISPLAY 1\nhud=other\n","HYPERION DISPLAY 1\nhud=enhanced\nextra"}){std::ofstream(pref)<<invalid;check(!hyperion::loadEnhancedHUD(pref),"Malformed or newer display preference was accepted");}
  auto protectedFile=folder/"protected.txt";std::ofstream(protectedFile)<<"untouched";std::filesystem::remove(pref);std::filesystem::create_symlink(protectedFile,pref);check(!hyperion::saveEnhancedHUD(pref,true),"Preference writer follows a destination symlink");std::string value;std::ifstream(protectedFile)>>value;check(value=="untouched","Preference writer modified a symlink target");
  std::filesystem::remove_all(folder);std::cout<<"PASS: "<<checks<<" HUD checks / safe layout, contextual guidance, clipped roll, pole/retrograde/closure telemetry, real berth positions and independent atomic display preferences\n";return 0;
 }catch(const std::exception& e){std::filesystem::remove_all(folder);std::cerr<<"HUD regression after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
