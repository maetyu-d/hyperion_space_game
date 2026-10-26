#pragma once
#include <filesystem>
#include <fstream>
#include <string>
namespace hyperion {
inline bool loadLowGraphics(const std::filesystem::path& path){
 std::error_code ec;if(std::filesystem::file_size(path,ec)>80||ec)return false;
 std::ifstream in(path);std::string header,value;
 return bool(std::getline(in,header))&&bool(std::getline(in,value))&&in.peek()==std::char_traits<char>::eof()&&header=="HYPERION GRAPHICS 1"&&value=="graphics=low";
}
inline bool saveLowGraphics(const std::filesystem::path& path,bool low){
 namespace fs=std::filesystem;std::error_code ec;auto pending=path;pending+=".pending";
 if(fs::is_symlink(path,ec)||fs::is_symlink(pending,ec))return false;
 if(!path.parent_path().empty()){fs::create_directories(path.parent_path(),ec);if(ec)return false;}
 {std::ofstream out(pending,std::ios::trunc);out<<"HYPERION GRAPHICS 1\ngraphics="<<(low?"low":"high")<<'\n';out.flush();if(!out){out.close();fs::remove(pending,ec);return false;}}
 fs::rename(pending,path,ec);if(!ec)return true;fs::remove(pending,ec);return false;
}
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
}
