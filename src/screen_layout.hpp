#pragma once
#include <algorithm>
namespace hyperion {
// UI units are independent of Retina framebuffer pixels and small native windows.
struct UIViewport {
 float scale,width,height;
 explicit UIViewport(float windowWidth,float windowHeight){
  windowWidth=std::max(1.f,windowWidth);windowHeight=std::max(1.f,windowHeight);
  scale=std::min({1.f,windowWidth/960.f,windowHeight/600.f});
  width=windowWidth/scale;height=windowHeight/scale;
 }
 float logical(float windowCoordinate)const{return windowCoordinate/scale;}
 float native(float uiCoordinate)const{return uiCoordinate*scale;}
};
struct WindowRect {int x,y,width,height;};
struct WindowFrame {int left=0,top=0,right=0,bottom=0;};
inline WindowRect fittedWindow(WindowRect work,WindowRect content,WindowFrame frame,bool centre=false){
 const int mx=std::min(12,std::max(0,(work.width-frame.left-frame.right-1)/2));
 const int my=std::min(12,std::max(0,(work.height-frame.top-frame.bottom-1)/2));
 const int availableW=std::max(1,work.width-frame.left-frame.right-2*mx);
 const int availableH=std::max(1,work.height-frame.top-frame.bottom-2*my);
 content.width=std::clamp(content.width,1,availableW);content.height=std::clamp(content.height,1,availableH);
 const int x=work.x+frame.left+mx,y=work.y+frame.top+my;
 content.x=centre?x+(availableW-content.width)/2:std::clamp(content.x,x,x+availableW-content.width);
 content.y=centre?y+(availableH-content.height)/2:std::clamp(content.y,y,y+availableH-content.height);
 return content;
}
}
