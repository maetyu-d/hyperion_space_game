#pragma once
#include "pointer_events.hpp"
#include "raylib.h"
#if defined(HYPERION_GLFW_INPUT)
#define GLFW_INCLUDE_NONE
#include "raylib/src/external/glfw/include/GLFW/glfw3.h"
#endif
namespace hyperion {
// Preserve short clicks even when both edges arrive between two render frames.
// The original raylib callback still receives every event for normal flight input.
class NativePointer {
 PointerEvents events;
#if defined(HYPERION_GLFW_INPUT)
 GLFWwindow* window=nullptr;GLFWmousebuttonfun previous=nullptr;
 inline static NativePointer* active=nullptr;
 static void callback(GLFWwindow* window,int button,int action,int modifiers){
  if(!active)return;
  // Keep press coordinates in the same logical space as the UI hit tests.
  if(button==GLFW_MOUSE_BUTTON_LEFT&&(action==GLFW_PRESS||action==GLFW_RELEASE)){auto p=GetMousePosition();active->events.push(action==GLFW_PRESS,{p.x,p.y});}
  if(active->previous)active->previous(window,button,action,modifiers);
 }
#endif
public:
 NativePointer(){
#if defined(HYPERION_GLFW_INPUT)
  window=glfwGetCurrentContext();if(window){active=this;previous=glfwSetMouseButtonCallback(window,callback);}
#endif
 }
 NativePointer(const NativePointer&)=delete;NativePointer& operator=(const NativePointer&)=delete;
 ~NativePointer(){detach();}
 void detach(){
#if defined(HYPERION_GLFW_INPUT)
  if(window){glfwSetMouseButtonCallback(window,previous);window=nullptr;active=nullptr;}
#endif
 }
 PointerFrame next(){
#if defined(HYPERION_GLFW_INPUT)
  if(window)return events.next();
#endif
  auto p=GetMousePosition();return {IsMouseButtonPressed(MOUSE_BUTTON_LEFT),IsMouseButtonReleased(MOUSE_BUTTON_LEFT),{p.x,p.y},{p.x,p.y}};
 }
};
}
