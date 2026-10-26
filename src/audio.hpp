#pragma once
#include "raylib.h"
#include <string>
#include <vector>
namespace hyperion {
class Audio {
 bool ready=false,enabled=false;std::vector<Sound> sounds;AudioStream drone{};
public:
 ~Audio();bool toggle();bool active()const{return enabled;}void play(const std::string&);
};
}
