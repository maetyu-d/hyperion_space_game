#include "audio.hpp"
#include <cmath>
#include <cstdint>
namespace hyperion {
Audio::~Audio(){if(ready){for(auto s:sounds)UnloadSound(s);UnloadAudioStream(drone);CloseAudioDevice();}}
bool Audio::toggle(){if(!ready){InitAudioDevice();ready=IsAudioDeviceReady();if(!ready)return false;
 drone=LoadAudioStream(44100,32,1);SetAudioStreamCallback(drone,[](void* output,unsigned frames){static double phase=0;auto samples=static_cast<float*>(output);for(unsigned i=0;i<frames;i++){phase+=6.283185307179586*59/44100;if(phase>6.283185307179586)phase-=6.283185307179586;samples[i]=float(std::sin(phase));}});SetAudioStreamVolume(drone,.024f);
 for(int k=0;k<12;k++){unsigned n=44100;std::vector<float> data(n);double phase=0;double start=k==0?1050:k==1?650:k==2?110:220,end=k==0?180:k==1?1170:k==2?35:50;
 if(k>=4){start=k==4?1700:k==5?900:k==6?280:k==7?180:k==8?130:k==9?2400:k==11?780:65;end=k==4?180:k==5?100:k==6?42:k==7?50:k==8?25:k==9?650:k==11?120:24;}
 uint32_t noise=0x91ac;for(unsigned i=0;i<n;i++){double t=double(i)/n,f=start*std::pow(end/start,std::min(1.,t/.65));phase+=6.283185*f/44100;double env=std::min(1.,t/(k>=4?.003:.025))*std::exp(-t*(k==5?35:k==6?8:k==10?5:13));if(k<4)env=std::min(1.,t/.025)*std::exp(-t*7.);noise=noise*1664525u+1013904223u;double grit=double(noise>>8)/8388607.5-1;data[i]=float((std::sin(phase)*(k>=8?.3:1)+grit*(k==5?.65:k>=8?.8:.08))*env*.10);}
 Wave wave{n,44100,32,1,data.data()};sounds.push_back(LoadSoundFromWave(wave));}}
 enabled=!enabled;if(enabled)PlayAudioStream(drone);else StopAudioStream(drone);return enabled;}
void Audio::play(const std::string& e){if(!ready||!enabled||e.empty())return;int i=e=="sonar"?0:e=="find"?1:e=="jump"?2:e=="pulse"?4:e=="kinetic"?5:e=="plasma"?6:e=="seeker"?7:e=="impact"?8:e=="decoy"?9:e=="explosion"?10:e=="resonance"?11:3;PlaySound(sounds[size_t(i)]);}
}
