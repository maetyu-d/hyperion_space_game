#include "universe.hpp"
#include "biosphere.hpp"
#include <sstream>
namespace hyperion {
SystemProfile Universe::profile(const std::string& star)const{
 auto h=hash(int32_t(stringHash(star)),7781);SystemProfile p;p.architecture=int(h%6);
 static const char* layouts[]={"Compact planetary chain","Wide giant system","Fragmented inner system","Ocean cradle","Ancient mineral system","Volatile frontier"};
 static const char* frontiers[]={"Unsettled frontier","Extraction enclave","Agrarian network","Exchange corridor","Archive frontier","Contested charters"};
 p.layout=layouts[p.architecture];int society=int((h>>8)%6);p.frontier=frontiers[society];p.settlements=society==0?0:society==1?2:society==2?3:society==3?6:society==4?2:4;
 if(star=="HOME"){p.frontier="Meridian charter network";p.settlements=4;}return p;
}
std::vector<Planet> generatePlanets(const Star& star,uint32_t seed,const SystemProfile& system){
 std::vector<Planet> result;double orbit=std::sqrt(star.luminosity)*(.20+double(star.seed%127)/600.);
 static const char* roman[]={"I","II","III","IV","V","VI","VII","VIII","IX","X","XI","XII"};
 for(int i=0;i<star.numPlanets;i++){
  Planet p;p.generation=2;p.seed=proc::hash(seed,int(star.seed),i,930);Random r{p.seed^0x96a71u};p.starId=star.id;p.index=i;p.id=star.id+"#"+std::to_string(i);p.name=star.name+" "+roman[i];
  double jitter=r();double spacing=system.architecture==0?1.28+jitter*.18:system.architecture==1?1.65+jitter*.26:system.architecture==2?(i==3?2.05+jitter*.35:1.2+jitter*.15):system.architecture==3?1.48+jitter*.20:system.architecture==4?1.52+jitter*.34:1.32+jitter*.50;if(i)orbit*=spacing;p.orbit=std::round(orbit*1000)/1000;
  double relative=orbit/std::sqrt(star.luminosity),volatiles=.1+r()*.7+(system.architecture==3?.23:system.architecture==4?-.18:system.architecture==2?-.10:system.architecture==5?.12:0);
  p.giant=i>1&&relative>1.7&&r()<(system.architecture==1?.72:system.architecture==5?.52:.38);p.size=p.giant?1.9+r()*1.1:.32+r()*1.38;
  p.size=std::round(p.size*100)/100;p.gravity=std::round((p.giant?1.1+p.size*.4:p.size*(.55+star.metallicity*.22)+r()*.18)*100)/100;
  p.pressure=p.giant?80+r()*600:std::max(0.,p.size*p.size*(.12+volatiles*1.5)-r()*.24);if(p.size<.55)p.pressure*=.14;
  double temperature=278/std::sqrt(relative)+p.pressure*14;p.temp=int(std::round(clamp(temperature,55.,1400.)));
  p.water=p.giant?0:clamp(volatiles+(relative>1?.1:0)-(temperature>370?(temperature-370)/240:0),0.,.98);
  p.tectonic=clamp(p.size*.25+r()*.65-star.age*.012,0.,1.);p.metal=clamp(int(std::round(1+star.metallicity*6+r()*2)),1,9);
  p.hazards=clamp((p.pressure>1.8?.3:0)+(p.temp>380?.3:0)+p.tectonic*.18+r()*.35,0.,1.);
  p.life=!p.giant&&p.temp>210&&p.temp<365&&p.water>.2&&p.pressure>.18?clamp((.16+r()*.68)*p.water*1.25,0.,.92):(!p.giant&&p.water>.2&&p.tectonic>.55?r()*.055:0);
  p.type=p.giant?"gas":p.temp>560?"volcanic":p.temp<220?"frozen":p.pressure<.12?"rocky":p.hazards>.68?"toxic":p.temp>365?"arid":p.water>.69?"ocean":p.life>.43&&p.water>.45?"jungle":p.water>.32?"temperate":"arid";
  p.landform=p.type=="volcanic"?4:p.type=="frozen"?5:p.type=="rocky"?3:p.type=="arid"?2:p.type=="toxic"?6:p.type=="ocean"?(r()<.65?1:7):(r()<.7?0:6);
  p.formation=p.giant?"Sheared atmospheric jets":proc::formation(p.landform);p.dominant=p.giant?"Banded giant":p.type=="arid"?"Arid":p.type=="frozen"?"Cryogenic":p.type=="volcanic"?"Basaltic":p.type=="toxic"?"Corrosive":p.type=="rocky"?"Airless":p.type=="ocean"?"Pelagic":p.type=="jungle"?"Cloud forest":"Continental";
  p.bio=p.life>.65?"Abundant":p.life>.35?"Complex":p.life>.13?"Microbial":p.life>.03?"Trace":"Undetected";
  p.cloud=std::round(clamp(p.pressure*.15+p.water*.7+r()*.2,0.,1.)*100)/100;p.wind=int(3+std::min(120.,p.pressure*14)+r()*30);p.rad=int(15+star.luminosity/(relative*relative+1)*25/(.2+p.pressure));
  p.salinity=std::round((1.2+r()*8+(1-p.water)*3)*10)/10;p.axialTilt=int(r()*58);p.period=int(9+std::pow(relative,1.5)*18);p.moonCount=p.giant?3+int(r()*5):int(r()*3);p.phase=r()*tau;p.anomaly=r()<.065;
  p.oceanMax=int(2200+p.water*6500+p.tectonic*1100);result.push_back(p);
 }
 return result;
}
Region Universe::region(const Planet& p,V2 at,double depth)const{
 Region r;auto n=proc::chart(at.x,at.y);r.elevation=elevation(p,at.x,at.y);
 double fault=1-std::abs(proc::noise(p.seed^0x156addu,proc::scaled(n,7.7,3.))*2-1);
 r.geothermal=std::pow(fault,6)*p.tectonic;r.moisture=clamp(p.water*.55+proc::noise(p.seed^0x8213u,proc::scaled(n,4.7,2.))*.65-std::max(0.,r.elevation-seaLevel(p))*.4,0.,1.);
 r.temperature=p.temp+8*(1-std::abs(n.y))-32*std::abs(n.y)-std::max(0.,r.elevation-seaLevel(p))*45;
 if(depth>0)r.temperature=std::max(272.,r.temperature-depth*.012)+r.geothermal*14;
 r.ore=.3+proc::noise(p.seed^0x2813u,proc::scaled(n,5.6,15.))*1.55+r.geothermal*.65;
 r.crystal=.25+std::pow(proc::noise(p.seed^0x5613u,proc::scaled(n,8.2,4.)),2)*2.3+r.geothermal*.55;
 r.biomass=p.life*(.25+r.moisture*.75)*std::exp(-std::abs(r.temperature-289)/90.);
 if(depth>0)r.biomass*=.12+std::exp(-depth*.0014)*.7+r.geothermal*.8;
 double sea=seaLevel(p);r.biome=depth>0?(depth<200?"Sunlit shelf":depth<1000?"Twilight water":r.geothermal>.45?"Vent oasis":depth>5500?"Rift trench":"Abyssal plain"):p.type=="frozen"||r.temperature<235?"Glacial shelf":r.elevation<sea-.055?"Open ocean":r.elevation<sea?"Reef shallows":r.elevation<sea+.025?"Tidal flats":r.elevation>.70?"Fault highlands":p.type=="volcanic"?"Basalt terraces":p.type=="toxic"?"Chemical marsh":p.type=="rocky"?"Impact regolith":r.moisture<.30?"Dune basin":r.biomass>.35?"Luminous forest":r.biomass>.12?"Spore grassland":"Mineral steppe";
 static const char* prefixes[]={"Opal","Violet","Glass","Ilex","Nacre","Sable","Amber","Lumen","Pearl","Cobalt","Silver","Choir","Ochre","Halo","Moth","Indigo"};
 auto h=hash(int32_t(p.seed),int(std::floor(at.x/640)),int(std::floor(at.y/640)),441);r.name=std::string(prefixes[h%16])+" "+(depth>0?"Basin":p.landform==1?"Isles":p.landform==2?"Erg":p.landform==3?"Basin":p.landform==4?"Caldera":p.landform==5?"Shelf":p.landform==7?"Rift":"Reach");return r;
}
namespace {
void identify(Contact& o,const Planet& p,const Region& region,int slot,bool ocean){
 o.habitatBiome=region.biome;
 if(o.kind=="life"||o.kind=="flora"){
  int band=ocean?int(o.depth/1000):0;auto species=proc::hash(p.seed,int(stringHash(region.biome)),band,slot%5);o.n=species;o.name=objectName(o.kind,species);
  const char* niches[]={"filter-feeding colony","grazing symbiont","ambush predator","detritivore","chemotrophic colony"};
  std::string niche=o.kind=="flora"?"photosynthetic colony":region.geothermal>.45?"sulphide symbiont":niches[species%5];
  o.desc=region.name+" / "+region.biome+". A "+niche+" belonging to the regional food web. "+(ocean&&o.depth>1000?"Sinking organic matter and vent chemistry sustain this deep-water niche.":"Local light, moisture and substrate determine its range.");o.desc+=biologicalTraits(o);
 }else{
  o.name=objectName(o.kind,o.n);o.desc=description(o,p)+" "+region.name+" / "+region.biome+".";
  if(o.kind=="mineral")o.desc+=region.crystal>region.ore?" Crystal-rich fault deposits recur along this province.":" Metal-bearing strata recur along this province.";
  if(o.kind=="relic"||o.kind=="signal")o.desc+=" Its signature belongs to the same regional anomaly complex as nearby returns.";
 }
}
}
std::vector<Contact> generateSurface(const Universe& u,const Planet& p,int cx,int cy){
 if(p.giant)return {};Random random{u.hash(cx,cy,int32_t(p.seed),201)};std::vector<Contact> contacts;int count=1+int(random()*3);
 for(int i=0;i<count;i++){
  Contact o;o.pos={cx*86+8+random()*70,cy*86+8+random()*70};auto region=u.region(p,o.pos);bool water=region.elevation<u.seaLevel(p);double q=random();
  double ancient=proc::noise(p.seed^0x51943u,proc::scaled(proc::chart(o.pos.x,o.pos.y),9.1,8.));
  o.kind=q<.47&&region.biomass>.045?(water?"life":"flora"):q>.90&&ancient>.62?"relic":q>.81&&ancient>.59?"signal":q>.56&&water&&region.geothermal>.40?"vent":"mineral";
  o.n=u.hash(cx,cy,i,int32_t(p.seed));o.id="S2:"+p.id+":"+std::to_string(cx)+":"+std::to_string(cy)+":"+std::to_string(i);identify(o,p,region,i,false);contacts.push_back(o);
 }
 auto feature=habitatFeature(u,p,cx,cy,false);if(!feature.id.empty())contacts.push_back(feature);
 return contacts;
}
std::vector<Contact> generateOcean(const Universe& u,const Planet& p,int cx,double y){
 if(p.giant||p.water<.2)return {};int transect=int(std::floor(y/130));double anchorY=(transect+.5)*130;
 Random random{u.hash(cx,int32_t(p.seed),transect,9891)};std::vector<Contact> contacts;int count=4+int(random()*5);
 // A track intersects regional populations and seams. Identity and taxonomy
 // stay anchored to the band; depth follows the actual surveyed seafloor.
 for(int i=0;i<count;i++){
  Contact o;o.pos={cx*130+8+random()*115,y};double floor=u.floorDepth(p,o.pos.x,y),anchorFloor=u.floorDepth(p,o.pos.x,anchorY),t=random();
  auto depth=[&](double bottom){return std::min(i==0?55+t*95:i==1?220+t*300:40+t*(bottom-85),bottom-35);};o.depth=depth(floor);double referenceDepth=depth(anchorFloor);
  auto region=u.region(p,{o.pos.x,anchorY},referenceDepth);double q=random();double ancient=proc::noise(p.seed^0x51943u,proc::scaled(proc::chart(o.pos.x,anchorY),9.1,8.));
  o.kind=q<.58&&region.biomass>.01?"life":q>.93&&ancient>.62?"relic":q>.84&&ancient>.59?"signal":q>.6&&region.geothermal>.34?"vent":"mineral";
  if(o.kind=="vent"){t=random();o.depth=std::max(50.,floor-45-t*80);referenceDepth=std::max(50.,anchorFloor-45-t*80);}
  o.n=u.hash(cx,i,int32_t(p.seed),transect+87);o.id="O2:"+p.id+":"+std::to_string(transect)+":"+std::to_string(cx)+":"+std::to_string(i);
  o.habitatPos={o.pos.x,anchorY};o.habitatDepth=referenceDepth;double actualDepth=o.depth;o.depth=referenceDepth;identify(o,p,u.region(p,{o.pos.x,anchorY},referenceDepth),i,true);o.depth=actualDepth;contacts.push_back(o);
 }
 auto feature=habitatFeature(u,p,cx,transect,true,y);if(!feature.id.empty())contacts.push_back(feature);
 return contacts;
}
}
