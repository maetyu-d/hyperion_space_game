#include "save_session.hpp"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <stdexcept>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#endif
namespace hyperion {
namespace fs=std::filesystem;
namespace {
struct SaveLock {
#if defined(__unix__) || defined(__APPLE__)
 int file=-1;
 explicit SaveLock(const std::string& path){
  file=::open((path+".lock").c_str(),O_CREAT|O_RDWR,0600);
  if(file<0)throw std::runtime_error("Unable to lock campaign save");
  if(::flock(file,LOCK_EX|LOCK_NB)<0){::close(file);file=-1;throw std::runtime_error("Another copy is saving / retry shortly");}
 }
 ~SaveLock(){if(file>=0){::flock(file,LOCK_UN);::close(file);}}
#else
 explicit SaveLock(const std::string&){}
#endif
};
}
std::string automaticSaveSource(const std::string& destination,const std::string& previous,bool fresh){
 if(fresh)return {};
 if(fs::exists(destination))return destination;
 return !previous.empty()&&fs::exists(previous)?previous:std::string{};
}
SaveSession::Stamp SaveSession::inspect(const std::string& file){
 Stamp stamp;stamp.exists=fs::exists(file);if(!stamp.exists)return stamp;
 if(fs::file_size(file)>64*1024*1024)throw std::runtime_error("Campaign save exceeds 64 MB");
 std::ifstream in(file,std::ios::binary);if(!in)throw std::runtime_error("Unable to read campaign save");
 stamp.hash=14695981039346656037ull;char buffer[8192];
 while(in){in.read(buffer,sizeof buffer);auto count=in.gcount();stamp.size+=uint64_t(count);for(std::streamsize i=0;i<count;i++){stamp.hash^=static_cast<unsigned char>(buffer[i]);stamp.hash*=1099511628211ull;}}
 if(!in.eof())throw std::runtime_error("Unable to verify campaign save");return stamp;
}
SaveSession::SaveSession(std::string file):path(file),originalPath(std::move(file)){std::string error;acknowledge(error);}
bool SaveSession::acknowledge(std::string& error){try{auto stamp=inspect(originalPath);path=originalPath;expected=stamp;observed=true;error.clear();return true;}catch(const std::exception& e){observed=false;error=e.what();return false;}}
SaveResult SaveSession::save(const State& state,std::string& error,std::string& recovery){try{
 recovery.clear();if(!observed)throw std::runtime_error("Campaign save could not be verified / export your progress");
 fs::path target(path);if(target.has_parent_path())fs::create_directories(target.parent_path());SaveLock lock(path);
 if(!(inspect(path)==expected)){
  auto tick=std::chrono::high_resolution_clock::now().time_since_epoch().count();
  recovery=(target.parent_path()/("recovery-conflict-"+std::to_string(tick)+".json")).string();
  if(!saveState(state,recovery,error))return SaveResult::Failed;
  path=recovery;expected=inspect(path);
  error="Autosave redirected / campaign changed in another copy / original preserved / progress in "+fs::path(recovery).filename().string();return SaveResult::Conflict;
 }
 if(!saveState(state,path,error))return SaveResult::Failed;
 expected=inspect(path);error.clear();return SaveResult::Saved;
 }catch(const std::exception& e){error=e.what();return SaveResult::Failed;}}
}
