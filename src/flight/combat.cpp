#include "game.hpp"
#include <limits>
namespace hyperion::flight {
namespace {
constexpr double infinity=std::numeric_limits<double>::infinity();
double sphereHit(V3 a,V3 b,V3 centre,double radius){
 V3 o=a-centre,d=b-a;double c=dot(o,o)-radius*radius;if(c<=0)return 0;
 double aa=dot(d,d),bb=dot(o,d),h=bb*bb-aa*c;if(aa<1e-12||h<0)return infinity;
 double t=(-bb-std::sqrt(h))/aa;return t>=0&&t<=1?t:infinity;
}
double segmentDistance(V3 a,V3 b,V3 p){auto d=b-a;return length(a+d*std::clamp(dot(p-a,d)/std::max(1e-12,dot(d,d)),0.,1.)-p);}
V3 turn(V3 current,V3 wanted,double limit){current=unit(current,{0,0,-1});wanted=unit(wanted,current);double angle=std::acos(std::clamp(dot(current,wanted),-1.,1.));return rotate(current,unit(cross(current,wanted),unit(cross(current,{0,1,0}),{1,0,0})),std::min(angle,limit));}
}
const WeaponDef& weaponDef(int type){
 static const std::array<WeaponDef,5> weapons{{
  {"Pulse lance",18,1.65,.7,.15,1150,.22,6,7,2.2},
  {"Kinetic repeater",12,.55,1.35,.6,1600,.10,1.4,2.8,1.6},
  {"Plasma caster",52,1.05,1.25,.25,380,.9,24,26,3.5},
  {"Seeker missile",68,1,1.35,.45,320,1.4,8,12,7},
  {"Resonance arc",14,2.2,.45,.1,1450,.36,12,15,1.5}
 }};return weapons[std::clamp(type,0,4)];
}
int primaryWeapon(int slot){return slot==3?Resonance:std::clamp(slot,0,2);}
const char* focusName(int focus){static const char* names[]={"Hull","Drive","Weapons","Shields"};return names[std::clamp(focus,0,3)];}
double interceptTime(V3 offset,V3 velocity,double speed){
 double a=dot(velocity,velocity)-speed*speed,b=2*dot(offset,velocity),c=dot(offset,offset);if(c<1e-12||speed<=0)return 0;
 if(std::abs(a)<1e-8)return b<0?-c/b:0;double discriminant=b*b-4*a*c;if(discriminant<0)return 0;
 double root=std::sqrt(discriminant),t1=(-b-root)/(2*a),t2=(-b+root)/(2*a);if(t1>0&&t2>0)return std::min(t1,t2);return t1>0?t1:t2>0?t2:0;
}
const char* enemyStateName(const Traffic& t){static const char* states[]={"Patrol","Closing","Attack run","Breaking away","Withdrawing","Evasive burn","Venting heat","At workshop"};return states[std::clamp(t.state,0,7)];}
const char* systemName(int index){static const char* names[]={"Drive","Reactor","Shield generator","Weapon mounts","Sensors"};return names[std::clamp(index,0,SystemCount-1)];}
const EnemyDef& enemyDef(int role){static const std::array<EnemyDef,4> types{{{"Raider",100,30,105,360,8,0},{"Interceptor",80,40,155,210,6,1},{"Marauder",180,65,68,540,13,2},{"Prism disruptor",115,80,90,430,10,Resonance}}};return types[std::clamp(role,0,3)];}
bool Game::enemiesOf(const Traffic& a,const Traffic& b)const{
 if(a.kind!=1||a.id==b.id)return false;
 if(!a.managed)return b.kind==2;
 if(a.pirate&&b.kind==2&&a.band==b.band)return false;
 if(a.pirate)return b.pirate?a.band!=b.band:b.kind==1||b.kind==2;
 if(b.pirate)return true;return b.faction>=0&&b.faction<5&&a.enemies[b.faction];
}
bool Game::protectedPort(V3 at)const{for(const auto& p:ports)if(!p.settlement.empty()&&length(at-p.position)<150)return true;return false;}
double Game::terrainHit(V3 a,V3 b)const{
 double first=minorHit(a,b),span=length(b-a);
 for(const auto& p:planets){if(segmentDistance(a,b,p.centre)>p.radius+23)continue;
  auto clearance=[&](double t){auto q=mix(a,b,t)-p.centre;return length(q)-p.radius-terrain(p,unit(q))-2;};
  if(clearance(0)<=0){first=0;continue;}
  int steps=std::clamp(int(std::ceil(span/8)),1,512);double previous=0;
  for(int i=1;i<=steps;i++){double t=double(i)/steps;if(t>first)break;if(clearance(t)<=0){double lo=previous,hi=t;for(int k=0;k<14;k++){double m=(lo+hi)*.5;if(clearance(m)>0)lo=m;else hi=m;}first=hi;break;}previous=t;}
 }
 return first;
}
bool Game::lineOfSight(V3 a,V3 b)const{return !std::isfinite(terrainHit(a,b));}
int Game::combatTargetIndex()const{if(combatTarget.empty())return -1;for(int i=0;i<int(traffic.size());i++)if(traffic[i].id==combatTarget&&!traffic[i].dead)return i;return -1;}
void Game::cycleCombatTarget(){
 int old=combatTargetIndex();for(int n=1;n<=int(traffic.size());n++){int i=(old+n)%int(traffic.size());const auto& t=traffic[i];if(t.kind==1&&t.hostile&&!t.dead&&length(t.position-ship.position)<2400*sensorFactor(ship.vessel)){combatTarget=t.id;lock=0;message(std::string("Target / ")+enemyDef(t).name);return;}}
 combatTarget.clear();lock=0;message("No hostile contacts in sensor range.");
}
bool Game::selectWeapon(int type){if(type<0||type>4||type==Seeker)return false;ship.combat.weapon=type;fireClock=std::max(fireClock,.12);message(std::string("Weapon / ")+weaponDef(type).name);return true;}
void Game::effect(V3 at,int kind,V3 velocity,V3 end){if(effects.size()>=128)effects.erase(effects.begin());effects.push_back({at,velocity,0,kind==2?2.6:kind==3?1.8:kind==4?.32:kind==5?.085:.6,kind,end});}
void Game::damageShip(double amount,int weapon,V3 from){
 if(weapon==CollisionDamage)lastAttacker.clear();
 if(docked||amount<=0||!std::isfinite(amount))return;auto& c=ship.combat;const auto& w=weaponDef(std::max(0,weapon));
 if(weapon==Resonance)c.energy=std::max(0.,c.energy-amount*.65);hitDirection=unit(from-ship.position,ship.forward);damageCue=.85;
 impactShield=impactHull=0;impactSystem=-1;
 double shieldFactor=weapon<0?1:w.shieldFactor,hullFactor=weapon<0||weapon==CollisionDamage?1:w.hullFactor;
 if(weapon!=CollisionDamage){double absorbed=std::min(ship.shield,amount*shieldFactor);ship.shield-=absorbed;impactShield=absorbed;amount-=absorbed/shieldFactor;}
 double raw=amount*hullFactor,plating=weapon<0?0:raw*.38*c.armour/100*(1-(weapon==CollisionDamage?.1:w.penetration));
 impactHull=raw-plating;double hullScale=100/hullCapacity(ship.vessel);c.armour=std::max(0.,c.armour-plating*.6*hullScale);ship.hull-=(raw-plating)*hullScale;c.shieldDelay=weapon==Resonance?6:4.5;hitFlash=std::min(1.,.2+raw*.025);
 if(raw>0){int module;double front=dot(unit(from-ship.position),ship.forward);if(front>.25)module=(c.hits%2)?Sensors:WeaponMounts;else if(front<-.25)module=(c.hits%2)?PowerCore:Drive;else module=ShieldGenerator;c.hits++;
  impactSystem=module;c.systems[module]=std::max(0.,c.systems[module]-(raw-plating)*.9*hullScale);}
}
void Game::damageTraffic(int index,double amount,int weapon,V3 at,bool credited,int module,const std::string& attacker){
 if(index<0||index>=int(traffic.size())||traffic[index].dead||amount<=0||!std::isfinite(amount))return;auto& t=traffic[index];const auto& w=weaponDef(std::max(0,weapon));
 if(weapon==Resonance)t.energy=std::max(0.,t.energy-amount*.65);
 double sf=weapon<0?1:w.shieldFactor,hf=weapon<0?1:w.hullFactor,absorbed=std::min(t.shield,amount*sf);t.shield-=absorbed;amount-=absorbed/sf;
 double raw=amount*hf,plate=weapon<0?0:raw*.32*t.armour/100*(1-w.penetration);t.armour=std::max(0.,t.armour-plate*.6);double hullDamage=std::max(0.,raw-plate);t.health-=hullDamage*(module>0?.82:1.);t.shieldDelay=weapon==Resonance?6:5;t.hitFlash=.6;
 if(t.kind==1&&hullDamage>0){int system=module>0?std::clamp(module-1,0,2):dot(unit(at-t.position),t.forward)<-.25?0:1;auto& condition=t.modules[system];condition=std::max(0.,condition-hullDamage*(module>0?1.5:.32));}
 if(credited&&t.kind!=1&&!t.assaultReported){events.push_back({"civilianAssault",t.home,t.faction,t.routeId,"NOMA",t.id});t.assaultReported=true;}
 if(credited&&t.managed&&!t.assaultReported){events.push_back({t.hostile?"skirmish":"aggression",t.home,t.faction,"","NOMA",t.id});t.assaultReported=true;t.hostile=true;}
 effect(at,absorbed>0?0:1,t.velocity*.1);if(credited)hitConfirm=.18;
 if(t.health>0)return;
 t.dead=true;t.respawn=t.kind==2?1e6:240;effect(t.position,2,t.velocity*.12);
 if(t.kind==1){if(t.managed){events.push_back({"shipLoss",t.home,t.faction,"",credited?"NOMA":attacker,t.id});if(credited){kills++;message("Ship destroyed / political consequences reported / salvage nearby.");}}
  else if(credited){kills++;events.push_back({"bounty",t.home,t.faction,""});message("Hostile destroyed / bounty reported / salvage available nearby.");}
  if(wrecks.size()>=48)wrecks.erase(wrecks.begin());wrecks.push_back({t.position,t.velocity*.08,180,t.managed?std::array<double,3>{2.+t.role*.5,.5,.5}:std::array<double,3>{4.+t.role*2,1.+t.role,2.},t.id});
 }else if(credited){events.push_back({"civilian",t.home,t.faction,""});message("Civilian destroyed / authority notified.");}
 if(!t.routeId.empty())events.push_back({"freightLoss",t.home,t.faction,t.routeId,credited?"NOMA":attacker,t.id});
}
void Game::chainArc(int first,double damage){
 if(first<0||first>=int(traffic.size())||damage<=0||!std::isfinite(damage))return;
 std::array<int,3> used{first,-1,-1};V3 origin=traffic[first].position;
 for(int hop=1;hop<=2;hop++){int next=-1;double closest=85;
  for(int i=0;i<int(traffic.size());i++){const auto& t=traffic[i];if(t.dead||t.kind!=1||!t.hostile||std::find(used.begin(),used.end(),i)!=used.end())continue;double distance=length(t.position-origin);if(distance<closest&&lineOfSight(origin,t.position)){next=i;closest=distance;}}
  if(next<0)break;damage*=.55;effect(origin,4,{},traffic[next].position);damageTraffic(next,damage,Resonance,traffic[next].position,true);used[hop]=next;origin=traffic[next].position;
 }
}
void Game::prepareCombat(double dt,Input& input){
 auto& c=ship.combat;c.shieldDelay=std::max(0.,c.shieldDelay-dt);c.missileClock=std::max(0.,c.missileClock-dt);c.decoyClock=std::max(0.,c.decoyClock-dt);
 c.energy=std::min(100.,c.energy+dt*(5+9*c.systems[PowerCore]/100)*reactorFactor(ship.vessel));c.heat=std::max(0.,c.heat-dt*13*(1+.1*ship.vessel.fitted[WeaponCapacitors]));if(c.overheated&&c.heat<42)c.overheated=false;
 double shieldMax=shieldCapacity(ship.vessel)*c.systems[ShieldGenerator]/100;ship.shield=std::min(ship.shield,shieldMax);if(c.shieldDelay<=0)ship.shield=std::min(shieldMax,ship.shield+dt*1.8*c.systems[ShieldGenerator]/100*shieldCapacity(ship.vessel)/100);
 damageCue=std::max(0.,damageCue-dt);hitConfirm=std::max(0.,hitConfirm-dt);for(auto& e:effects){e.age+=dt;e.position+=e.velocity*dt;}effects.erase(std::remove_if(effects.begin(),effects.end(),[](const auto& e){return e.age>=e.duration;}),effects.end());
 for(auto& w:wrecks){w.life-=dt;w.position+=w.velocity*dt;w.velocity=w.velocity*std::exp(-dt*.2);}wrecks.erase(std::remove_if(wrecks.begin(),wrecks.end(),[](const auto& w){return w.life<=0;}),wrecks.end());
 for(int i=0;i<int(traffic.size());i++){auto& t=traffic[i];if(t.id.empty())t.id="local:"+std::to_string(t.home)+":"+std::to_string(t.kind)+":"+std::to_string(t.role)+":"+std::to_string(i);}
 if(combatTargetIndex()<0&&!docked){combatTarget.clear();double best=.45;for(auto& t:traffic)if(t.kind==1&&t.hostile&&!t.dead&&length(t.position-ship.position)<1800){double score=dot(unit(t.position-ship.position),ship.forward);if(score>best){best=score;combatTarget=t.id;}}}
 int selected=combatTargetIndex();
 if(strategicCombat&&combatOrder==1&&selected>=0&&traffic[selected].hostile&&!docked){auto& t=traffic[selected];cancelNavigation();double lead=interceptTime(t.position-ship.position,t.velocity-ship.velocity,weaponDef(c.weapon).speed);auto direction=unit(t.position+(t.velocity-ship.velocity)*std::min(3.,lead)-ship.position);orient(turn(ship.forward,direction,dt*1.4),ship.up);ship.assist=true;ship.cruise=false;ship.throttle=length(t.position-ship.position)>350?.8:.2;input.fire=dot(direction,ship.forward)>.985;}
 bool tracking=selected>=0&&!docked&&length(traffic[selected].position-ship.position)<(800+1000*c.systems[Sensors]/100)*sensorFactor(ship.vessel)&&dot(unit(traffic[selected].position-ship.position),ship.forward)>.965;
 if(tracking&&lineOfSight(ship.position,traffic[selected].position))lock=std::min(1.,lock+dt*(.35+.65*c.systems[Sensors]/100)*sensorFactor(ship.vessel));else lock=std::max(0.,lock-dt*3);
}
void Game::updateCombat(double dt,const Input& input){
 auto& c=ship.combat;bool weaponsClear=!docked&&!protectedPort(ship.position);
 auto shoot=[&](int type){const auto& w=weaponDef(type);Bolt b;auto right=unit(cross(ship.forward,ship.up));b.position=b.previous=ship.position+ship.forward*6+ship.up*1.5+right*((c.shots%2?1:-1)*1.2);b.velocity=ship.forward*w.speed+ship.velocity*(type==Seeker?.3:1.);b.life=w.life;b.hostile=false;b.weapon=type;b.damage=w.damage*(.35+.65*c.systems[WeaponMounts]/100)*weaponFactor(ship.vessel);b.owner="NOMA";
  if(type==Seeker)b.target=combatTarget;else if(aimSystem>0&&lock>=.85){b.target=combatTarget;b.module=aimSystem;}
  bolts.push_back(b);c.shots++;effect(b.position,5,ship.velocity);c.energy-=w.energy;c.heat=std::min(100.,c.heat+w.heat);if(c.heat>=100)c.overheated=true;};
 if(weaponsClear&&input.fire&&fireClock<=0&&!c.overheated&&c.systems[WeaponMounts]>5){const auto& w=weaponDef(c.weapon);if(c.energy>=w.energy&&(c.weapon!=1||c.rounds>0)&&bolts.size()<768){shoot(c.weapon);fireClock=w.interval;if(c.weapon==1)c.rounds--;}}
 bool missilePressed=input.missile&&!missileHeld,decoyPressed=input.decoy&&!decoyHeld;missileHeld=input.missile;decoyHeld=input.decoy;
 if(missilePressed&&!docked){if(weaponsClear&&bolts.size()<768&&c.missiles>0&&lock>=1&&c.missileClock<=0&&c.energy>=weaponDef(3).energy&&!c.overheated&&c.systems[WeaponMounts]>5){shoot(3);c.missiles--;c.missileClock=1.4;lock=0;message("Seeker away.");}else message(weaponsClear?"Seeker requires target lock, ammunition, charge and cooled mounts.":"Port approach / clear the 150 km protection buffer before firing.");}
 if(decoyPressed&&!docked&&c.decoys>0&&c.decoyClock<=0){c.decoys--;c.decoyClock=2;effect(ship.position-ship.forward*6,3,ship.velocity*.4);for(auto& b:bolts)if(b.hostile&&b.weapon==3&&b.target=="NOMA"&&length(b.position-ship.position)<900)b.target.clear();message("Countermeasure burst / nearby seekers lost lock.");}
 for(int i=0;i<int(traffic.size());i++){
  auto& t=traffic[i];t.hitFlash=std::max(0.,t.hitFlash-dt);
  if(t.dead){if(t.managed)continue;t.respawn-=dt;if(t.respawn>0)continue;
   if(length(t.position-ship.position)<2000||length(ports[std::clamp(t.home,0,int(ports.size())-1)].position-ship.position)<3000){t.respawn=10;continue;}
   t.dead=false;t.health=t.kind==1?enemyDef(t).hull:100;t.shield=t.kind==1?enemyDef(t).shield:0;t.armour=100;t.modules={100,100,100};t.energy=100;t.heat=t.charge=0;t.ammunition=180;t.missiles=t.countermeasures=2;t.salvos=0;t.chargeWeapon=-1;t.venting=false;t.decoyClock=t.evadeClock=t.serviceClock=0;t.positioned=false;
  }
  if(t.kind!=2||!t.positioned)t.previous=t.position;
  const auto& home=ports[std::clamp(t.home,0,int(ports.size())-1)];
  if(!t.positioned&&t.kind!=2){double a=time*(t.kind==1?.035:.023)+t.phase;t.position=home.position+V3{std::cos(a)*(t.kind==1?360:240),std::sin(a*(t.kind==1?1.5:1.3))*(t.kind==1?160:95),std::sin(a)*(t.kind==1?360:240)};t.previous=t.position;t.forward=unit(cross(home.normal,{0,1,0}),{0,0,-1});}t.positioned=true;
  if(t.kind==0){double a=time*.023+t.phase;t.position=home.position+V3{std::cos(a)*240,std::sin(a*1.3)*95,std::sin(a)*240};t.velocity=dt>0?(t.position-t.previous)/dt:V3{};t.forward=unit(t.velocity,t.forward);continue;}
  if(t.kind!=1)continue;
  const auto& type=enemyDef(t);t.fireClock-=dt;t.stateClock=std::max(0.,t.stateClock-dt);t.shieldDelay=std::max(0.,t.shieldDelay-dt);t.decoyClock=std::max(0.,t.decoyClock-dt);t.evadeClock=std::max(0.,t.evadeClock-dt);
  t.energy=std::min(100.,t.energy+dt*8*type.reactor);t.heat=std::max(0.,t.heat-dt*6*type.cooling);if(t.heat>=85)t.venting=true;if(t.heat<38)t.venting=false;
  double shieldCapacity=type.shield*t.modules[2]/100;t.shield=std::min(t.shield,shieldCapacity);if(t.shieldDelay<=0)t.shield=std::min(shieldCapacity,t.shield+dt*2.5*t.modules[2]/100);
  V3 victim=ship.position,victimVelocity=ship.velocity;std::string victimId="NOMA";bool attack=t.hostile&&!docked&&!protectedPort(ship.position)&&length(t.position-ship.position)<1800;
  if(!attack){double closest=1200;for(const auto& contact:traffic)if(!contact.dead&&enemiesOf(t,contact)){double range=length(contact.position-t.position);if(range<closest&&!protectedPort(contact.position)){victim=contact.position;victimVelocity=contact.velocity;victimId=contact.id;attack=true;closest=range;}}}
  bool incoming=false,lockedSeeker=false;
  for(auto& b:bolts)if(b.life>0&&b.owner!=t.id){bool threat=!b.hostile&&t.hostile;for(const auto& owner:traffic)if(owner.id==b.owner)threat=enemiesOf(t,owner);if(!threat)continue;double distance=length(b.position-t.position);if(distance>500)continue;
   if(b.weapon==Seeker&&b.target==t.id&&distance<380){lockedSeeker=true;incoming=true;}
   auto relative=b.velocity-t.velocity;double closest=dot(t.position-b.position,relative)/std::max(1.,dot(relative,relative));
   if(closest>0&&closest<.7&&length(b.position+relative*closest-t.position)<type.radius*2+4)incoming=true;
  }
  if(lockedSeeker&&t.countermeasures>0&&t.decoyClock<=0){
   t.countermeasures--;t.decoyClock=9;effect(t.position-t.forward*type.radius,3,t.velocity*.4);
   for(auto& b:bolts)if(b.weapon==Seeker&&b.target==t.id&&length(b.position-t.position)<500)b.target.clear();
  }
  if(incoming&&(t.role==1||t.role==3||type.evades))t.evadeClock=std::max(t.evadeClock,.85);
  double distance=length(victim-t.position),sideSign=std::sin(t.phase+1)>0?1:-1;auto toward=unit(victim-t.position),side=unit(cross(toward,home.normal),{1,0,0})*sideSign;V3 desired;
  bool withdrawing=t.health<type.hull*type.retreat||(type.weapon==Kinetic&&t.ammunition<=0)||t.modules[1]<=5;
  double speed=type.speed*(.18+.82*t.modules[0]/100);
  if(withdrawing){
   V3 berth=home.position+home.normal*110;double homeDistance=length(berth-t.position);t.state=homeDistance<85?7:4;
   desired=unit(berth-t.position,t.forward)*std::min(speed,homeDistance*.8);
   if(t.state==7){t.serviceClock+=dt;if(t.serviceClock>=8){events.push_back({"rearmEnemy",t.home,t.faction,t.id});t.serviceClock=0;}}else t.serviceClock=0;
  }else if(attack){
   t.serviceClock=0;
   if(t.venting){t.state=6;desired=(-toward*.45+side*.7)*speed;}
   else if(t.evadeClock>0){t.state=5;desired=(side+unit(cross(toward,side))*.3-toward*.15)*speed;}
   else if(distance<70||(t.state==3&&t.stateClock>0)){t.state=3;desired=(-toward+side*.7)*speed;if(t.stateClock<=0)t.stateClock=1.8;}
   else{t.state=distance<type.range?2:1;double standOff=type.range*(type.standOff>0?type.standOff:t.role==1?.58:t.role==2?.83:.72);double radial=std::clamp((distance-standOff)/100.,-.6,1.);desired=(toward*radial+side*(type.strafe>0?type.strafe:t.role==1?.8:t.role==2?.18:.38))*speed;}
  }else{t.state=0;t.serviceClock=0;double a=time*.035+t.phase;V3 patrol=home.position+V3{std::cos(a)*360,std::sin(a*1.5)*160,std::sin(a)*360};
   if(t.managed&&t.pirate){double closest=10000;for(const auto& convoy:traffic)if(convoy.kind==2&&!convoy.dead&&enemiesOf(t,convoy)&&!protectedPort(convoy.position)){double d=length(convoy.position-t.position);if(d<closest){closest=d;patrol=convoy.position;}}}
   if(t.managed&&!t.pirate&&t.escortDuty)for(const auto& convoy:traffic)if(convoy.kind==2&&!convoy.dead&&convoy.reason=="Commissioned escort"&&!t.enemies[convoy.faction]){patrol=convoy.position+home.normal*90;break;}
   desired=unit(patrol-t.position,t.forward)*std::min(speed,length(patrol-t.position));}
  // Separated wing slots stop all four roles collapsing into the same firing line.
  for(int j=0;j<int(traffic.size());j++)if(j!=i&&traffic[j].kind==1&&!traffic[j].dead){auto away=t.position-traffic[j].position;double gap=length(away);if(gap<80)desired+=unit(away,side)*(speed*(1-gap/80)*.7);}
  t.velocity=mix(t.velocity,desired,1-std::exp(-dt*(t.state==5?3.5:1.3)));t.position+=t.velocity*dt;
  for(const auto& p:planets){auto local=t.position-p.centre;double floor=p.radius+terrain(p,unit(local))+27;if(length(local)<floor){auto n=unit(local);t.position=p.centre+n*floor;t.velocity+=n*std::max(0.,-dot(t.velocity,n));}}
  // Thrusters allow the nose to track independently of sideways movement.
  double lead=interceptTime(victim-t.position,victimVelocity,weaponDef(type.weapon).speed*.8);V3 aim=unit(victim+victimVelocity*std::min(2.,lead)-t.position);
  t.forward=turn(t.forward,attack&&!withdrawing?aim:unit(t.velocity,t.forward),dt*(t.role==1?2.8:1.6)*type.agility);
  bool canAttack=attack&&!withdrawing&&t.state!=5&&!t.venting&&t.fireClock<=0&&t.modules[1]>5&&distance<type.range+140&&dot(aim,t.forward)>.92&&lineOfSight(t.position,victim);
  if(canAttack){
   int weapon=t.chargeWeapon>=0?t.chargeWeapon:t.role==2&&t.missiles>0&&t.salvos%3==0&&distance>180?Seeker:type.weapon;
   const auto& w=weaponDef(weapon);double warmup=weapon==Seeker?1.0:weapon==Plasma?.65:weapon==Resonance?.4:0;
   if(t.energy>=w.energy*.8&&(weapon!=Kinetic||t.ammunition>0)&&(weapon!=Seeker||t.missiles>0)){
    t.chargeWeapon=weapon;t.charge=warmup>0?std::min(1.,t.charge+dt/warmup):1;
    if(t.charge>=1&&bolts.size()<768){
     double muzzleSpeed=weapon==Seeker?300:w.speed*.8;double solution=interceptTime(victim-t.position,victimVelocity,muzzleSpeed);auto direction=unit(victim+victimVelocity*std::min(2.,solution)-t.position);
     if(weapon!=Seeker&&type.aimJitter>0){auto axis=unit(cross(direction,home.normal),{1,0,0});double a=t.phase+t.salvos*2.3999632297;direction=unit(direction+axis*(std::sin(a)*type.aimJitter)+unit(cross(axis,direction))*(std::cos(a)*type.aimJitter));}
     Bolt b;b.position=b.previous=t.position+direction*(type.radius+2);b.velocity=direction*muzzleSpeed;b.life=w.life;b.hostile=true;b.weapon=weapon;b.damage=w.damage*.65*(.3+.7*t.modules[1]/100);b.owner=t.id;if(weapon==Seeker)b.target=victimId;
     bolts.push_back(b);t.energy-=w.energy*.8;t.heat=std::min(100.,t.heat+w.heat*.8);if(t.heat>=85)t.venting=true;
     if(weapon==Kinetic)t.ammunition--;if(weapon==Seeker)t.missiles--;t.salvos=std::min(1000000000,t.salvos+1);t.charge=0;t.chargeWeapon=-1;
     t.fireClock=(weapon==Seeker?4.5:t.role==1?(t.salvos%3==0?1.25:.16):t.role==2?1.55:t.role==3?.8:1.05)*type.cadence;
    }
   }else{t.charge=std::max(0.,t.charge-dt*2);if(t.charge==0)t.chargeWeapon=-1;}
  }else{t.charge=std::max(0.,t.charge-dt*2);if(t.charge==0)t.chargeWeapon=-1;}
 }
 for(auto& b:bolts){
  if(b.life<=0)continue;
  if(b.weapon==3&&!b.target.empty()){V3 target,velocity;bool found=false;if(b.target=="NOMA"&&!docked){target=ship.position;velocity=ship.velocity;found=true;}else for(const auto& t:traffic)if(t.id==b.target&&!t.dead){target=t.position;velocity=t.velocity;found=true;break;}
   if(found){double speed=length(b.velocity);b.velocity=turn(b.velocity,target+velocity*.2-b.position,dt*1.65)*speed;}else b.target.clear();}
  double step=std::min(dt,b.life),fraction=dt>0?step/dt:1;b.previous=b.position;b.position+=b.velocity*step;b.life-=dt;double nearest=terrainHit(b.previous,b.position);int hit=-3;
  const Traffic* shooter=nullptr;for(const auto& t:traffic)if(t.id==b.owner){shooter=&t;break;}
  if(b.hostile&&(!shooter||shooter->hostile)&&!docked){double h=sphereHit(b.previous-ship.previous,b.position-mix(ship.previous,ship.position,fraction),{},3);if(h<nearest){nearest=h;hit=-2;}}
  for(int i=0;i<int(traffic.size());i++){auto& t=traffic[i];if(t.dead||t.id==b.owner||(b.hostile&&(shooter?!enemiesOf(*shooter,t):t.kind!=2)))continue;double radius=t.kind==1?enemyDef(t).radius:t.kind==2?9:5;
   double h=sphereHit(b.previous-(dt>0?t.previous:t.position),b.position-mix(dt>0?t.previous:t.position,t.position,fraction),{},radius);if(h<nearest){nearest=h;hit=i;}}
  if(std::isfinite(nearest)){auto at=mix(b.previous,b.position,nearest);double damage=b.damage>0?b.damage:(b.hostile?12:34);if(hit==-2){lastAttacker=b.owner;damageShip(damage,b.weapon,b.previous);}else if(hit>=0){damageTraffic(hit,damage,b.weapon,at,!b.hostile,b.target==traffic[hit].id?b.module:0,b.owner);if(b.weapon==Resonance&&!b.hostile)chainArc(hit,damage);}else effect(at,1);b.life=0;}
 }
 for(auto& t:traffic)if(t.kind==2)t.previous=t.position;
 bolts.erase(std::remove_if(bolts.begin(),bolts.end(),[](const auto& b){return b.life<=0;}),bolts.end());
}
}
