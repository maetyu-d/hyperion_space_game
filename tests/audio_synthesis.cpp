#include "audio_synthesis.hpp"
#include <iostream>
#include <stdexcept>
int main(){try{
 double energies[15]{};
 for(int kind=0;kind<15;kind++){
  auto samples=hyperion::synthesizeSound(kind);auto repeated=hyperion::synthesizeSound(kind);
  if(samples!=repeated)throw std::runtime_error("Non-deterministic sound");
  double peak=0,energy=0,tail=0;for(size_t i=0;i<samples.size();i++){auto v=samples[i];if(!std::isfinite(v))throw std::runtime_error("Nonfinite sample");peak=std::max(peak,double(std::abs(v)));energy+=v*v;if(i>samples.size()-200)tail+=v*v;}
  if(peak>.241||peak<.025||energy<.1||std::abs(samples.front())>1e-8||std::abs(samples.back())>1e-4||tail>.002)throw std::runtime_error("Clipped, silent or discontinuous sound: "+std::to_string(kind));
  energies[kind]=energy;std::cout<<kind<<" samples="<<samples.size()<<" peak="<<peak<<" energy="<<energy<<" tail="<<tail<<'\n';
 }
 if(energies[10]<energies[5]*3||energies[6]<energies[5]*2)throw std::runtime_error("Heavy weapons lack a sustained body");
 std::cout<<"PASS: 15 deterministic, bounded sounds with silent boundaries and distinct heavy tails\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
