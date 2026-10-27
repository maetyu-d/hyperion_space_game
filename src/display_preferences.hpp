#pragma once
#include <filesystem>
#include <fstream>
#include <string>
#include "graphics_quality.hpp"
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
inline bool loadPsychedelicLook(const std::filesystem::path& path){
 std::error_code ec;if(std::filesystem::file_size(path,ec)>80||ec)return false;
 std::ifstream in(path);std::string header,value;
 return bool(std::getline(in,header))&&bool(std::getline(in,value))&&in.peek()==std::char_traits<char>::eof()&&header=="HYPERION LOOK 1"&&value=="look=psychedelic";
}
inline bool savePsychedelicLook(const std::filesystem::path& path,bool psychedelic){
 namespace fs=std::filesystem;std::error_code ec;auto pending=path;pending+=".pending";
 if(fs::is_symlink(path,ec)||fs::is_symlink(pending,ec))return false;
 if(!path.parent_path().empty()){fs::create_directories(path.parent_path(),ec);if(ec)return false;}
 {std::ofstream out(pending,std::ios::trunc);out<<"HYPERION LOOK 1\nlook="<<(psychedelic?"psychedelic":"standard")<<'\n';out.flush();if(!out){out.close();fs::remove(pending,ec);return false;}}
 fs::rename(pending,path,ec);if(!ec)return true;fs::remove(pending,ec);return false;
}

}
