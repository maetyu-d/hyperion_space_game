#include "screen_layout.hpp"
#include "ui_layout.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace hyperion;
int main(){int checks=0;auto check=[&](bool pass,const char* why){++checks;if(!pass)throw std::runtime_error(why);};
 try{
  for(int width:{320,640,720,800,960,1280,1440,1920,3440})for(int height:{240,400,450,500,600,720,900,1080,1440}){
   UIViewport v{float(width),float(height)};
   check(v.width>=959.99f&&v.height>=599.99f,"the full minimum UI remains available");
   check(v.scale>0&&v.scale<=1,"scaling only fits down to the available window");
   check(std::abs(v.width/v.height-double(width)/height)<.0001,"fitting preserves camera and chart aspect");
   for(float x:{0.f,.17f,.5f,1.f}){
    check(std::abs(v.native(v.logical(width*x))-width*x)<.002,"pointer positions round-trip through the fitted canvas");
    check(std::abs(v.native(v.width*x)-width*x)<.002,"drawing occupies the complete window without cropping");
   }
   float inset=UILayout::safeInset(v.width,v.height);
   float instrumentY=v.logical(v.native(v.height-inset-50));
   check(instrumentY>=UILayout::cockpitTop(v.width,v.height),"clicking fitted cockpit instruments stays outside the weapon firing area");
   check(inset>=24&&inset<=40&&v.native(v.height-inset)<height,"the instrument deck has a visible bottom safe area");
   for(float dpi:{1.f,1.5f,2.f,3.f})check(std::abs(v.native(v.width)*dpi-width*dpi)<.01,"Retina pixels do not change UI coordinate bounds");
  }
  for(WindowRect work: {WindowRect{0,24,1440,834},WindowRect{-1920,0,1920,1040},WindowRect{1440,-900,1280,850},WindowRect{0,24,640,376}})
   for(WindowRect wanted:{WindowRect{-20000,-20000,3840,2160},WindowRect{20000,20000,800,500},WindowRect{100,100,1280,800}})
    for(bool centre:{false,true}){
     WindowFrame frame{1,28,1,1};auto fit=fittedWindow(work,wanted,frame,centre);
     check(fit.x-frame.left>=work.x&&fit.y-frame.top>=work.y,"outer window begins inside the current work area");
     check(fit.x+fit.width+frame.right<=work.x+work.width&&fit.y+fit.height+frame.bottom<=work.y+work.height,"window decorations also fit on the display");
     check(fit.width<=wanted.width&&fit.height<=wanted.height,"fitting never expands a requested window");
     auto again=fittedWindow(work,fit,frame,centre);check(again.x==fit.x&&again.y==fit.y&&again.width==fit.width&&again.height==fit.height,"refitting does not cause resizing oscillation");
    }
  UIViewport normal(1440,900),small(720,450);
  check(normal.scale==1&&small.scale==.75f,"normal and small windows use their intended scale");
  check(std::abs(small.logical(679.5f)-(small.width-54))<.001,"native clicks reach the fitted menu button");
  std::cout<<"PASS: "<<checks<<" screen layout checks / fitting, pointer mapping, safe area, Retina and decorated monitor bounds\n";
 }catch(const std::exception& e){std::cerr<<"Screen fit regression after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}
}
