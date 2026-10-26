#include "game.hpp"
#include "json.hpp"
#include "economy_storage.hpp"
#include "flight_storage.hpp"
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <stdexcept>
#include <chrono>
#include <atomic>
namespace hyperion {
using J=nlohmann::json;namespace fs=std::filesystem;
namespace {
J pos(V2 v){return {{"x",v.x},{"y",v.y}};}
J encode(const State& s){J j={{"version",3},{"nativeVersion",5},{"seed",s.seed},{"mode",s.mode==Mode::Galaxy?"galaxy":s.mode==Mode::System?"system":s.mode==Mode::Surface?"surface":s.mode==Mode::Flight?"flight":"ocean"},
 {"expeditionMode",s.expeditionMode==Mode::Ocean?"ocean":"surface"},{"currentStar",s.currentStar},{"selectedStar",s.selectedStar},{"viewStar",s.viewStar},{"activeStar",s.activeStar},{"selectedPlanet",s.selectedPlanet},{"activePlanet",s.activePlanet},
 {"galaxy",{{"x",s.galaxy.x},{"y",s.galaxy.y},{"z",s.galaxyZoom}}},{"surface",{{"x",s.surface.x},{"y",s.surface.y},{"tx",s.waypoint.x},{"ty",s.waypoint.y}}},
 {"ocean",{{"x",s.oceanX},{"depth",s.depth},{"atY",s.oceanAtY},{"tx",s.oceanTargetX},{"tDepth",s.targetDepth}}},
 {"fuel",s.fuel},{"maxFuel",s.maxFuel},{"hull",s.hull},{"battery",s.battery},{"hours",s.hours},{"totalDistance",s.totalDistance},{"turns",s.turns},
 {"resources",{{"metal",s.resources[0]},{"crystal",s.resources[1]},{"organic",s.resources[2]},{"data",s.resources[3]}}},
 {"upgrades",{{"drive",s.upgrades[0]},{"sonar",s.upgrades[1]},{"pressure",s.upgrades[2]},{"exosuit",s.upgrades[3]}}},
 {"visitedStars",s.visitedStars},{"visitedPlanets",s.visitedPlanets},{"flags",{{"signals",s.signals},{"relics",s.relics}}}};
 j["lifeRevision"]=s.lifeRevision;j["generation"]={{"version",s.generation},{"legacySystems",s.legacySystems}};j["economy"]=encodeEconomy(s.economy);j["flight"]=encodeFlight(s.flight,s.economy);
 j["scanned"]=J::object();for(auto& k:s.scanned)j["scanned"][k]=1;
 j["locations"]=J::object();for(auto& p:s.locations)j["locations"][p.first]=pos(p.second);
 j["discovered"]=J::array();for(auto& d:s.discovered)j["discovered"].push_back({{"id",d.id},{"title",d.title},{"kind",d.kind},{"planet",d.planet},{"star",d.star},{"description",d.description},{"depth",d.depth},{"hours",d.hours}});
 j["log"]=J::array();for(auto& l:s.log)j["log"].push_back({{"time",l.time},{"msg",l.msg}});return j;
}
double number(const J& j,const char* key,double def,double lo,double hi){double v=j.value(key,def);if(!std::isfinite(v)||v<lo||v>hi)throw std::runtime_error(std::string("Invalid value: ")+key);return v;}
std::string text(const J& j,const char* key,const std::string& def,size_t max=4096){auto s=j.value(key,def);if(s.size()>max)throw std::runtime_error(std::string("Oversized field: ")+key);return s;}
V2 readPos(const J& j){return {number(j,"x",0,-1e7,1e7),number(j,"y",0,-1e7,1e7)};}
}
bool saveState(const State& s,const std::string& file,std::string& error){fs::path temp;try{
 fs::path p(file);if(p.has_parent_path())fs::create_directories(p.parent_path());
 static std::atomic<unsigned> serial{0};temp=p;
 temp+=".tmp-"+std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count())+"-"+std::to_string(serial++);
 {std::ofstream out(temp,std::ios::binary);out<<encode(s).dump(2)<<'\n';out.flush();if(!out)throw std::runtime_error("Unable to write save file");}
 fs::rename(temp,p);error.clear();return true;
 }catch(const std::exception& e){if(!temp.empty()){std::error_code ignored;fs::remove(temp,ignored);}error=e.what();return false;}}
bool loadState(State& dest,const std::string& file,std::string& error){try{
 if(fs::file_size(file)>64*1024*1024)throw std::runtime_error("Save file exceeds 64 MB");std::ifstream in(file);J j;in>>j;
 if(j.at("version")!=3||!j.contains("resources")||!j.contains("currentStar"))throw std::runtime_error("Incompatible save format");State s;
 s.lifeRevision=0;if(j.contains("lifeRevision")){if(!j["lifeRevision"].is_number_integer()||(j["lifeRevision"]!=0&&j["lifeRevision"]!=1))throw std::runtime_error("Unsupported living universe revision");s.lifeRevision=j["lifeRevision"].get<int>();}
 s.seed=text(j,"seed","",80);if(s.seed.empty())throw std::runtime_error("Missing universe seed");s.generation=1;s.legacySystems.clear();
 if(j.contains("generation")){const auto& g=j.at("generation");if(!g.is_object()||!g.at("version").is_number_integer())throw std::runtime_error("Invalid generation version");if(g.at("version")!=1&&g.at("version")!=2)throw std::runtime_error("Unsupported generation version");s.generation=g.at("version").get<int>();if(s.generation<1||s.generation>2)throw std::runtime_error("Unsupported generation version");const auto& systems=g.at("legacySystems");if(!systems.is_array()||systems.size()>80000)throw std::runtime_error("Invalid preserved systems");for(const auto& id:systems){auto v=id.get<std::string>();if(v.size()>100||!s.legacySystems.insert(v).second)throw std::runtime_error("Invalid preserved system");}}
 Universe u(s.seed,s.generation,s.legacySystems);for(const auto& id:s.legacySystems)if(u.star(id).id!=id)throw std::runtime_error("Unknown preserved system");
 auto readStar=[&](const char* key){auto id=text(j,key,"HOME",100);if(u.star(id).id!=id)throw std::runtime_error(std::string("Invalid star: ")+key);return id;};
 s.currentStar=readStar("currentStar");s.selectedStar=readStar("selectedStar");s.viewStar=readStar("viewStar");s.activeStar=readStar("activeStar");
 s.selectedPlanet=clamp(j.value("selectedPlanet",2),0,int(u.planets(s.viewStar).size())-1);s.activePlanet=clamp(j.value("activePlanet",2),0,int(u.planets(s.activeStar).size())-1);
 auto mode=j.value("mode",std::string("galaxy"));if(mode!="galaxy"&&mode!="system"&&mode!="surface"&&mode!="ocean"&&mode!="flight")throw std::runtime_error("Invalid view mode");s.mode=mode=="galaxy"?Mode::Galaxy:mode=="system"?Mode::System:mode=="surface"?Mode::Surface:mode=="flight"?Mode::Flight:Mode::Ocean;
 auto layer=j.value("expeditionMode",mode=="ocean"?std::string("ocean"):std::string("surface"));
 if(layer!="surface"&&layer!="ocean")throw std::runtime_error("Invalid expedition layer");s.expeditionMode=layer=="ocean"?Mode::Ocean:Mode::Surface;
 if(s.mode==Mode::Surface||s.mode==Mode::Ocean)s.expeditionMode=s.mode;
 const auto& g=j.at("galaxy");s.galaxy=readPos(g);s.galaxyZoom=number(g,"z",.74,.2,4.5);
 const auto& v=j.at("surface");s.surface=readPos(v);s.waypoint={number(v,"tx",s.surface.x,-1e7,1e7),number(v,"ty",s.surface.y,-1e7,1e7)};
 const auto& o=j.at("ocean");s.oceanX=number(o,"x",0,-1e7,1e7);s.depth=number(o,"depth",75,0,15000);s.oceanAtY=number(o,"atY",0,-1e7,1e7);s.oceanTargetX=number(o,"tx",s.oceanX,-1e7,1e7);s.targetDepth=number(o,"tDepth",s.depth,0,15000);
 s.maxFuel=number(j,"maxFuel",240,1,100000);s.fuel=number(j,"fuel",190,0,s.maxFuel);s.hull=number(j,"hull",100,0,100);s.battery=number(j,"battery",100,0,100);s.hours=number(j,"hours",0,0,1e12);s.totalDistance=number(j,"totalDistance",0,0,1e15);s.turns=int(number(j,"turns",0,0,1e9));
 const char* resources[]={"metal","crystal","organic","data"};for(int i=0;i<4;i++){s.resources[i]=int(number(j.at("resources"),resources[i],0,0,1e8));s.upgrades[i]=int(number(j.at("upgrades"),upgradeDefs[i].key,0,0,upgradeDefs[i].max));}
 s.visitedStars.clear();s.visitedPlanets.clear();
 for(auto& id:j.value("visitedStars",J::array())){if(!id.is_string()||id.get<std::string>().size()>100||s.visitedStars.size()>20000)throw std::runtime_error("Invalid stellar catalogue");s.visitedStars.insert(id.get<std::string>());}s.visitedStars.insert(s.currentStar);
 for(auto& id:j.value("visitedPlanets",J::array())){if(!id.is_string()||id.get<std::string>().size()>120||s.visitedPlanets.size()>20000)throw std::runtime_error("Invalid planet catalogue");s.visitedPlanets.insert(id.get<std::string>());}
 auto scanned=j.value("scanned",J::object());if(!scanned.is_object()||scanned.size()>100000)throw std::runtime_error("Invalid survey catalogue");for(auto it=scanned.begin();it!=scanned.end();++it)if(it.value()!=0)s.scanned.insert(it.key());
 auto loc=j.value("locations",J::object());if(!loc.is_object()||loc.size()>20000)throw std::runtime_error("Invalid landing catalogue");for(auto it=loc.begin();it!=loc.end();++it)s.locations[it.key()]=readPos(it.value());
 auto discoveries=j.value("discovered",J::array());if(!discoveries.is_array()||discoveries.size()>900)throw std::runtime_error("Invalid discovery log");for(auto& d:discoveries)s.discovered.push_back({text(d,"id",""),text(d,"title",""),text(d,"kind",""),text(d,"planet",""),text(d,"star",""),text(d,"description",""),number(d,"depth",0,0,15000),number(d,"hours",0,0,1e12)});
 auto logs=j.value("log",J::array());if(!logs.is_array()||logs.size()>120)throw std::runtime_error("Invalid ship log");for(auto& l:logs)s.log.push_back({number(l,"time",0,0,1e12),text(l,"msg","")});
 auto flags=j.value("flags",J::object());s.signals=int(number(flags,"signals",0,0,1e8));s.relics=int(number(flags,"relics",0,0,1e8));
 if(j.contains("economy"))s.economy=decodeEconomy(j.at("economy"),u,s.hours);
 if(j.contains("flight"))s.flight=decodeFlight(j.at("flight"),s,u);else if(s.mode==Mode::Flight)throw std::runtime_error("Missing flight state");
 if((s.flight.deployed||s.mode==Mode::Surface||s.mode==Mode::Ocean)&&(s.activeStar!=s.currentStar||u.planet(s.activeStar,s.activePlanet).giant))throw std::runtime_error("Expedition has no accessible local surface");
 dest=std::move(s);error.clear();return true;
 }catch(const std::exception& e){error=e.what();return false;}}
std::string defaultSavePath(){const char* home=std::getenv("HOME");if(!home)return "hyperion-save.json";
#ifdef __APPLE__
 return std::string(home)+"/Library/Application Support/Hyperion/save.json";
#elif defined(_WIN32)
 const char* appdata=std::getenv("APPDATA");return std::string(appdata?appdata:home)+"/Hyperion/save.json";
#else
 const char* xdg=std::getenv("XDG_DATA_HOME");std::string base=xdg?std::string(xdg):std::string(home)+"/.local/share";return base+"/hyperion/save.json";
#endif
}
}
