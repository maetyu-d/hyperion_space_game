#include "universe.hpp"
#include <sstream>
#include <stdexcept>
namespace hyperion {
// Browser seeds are hashed as UTF-16 code units, including surrogate pairs.
uint32_t stringHash(const std::string& s){uint32_t h=2166136261u;auto unit=[&](uint32_t v){h^=v;h*=16777619u;};
 for(size_t i=0;i<s.size();){uint32_t cp=static_cast<unsigned char>(s[i++]);int extra=cp<128?0:cp<224?1:cp<240?2:3;
  if(extra){cp&=(1u<<(6-extra))-1;for(int k=0;k<extra&&i<s.size();k++)cp=(cp<<6)|(static_cast<unsigned char>(s[i++])&63u);}
  if(cp<=0xffff)unit(cp);else{cp-=0x10000;unit(0xd800+(cp>>10));unit(0xdc00+(cp&1023));}}
 return h;}
double Random::operator()(){x+=0x6D2B79F5u;uint32_t t=x;t=(t^(t>>15))*(t|1u);t^=t+((t^(t>>7))*(t|61u));return (t^(t>>14))/4294967296.;}
void Universe::reset(const std::string& s,int generation,const std::set<std::string>& legacy){if(generation<1||generation>2)throw std::runtime_error("Unsupported procedural generation version");seed_=stringHash(s);generation_=generation;legacy_=legacy;stars_.clear();systems_.clear();minorSystems_.clear();}
uint32_t Universe::hash(int32_t a,int32_t b,int32_t c,int32_t d)const{
 uint32_t h=seed_^(uint32_t(a)*374761393u)^(uint32_t(b)*668265263u)^(uint32_t(c)*2246822519u)^(uint32_t(d)*3266489917u);
 h^=h>>15;h*=2246822519u;h^=h>>13;h*=3266489917u;h^=h>>16;return h;
}
double Universe::noise(double x,double y,int32_t s)const{
 int32_t ix=int32_t(std::floor(x)),iy=int32_t(std::floor(y));double fx=x-ix,fy=y-iy;fx=fx*fx*(3-2*fx);fy=fy*fy*(3-2*fy);
 double a=rnd(ix,iy,s)*(1-fx)+rnd(ix+1,iy,s)*fx,b=rnd(ix,iy+1,s)*(1-fx)+rnd(ix+1,iy+1,s)*fx;return (a*(1-fy)+b*fy)*2-1;
}
double Universe::fbm(double x,double y,int32_t s,int oct)const{double a=0,v=0,k=.55;for(int i=0;i<oct;i++){v+=noise(x,y,s+i*97)*k;a+=k;x*=2.02;y*=2.02;k*=.51;}return v/a;}
namespace {
const std::vector<std::string> syllA={"Al","An","Ar","Ash","Ba","Bel","Cal","Car","Cen","Cor","Da","Del","Dor","Eid","El","En","Fal","Fen","Gal","Hel","Ira","Is","Jar","Ka","Kel","Kor","Lar","Ly","Mar","Mer","Nar","Ner","Nor","Or","Os","Pel","Per","Qua","Ra","Ren","Sar","Sel","Sol","Tal","Ter","Thal","Ul","Val","Var","Vel","Ver","Vor","Wen","Xan","Yar","Zen"};
const std::vector<std::string> syllB={"adir","alis","ara","aris","astra","axis","eon","eron","esh","ian","idor","ilis","ion","ira","is","iv","mere","nora","ora","oris","os","oth","ova","rium","sol","tan","th","thea","tis","tor","um","una","ux","yr"};
const char* roman[]={"I","II","III","IV","V","VI","VII","VIII","IX","X","XI","XII"};
std::string name(Random& r){auto a=syllA[size_t(r()*syllA.size())];return a+syllB[size_t(r()*syllB.size())];}
double roundTo(double v,double d){return std::round(v*d)/d;}
const std::map<std::string,std::array<std::string,3>> climates={
 {"ocean",{"Pelagic","Archipelago","Thalassic"}},{"temperate",{"Temperate","Boreal","Continental"}},
 {"arid",{"Arid","Salt desert","Windswept"}},{"frozen",{"Cryogenic","Icebound","Glacial"}},
 {"volcanic",{"Basaltic","Volcanic","Fumarolic"}},{"toxic",{"Corrosive","Sulphurous","Caustic"}},
 {"gas",{"Gas giant","Ammonia giant","Gas giant"}},{"rocky",{"Airless","Cratered","Regolith"}},
 {"jungle",{"Rainforest","Cloud forest","Biotic"}}};
const char* formations[]={"Silicate mantle","Carbonaceous crust","Basalt terraces","Nickel-iron core","Hydrothermal mantle","Metamorphic plate","Subglacial shell","Porous carbonate"};
}
const std::vector<Star>& Universe::cell(int cx,int cy){
 auto key=std::make_pair(cx,cy);auto found=stars_.find(key);if(found!=stars_.end())return found->second;
 Random r{hash(cx,cy,198)};std::vector<Star> list;
 double shape=noise(cx/22.,cy/22.,11)*.33+noise(cx/6.,cy/6.,12)*.18;
 int count=clamp(int(std::floor(1.6+r()*2.2+shape*2.2)),0,5);
 if(cx==0&&cy==0){Star s;s.id="HOME";s.name="MERIDIAN";s.type="G";s.size=1.09;s.age=4.9;s.metallicity=.72;s.luminosity=1.15;s.numPlanets=6;s.anomaly=true;s.seed=hash(0,0,948);list.push_back(s);}
 for(int i=0;i<count;i++){
  double x=(cx+(r()-.5)*.87)*182,y=(cy+(r()-.5)*.87)*182;if(cx==0&&cy==0&&std::hypot(x,y)<44)continue;
  Star s;s.cx=cx;s.cy=cy;s.pos={x,y};s.id=std::to_string(cx)+","+std::to_string(cy)+","+std::to_string(i);
  double v=r();s.type=v<.015?"O":v<.06?"B":v<.14?"A":v<.30?"F":v<.51?"G":v<.75?"K":"M";s.name=name(r);
  s.size=clamp(.65+r()*1.38,.5,2.2);s.age=roundTo(.1+r()*12.4,10);s.metallicity=roundTo(.1+r()*.9,100);s.luminosity=roundTo(.08+r()*7.2,100);
  s.numPlanets=2+int(r()*9);s.anomaly=r()<.09;s.seed=hash(cx,cy,i,17);
  if(generationFor(s.id)>=2){Random physics{s.seed^0x512acdu};double base=s.type=="O"?22:s.type=="B"?12:s.type=="A"?5:s.type=="F"?2.1:s.type=="G"?1:s.type=="K"?.42:.12;s.luminosity=roundTo(base*(.7+physics()*.6),100);s.size=roundTo(std::pow(base,.24)*(.85+physics()*.3),100);s.age=roundTo((s.type=="O"||s.type=="B"?.2:s.type=="A"?2.:12.)*(.15+physics()*.85),100);}
  list.push_back(s);
 }
 if(stars_.size()>6000)stars_.clear();return stars_.emplace(key,std::move(list)).first->second;
}
Star Universe::star(const std::string& id){if(id=="HOME")return cell(0,0).front();int cx=0,cy=0,i=0;char a=0,b=0;std::istringstream in(id);
 if(in>>cx>>a>>cy>>b>>i&&a==','&&b==','&&std::abs(double(cx))<1000000&&std::abs(double(cy))<1000000){for(auto& s:cell(cx,cy))if(s.id==id)return s;}return cell(0,0).front();}
const std::vector<Planet>& Universe::planets(const std::string& id){auto f=systems_.find(id);if(f!=systems_.end())return f->second;
 Star s=star(id);
 if(generationFor(id)>=2&&id!="HOME"){auto list=generatePlanets(s,seed_,profile(id));if(systems_.size()>800)systems_.clear();return systems_.emplace(id,std::move(list)).first->second;}
 Random r{s.seed^seed_^0x9abcu};std::vector<Planet> list;
 for(int i=0;i<s.numPlanets;i++){
  Planet p;p.starId=id;p.index=i;p.id=id+"#"+std::to_string(i);p.orbit=.34+i*.47+i*i*.047+r()*.16;
  p.temp=int(std::round(370*std::pow(s.luminosity,.21)/std::sqrt(p.orbit)+r()*75-45));
  p.size=roundTo(.3+r()*2.4,100);p.gravity=roundTo(p.size*.45+r()*.65,100);
  bool giant=(p.size>1.9&&r()>.27)||r()<.045;
  p.water=clamp(r()*.93+(p.temp<305&&p.temp>205?.16:0)-(p.temp>450?.58:0),0.,1.);
  p.pressure=giant?100+std::round(r()*1000):roundTo(r()*1.75+(p.water>.45?.25:0),100);
  p.life=(!giant&&p.temp>205&&p.temp<365&&p.water>.26)?clamp(r()*.81+(p.water>.7?.12:0),0.,1.):r()*.085;
  p.tectonic=r();p.metal=1+int(r()*9);p.hazards=r();
  p.type=giant?"gas":p.temp>580?"volcanic":p.temp<160?"frozen":p.pressure<.18?"rocky":p.temp>410?"arid":p.water>.68?"ocean":p.water>.43&&p.life>.5?"jungle":p.water>.32?"temperate":p.hazards>.7?"toxic":"arid";
  p.name=s.name+" "+roman[i];
  if(id=="HOME"){
   const char* home[]={"CINDER","CALLOW","NACRE","VESPERS","THE GLASS","OUTERWARD"};p.name=home[i];
   if(i==2){p.type="ocean";p.water=.82;p.pressure=1.1;p.gravity=.89;p.size=1.01;p.life=.72;p.hazards=.29;p.temp=289;}
   if(i==1){p.type="arid";p.water=.08;p.life=.03;}if(i==4){p.type="gas";p.pressure=260;p.water=0;}
  }
  p.seed=hash(int32_t(s.seed),i,930);p.cloud=roundTo(r(),100);p.wind=int(std::round(3+r()*78));p.rad=int(std::round(r()*430));p.salinity=roundTo(1.6+r()*10.7,10);
  p.axialTilt=int(std::round(r()*73));p.period=int(std::round(9+std::pow(p.orbit,1.5)*18));p.moonCount=int(r()*5);p.formation=formations[int(r()*8)];p.phase=r()*tau;p.anomaly=r()<.09||(id=="HOME"&&i==2);
  p.bio=p.life>.65?"Abundant":p.life>.35?"Complex":p.life>.13?"Microbial":p.life>.03?"Trace":"Undetected";
  p.dominant=climates.at(p.type)[size_t(r()*3)];p.giant=p.type=="gas";
  p.generation=generationFor(id);if(p.generation>=2){p.landform=p.type=="arid"?2:p.type=="rocky"?3:p.type=="volcanic"?4:p.type=="frozen"?5:p.type=="toxic"?6:p.type=="ocean"?1:0;p.formation=p.giant?"Sheared atmospheric jets":proc::formation(p.landform);}p.oceanMax=int(p.type=="ocean"?4500+p.water*4200:1600+p.water*5900);list.push_back(p);
 }
 if(systems_.size()>800)systems_.clear();return systems_.emplace(id,std::move(list)).first->second;
}
const std::vector<MinorBody>& Universe::minorBodies(const std::string& id){auto found=minorSystems_.find(id);if(found!=minorSystems_.end())return found->second;auto list=generateMinorBodies(*this,star(id),planets(id));if(minorSystems_.size()>800)minorSystems_.clear();return minorSystems_.emplace(id,std::move(list)).first->second;}
Planet Universe::planet(const std::string& id,int i){auto& a=planets(id);return a[size_t(clamp(i,0,int(a.size())-1))];}
double Universe::elevation(const Planet& p,double x,double y)const{
 if(p.generation>=2)return proc::elevation(spec(p),proc::chart(x,y));
 uint32_t s=p.seed;double broad=fbm((x+s%1731)*.0032,(y+s%937)*.0032,s%8000,4),hills=fbm(x*.0205,y*.0205,s%7000+43,3),ridge=std::abs(noise(x*.0052+77,y*.0052-34,s%211+55));
 return clamp(.50+broad*.33+hills*.105+ridge*.085+std::sin(x*.0027+y*.0013+s*.000001)*.075,0.,1.);
}
double Universe::floorDepth(const Planet& p,double x,double y)const{
 if(p.generation>=2){auto r=region(p,{x,y});double shelf=clamp((seaLevel(p)-r.elevation)*7,0.,1.);double trench=std::pow(r.geothermal,3)*p.oceanMax*.32;return clamp(620+shelf*p.oceanMax*.62+trench,620.,double(p.oceanMax));}
 uint32_t s=p.seed;double base=1000+std::max(0.,fbm(x*.004,s*.000021,s%191+1,4)+.85)*2300;
 double f=std::abs(noise(x*.0018,3.71,s%519+52)),trench=f>.44?std::pow((f-.44)/.56,1.6)*5000:0;return clamp(base+trench,620.,double(p.oceanMax));}
V2 Universe::shoreline(const Planet& p)const{
 if(p.generation>=2&&p.water>=.15){Random r{p.seed^8712u};V2 best{};double error=10;
  for(int i=0;i<384;i++){V2 at{(r()-.5)*12000,(r()-.5)*12000};double e=std::abs(elevation(p,at.x,at.y)-seaLevel(p)-.003);if(e<error){best=at;error=e;}}
  for(double span:{400.,120.,35.,10.})for(int i=0;i<24;i++){V2 at{best.x+(r()-.5)*span,best.y+(r()-.5)*span};double e=std::abs(elevation(p,at.x,at.y)-seaLevel(p)-.003);if(e<error){best=at;error=e;}}return {std::round(best.x),std::round(best.y)};
 }
 if(p.water<.15)return {};Random r{p.seed^8712u};V2 best;double error=10;
 for(int i=0;i<240;i++){double x=(r()-.5)*900,y=(r()-.5)*900,e=std::abs(elevation(p,x,y)-seaLevel(p));if(e<error){best={std::round(x),std::round(y)};error=e;if(e<.004)break;}}return best;}
std::string Universe::terrain(const Planet& p,V2 v)const{if(p.generation>=2)return region(p,v).biome;double e=elevation(p,v.x,v.y),s=seaLevel(p);return e<s-.06?"DEEP WATER":e<s?"SHALLOWS":e<s+.028?"LITTORAL":e>.79?"HIGHLANDS":p.type=="frozen"?"GLACIAL PLAIN":p.type=="arid"?"DRY BASIN":p.type=="volcanic"?"BASALT FIELD":p.type=="jungle"?"CANOPY":p.life>.4?"VEGETATION":"STONEPLAIN";}
std::string atmosphere(const Planet& p){return p.pressure<.1?"Trace / absent":p.pressure<.65?"Thin nitrogen mix":p.type=="toxic"?"Corrosive aerosols":p.type=="frozen"?"Nitrogen / methane":p.water>.35?"Nitrogen / oxygen":"Dense carbon dioxide";}
std::string oceanZone(double d){return d<200?"EPIPELAGIC":d<1000?"MESOPELAGIC":d<4000?"BATHYPELAGIC":d<6000?"ABYSSOPELAGIC":"HADOPELAGIC";}
std::string objectName(const std::string& kind,uint32_t n){Random r{n};
 if(kind=="life"||kind=="flora"){
  const char* a[]={"bio","endo","halo","hyper","meso","proto","abyss","hydro","xeno","thermo","chrono","luma","paleo","rhizo","bathyo"};
  const char* b[]={"cera","dera","glyph","lith","mora","nema","phore","phyll","poda","pora","soma","spira","thrix","tide","vora","zoa"};
  const char* c[]={"minor","major","noctis","pelagi","lucens","abyssi","nacrei","undulata","obscura","vagans"};
  std::string first=a[int(r()*15)];first+=b[int(r()*16)];return first+" "+c[int(r()*10)];
 }
 const std::map<std::string,std::vector<std::string>> names={
 {"mineral",{"Ferric needle field","Obsidian outcrop","Cobalt-rich nodules","Crystalline lattice","Lithium salt diapir","Serrated basalt","Piezoelectric seam","Manganese crust","Iridium spheroid","Prismatic sulphide"}},
 {"signal",{"Interval transmission","Pulsed dark beacon","Incoherent carrier","Nine-tone sequence","Counterphase pulse","Nonlocal echo","Aperiodic resonator","Repeating impulse"}},
 {"relic",{"Collapsed arcology","Standing resonator","Buried observatory","Geometric wreck","Null inscription","Architectural void","Unpowered lattice"}},
 {"vent",{"Black smoker vent","Iron chimney","Alkaline seep","Sulphur column","Superheated fissure"}}};
 auto f=names.find(kind);return f==names.end()?"Unresolved anomaly":f->second[size_t(r()*f->second.size())];
}
std::string description(const Contact& o,const Planet& p){
 const char* adjectives[]={"unusually stable","highly irregular","nested","partly buried","fractally repeated","short-lived","slowly rotating","unclassified","spatially distributed","densely interlinked"};std::string a=adjectives[o.n%10];
 if(o.kind=="signal")return "A "+a+" low-frequency sequence. The timing is precise, but no physical transmitter is immediately evident.";
 if(o.kind=="relic")return "An "+a+" structure whose composition and geometry cannot be reconciled with local geological processes.";
 if(o.kind=="life")return "An "+a+" organism detected in "+p.dominant+" conditions. Motility and metabolism vary with temperature and pressure.";
 if(o.kind=="flora")return "A "+a+" autotrophic colony with site-specific pigmentation and light-response dynamics.";
 if(o.kind=="vent")return "A "+a+" hydrothermal discharge, depositing sulphide minerals and sustaining chemically distinct niches.";
 return "A "+a+" geological concentration; useful trace elements are accessible for laboratory and fabrication work.";
}
std::vector<Contact> Universe::surfaceObjects(const Planet& p,int cx,int cy)const{if(p.generation>=2)return generateSurface(*this,p,cx,cy);Random r{hash(cx,cy,int32_t(p.seed))};int count=1+int(r()*3);std::vector<Contact> a;
 for(int i=0;i<count;i++){Contact o;o.pos.x=cx*86+8+r()*70;o.pos.y=cy*86+8+r()*70;bool water=elevation(p,o.pos.x,o.pos.y)<seaLevel(p);double q=r();
  o.kind=q<.32?"mineral":q<.59&&p.life>.14?(water?"life":"flora"):q<.67?"relic":q<.75?"signal":q<.82&&water?"vent":"mineral";
  o.n=hash(cx,cy,i,int32_t(p.seed));o.id="S:"+p.id+":"+std::to_string(cx)+":"+std::to_string(cy)+":"+std::to_string(i);o.name=objectName(o.kind,o.n);o.desc=description(o,p);a.push_back(o);
 }return a;
}
std::vector<Contact> Universe::oceanObjects(const Planet& p,int cx,double y)const{if(p.generation>=2)return generateOcean(*this,p,cx,y);Random r{hash(cx,int32_t(p.seed),898)};int count=6+int(r()*5);std::vector<Contact> a;
 for(int i=0;i<count;i++){Contact o;o.pos.x=cx*130+(i==0?104+r()*19:8+r()*115);double floor=floorDepth(p,o.pos.x);
  o.depth=i==0?80+r()*170:i==1?230+r()*550:i==2?600+r()*950:40+r()*(floor-80);o.depth=std::min(o.depth,floor-45);double q=r();
  o.kind=q<.42?"life":q<.59?"mineral":q<.72?"vent":q<.83?"signal":q<.89?"relic":"life";
  if(o.kind=="vent")o.depth=clamp(floor-60-r()*170,50.,floor-30);
  o.n=hash(cx,i,int32_t(p.seed),87);o.id="O:"+p.id+":"+std::to_string(cx)+":"+std::to_string(i);o.name=objectName(o.kind,o.n);o.desc=description(o,p);a.push_back(o);
 }return a;
}
}
