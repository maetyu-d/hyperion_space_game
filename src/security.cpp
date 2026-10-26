#include "economy.hpp"
#include "flight/game.hpp"
namespace hyperion {
Captain* findCaptain(Economy& e,const std::string& id){for(auto& c:e.captains)if(c.id==id)return &c;return nullptr;}
PirateBand* findPirates(Economy& e,const std::string& id){for(auto& b:e.pirates)if(b.id==id)return &b;return nullptr;}
bool factionsAtWar(const Economy& e,int a,int b){return a>0&&b>0&&a!=b&&std::min(e.factions[a].relations[b],e.factions[b].relations[a])<=-65;}
int64_t pirateTrucePrice(const PirateBand& b){return 250+int64_t(std::ceil(b.grievance*10));}
namespace {
bool fitShip(Economy& e,Captain& c,bool replacement){
 auto* p=findSettlement(e,c.home);auto* band=findPirates(e,c.band);if(!p||!p->residents)return false;
 if(!band&&p->owner!=c.faction)return false;
 if(band&&objectIntegrity(e.worldDamage,"port:den:"+band->id)<=0)return false;
 if(!band&&objectIntegrity(e.worldDamage,"port:orbit:"+p->id)<=0&&objectIntegrity(e.worldDamage,"port:surface:"+p->id)<=0)return false;
 auto& stock=band?band->stock:p->stock;auto& money=band?band->treasury:e.factions[c.faction].treasury;
 double multiplier=replacement?2:1;int64_t cost=replacement?240:120;
 if(stock[Alloy]<4*multiplier||stock[Components]<2*multiplier||stock[Propellant]<2*multiplier||money<cost)return false;
 stock[Alloy]-=4*multiplier;stock[Components]-=2*multiplier;stock[Propellant]-=2*multiplier;
 money-=cost;p->treasury+=cost;const auto& type=hyperion::flight::enemyDef(c.role,c.genome);
 c.destroyed=false;c.hull=type.hull;c.shield=type.shield;c.armour=100;c.modules={100,100,100};c.energy=100;c.heat=0;c.rounds=180;c.missiles=c.decoys=2;
 return true;
}
}
void initializeActorProfiles(Economy& e,Universe& u){
 for(auto& c:e.captains)if(!c.genome.revision)c.genome={1,u.hash(int32_t(stringHash(c.id)),int32_t(u.star(c.star).seed),329),true};
}
void discoverSecurity(Economy& e,Universe& u,const std::string& star){
 if(e.securitySystems.count(star))return;e.securitySystems.insert(star);
 std::vector<std::string> ports;for(auto& p:e.settlements)if(p.star==star&&p.owner>0)ports.push_back(p.id);
 if(ports.empty())return;
 Random r{u.star(star).seed^0x6e656d65u};const char* first[]={"Ilex","Sora","Moth","Vela","Nacre","Kestrel","Oriel","Rook","Echo","Lumen"};
 const char* last[]={"Voss","Neri","Quill","Vale","Sable","Reef","Rin","Glass","Thorne","Orin"};
 auto name=[&](){return std::string(first[int(r()*10)])+" "+last[int(r()*10)];};
 auto haven=*std::min_element(ports.begin(),ports.end(),[&](const auto& a,const auto& b){auto* x=findSettlement(e,a);auto* y=findSettlement(e,b);return x->morale-x->pollution*35<y->morale-y->pollution*35;});
 auto* port=findSettlement(e,haven);PirateBand b;b.id="band:"+star;b.star=star;b.haven=haven;
 const char* titles[]={"Violet Wake","Glass Corsairs","Nacre Knives","Choir of Rust","Opal Reavers","Neon Undertow"};b.name=titles[int(r()*6)];b.nextRaid=e.hour+6;
 // A single documented diversion establishes the den; no infinite pirate bank.
 b.treasury=std::min(int64_t(600),port->treasury);port->treasury-=b.treasury;
 for(auto [good,amount]:{std::pair<int,double>{Alloy,20},{Components,10},{Propellant,10}}){double taken=std::min(amount,port->stock[good]);port->stock[good]-=taken;b.stock[good]=taken;}
 e.pirates.push_back(b);
 economicEvent(e,port->name,b.name+" established a hidden anchorage with diverted funds and supplies. Patrols, convoy escorts and trade shortages now affect their operations.");
 for(auto& id:ports){auto* p=findSettlement(e,id);Captain c;c.id="patrol:"+id;c.name=name();c.star=star;c.home=id;c.faction=p->owner;c.genome={1,u.hash(int32_t(stringHash(c.id)),int32_t(u.star(star).seed),329),false};
  int roll=int(hyperion::flight::actorHash(c.genome.seed,31)%8);c.role=roll<3?(p->built[Fabricator]>0?2:1):roll<5?0:roll<7?3:2;
  c.destroyed=true;c.readyAt=e.hour;if(!fitShip(e,c,false))c.readyAt=e.hour+6;e.captains.push_back(c);
 }
 for(int i=0;i<3;i++){Captain c;c.id=b.id+":captain:"+std::to_string(i);c.name=name();c.star=star;c.home=haven;c.band=b.id;c.faction=port->owner;c.genome={1,u.hash(int32_t(stringHash(c.id)),int32_t(u.star(star).seed),329),false};c.role=int(hyperion::flight::actorHash(c.genome.seed,31)%4);c.destroyed=true;c.readyAt=e.hour;
  if(!fitShip(e,c,false))c.readyAt=e.hour+6;e.captains.push_back(c);
 }
}
void advanceSecurity(Economy& e){
 for(auto& c:e.captains)if(c.band.empty()){
  auto* base=findSettlement(e,c.home);if(base&&base->owner==c.faction)continue;
  Settlement* next=nullptr;for(auto& p:e.settlements)if(p.star==c.star&&p.owner==c.faction&&p.residents){next=&p;break;}
  if(next)c.home=next->id;
  else if(!c.destroyed){c.destroyed=true;c.hull=0;c.readyAt=e.hour+24;economicEvent(e,c.name,"Patrol withdrawn after its home charter changed. Its former authority cannot outfit ships from the new owner's warehouse.");}
 }

 for(auto& c:e.captains)if(c.destroyed&&e.hour+1e-8>=c.readyAt){
  if(fitShip(e,c,true))economicEvent(e,c.name,"Returned in a replacement ship paid for with finite funds, alloy, components and fuel. Previous defeats and grudges remain on record.");
  c.readyAt=e.hour+6;
 }
 for(auto& b:e.pirates){
  if(e.hour+1e-8<b.nextRaid)continue;b.nextRaid=e.hour+6;
  auto* haven=findSettlement(e,b.haven);if(!haven||!haven->residents)continue;
  int raiders=0,patrols=0;for(const auto& c:e.captains)if(c.star==b.star&&!c.destroyed){if(c.band==b.id)raiders++;else if(c.band.empty())patrols++;}
  // Stolen goods are sold only against an actual buyer's need, cash and space.
  for(int good=0;good<goodCount;good++)if(b.stock[good]>(good==Alloy?8:good==Components||good==Propellant?4:0)){
   double reserve=good==Alloy?8:good==Components||good==Propellant?4:0;
   double amount=std::min({b.stock[good]-reserve,std::max(0.,reserveFor(*haven,good)*1.4-haven->stock[good]),std::max(0.,storageCapacity(*haven)-stockTotal(haven->stock)),double(haven->treasury)/std::max(1.,goods[good].value)});
   int64_t price=int64_t(std::floor(amount*goods[good].value));if(price<=0)continue;b.stock[good]-=amount;haven->stock[good]+=amount;haven->treasury-=price;b.treasury+=price;
  }
  if(!raiders||b.star==e.securityWitness)continue;
  TradeRoute* target=nullptr;double value=0;
  for(auto& r:e.routes)if(r.outbound&&stockTotal(r.cargo)>0){auto* from=findSettlement(e,r.from);auto* to=findSettlement(e,r.to);if(!from||!to||from->star!=b.star||to->star!=b.star)continue;
   if(r.owner==0&&b.truceUntil>e.hour&&b.grievance<10)continue;
   double score=stockTotal(r.cargo)*goods[r.good].value;if(score>value){target=&r;value=score;}
  }
  if(!target)continue;
  bool escort=false;for(const auto& c:e.captains)if(c.star==b.star&&c.band.empty()&&!c.destroyed&&e.factions[c.faction].relations[3]>=20&&e.factions[c.faction].warrant==0)escort=true;
  if(target->escortUntil>e.hour&&escort){target->status="Escorted convoy / raid deterred";continue;}
  double budget=std::max(0.,raiders*4.-patrols*2.+std::max(0.,55-haven->morale)*.12+haven->pollution*3);
  double taken=0,room=std::max(0.,600-stockTotal(b.stock));for(int g=0;g<goodCount;g++){double amount=std::min({target->cargo[g],budget,room});target->cargo[g]-=amount;b.stock[g]+=amount;taken+=amount;budget-=amount;room-=amount;}
  if(taken>.001){b.raids++;b.stolen+=taken;target->status="Raided / surviving cargo continues";
   economicEvent(e,b.name,"Raided convoy "+target->id+": "+std::to_string(int(std::ceil(taken)))+" supplies transferred from its actual cargo into the pirate stores. Fewer patrols and local unrest increase losses.");}
 }
}
}
