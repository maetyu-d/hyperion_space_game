#include "pointer_events.hpp"
#include <iostream>
#include <stdexcept>
using namespace hyperion;
int main(){int checks=0;auto check=[&](bool pass,const char* message){++checks;if(!pass)throw std::runtime_error(message);};try{
 PointerEvents events;check(!events.next().pressed,"idle input stays idle");
 events.push(true,{25,40});events.push(false,{25,40});auto tap=events.next();check(tap.pressed&&tap.released&&tap.press.x==25&&tap.release.y==40,"a complete short click survives between render frames");check(!events.next().pressed,"a click is delivered exactly once");
 events.push(true,{10,20});auto hold=events.next();check(hold.pressed&&!hold.released,"held press starts a drag");check(!events.next().pressed,"holding does not repeat a click");events.push(false,{170,80});auto release=events.next();check(!release.pressed&&release.released&&release.release.x==170,"a later release retains the drag endpoint");
 for(int i=0;i<100;i++){events.push(true,{double(i),double(i*2)});events.push(false,{double(i+3),double(i*2+4)});}for(int i=0;i<100;i++){auto next=events.next();check(next.pressed&&next.released&&next.press.x==i&&next.release.x==i+3,"a burst preserves click order and endpoints across frames");}check(!events.next().pressed&&!events.next().released,"a burst leaves no repeated or stuck input");
 events.push(false,{5,6});events.push(true,{8,9});events.push(false,{8,9});auto first=events.next(),second=events.next();check(first.released&&!first.pressed&&second.pressed&&second.released,"an old drag release cannot release a new held gesture");
 std::cout<<"PASS: "<<checks<<" pointer checks; short clicks, one dispatch, held drags, ordered bursts and gesture boundaries\n";return 0;
 }catch(const std::exception& e){std::cerr<<"Pointer regression after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
