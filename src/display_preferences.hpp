#pragma once
#include <filesystem>
#include <fstream>
#include <string>
#include "graphics_quality.hpp"
#include "cga_palette.hpp"
namespace hyperion {
inline GraphicsQuality loadGraphicsQuality(const std::filesystem::path& path){
 std::error_code ec;if(std::filesystem::file_size(path,ec)>80||ec)return GraphicsQuality::High;
 std::ifstream in(path);std::string header,value;GraphicsQuality quality=GraphicsQuality::High;
 if(!std::getline(in,header)||!std::getline(in,value)||in.peek()!=std::char_traits<char>::eof()||header!="HYPERION GRAPHICS 1"||value.rfind("graphics=",0)!=0)return GraphicsQuality::High;
 return parseGraphicsQuality(std::string_view(value).substr(9),quality)?quality:GraphicsQuality::High;
}
inline bool saveGraphicsQuality(const std::filesystem::path& path,GraphicsQuality quality){
 namespace fs=std::filesystem;std::error_code ec;auto pending=path;pending+=".pending";
 if(fs::is_symlink(path,ec)||fs::is_symlink(pending,ec))return false;
 if(!path.parent_path().empty()){fs::create_directories(path.parent_path(),ec);if(ec)return false;}
 {std::ofstream out(pending,std::ios::trunc);out<<"HYPERION GRAPHICS 1\ngraphics="<<graphicsQualityName(quality)<<'\n';out.flush();if(!out){out.close();fs::remove(pending,ec);return false;}}
 fs::rename(pending,path,ec);if(!ec)return true;fs::remove(pending,ec);return false;
}
inline bool loadLowGraphics(const std::filesystem::path& path){return loadGraphicsQuality(path)==GraphicsQuality::Low;}
inline bool saveLowGraphics(const std::filesystem::path& path,bool low){return saveGraphicsQuality(path,low?GraphicsQuality::Low:GraphicsQuality::High);}
// Display choice lives outside the campaign schema. Invalid/newer files fall
// back to the minimal HUD; diagnostic runs never load or write this file.
inline bool loadEnhancedHUD(const std::filesystem::path& path){
 std::error_code ec;if(std::filesystem::file_size(path,ec)>80||ec)return false;
 std::ifstream in(path);std::string header,value;
 return bool(std::getline(in,header))&&bool(std::getline(in,value))&&in.peek()==std::char_traits<char>::eof()&&header=="HYPERION DISPLAY 1"&&value=="hud=enhanced";
}
inline bool saveEnhancedHUD(const std::filesystem::path& path,bool enhanced){
 namespace fs=std::filesystem;std::error_code ec;auto pending=path;pending+=".pending";
 if(fs::is_symlink(path,ec)||fs::is_symlink(pending,ec))return false;
 if(!path.parent_path().empty()){fs::create_directories(path.parent_path(),ec);if(ec)return false;}
 {std::ofstream out(pending,std::ios::trunc);out<<"HYPERION DISPLAY 1\nhud="<<(enhanced?"enhanced":"minimal")<<'\n';out.flush();if(!out){out.close();fs::remove(pending,ec);return false;}}
 fs::rename(pending,path,ec);if(!ec)return true;fs::remove(pending,ec);return false;
}
// The visual style is independent of render quality, HUD and campaign state.
enum class FlightLook { Standard, Psychedelic, Wireframe };
inline constexpr const char* flightLookName(FlightLook look){return look==FlightLook::Wireframe?"wireframe":look==FlightLook::Psychedelic?"psychedelic":"standard";}
inline bool parseFlightLook(std::string_view name,FlightLook& look){if(name=="standard")look=FlightLook::Standard;else if(name=="psychedelic")look=FlightLook::Psychedelic;else if(name=="wireframe")look=FlightLook::Wireframe;else return false;return true;}
inline FlightLook loadFlightLook(const std::filesystem::path& path){
 std::error_code ec;if(std::filesystem::file_size(path,ec)>80||ec)return FlightLook::Standard;
 std::ifstream in(path);std::string header,value;FlightLook look=FlightLook::Standard;
 if(!std::getline(in,header)||!std::getline(in,value)||in.peek()!=std::char_traits<char>::eof()||header!="HYPERION LOOK 1"||value.rfind("look=",0)!=0)return FlightLook::Standard;
 return parseFlightLook(std::string_view(value).substr(5),look)?look:FlightLook::Standard;
}
inline bool saveFlightLook(const std::filesystem::path& path,FlightLook look){
 namespace fs=std::filesystem;std::error_code ec;auto pending=path;pending+=".pending";
 if(fs::is_symlink(path,ec)||fs::is_symlink(pending,ec))return false;
 if(!path.parent_path().empty()){fs::create_directories(path.parent_path(),ec);if(ec)return false;}
 {std::ofstream out(pending,std::ios::trunc);out<<"HYPERION LOOK 1\nlook="<<flightLookName(look)<<'\n';out.flush();if(!out){out.close();fs::remove(pending,ec);return false;}}
 fs::rename(pending,path,ec);if(!ec)return true;fs::remove(pending,ec);return false;
}
inline bool loadPsychedelicLook(const std::filesystem::path& path){return loadFlightLook(path)==FlightLook::Psychedelic;}
inline bool savePsychedelicLook(const std::filesystem::path& path,bool psychedelic){return saveFlightLook(path,psychedelic?FlightLook::Psychedelic:FlightLook::Standard);}
inline WirePalette loadWirePalette(const std::filesystem::path& path){
 std::error_code ec;if(std::filesystem::file_size(path,ec)>80||ec)return WirePalette::CGA;
 std::ifstream in(path);std::string header,value;WirePalette p=WirePalette::CGA;
 if(!std::getline(in,header)||!std::getline(in,value)||in.peek()!=std::char_traits<char>::eof()||header!="HYPERION WIRE PALETTE 1"||value.rfind("palette=",0)!=0)return WirePalette::CGA;
 return parseWirePalette(std::string_view(value).substr(8),p)?p:WirePalette::CGA;
}
inline bool saveWirePalette(const std::filesystem::path& path,WirePalette p){
 namespace fs=std::filesystem;std::error_code ec;auto pending=path;pending+=".pending";
 if(fs::is_symlink(path,ec)||fs::is_symlink(pending,ec))return false;
 if(!path.parent_path().empty()){fs::create_directories(path.parent_path(),ec);if(ec)return false;}
 {std::ofstream out(pending,std::ios::trunc);out<<"HYPERION WIRE PALETTE 1\npalette="<<wirePaletteName(p)<<'\n';out.flush();if(!out){out.close();fs::remove(pending,ec);return false;}}
 fs::rename(pending,path,ec);if(!ec)return true;fs::remove(pending,ec);return false;
}

}
