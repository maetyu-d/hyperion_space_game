#include "save_session.hpp"
#include "json.hpp"
#include <filesystem>
#include <cctype>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace hyperion;
namespace {
int checks=0;void check(bool v,const char* m){++checks;if(!v)throw std::runtime_error(m);}
std::string bytes(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
}
int main(){try{
 namespace fs=std::filesystem;auto root=fs::temp_directory_path()/"hyperion-branding-save-test";fs::remove_all(root);fs::create_directories(root);
 auto previous=(root/"previous/save.json").string(),current=(root/"Hyperion/save.json").string(),explicitFile=(root/"custom/save.json").string();
 auto named=defaultSavePath();for(char& c:named)c=char(std::tolower(static_cast<unsigned char>(c)));
 check(defaultSavePath()!=legacySavePath()&&named.find("hyperion")!=std::string::npos,"new campaign location uses Hyperion on every platform");
 check(automaticSaveSource(current,previous,false).empty(),"absent saves start a new campaign");
 Game g;g.enterFlight();g.s.hull=62;g.s.flight.ship.shield=44;g.s.fuel=123;g.s.flight.ship.angularVelocity={.1,.2,-.3};g.s.flight.ship.thrusters={.2,-.1,.15};g.system("HOME");std::string error,recovery;
 check(saveState(g.s,previous,error),"established campaign fixture saves");auto original=bytes(previous);
 check(automaticSaveSource(current,previous,false)==previous,"first launch discovers the established campaign");
 check(automaticSaveSource(current,previous,true).empty(),"fresh launch explicitly skips adoption");
 check(automaticSaveSource(explicitFile,"",false).empty(),"an explicit save destination never adopts another campaign");
 State loaded;check(loadState(loaded,automaticSaveSource(current,previous,false),error),"established campaign loads through the new source selector");
 SaveSession destination(current);check(destination.save(loaded,error,recovery)==SaveResult::Saved,"adopted progress saves to the new destination");
 check(nlohmann::json::parse(bytes(current))==nlohmann::json::parse(original),"adoption preserves every persisted campaign field");
 check(bytes(previous)==original,"adoption leaves original bytes untouched");
 check(automaticSaveSource(current,previous,false)==current,"Hyperion campaign takes priority on reopening");
 check(loadState(loaded,current,error)&&loaded.hull==62&&loaded.fuel==123&&loaded.flight.ship.shield==44,"ship condition and resources reopen intact");
 auto adopted=bytes(current);g.s.resources[0]+=7;check(saveState(g.s,previous,error),"old installation can retain its independent progress");
 check(automaticSaveSource(current,previous,false)==current&&bytes(current)==adopted,"a newer legacy campaign cannot silently replace current progress");
 {std::ofstream out(current);out<<"invalid";}
 check(automaticSaveSource(current,previous,false)==current,"a damaged current save cannot silently fall back to older progress");
 check(!loadState(loaded,current,error),"invalid selected campaign is reported");
 check(destination.save(g.s,error,recovery)==SaveResult::Conflict&&!recovery.empty(),"existing conflict protection still preserves externally changed saves");
 check(bytes(current)=="invalid","conflict recovery cannot overwrite an existing file");
 check(flight::vesselDef(2).name==std::string("Hyperion mule"),"ship catalogue uses the new game name");
 fs::remove_all(root);std::cout<<"Branding: "<<checks<<" checks passed / renamed catalogue, save paths, safe adoption, exact fields, precedence, fresh/custom saves and conflict protection\n";return 0;
}catch(const std::exception& e){std::cerr<<"Branding failure after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
