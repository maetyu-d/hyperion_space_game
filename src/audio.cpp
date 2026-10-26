#include "audio.hpp"
#include "audio_synthesis.hpp"
namespace hyperion {
Audio::~Audio(){if(ready){for(auto voice:voices)UnloadSoundAlias(voice);for(auto sound:sounds)UnloadSound(sound);UnloadAudioStream(drone);CloseAudioDevice();}}
bool Audio::toggle(){
 if(!ready){
  InitAudioDevice();ready=IsAudioDeviceReady();if(!ready)return false;
  drone=LoadAudioStream(44100,32,1);
  SetAudioStreamCallback(drone,[](void* output,unsigned frames){static double phase=0;auto samples=static_cast<float*>(output);for(unsigned i=0;i<frames;i++){phase+=6.283185307179586*59/44100;if(phase>6.283185307179586)phase-=6.283185307179586;samples[i]=float(std::sin(phase));}});
  SetAudioStreamVolume(drone,.024f);
  for(int kind=0;kind<15;kind++){
   auto data=synthesizeSound(kind);Wave wave{unsigned(data.size()),44100,32,1,data.data()};sounds.push_back(LoadSoundFromWave(wave));
   // Independent playheads share the buffer. Rapid fire keeps its tails.
   for(int i=0;i<4;i++)voices.push_back(LoadSoundAlias(sounds.back()));
  }
 }
 enabled=!enabled;
 if(enabled)PlayAudioStream(drone);else{StopAudioStream(drone);for(auto voice:voices)StopSound(voice);}
 return enabled;
}
void Audio::play(const std::string& event){
 if(!ready||!enabled||event.empty())return;
 int kind=event=="sonar"?0:event=="find"?1:event=="jump"?2:event=="pulse"?4:event=="kinetic"?5:event=="plasma"?6:event=="seeker"?7:event=="impact"?8:event=="decoy"?9:event=="explosion"?10:event=="resonance"?11:event=="shield"?12:event=="hull"?13:event=="shield-break"?14:3;
 size_t sequence=nextVoice[kind]++;auto voice=voices[size_t(kind)*4+sequence%4];
 SetSoundPitch(voice,float(1.+(int(sequence%5)-2)*.012));
 SetSoundVolume(voice,kind==12?.52f:kind==13?.85f:1.f);PlaySound(voice);
}
}
