#pragma once
#include "screen_layout.hpp"
#include "raylib.h"
#if defined(HYPERION_GLFW_INPUT)
#define GLFW_INCLUDE_NONE
#include "raylib/src/external/glfw/include/GLFW/glfw3.h"
#endif
namespace hyperion {
class WindowFit {
 WindowRect previousWork{};int previousMonitor=-1;
public:
 WindowRect workArea()const{
  int monitor=GetCurrentMonitor();auto position=GetMonitorPosition(monitor);
  WindowRect work{int(position.x),int(position.y),GetMonitorWidth(monitor),GetMonitorHeight(monitor)};
#if defined(HYPERION_GLFW_INPUT)
  int count=0;auto monitors=glfwGetMonitors(&count);
  if(monitor>=0&&monitor<count)glfwGetMonitorWorkarea(monitors[monitor],&work.x,&work.y,&work.width,&work.height);
#endif
  return work;
 }
 WindowFrame frame()const{
  WindowFrame border;
#if defined(HYPERION_GLFW_INPUT)
  if(auto window=glfwGetCurrentContext())glfwGetWindowFrameSize(window,&border.left,&border.top,&border.right,&border.bottom);
#endif
  return border;
 }
 void update(bool force=false,bool centre=false){
  if(IsWindowMinimized()||IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)||IsWindowFullscreen())return;
  auto work=workArea();if(work.width<=0||work.height<=0)return;int monitor=GetCurrentMonitor();
  bool changed=monitor!=previousMonitor||work.x!=previousWork.x||work.y!=previousWork.y||work.width!=previousWork.width||work.height!=previousWork.height;
  if(!force&&!changed&&!IsWindowResized())return;
  previousMonitor=monitor;previousWork=work;
  auto position=GetWindowPosition();auto border=frame();
  auto fit=fittedWindow(work,{int(position.x),int(position.y),GetScreenWidth(),GetScreenHeight()},border,centre);
  SetWindowMinSize(std::min(720,fit.width),std::min(450,fit.height));
  if(fit.width!=GetScreenWidth()||fit.height!=GetScreenHeight())SetWindowSize(fit.width,fit.height);
  if(fit.x!=int(position.x)||fit.y!=int(position.y))SetWindowPosition(fit.x,fit.y);
 }
 void toggle(){ToggleBorderlessWindowed();update(true);}
 bool contained()const{
  if(IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)||IsWindowFullscreen())return true;
  const auto work=workArea();const auto border=frame();const auto p=GetWindowPosition();
  return p.x-border.left>=work.x-1&&p.y-border.top>=work.y-1&&p.x+GetScreenWidth()+border.right<=work.x+work.width+1&&p.y+GetScreenHeight()+border.bottom<=work.y+work.height+1;
 }
};
}
