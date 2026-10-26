#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
namespace hyperion {
// Shared by the audio device and offline signal checks. No recorded assets.
inline std::vector<float> synthesizeSound(int kind){
 constexpr unsigned rate=44100;constexpr double tau=6.283185307179586;
 const double duration=kind==10?2.2:kind==5?.22:kind>=4?.75:1.;
 unsigned count=unsigned(rate*duration);std::vector<float> samples(count);
 double phase=0,bodyPhase=0,filteredNoise=0;uint32_t noise=0x91ac+uint32_t(kind)*7919;
 for(unsigned i=0;i<count;i++){
  double t=double(i)/rate;noise=noise*1664525u+1013904223u;double grit=double(noise>>8)/8388607.5-1.;filteredNoise+=.14*(grit-filteredNoise);
  double start=kind==0?1050:kind==1?650:kind==2?110:220,end=kind==0?180:kind==1?1170:kind==2?35:50;
  if(kind>=4){start=kind==4?1850:kind==5?1150:kind==6?330:kind==7?220:kind==9?2400:kind==11?920:kind==12?1300:kind==14?190:90;end=kind==4?160:kind==5?75:kind==6?38:kind==7?48:kind==9?650:kind==11?170:kind==12?400:kind==14?35:24;}
  double f=start*std::pow(end/start,std::min(1.,t/(kind==5?.065:.45)));
  phase+=tau*f/rate;bodyPhase+=tau*(kind==6?48:kind==10?36:kind==13?62:85)/rate;
  double attack=std::min(1.,t/(kind>=4?.0025:.025)),signal=0;
  if(kind<4)signal=(std::sin(phase)+grit*.06)*std::exp(-t*7);
  else if(kind==4)signal=(std::sin(phase)*.62+std::sin(phase*.5)*.25)*std::exp(-t*18)+grit*.2*std::exp(-t*110);
  else if(kind==5)signal=(grit*.65+std::sin(phase)*.30)*std::exp(-t*55)+std::sin(bodyPhase)*.40*std::exp(-t*32);
  else if(kind==6)signal=(std::sin(phase+.9*std::sin(phase*.503))*.54+std::sin(bodyPhase)*.32+filteredNoise*.6)*std::exp(-t*7);
  else if(kind==7)signal=(filteredNoise*1.4+std::sin(phase)*.2+std::sin(bodyPhase)*.24)*std::exp(-t*9);
  else if(kind==9)signal=(std::sin(phase)*.5+grit*.26)*std::exp(-t*16);
  else if(kind==10){
   signal=(filteredNoise*1.5+std::sin(phase)*.18+std::sin(bodyPhase)*.3)*std::exp(-t*3.4)+grit*.55*std::exp(-t*50);
   for(int burst=0;burst<4;burst++){double age=t-(.24+burst*.2);if(age>0)signal+=(filteredNoise*.8+std::sin(bodyPhase*(1.3+burst*.31))*.15)*std::min(1.,age/.006)*std::exp(-age*(9+burst));}
  }
  else if(kind==11)signal=(std::sin(phase)*.45+std::sin(phase*1.501)*.22+std::sin(phase*2.003)*.12)*(1+.12*std::sin(t*93))*std::exp(-t*12);
  else if(kind==12)signal=(std::sin(phase)*.40+std::sin(phase*1.414)*.17)*std::exp(-t*22)+filteredNoise*.6*std::exp(-t*30);
  else if(kind==14)signal=grit*.5*std::exp(-t*27)+(std::sin(phase)*.28+std::sin(phase*1.618)*.22)*std::exp(-t*7);
  else signal=(filteredNoise*1.5+std::sin(phase)*.22+std::sin(bodyPhase)*.3)*std::exp(-t*15)+grit*.4*std::exp(-t*65);
  // Taper even the bass tail to a silent boundary; leave mixing headroom.
  double release=std::min(1.,(duration-t)/.035);
  samples[i]=float(std::tanh(signal*1.2)*attack*release*(kind<4?.12:kind==10?.24:.20));
 }
 return samples;
}
}
