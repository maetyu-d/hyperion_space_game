#include "game.hpp"
#include <cstdlib>
namespace hyperion {
// These paths are the compatibility boundary for established installations.
// New games and all subsequent writes use defaultSavePath(), never this path.
std::string legacySavePath(){const char* home=std::getenv("HOME");if(!home)return "pelagic-save.json";
#ifdef __APPLE__
 return std::string(home)+"/Library/Application Support/Pelagic Infinity/save.json";
#elif defined(_WIN32)
 const char* appdata=std::getenv("APPDATA");return std::string(appdata?appdata:home)+"/PelagicInfinity/save.json";
#else
 const char* xdg=std::getenv("XDG_DATA_HOME");std::string base=xdg?std::string(xdg):std::string(home)+"/.local/share";return base+"/pelagic-infinity/save.json";
#endif
}
}
