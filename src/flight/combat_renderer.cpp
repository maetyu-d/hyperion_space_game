#include "renderer.hpp"
#include "rlgl.h"

namespace hyperion::flight {
namespace {
constexpr Color ice{91,221,255,255},violet{203,119,255,255},mint{143,255,198,255},amber{255,181,86,255},rose{255,96,168,255},hot{255,241,209,255};
Color alpha(Color c,double a){c.a=static_cast<unsigned char>(std::clamp(a,0.,1.)*255);return c;}
Color pigment(int weapon){return weapon==Kinetic?amber:weapon==Plasma?violet:weapon==Seeker?rose:weapon==Resonance?mint:ice;}
Vector3 vec(V3 p){return {float(p.x),float(p.y),float(p.z)};}
void line(V3 a,V3 b,Color c){DrawLine3D(vec(a),vec(b),c);}
void tube(V3 a,V3 b,double r,Color c){if(length(b-a)>.001)DrawCylinderEx(vec(a),vec(b),float(r),float(r*.7),5,c);}
void ring(V3 at,V3 a,V3 b,double r,double width,Color c,int segments){
 rlBegin(RL_TRIANGLES);rlColor4ub(c.r,c.g,c.b,c.a);
 for(int i=0;i<segments;i++){
  auto u=a*std::cos(i*2*pi/segments)+b*std::sin(i*2*pi/segments),v=a*std::cos((i+1)*2*pi/segments)+b*std::sin((i+1)*2*pi/segments);
  V3 A=at+u*(r-width),B=at+v*(r-width),C=at+u*(r+width),D=at+v*(r+width);
  for(auto p:{A,B,C,B,D,C,A,C,B,B,C,D})rlVertex3f(float(p.x),float(p.y),float(p.z));
 }rlEnd();
}
// Fixed analytic trajectories: no frame-dependent random numbers or particle allocation.
double seed(int i,double offset=0){double s=std::sin(i*127.1+offset*31.7)*43758.5453;return s-std::floor(s);}
V3 radial(int i,V3 normal,V3 a,V3 b,bool hemisphere){double z=hemisphere?.15+.85*seed(i,7):seed(i,7)*2-1,angle=i*2.3999632297;return normal*z+(a*std::cos(angle)+b*std::sin(angle))*std::sqrt(1-z*z);}
}

void Renderer::drawCombat(const Game& g){
 if(!combatGlow.id&&(!g.effects.empty()||!g.bolts.empty())){
  Image image=GenImageColor(96,96,BLANK);auto* pixels=static_cast<Color*>(image.data);
  for(int y=0;y<96;y++)for(int x=0;x<96;x++){double r=std::hypot((x-47.5)/47.5,(y-47.5)/47.5);pixels[y*96+x]={255,255,255,static_cast<unsigned char>(255*std::exp(-r*r*5)*std::clamp((1-r)*5,0.,1.))};}
  combatGlow=LoadTextureFromImage(image);UnloadImage(image);GenTextureMipmaps(&combatGlow);SetTextureFilter(combatGlow,TEXTURE_FILTER_TRILINEAR);
 }
 auto glow=[&](V3 at,double size,Color tint,double opacity){if(size>.005&&opacity>.001)DrawBillboard(camera,combatGlow,vec(at),float(size),alpha(tint,opacity));};
 const int segments=reduced?12:lowGraphics?20:32;
 int sparkBudget=reduced?120:lowGraphics?360:720;
 auto right=unit(cross(g.ship.forward,g.ship.up)),up=g.ship.up;
 // Cool smoke and solid fragments remain visible between the very short hot flashes.
 rlDisableDepthMask();
 for(const auto& e:g.effects){
  double distance=length(e.position-g.ship.position);if(distance>1800||dot(e.position-g.ship.position,g.ship.forward)<-40)continue;
  if(e.kind!=ShipExplosion&&e.kind!=HullImpact)continue;
  double t=e.age/e.duration,fade=1-t;
  auto a=unit(cross(e.normal,up),right),b=unit(cross(a,e.normal));
  int count=e.kind==ShipExplosion?(reduced?4:lowGraphics?7:11):(reduced?2:4);
  for(int i=0;i<count;i++){
   auto direction=radial(i,e.normal,a,b,e.kind==HullImpact);double travel=(1-std::exp(-e.age*1.7))*(e.kind==ShipExplosion?e.size*3.8:5)*(.4+seed(i,4));
   auto at=e.position+direction*travel;
   if(e.kind==ShipExplosion){
    double size=e.size*(.018+.035*seed(i,2))*(1-.4*t);
    DrawCubeV(vec(at),{float(size),float(size*.65),float(size*1.8)},alpha(i%2?Color{58,35,64,255}:Color{31,54,60,255},fade));
   }
   if(!reduced&&e.age>.10){
    auto smokeAt=e.position+direction*travel*.55;
    double size=(e.kind==ShipExplosion?e.size*.15:.25)*(1+e.age*2.5)*(.7+seed(i,3));
    glow(smokeAt,size*4,i%2?Color{36,29,55,255}:Color{23,44,48,255},fade*.32);
   }
  }
 }
 BeginBlendMode(BLEND_ADDITIVE);
 for(const auto& bolt:g.bolts){
  auto direction=unit(bolt.velocity),a=unit(cross(direction,up),right),b=unit(cross(a,direction));
  if(length(bolt.position-g.ship.position)>3000)continue;
  Color c=pigment(bolt.weapon);
  if(bolt.hostile){c=rose;for(const auto& owner:g.traffic)if(owner.id==bolt.owner&&owner.managed&&!owner.pirate&&!owner.hostile){auto rgb=factionStyle(owner.faction).accent;c={rgb[0],rgb[1],rgb[2],255};break;}}
  double trail=std::min(bolt.weapon==Seeker?58.:bolt.weapon==Kinetic?26.:42.,length(bolt.velocity)*.035);
  if(bolt.weapon==Resonance){
   Color colours[]={ice,violet,mint};int steps=reduced?5:10;
   for(int strand=0;strand<(reduced?2:3);strand++)for(int k=0;k<steps;k++){
    auto point=[&](int i){double t=double(i)/steps,angle=t*2*pi+strand*2*pi/3+g.time*15;return bolt.position-direction*(trail*t)+(a*std::cos(angle)+b*std::sin(angle))*(.45+.3*t);};
    tube(point(k),point(k+1),.13,alpha(colours[strand],1.-k*.65/steps));
   }
  }else{
   auto end=bolt.position-direction*trail;
   tube(bolt.position,end,bolt.weapon==Plasma?.54:bolt.weapon==Seeker?.30:.17,alpha(c,bolt.weapon==Plasma?.09:.35));
   if(bolt.weapon==Plasma)tube(bolt.position,end,.15,alpha(violet,.42));
   tube(bolt.position,end,bolt.weapon==Kinetic?.065:.10,alpha(hot,.9));
   if(bolt.weapon==Plasma){
    glow(bolt.position,4.2,violet,.55);glow(bolt.position,1.2,hot,.85);
    ring(bolt.position,a,b,1.15,.09,alpha(ice,.6),segments);
    if(!reduced)for(int i=0;i<3;i++){double angle=g.time*17+i*2*pi/3;auto at=bolt.position+(a*std::cos(angle)+b*std::sin(angle))*.85;glow(at,.85,i%2?rose:mint,.65);}
   }else if(bolt.weapon==Seeker){
    glow(bolt.position,1.4,hot,.85);
    for(int i=0;i<(reduced?3:7);i++){double u=(i+1)/8.;auto at=bolt.position-direction*(trail*u);ring(at,a,b,.25+u*.9,.1,alpha(i%2?amber:violet,(1-u)*.5),12);}
   }
  }
 }
 for(const auto& e:g.effects){
  if(length(e.position-g.ship.position)>3000||dot(e.position-g.ship.position,g.ship.forward)<-40)continue;
  double t=std::clamp(e.age/e.duration,0.,1.),fade=1-t;
  auto a=unit(cross(e.normal,up),right),b=unit(cross(a,e.normal));Color c=pigment(e.weapon);
  if(e.kind==ChainDischarge){
   auto direction=unit(e.end-e.position);a=unit(cross(direction,g.ship.forward),right);b=unit(cross(a,direction));
   Color colours[]={ice,violet,mint};int steps=reduced?8:16;
   for(int strand=0;strand<(reduced?1:3);strand++)for(int k=0;k<steps;k++){
    auto point=[&](int i){double u=double(i)/steps,angle=u*4*pi+strand*2*pi/3+e.age*23;return mix(e.position,e.end,u)+(a*std::cos(angle)+b*std::sin(angle))*(std::sin(u*pi)*1.2);};
    tube(point(k),point(k+1),.16,alpha(colours[strand],fade));
   }
   continue;
  }
  if(e.kind==MuzzleFlash){
   double envelope=fade*fade,scale=e.strength*e.size;
   tube(e.position,e.position+e.normal*((e.weapon==Kinetic?3.:1.8)*envelope+.2),.075*envelope*scale,alpha(hot,envelope));
   glow(e.position,(.5+envelope*2.2)*scale,c,envelope*.6);
   ring(e.position,a,b,(.18+.8*t)*scale,.10*envelope,alpha(c,envelope*.7),segments);
   for(int i=0;i<(reduced?3:5);i++){double angle=i*2*pi/5+t*.7;auto dir=a*std::cos(angle)+b*std::sin(angle);tube(e.position,e.position+dir*(.2+envelope*.6)*scale,.055,alpha(c,envelope*.8));}
   continue;
  }
  if(e.kind==ShieldImpact){
   double radius=(.6+std::sqrt(t)*6)*std::sqrt(e.strength);
   ring(e.position+e.normal*.08,a,b,radius,.12+fade*.13,alpha(ice,fade*.7),segments);
   ring(e.position+e.normal*.12,a,b,radius*.66,.07,alpha(violet,fade*.55),segments);
   // A small hexagonal interference patch follows the impact plane.
   if(!reduced)for(int i=0;i<7;i++){double angle=i*2*pi/6;auto centre=e.position+(i<6?(a*std::cos(angle)+b*std::sin(angle))*(radius*.47):V3{});ring(centre+e.normal*.16,a,b,radius*.27,.035,alpha(i%2?mint:violet,fade*fade*.36),6);}
   if(t<.3)glow(e.position,(1-t/.3)*2.2*e.strength,hot,fade*.8);
   continue;
  }
  if(e.kind==ShieldBreak){
   double radius=e.size*(1.05+t*.8);
   ring(e.position,right,up,radius,.10+fade*.18,alpha(violet,fade*.55),segments);
   int count=std::min(sparkBudget,reduced?7:lowGraphics?14:24);sparkBudget-=count;
   for(int i=0;i<count;i++){
    auto dir=radial(i,e.normal,a,b,false),at=e.position+dir*radius;
    auto tangent=unit(cross(dir,up),right),across=unit(cross(dir,tangent));
    double shard=e.size*(.10+.06*seed(i,4))*fade;
    ring(at,tangent,across,shard,.04,alpha(i%3==0?mint:i%3==1?ice:violet,fade*.7),3);
   }continue;
  }
  if(e.kind==ShipExplosion){
   double radius=e.size*(.45+std::sqrt(t)*3.5),flash=std::exp(-e.age*13);
   ring(e.position,right,up,radius,.08+fade*.14,alpha(rose,fade*.5),segments);
   if(!reduced)ring(e.position,a,b,radius*.84,.12,alpha(mint,fade*.35),segments);
   glow(e.position,e.size*(1.1+e.age*1.3),hot,flash*.95);
   glow(e.position,e.size*(2+e.age*2.5),violet,std::exp(-e.age*4)*.22);
   for(int i=0;i<(reduced?3:7);i++){
    auto dir=radial(i,e.normal,a,b,false);auto at=e.position+dir*(e.size*(1-std::exp(-e.age*2.4))*(.6+seed(i,9)));
    double envelope=std::max(0.,1-e.age/(.55+seed(i,6)*.55));
    if(envelope>0){double size=e.size*(.3+seed(i,3)*.5)*(1+e.age)*envelope;glow(at,size*2,i%3==0?violet:i%3==1?amber:rose,envelope*.65);glow(at,size*.55,hot,envelope*envelope*.6);}
   }
  }else if(e.kind==HullImpact){
   double flash=std::exp(-e.age*24)*e.strength;
   glow(e.position,.6+flash*2.6,hot,std::min(1.,flash));
   if(!reduced)ring(e.position+e.normal*.1,a,b,.4+t*2,.07,alpha(c,fade*fade*.35),segments);
  }else if(e.kind==DecoyBurst){ring(e.position,right,up,2+20*t,.15,alpha(mint,fade*.65),segments);}
  int wanted=e.kind==ShipExplosion?(reduced?9:lowGraphics?22:36):e.kind==DecoyBurst?10:reduced?4:lowGraphics?9:16;
  int count=std::min(sparkBudget,wanted);sparkBudget-=count;
  for(int i=0;i<count;i++){
   auto direction=radial(i,e.normal,a,b,e.kind==HullImpact);
   double speed=(e.kind==ShipExplosion?e.size*2.6:e.kind==DecoyBurst?18:10)*(.45+seed(i,3));
   double travel=(1-std::exp(-e.age*2))*speed,tail=std::min(travel,.25+speed*.025)*fade;
   auto at=e.position+direction*travel;
   Color tint=e.kind==HullImpact?(i%4==0?c:amber):i%3==0?mint:i%3==1?violet:rose;
   tube(at,at-direction*tail,reduced?.025:.04,alpha(tint,fade*fade*.85));
   if(!reduced&&i%3==0)line(at,at+direction*tail*.25,alpha(hot,fade*.7));
  }
 }
 EndBlendMode();rlEnableDepthMask();
}
}
