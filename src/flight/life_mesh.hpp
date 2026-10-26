#pragma once
#include "game.hpp"
#include <utility>
namespace hyperion::flight {
struct LifeMesh {std::vector<float> positions,normals;std::vector<unsigned char> colours;};
enum LifeMaterial : unsigned char {LifeFlesh=75,LifeShell=120,LifeMembrane=170,LifeVein=210,LifeEye=238,LifePupil=255};
namespace life_detail {
struct Builder {
 LifeMesh mesh;bool detailed;int part=0;
 void vertex(V3 p,V3 n,unsigned char material,double flex=0){
  for(double v:{p.x,p.y,p.z})mesh.positions.push_back(float(v));
  for(double v:{n.x,n.y,n.z})mesh.normals.push_back(float(v));
  mesh.colours.insert(mesh.colours.end(),{material,static_cast<unsigned char>(std::clamp(flex,0.,1.)*255),static_cast<unsigned char>((part%16)*16),255});
 }
 void face(V3 a,V3 b,V3 c,unsigned char material,double flex=0){auto n=unit(cross(b-a,c-a));for(auto p:{a,b,c})vertex(p,n,material,flex);}
 void ball(V3 c,V3 r,unsigned char material=LifeFlesh,double flex=0){
  int around=detailed?14:10,rings=detailed?8:6;
  auto pt=[&](int x,int y){double a=x*2*pi/around,t=y*pi/rings;return c+V3{r.x*std::sin(t)*std::cos(a),r.y*std::cos(t),r.z*std::sin(t)*std::sin(a)};};
  auto tri=[&](V3 a,V3 b,V3 d){for(auto p:{a,b,d}){auto n=p-c;vertex(p,unit({n.x/(r.x*r.x),n.y/(r.y*r.y),n.z/(r.z*r.z)}),material,flex);}};
  for(int y=0;y<rings;y++)for(int x=0;x<around;x++){auto a=pt(x,y),b=pt(x+1,y),d=pt(x,y+1),e=pt(x+1,y+1);if(y)tri(a,b,d);if(y+1<rings)tri(b,e,d);}
 }
 void tube(V3 a,V3 control,V3 b,double ra,double rb,unsigned char material=LifeFlesh,double flexibility=1,double rootFlex=0){
  bool fine=ra<.021;int sides=fine?4:detailed?7:5,steps=fine?2:detailed?5:3;double total=length(control-a)+length(b-control);auto axisDirection=unit(b-a),reference=std::abs(axisDirection.y)>.9?V3{1,0,0}:V3{0,1,0};
  auto centre=[&](double t){return a*((1-t)*(1-t))+control*(2*t*(1-t))+b*(t*t);};
  auto ring=[&](int row,int k){double t=double(row)/steps,angle=k*2*pi/sides;auto dir=unit((control-a)*(1-t)+(b-control)*t),axis=unit(cross(dir,reference)),side=unit(cross(dir,axis));auto radial=axis*std::cos(angle)+side*std::sin(angle);return std::pair<V3,V3>{centre(t)+radial*(ra+(rb-ra)*t),unit(radial-dir*((rb-ra)/std::max(.001,total)))};};
  auto emit=[&](int row,int k){auto p=ring(row,k);vertex(p.first,p.second,material,rootFlex+(flexibility-rootFlex)*row/steps);};
  for(int j=0;j<steps;j++)for(int k=0;k<sides;k++){emit(j,k);emit(j,k+1);emit(j+1,k);emit(j,k+1);emit(j+1,k+1);emit(j+1,k);}
  for(int k=0;k<sides;k++){face(a,ring(0,k+1).first,ring(0,k).first,material,rootFlex);face(b,ring(steps,k).first,ring(steps,k+1).first,material,flexibility);}
 }
 void leaf(V3 a,V3 bend,V3 b,V3 side,double width,unsigned char material=LifeMembrane){
  int steps=detailed?6:4;side=unit(side);V3 oldL=a,oldR=a;double oldT=0;
  auto normal=[&](double t){return unit(cross(side,(bend-a)*(1-t)+(b-bend)*t));};
  auto tri=[&](V3 p,V3 q,V3 r,double tp,double tq,double tr,bool back){auto emit=[&](V3 v,double t){vertex(v,normal(t)*(back?-1:1),material,t);};if(back){emit(r,tr);emit(q,tq);emit(p,tp);}else{emit(p,tp);emit(q,tq);emit(r,tr);}};
  for(int j=1;j<=steps;j++){double t=double(j)/steps,w=j==steps?0:std::sin(pi*t)*width;auto c=a*((1-t)*(1-t))+bend*(2*t*(1-t))+b*(t*t),l=c+side*w,r=c-side*w;
   for(bool back:{false,true}){if(j>1)tri(oldL,l,oldR,oldT,t,oldT,back);if(j<steps)tri(oldR,l,r,oldT,t,t,back);}oldL=l;oldR=r;oldT=t;
  }
  tube(a,bend,b,.014,.007,LifeVein,.85);
  if(detailed&&width>.17)for(double t:{.35,.65}){auto c=a*((1-t)*(1-t))+bend*(2*t*(1-t))+b*(t*t);double w=std::sin(pi*t)*width*.83;for(int sign:{-1,1})tube(c,c+side*(w*sign*.5)+(b-a)*.035,c+side*(w*sign),.006,.004,LifeVein,.8);}
 }
 void disc(V3 c,V3 normal,double r,unsigned char material){
  normal=unit(normal);auto x=unit(cross(normal,std::abs(normal.y)>.9?V3{1,0,0}:V3{0,1,0})),y=cross(normal,x);int n=detailed?12:8;
  for(int k=0;k<n;k++){double a=k*2*pi/n,b=(k+1)*2*pi/n;for(auto p:{c,c+(x*std::cos(a)+y*std::sin(a))*r,c+(x*std::cos(b)+y*std::sin(b))*r})vertex(p,normal,material);}
 }
 void eye(V3 c,V3 normal,double r){
  normal=unit(normal);disc(c,normal,r*1.18,LifeShell);disc(c+normal*.007,normal,r,LifeEye);disc(c+normal*.012,normal,r*.58,LifeVein);disc(c+normal*.017,normal,r*.28,LifePupil);
  auto side=unit(cross(normal,{0,1,0}),{1,0,0});disc(c+normal*.021+side*r*.22+V3{0,r*.25,0},normal,r*.13,LifeEye);
 }
 void cup(V3 c,double size){
  int n=detailed?14:9;for(int j=0;j<3;j++)for(int k=0;k<n;k++){
   auto pt=[&](int row,int col){double t=row/3.,r=(.07+.29*t*t)*size,a=col*2*pi/n;return c+V3{std::cos(a)*r,t*.55*size,std::sin(a)*r};};
   auto a=pt(j,k),b=pt(j,k+1),d=pt(j+1,k),e=pt(j+1,k+1);
   for(bool back:{false,true}){auto tri=[&](V3 x,V3 y,V3 z){auto n=unit(cross(y-x,z-x));if(back){std::swap(y,z);n=-n;}for(auto p:{x,y,z})vertex(p,n,back?LifeFlesh:LifeMembrane,double(j+1)/3);};tri(a,d,b);tri(b,d,e);}
  }
  ball(c+V3{0,.16*size,0},{.09*size,.1*size,.09*size},LifeVein);
 }
};
}
inline LifeMesh lifeMesh(int plan,bool flora,bool detailed=false){
 using namespace life_detail;Builder b{{},detailed};
 auto stem=[&](V3 a,V3 mid,V3 end,double r=.06){b.tube(a,mid,end,r,r*.35,LifeShell,.75);};
 if(flora){
  // Embedded root collars, curved primary growth and separate membrane/vein tissue.
  b.ball({0,.12,0},{.22,.12,.22},LifeShell);
  for(int k=0;k<5;k++){double a=k*2*pi/5;V3 d{std::cos(a),0,std::sin(a)};b.part=k;stem({0,.13,0},d*.18+V3{0,.07,0},d*.36+V3{0,.025,0},.04);}
  if(plan==0||plan==1){stem({0,.1,0},{.18,1.1,.06},{0,2.05,0},.085);int count=plan==0?7:11;
   for(int k=0;k<count;k++){b.part=k;double angle=k*2.39996;V3 d{std::cos(angle),0,std::sin(angle)},side=unit(cross(d,{0,1,0}));double y=.35+k*(plan==0?.2:.14);V3 a{0,y,0},tip=d*(plan==0?.8:.64)+V3{0,y+.42,0},mid=mix(a,tip,.55)+V3{0,.22,0};
    b.leaf(a,mid,tip,side,plan==0?.34:.19);if(plan==1&&detailed)for(int sign:{-1,1}){auto root=mix(a,tip,.58);b.leaf(root,root+d*.12+side*(sign*.14),root+d*.25+side*(sign*.22),d,.075);}
   }
  }else if(plan==2){for(int k=0;k<4;k++){b.part=k;double a=k*2.4;V3 d{std::cos(a),0,std::sin(a)},tip=d*.5+V3{0,.8+k*.23,0};stem({0,.15,0},d*.35+V3{0,.6,0},tip,.065);b.cup(tip,.85+k*.09);}}
  else if(plan==3){for(int k=0;k<4;k++){b.part=k;double a=k*2.4;V3 d{std::cos(a),0,std::sin(a)},tip=d*.45+V3{0,1.2+k*.25,0};stem(d*.1+V3{0,.15,0},d*.32+V3{0,.9,0},tip,.14);
    for(int j=0;j<4;j++){double t=(j+1)/5.;auto c=mix(d*.1+V3{0,.15,0},tip,t);b.ball(c,{.14*(1-t*.5),.065,.14*(1-t*.5)},LifeVein);for(int sign:{-1,1})b.leaf(c,c+d*(sign*.17)+V3{0,.12,0},c+d*(sign*.3)+V3{0,.18,0},unit(cross(d,{0,1,0})),.12);}
   }}
  else if(plan==4){for(int k=0;k<5;k++){b.part=k;double a=k*2.4;V3 d{std::cos(a),0,std::sin(a)},tip=d*.58+V3{0,1.05+k*.16,0};stem({0,.13,0},d*.45+V3{0,.75,0},tip,.075);b.ball(tip,{.24,.32,.24},LifeMembrane,.5);
    for(int j=0;j<(detailed?8:5);j++){double t=j*2*pi/(detailed?8:5);V3 side{std::cos(t),.45,std::sin(t)};b.tube(tip+side*.13,tip+side*.3+V3{0,.1,0},tip+side*.4,.014,.006,LifeVein);}
   }}
  else{for(int k=0;k<3;k++){b.part=k;double a=k*2*pi/3;V3 d{std::cos(a),0,std::sin(a)};stem(d*.13+V3{0,.12,0},d*.85+V3{0,1.1,0},d*.22+V3{0,2.1,0},.075);
    auto side=unit(cross(d,{0,1,0}));for(int j=0;j<5;j++){double y=.35+j*.3;V3 c=d*(.2+std::sin((j+1)*.5)*.3)+V3{0,y,0};b.tube(c-side*.21,c+V3{0,.08,0},c+side*.21,.024,.017,LifeVein);if(j%2==0)b.leaf(c-side*.2,c+V3{0,.16,0},c+side*.2,d,.1);}
   }}
  return std::move(b.mesh);
 }
 if(plan==0){
  // Open bell, oral disk and separate trailing oral arms.
  int n=detailed?18:12,rows=detailed?7:4;
  auto pt=[&](int y,int k){double t=double(y)/rows*pi*.5,a=k*2*pi/n;return V3{.78*std::sin(t)*std::cos(a),1.02+.58*std::cos(t),.78*std::sin(t)*std::sin(a)};};
  auto face=[&](V3 a,V3 c,V3 d){for(auto q:{a,c,d})b.vertex(q,unit({q.x/(.78*.78),(q.y-1.02)/(.58*.58),q.z/(.78*.78)}),LifeMembrane,.15);};
  for(int y=0;y<rows;y++)for(int k=0;k<n;k++){auto a=pt(y,k),c=pt(y,k+1),d=pt(y+1,k),e=pt(y+1,k+1);if(y)face(a,c,d);face(c,e,d);}
  b.ball({0,1.02,0},{.38,.1,.38},LifeVein);
  for(int k=0;k<8;k++){b.part=k;double a=k*2*pi/8;V3 d{std::cos(a),0,std::sin(a)};b.tube(d*.68+V3{0,1.04,0},d*.9+V3{0,.6,0},d*.7+V3{0,.1,0},.045,.012,LifeMembrane);if(k%2==0)b.tube(d*.2+V3{0,1,0},d*.35+V3{0,.6,0},d*.18+V3{0,.24,0},.065,.02,LifeFlesh);}
 }else if(plan==1){
  b.tube({0,.47,-1.04},{.13,.55,0},{0,.4,1.27},.29,.045,LifeFlesh,.65);
  for(int k=0;k<5;k++){b.part=k;double z=-.78+k*.4;for(int sign:{-1,1})b.leaf({sign*.12,.5,z},{sign*.48,.6,z+.1},{sign*.61,.49,z+.35},{0,1,0},.14);}
  for(int sign:{-1,1})b.eye({sign*.15,.57,-1.03},{sign*.3,.1,-1},.055);
  b.disc({0,.4,-1.055},{0,0,-1},.075,LifePupil);
 }else if(plan==2){
  b.ball({0,.78,.1},{.47,.36,.78});b.ball({0,.69,.62},{.33,.27,.37},LifeShell);
  b.tube({0,.86,-.4},{0,1.08,-.68},{0,1.05,-.98},.19,.15,LifeShell,.2);b.ball({0,1.05,-.99},{.25,.22,.29});
  for(int sign:{-1,1})for(int j=0;j<3;j++){b.part=j+int(sign>0)*4;double z=-.44+j*.45;V3 root{sign*.29,.66,z},knee{sign*.57,.33,z+.08},foot{sign*.49,.055,z+.23};b.tube(root,{sign*.51,.6,z},knee,.075,.045,LifeShell,.6);b.tube(knee,{sign*.56,.17,z+.2},foot,.045,.022,LifeFlesh,1,.6);}
  for(int sign:{-1,1}){b.part=sign+2;b.leaf({sign*.22,.9,.18},{sign*.62,1.35,.22},{sign*.87,1.23,.51},{0,0,1},.22);b.tube({sign*.13,1.19,-1},{sign*.24,1.45,-1.08},{sign*.32,1.42,-1.22},.028,.01,LifeVein);b.eye({sign*.17,1.12,-1.2},{sign*.3,.12,-1},.065);}
  b.disc({0,.98,-1.278},{0,-.1,-1},.074,LifePupil);
 }else if(plan==3){
  b.tube({0,.32,-.95},{0,.39,0},{0,.3,1.02},.28,.19,LifeFlesh,.2);
  for(int j=0;j<5;j++){double z=-.73+j*.37;b.ball({0,.54,z},{.5,.23,.28},LifeShell);for(int sign:{-1,1}){b.part=j+int(sign>0)*6;b.tube({sign*.35,.43,z},{sign*.67,.29,z+.05},{sign*.78,.055,z+.22},.05,.017,LifeShell);}}
  for(int sign:{-1,1}){b.eye({sign*.17,.54,-1},{sign*.15,.08,-1},.05);b.tube({sign*.19,.57,-.88},{sign*.35,.85,-1.12},{sign*.46,.82,-1.31},.018,.007,LifeVein);}
 }else if(plan==4){
  for(int k=0;k<6;k++){b.part=k;double a=k*2.4;V3 c{std::cos(a)*.55,.65+std::sin(a)*.18,std::sin(a)*.5};b.ball(c,{.14,.16,.29},LifeFlesh,.15);b.leaf(c+V3{0,0,.19},c+V3{0,.04,.38},c+V3{0,0,.57},{0,1,0},.19);b.leaf(c+V3{0,.09,-.03},c+V3{0,.28,.05},c+V3{0,.3,.24},{0,0,1},.1);for(int sign:{-1,1})b.eye(c+V3{sign*.095,.04,-.21},{sign*.5,0,-1},.032);}
 }else{
  b.ball({0,.43,0},{.32,.23,.48});
  for(int sign:{-1,1})for(int k=0;k<5;k++){b.part=k+int(sign>0)*6;double z=-.43+k*.22;V3 a{sign*.1,.51,z},mid{sign*.51,.88,z+.08},end{sign*.79,1.15,z+.23};b.tube(a,mid,end,.035,.012,LifeShell);for(int j=0;j<(detailed?4:2);j++){double t=.32+j*(detailed?.16:.3);auto root=mix(a,end,t);b.leaf(root,root+V3{sign*.16,.1,-.08},root+V3{sign*.23,.16,-.16},{0,1,0},.075);}}
  for(int sign:{-1,1})b.eye({sign*.12,.49,-.43},{sign*.2,0,-1},.045);
 }
 return std::move(b.mesh);
}
}
