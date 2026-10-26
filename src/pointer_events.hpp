#pragma once
#include <deque>
namespace hyperion {
struct PointerPoint {double x=0,y=0;};
struct PointerFrame {bool pressed=false,released=false;PointerPoint press,release;};
class PointerEvents {
 struct Event {bool down;PointerPoint point;};std::deque<Event> events;
public:
 void push(bool down,PointerPoint at){events.push_back({down,at});}
 PointerFrame next(){
  PointerFrame frame;if(events.empty())return frame;
  auto event=events.front();events.pop_front();
  if(!event.down){frame.released=true;frame.release=event.point;return frame;}
  frame.pressed=true;frame.press=event.point;
  if(!events.empty()&&!events.front().down){frame.released=true;frame.release=events.front().point;events.pop_front();}
  return frame;
 }
};
}
