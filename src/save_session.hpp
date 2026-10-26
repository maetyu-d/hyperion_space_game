#pragma once
#include "game.hpp"
namespace hyperion {
// Prefer the current campaign; adopt the previous location only if absent.
std::string automaticSaveSource(const std::string& destination,const std::string& previous,bool fresh);
enum class SaveResult {Saved,Conflict,Failed};
// Detect another copy or an external edit before replacing the campaign file.
// On conflict later autosaves continue in a separate recovery campaign.
class SaveSession {
 struct Stamp {bool exists=false;uint64_t hash=0,size=0;bool operator==(const Stamp& b)const{return exists==b.exists&&hash==b.hash&&size==b.size;}};
 std::string path,originalPath;Stamp expected;bool observed=false;
 static Stamp inspect(const std::string&);
public:
 explicit SaveSession(std::string file);
 bool acknowledge(std::string& error);
 bool redirected()const{return path!=originalPath;}
 SaveResult save(const State&,std::string& error,std::string& recovery);
};
}
