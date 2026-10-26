#include "game.hpp"
#include <sstream>
#include <iomanip>
namespace hyperion {
namespace {
double separation(const State& s,const Contact& c){return std::hypot((c.pos.x-s.oceanX)*5,c.depth-s.depth);}
int category(const Contact& c){return c.kind=="life"||c.kind=="flora"?0:c.kind=="signal"||c.kind=="relic"?2:1;}
}
std::vector<Contact> Game::diveSites(){
 if(!hasExpedition()||activePlanet().water<.2)return {};
 const V2 origin=diveState.star==s.activeStar&&diveState.planet==s.activePlanet?diveState.origin:V2{s.surface.x,s.oceanAtY};
 auto p=activePlanet();std::array<Contact,3> best;std::array<double,3> score{1e20,1e20,1e20};int cell=int(std::floor(origin.x/130));
 for(int x=cell-5;x<=cell+5;x++)for(const auto& c:universe.oceanObjects(p,x,origin.y)){
  if(objectIntegrity(s.economy.worldDamage,"field:"+c.id)<=0)continue;
  if(c.depth<25||c.depth>maxDepth()-25||c.depth>universe.floorDepth(p,c.pos.x,origin.y)-30)continue;
  int group=category(c);double cost=std::hypot((c.pos.x-origin.x)/(19+s.upgrades[3]*6),(c.depth-45)/185.);
  // Prefer useful descents over a shallow duplicate, and uncharted evidence.
  double rank=cost+(c.depth<150?3:0)+(s.scanned.count(c.id)?10000:0);
  if(rank<score[group]){score[group]=rank;best[group]=c;}
 }
 std::vector<Contact> out;for(const auto& c:best)if(!c.id.empty())out.push_back(c);
 std::stable_sort(out.begin(),out.end(),[&](const auto& a,const auto& b){auto rank=[&](const auto& c){return (s.scanned.count(c.id)?10000.:0.)+std::hypot((c.pos.x-origin.x)/19,(c.depth-45)/185.)+(c.depth<150?3:0);};return rank(a)<rank(b);});return out;
}
void Game::beginDive(bool plot){
 if(s.mode!=Mode::Ocean)return;
 bool same=diveState.star==s.activeStar&&diveState.planet==s.activePlanet&&distance(diveState.origin,{s.oceanX,s.oceanAtY})<2&&!diveState.finished;
 if(!same){diveState={};diveState.star=s.activeStar;diveState.planet=s.activePlanet;diveState.origin={s.surface.x,s.oceanAtY};diveState.started=s.hours;diveState.initial=shipStock();diveState.initialHull=s.hull;auto sites=diveSites();if(!sites.empty())diveState.goal=sites.front();}
 if(plot){discoverOceanWreck();diveReport={};diveState.started=s.hours;diveState.initial=shipStock();diveState.initialHull=s.hull;diveState.studies=diveState.collections=0;diveState.returning=false;}
 diveState.active=true;
 if(plot&&!planMove().valid&&!diveState.goal.id.empty()){s.oceanTargetX=diveState.goal.pos.x;s.targetDepth=diveState.goal.depth;focusedContact=diveState.goal.id;}
}
bool Game::oceanKnown(const Contact& c)const{return s.scanned.count(c.id)||diveState.revealed.count(c.id)||separation(s,c)<=65;}
double Game::diveAlert()const{return .15+.85*clamp(1-(s.hours-diveState.lastPing)/3,0.,1.);}
std::vector<Contact> Game::oceanContacts(){auto all=targets();all.erase(std::remove_if(all.begin(),all.end(),[&](const auto& c){return !oceanKnown(c);}),all.end());std::stable_sort(all.begin(),all.end(),[&](const auto& a,const auto& b){double da=separation(s,a),db=separation(s,b);return da!=db?da<db:a.id<b.id;});return all;}
Contact Game::nextDiveContact(){
 Contact best;double score=1e20;if(s.mode!=Mode::Ocean||s.hull<45||stockTotal(shipStock())>=cargoCapacity()-1)return best;
 for(const auto& c:oceanContacts())if(!s.scanned.count(c.id)&&c.id!=diveState.goal.id&&c.depth<=maxDepth()&&separation(s,c)<=1.5*(260+s.upgrades[1]*190)){
  double rank=separation(s,c)*(category(c)==category(diveState.goal)?1.15:1.);if(rank<score){score=rank;best=c;}
 }
 return best;
}
bool Game::diveReturnNeeded()const{
 if(!hasExpedition()||expeditionLayer()!=Mode::Ocean)return false;
 double origin=diveState.active&&diveState.star==s.activeStar&&diveState.planet==s.activePlanet?diveState.origin.x:s.surface.x;
 return s.depth>80||std::abs(s.oceanX-origin)>8;
}
DiveBrief Game::diveBrief(){
 DiveBrief b;if(s.mode!=Mode::Ocean)return b;if(!diveState.active)beginDive(false);
 b.goal=diveState.goal;for(const auto& c:targets())if(c.id==b.goal.id){b.goal=c;break;}
 b.hasGoal=!b.goal.id.empty();b.range=260+s.upgrades[1]*190;b.nearSurface=s.depth<=80;b.atBuoy=std::abs(s.oceanX-diveState.origin.x)<=8;
 b.homeTurns=int(std::ceil(std::hypot((s.oceanX-diveState.origin.x)/(19+s.upgrades[3]*6),(s.depth-35)/185.)));
 if(b.atBuoy&&b.nearSurface)b.homeTurns=0;
 b.distance=b.hasGoal?separation(s,b.goal):0;b.known=b.hasGoal&&oceanKnown(b.goal);b.studied=b.hasGoal&&s.scanned.count(b.goal.id);
 b.custom=planMove().valid&&!diveState.returning&&b.hasGoal&&(std::abs(s.oceanTargetX-b.goal.pos.x)>.1||std::abs(s.targetDepth-b.goal.depth)>.1);
 if(s.depth>maxDepth()){b.phase=4;b.instruction="PRESSURE DANGER / ascend one turn now";}
 else if(diveState.returning){b.phase=3;b.instruction=b.atBuoy&&b.nearSurface?"RECOVER / Enter or E / end the dive":"RETURN / Enter one step / K up to six steps";}
 else if(b.custom){b.phase=0;b.instruction="MANUAL TRANSECT / Enter advances your plotted route";}
 else if(b.studied){b.phase=3;b.instruction="Study recorded / choose another contact or return";}
 else if(!b.hasGoal){b.phase=0;b.instruction="EXPLORE / plot a route and use sonar to find a site";}
 else if(b.distance>b.range){b.phase=0;b.instruction="APPROACH / Enter / advance one turn toward the survey site";}
 else if(!b.known){b.phase=1;b.instruction="SONAR / Space / reveal the site before investigating";}
 else{b.phase=2;b.instruction="INVESTIGATE / Enter / study the site; collection is optional";}
 return b;
}
DiveSiteInfo Game::diveSiteInfo(const Contact& c)const{
 DiveSiteInfo info;int group=category(c);info.role=c.kind=="wreck"?"WRECK":group==0?"BIOLOGY":group==1?"GEOLOGY":"SIGNAL";
 info.purpose=c.kind=="wreck"?"Survey the hull, rescue survivors and recover finite supplies. Deliver them to the owning settlement.":group==0?"Research a living habitat. Optional biomass harvest affects its population.":group==1?"Research the formation. Optional ore and crystal collection supplies industry.":"Investigate a transmission or relic for research and mission evidence.";
 const double speed=19+s.upgrades[3]*6,span=separation(s,c),reach=260+s.upgrades[1]*190;
 double travel=std::hypot((c.pos.x-s.oceanX)/speed,(c.depth-s.depth)/185.);
 info.approach=int(std::ceil(travel*std::max(0.,1-reach/std::max(1.,span))));
 const V2 origin=diveState.active&&diveState.star==s.activeStar&&diveState.planet==s.activePlanet?diveState.origin:V2{s.surface.x,s.oceanAtY};
 info.home=int(std::ceil(std::hypot((c.pos.x-origin.x)/speed,(c.depth-35)/185.)));
 info.shell=int(std::ceil(c.depth/maxDepth()*100));return info;
}
DiveOrder Game::nextDiveOrder(){
 DiveOrder q;if(s.mode!=Mode::Ocean){q.text="Deploy a submersible first";return q;}auto b=diveBrief();q.available=true;
 if(b.phase==4){q.action="oceanAscend";q.label="Ascend / 1 h";q.text="One physical ascent step. Escape pressure exposure before doing other work.";}
 else if(b.phase==3){if(b.nearSurface&&b.atBuoy){q.action="surface";q.label=b.studied?"Recover survey / 1 h":"End dive / 1 h";q.text=b.studied?"Recover at the launch buoy and record this survey.":"Recover safely. The selected survey objective remains unrecorded.";}else{q.action="oceanHomeStep";q.label="Return one step / 1 h";q.text="One normal movement step toward the launch buoy. K follows up to six steps.";}}
 else if(b.phase==0){q.action="oceanAdvance";q.label="Advance course / 1 h";q.available=planMove().valid||b.hasGoal;q.text=q.available?"One normal movement step. Stop in survey range to ping or study.":"Choose a survey site or plot a waypoint first.";}
 else if(b.phase==1){q.action="scan";q.label="Ping sonar / 1 h";q.text="12 battery. Reveal signatures without taking specimens or ore.";}
 else{q.action="fieldStudy";q.arg=b.goal.id;q.label="Study site / 1 h";auto field=fieldQuote(q.action,q.arg);q.available=field.available;q.text=field.available?field.text:field.reason;}
 if(b.phase==3&&b.studied&&!diveState.returning&&rescuedCrew()==0&&b.goal.kind!="wreck"){auto c=nextDiveContact();if(!c.id.empty()){q.action="oceanNext";q.arg=c.id;q.label="Next contact / free";q.hours=0;q.text=c.name+" / choose this resolved contact without spending time. C samples the current site; K returns home.";}}
 if(b.goal.kind=="wreck"&&b.studied&&!diveState.returning&&!b.custom&&b.phase!=4){
  if(auto* w=findOceanWreck(s.economy,b.goal.id)){
   if(b.distance>80){q.action="oceanAdvance";q.arg.clear();q.label="Approach hull / 1 h";q.available=true;q.text="Wreck entry and salvage require 80 survey units / Enter advances one step";}
   else if(w->crew>0&&rescuedCrew()<6){auto rescue=wreckQuote("wreckRescue",w->contact.id);q.action="wreckRescue";q.arg=w->contact.id;q.label="Rescue crew / 1 h";q.available=rescue.available;q.text=rescue.available?rescue.text:rescue.reason;}
   else if(rescuedCrew()==0){auto take=wreckQuote("wreckSalvage",w->contact.id);if(take.available){q.action="wreckSalvage";q.arg=w->contact.id;q.label="Recover supplies / 1 h";q.available=true;q.text=take.text;}}
  }
 }
 if((q.action=="scan"&&s.battery<12)||(q.action=="fieldStudy"&&s.battery<6)){
  auto field=fieldQuote("fieldRecharge");q.action="fieldRecharge";q.arg.clear();q.available=field.available;q.hours=field.move.turns;q.label=field.available?"Recharge / up to "+std::to_string(q.hours)+" h":"Recharge unavailable";q.text=field.available?field.text:field.reason;
 }
 if(q.action=="oceanAdvance"||q.action=="oceanHomeStep"){
  auto move=q.action=="oceanHomeStep"?planMove({(diveState.origin.x-s.oceanX)/(19+s.upgrades[3]*6),(35-s.depth)/185.},false):planMove();
  if(move.power>0||move.wear>.01){q.text="Route exposure / "+std::to_string(int(move.power))+" power / "+std::to_string(move.wear).substr(0,4)+" hull / "+q.text;if(s.battery<move.power){q.available=false;q.text="Headwind needs "+std::to_string(int(move.power))+" power / N waits to recharge; E ascends";}}
 }
 if(q.action=="oceanHomeStep")for(const auto& e:upcoming(1))if(e.priority!=Notice::Status){q.text+=" A deadline falls within this hour.";break;}
 return q;
}
int Game::oceanSonar(bool quiet){
 if(s.mode!=Mode::Ocean)return 0;int cost=quiet?2:12;if(s.battery<cost){notify("Sonar needs "+std::to_string(cost)+" battery / wait or move normally");return 0;}if(!diveState.active)beginDive(false);
 discoverOceanWreck();double before=s.battery;s.battery-=cost;radar=0;diveState.pingRange=quiet?140+s.upgrades[1]*100:260+s.upgrades[1]*190;int found=0;for(const auto& c:targets())if((!quiet||c.kind!="mineral")&&separation(s,c)<=(quiet?140+s.upgrades[1]*100:260+s.upgrades[1]*190)&&diveState.revealed.insert(c.id).second)++found;
 if(!quiet)diveState.lastPing=s.hours+1;
 // A ping charts signatures, never mines deposits or takes living samples.
 advanceTime(1,true);lastTurn.battery=s.battery-before;std::string result=(quiet?"Quiet listening / ":"Sonar / ")+std::to_string(found)+" new signature(s) / "+(quiet?"life, vents and signals only":"wider returns; nearby fauna startled");lastTurn.events.insert(lastTurn.events.begin(),result);recentTurns.front()=lastTurn;notify(result);soundEvent="sonar";return found;
}
void Game::finishDive(bool safe){
 if(!diveState.active)return;bool evidence=!diveState.goal.id.empty()&&s.scanned.count(diveState.goal.id);bool home=std::abs(s.oceanX-diveState.origin.x)<=8;
 auto cargo=shipStock();std::string gains;for(int g:{Research,Biomass,Ore,Crystal}){int n=int(std::floor(cargo[g]-diveState.initial[g]+1e-7));if(n>0){if(!gains.empty())gains+=" / ";gains+="+"+std::to_string(n)+" "+goods[g].name;}}
 diveState.finished=evidence&&home&&safe;diveState.active=false;
 diveReport={};diveReport.valid=true;diveReport.complete=diveState.finished;diveReport.emergency=!safe;diveReport.star=diveState.star;diveReport.planet=diveState.planet;diveReport.goal=diveState.goal.name;diveReport.hours=s.hours-diveState.started;diveReport.ended=s.hours;diveReport.hull=s.hull-diveState.initialHull;diveReport.studies=diveState.studies;diveReport.collections=diveState.collections;for(int g=0;g<goodCount;g++)diveReport.delta[g]=cargo[g]-diveState.initial[g];
 if(diveState.finished){s.scanned.insert("DIVE:complete:"+diveState.goal.id);surveyRecall={};}
 diveDebrief=(!safe?"Emergency surface / survey ended":diveState.finished?"Survey recovered at launch buoy":evidence?"Evidence recovered / transect ended away from launch buoy":"Dive ended / survey objective remains unrecorded")+(gains.empty()?std::string{}:" / "+gains);
 if(lastTurn.valid&&!recentTurns.empty()){lastTurn.events.insert(lastTurn.events.begin(),diveDebrief);recentTurns.front()=lastTurn;}log(diveDebrief);notify(diveDebrief,Notice::Important);
}
bool Game::oceanAction(const std::string& action,const std::string& id){
 if(action!="oceanListen"&&action!="oceanNext"&&action!="oceanProceed"&&action!="oceanHomeStep"&&action!="oceanAdvance"&&action!="oceanSelect"&&action!="oceanReturn"&&action!="oceanAscend")return false;
 if(s.mode!=Mode::Ocean){notify("Deploy a submersible first");return true;}if(!diveState.active)beginDive(false);
 if(action=="oceanProceed"){auto q=nextDiveOrder();if(!q.available)notify(q.text);else this->action(q.action,q.arg);return true;}
 if(action=="oceanListen"){oceanSonar(true);return true;}
 if(action=="oceanNext"){auto c=nextDiveContact();if(c.id.empty()||(!id.empty()&&c.id!=id)){notify("No suitable resolved contact / choose a site or return");return true;}diveState.goal=c;diveState.returning=false;waypoint(c);notify("Next survey / "+c.name+" / world frozen");return true;}
 if(action=="oceanSelect"){
  auto sites=diveSites();auto wrecks=wreckSites();sites.insert(sites.end(),wrecks.begin(),wrecks.end());auto contacts=targets();sites.insert(sites.end(),contacts.begin(),contacts.end());for(const auto& c:sites)if(c.id==id){diveState.goal=c;diveState.returning=false;waypoint(c);notify("Dive objective / Study "+c.name+" / then return to the launch buoy");return true;}notify("Choose a site in this transect");return true;
 }
 if(action=="oceanAdvance"){if(!planMove().valid&&!diveState.goal.id.empty()){s.oceanTargetX=diveState.goal.pos.x;s.targetDepth=diveState.goal.depth;}moveTurn({},true);return true;}
 if(action=="oceanAscend"){if(s.depth<=35){notify("Near the surface / E recovers the submersible");return true;}moveTurn({0,-1});notify("Ascent / "+lastTurn.summary());return true;}
 if(s.depth>maxDepth()){notify("Ascend into certified depth before automatic return");return true;}
 if(std::abs(s.oceanX-diveState.origin.x)<=8&&s.depth<=80){notify("At the launch buoy / E recovers the submersible");return true;}
 if(action=="oceanReturn")for(const auto& e:upcoming(1))if(e.priority!=Notice::Status){notify("Deadline imminent / take one manual step before automatic return");return true;}
 s.oceanTargetX=diveState.origin.x;s.targetDepth=35;diveState.returning=true;if(action=="oceanHomeStep")moveTurn({},true);else followFieldRoute();return true;
}
}
