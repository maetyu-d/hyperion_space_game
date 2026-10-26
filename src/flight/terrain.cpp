#include "renderer.hpp"
#include "rlgl.h"
#include <cstring>

namespace hyperion::flight {
namespace {
Vector3 vec(V3 v){return {float(v.x),float(v.y),float(v.z)};}
double smooth(double a,double b,double v){v=std::clamp((v-a)/(b-a),0.,1.);return v*v*(3-2*v);}
V3 coarsePoint(const Planet& p,V3 n){
 // Intersect the exact triangle plane of the existing 192 x 96 globe.
 double u=std::atan2(n.z,n.x)/(2*pi);if(u<0)u+=1;double v=std::acos(std::clamp(n.y,-1.,1.))/pi;
 int x=std::min(191,int(u*192)),y=std::min(95,int(v*96));double fx=u*192-x,fy=v*96-y;
 auto point=[&](int a,int b){double t=b*pi/96,f=a*2*pi/192;V3 d{std::sin(t)*std::cos(f),std::cos(t),std::sin(t)*std::sin(f)};return d*(p.radius+terrain(p,d));};
 V3 a,b,c;if(fx+fy<=1){a=point(x,y);b=point(x+1,y);c=point(x,y+1);}else{a=point(x+1,y+1);b=point(x,y+1);c=point(x+1,y);}
 auto normal=cross(b-a,c-a);double den=dot(n,normal);return std::abs(den)>1e-9?n*(dot(a,normal)/den):n*(p.radius+terrain(p,n));
}
struct MeshData {
 std::vector<float> positions,normals,uv;std::vector<unsigned char> colours;std::vector<unsigned short> indices;
 void vertex(V3 p,V3 n,Color c=WHITE,double base=0){for(double v:{p.x,p.y,p.z})positions.push_back(float(v));for(double v:{n.x,n.y,n.z})normals.push_back(float(v));uv.insert(uv.end(),{float(base),0});for(auto v:{c.r,c.g,c.b,c.a})colours.push_back(v);}
 void triangle(V3 a,V3 b,V3 c,Color tint,double base,V3 roundedAt={},double rounded=0){auto n=unit(cross(b-a,c-a));for(auto point:{a,b,c})vertex(point,unit(mix(n,unit(point-roundedAt,n),rounded)),tint,base);}
 Model model(Shader shader,Texture2D noise,Texture2D surface,Texture2D geology,Texture2D detail){Mesh m{};m.vertexCount=int(positions.size()/3);m.triangleCount=indices.empty()?m.vertexCount/3:int(indices.size()/3);
  auto copy=[](const auto& v){auto* p=MemAlloc(unsigned(v.size()*sizeof(v[0])));std::memcpy(p,v.data(),v.size()*sizeof(v[0]));return p;};
  m.vertices=static_cast<float*>(copy(positions));m.normals=static_cast<float*>(copy(normals));m.texcoords=static_cast<float*>(copy(uv));m.colors=static_cast<unsigned char*>(copy(colours));if(!indices.empty())m.indices=static_cast<unsigned short*>(copy(indices));
  UploadMesh(&m,false);auto result=LoadModelFromMesh(m);auto& mat=result.materials[0];mat.shader=shader;mat.maps[MATERIAL_MAP_NORMAL].texture=noise;mat.maps[MATERIAL_MAP_ALBEDO].texture=surface;mat.maps[MATERIAL_MAP_ROUGHNESS].texture=geology;mat.maps[MATERIAL_MAP_METALNESS].texture=detail;return result;
 }
};
// Low, embedded bases and varied shoulders make outcrops read as part of
// the ground. Ice and basalt retain their hard crystal/column facets.
void outcrop(MeshData& mesh,V3 at,V3 up,V3 east,V3 north,double size,double height,double phase,int type,Color tint,double baseRadius,int quality=1){
 const int sides=type==2||type==3?6:quality==2?10:quality==1?8:6,ringCount=quality==2?4:quality==1?3:2;std::array<std::vector<V3>,4> rings;
 const double levels[]={-.10,.22,.68,.91},widths[]={.82,1.,.77,.38};
 V3 lean=(east*std::sin(phase*7.1)+north*std::cos(phase*5.3))*height*.15;
 for(int r=0;r<ringCount;r++)for(int i=0;i<sides;i++){
  int levelIndex=ringCount==4?r:ringCount==3?(r==2?3:r):(r==1?2:0);
  double angle=i*2*pi/sides+phase*6.28,irregular=type>=2?1.:.84+.16*std::sin(i*4.7+phase*13.);
  double width=type==2?(levelIndex==0?.68:levelIndex==1?.85:levelIndex==2?.64:.13):type==3?(levelIndex==3?.78:.9):widths[levelIndex];
  double level=levels[levelIndex];if(r>0)level+=std::sin(i*3.1+phase*11.)*(type>=2?.025:.075);
  rings[r].push_back(at+(east*std::cos(angle)+north*std::sin(angle))*(size*width*irregular)+up*(height*level)+lean*std::max(0.,level));
 }
 double rounded=type>=2?0.:type==1?.15:.36;auto centre=at+up*height*.4;
 for(int i=0;i<sides;i++){int next=(i+1)%sides;
  for(int r=0;r<ringCount-1;r++){mesh.triangle(rings[r][i],rings[r][next],rings[r+1][next],tint,baseRadius,centre,rounded);mesh.triangle(rings[r][i],rings[r+1][next],rings[r+1][i],tint,baseRadius,centre,rounded);}
  mesh.triangle(rings[ringCount-1][i],rings[ringCount-1][next],at+up*height+lean,tint,baseRadius,centre,rounded);
 }
}
uint32_t hash(int x,int y,int face,double seed){uint32_t h=uint32_t(x)*73856093u^uint32_t(y)*19349663u^uint32_t(face)*83492791u^uint32_t(seed*1300);h^=h>>16;h*=2246822519u;return h^(h>>13);}
V3 cube(int face,double u,double v){switch(face){case 0:return unit({1,v,-u});case 1:return unit({-1,v,u});case 2:return unit({u,1,-v});case 3:return unit({u,-1,v});case 4:return unit({u,v,1});default:return unit({-u,v,-1});}}
}
void Renderer::updateGround(const Game& g,int body){
 const auto& p=g.planets[body];double altitude=g.altitude();groundBlend=float(1-smooth(95,330,altitude));bool active=!p.giant&&altitude<335;
 if(!active){groundBody=-1;groundBlend=0;return;}auto n=unit(g.ship.position-p.centre);
 if(groundBody==body&&groundModel.meshCount&&length(n-groundAnchor)*p.radius<22)return;
 groundBody=body;groundAnchor=n;if(groundModel.meshCount)UnloadModel(groundModel);if(rockModel.meshCount){UnloadModel(rockModel);rockModel={};}
 auto east=unit(cross({0,1,0},n),{1,0,0}),north=unit(cross(n,east));constexpr int side=97;constexpr double span=210;std::vector<V3> points;points.reserve(side*side);std::vector<double> coarse;coarse.reserve(side*side);
 for(int y=0;y<side;y++)for(int x=0;x<side;x++){
  double u=(x/(side-1.)*2-1)*span,v=(y/(side-1.)*2-1)*span;auto d=unit(n+east*(u/p.radius)+north*(v/p.radius));double blend=smooth(128,169,std::hypot(u,v));
  auto fine=d*(p.radius+terrain(p,d)+2),base=coarsePoint(p,d)+d*.015;points.push_back(mix(fine,base,blend));coarse.push_back(length(base));
 }
 MeshData ground;for(int y=0;y<side;y++)for(int x=0;x<side;x++){
  auto dx=points[y*side+std::min(side-1,x+1)]-points[y*side+std::max(0,x-1)],dy=points[std::min(side-1,y+1)*side+x]-points[std::max(0,y-1)*side+x];ground.vertex(points[y*side+x],unit(cross(dx,dy),n),WHITE,coarse[y*side+x]);
  if(x+1<side&&y+1<side){unsigned short a=static_cast<unsigned short>(y*side+x),b=a+1,c=a+side,d=c+1;ground.indices.insert(ground.indices.end(),{a,b,c,b,d,c});}
 }
 groundModel=ground.model(groundShader,noiseTexture,surfaceMaps[body].texture,geologyMaps[body].texture,detailMap.texture);groundModel.materials[0].maps[MATERIAL_MAP_OCCLUSION].texture=sunScene.texture;
 // Stable cube-face cells keep small outcrops fixed when the detail patch moves.
 MeshData rocks;
 for(int face=0;face<6;face++){
  double den=dot(n,cube(face,0,0));if(den<.35)continue;double u=0,v=0;
  switch(face){case 0:u=-n.z/den;v=n.y/den;break;case 1:u=n.z/den;v=n.y/den;break;case 2:u=n.x/den;v=-n.z/den;break;case 3:u=n.x/den;v=n.z/den;break;case 4:u=n.x/den;v=n.y/den;break;default:u=-n.x/den;v=n.y/den;break;}
  int cx=std::clamp(int((u+1)*64),0,127),cy=std::clamp(int((v+1)*64),0,127);
 int reach=std::min(128,int(std::ceil(175*64/(p.radius*den*den))));
 for(int y=std::max(0,cy-reach);y<=std::min(127,cy+reach);y++)for(int x=std::max(0,cx-reach);x<=std::min(127,cx+reach);x++){
  auto h=hash(x,y,face,p.seed);double j=(h&65535)/65535.,k=(h>>16)/65535.;auto d=cube(face,(x+.2+j*.6)/64-1,(y+.2+k*.6)/64-1);
  if(length(d-n)*p.radius>145||materialElevation(p,d)<.28+.44*p.water)continue;auto at=d*(p.radius+terrain(p,d)+2);bool pad=false;
  for(const auto& port:g.ports)if(port.planet==body&&length(p.centre+at-port.position)<65){pad=true;break;}if(pad)continue;
  auto a=unit(cross({0,1,0},d),{1,0,0}),b=unit(cross(d,a));double size=.3+j*1.2,height=.25+k*.9;
  bool formation=h%11==0,crystal=p.kind==2&&h%5==0,column=p.kind==3&&h%7==0;
  if(formation){size*=2.5;height*=1.7;}if(p.kind==1){size*=1.35;height*=.7;}if(crystal){size*=.7;height=1.8+k*3.2;}if(column){size*=.8;height=1.4+k*2.6;}
  Color tint=p.kind==2?Color{164,220,236,255}:Color{static_cast<unsigned char>(155+j*70),static_cast<unsigned char>(153+k*65),static_cast<unsigned char>(159+j*68),255};
  int type=crystal?2:column?3:p.kind==1?1:0;double baseRadius=length(at);
  outcrop(rocks,at,d,a,b,size,height,j+k*.31,type,tint,baseRadius,formation||crystal||column?2:1);
  // Sparse small companions break the single-object-per-cell rhythm. Their
  // positions depend on the same fixed cell, never on the moving ground patch.
  if(h%4==0){
   double angle=j*13.7;auto offset=a*std::cos(angle)+b*std::sin(angle),smallD=unit(at+offset*(size*1.6+.45));
   auto smallAt=smallD*(p.radius+terrain(p,smallD)+2);
   bool clear=std::none_of(g.ports.begin(),g.ports.end(),[&](const Port& port){return port.planet==body&&length(p.centre+smallAt-port.position)<60;});
   if(clear&&materialElevation(p,smallD)>.28+.44*p.water)outcrop(rocks,smallAt,smallD,a,b,.16+j*.24,.11+k*.22,k+j*.57,0,tint,length(smallAt),0);
  }
 }
 }
 if(!rocks.positions.empty()){rockModel=rocks.model(groundShader,noiseTexture,surfaceMaps[body].texture,geologyMaps[body].texture,detailMap.texture);rockModel.materials[0].maps[MATERIAL_MAP_OCCLUSION].texture=sunScene.texture;}
}
void Renderer::drawGroundEffects(const Game& g){
 auto c=g.landing();if(!c.available||c.altitude>80)return;auto n=c.normal,a=unit(cross({1,0,0},n),{0,0,1}),b=unit(cross(a,n));auto anchor=c.ground;
 double guideFade=(1-smooth(38,55,c.altitude))*(g.docked?1-smooth(0,.9,g.touchdownAge):1);
 if(g.ship.gear&&guideFade>.01){
  auto guide=c.port>=0?g.ports[c.port].position+n*.32:anchor;Color tint=c.safe?Color{136,255,209,160}:Color{255,150,126,150};tint.a=static_cast<unsigned char>(tint.a*guideFade);
  for(int i=0;i<4;i++){double r=4+i*3.;for(int j=0;j<32;j++){double t=j*2*pi/32,u=(j+1)*2*pi/32;DrawLine3D(vec(guide+n*.09+(a*std::cos(t)+b*std::sin(t))*r),vec(guide+n*.09+(a*std::cos(u)+b*std::sin(u))*r),tint);}}
  DrawLine3D(vec(guide-a*18+n*.1),vec(guide+a*18+n*.1),tint);DrawLine3D(vec(guide-b*18+n*.1),vec(guide+b*18+n*.1),tint);
 }
 double burst=(1-smooth(.1,2.5,g.touchdownAge)),wash=(g.liftThrust*.7+std::max(0.,c.sink)*.06)*(1-smooth(2,22,c.altitude)),strength=std::clamp(std::max(burst,wash),0.,1.);
 if(strength<.015)return;
 auto right=unit(cross(g.ship.forward,g.ship.up)),up=unit(cross(right,g.ship.forward));BeginBlendMode(BLEND_ALPHA);rlDisableDepthMask();BeginShaderMode(dust);rlSetTexture(rlGetTextureIdDefault());rlBegin(RL_TRIANGLES);
 for(int i=0;i<(reduced?18:32);i++){
  double phase=std::fmod(g.time*.45+i*.6180339,1.),angle=i*2.399963+g.time*.08,spread=(2+phase*16)*strength;auto centre=anchor+(a*std::cos(angle)+b*std::sin(angle))*spread+n*(.2+phase*(c.water?2.6:3.8));double r=(c.water?.7:1.3)+phase*(c.water?1.8:3.);
  Color tint=c.water?Color{143,211,225,0}:Color{174,146,118,0};tint.a=static_cast<unsigned char>((c.water?75:65)*strength*std::sin(phase*pi));rlColor4ub(tint.r,tint.g,tint.b,tint.a);
  for(auto uv:std::array<std::array<float,2>,6>{{{{0,0}},{{1,0}},{{1,1}},{{0,0}},{{1,1}},{{0,1}}}}){auto point=centre+right*((uv[0]*2-1)*r)+up*((uv[1]*2-1)*r*.65);rlTexCoord2f(uv[0],uv[1]);rlVertex3f(float(point.x),float(point.y),float(point.z));}
 }
 rlEnd();rlDrawRenderBatchActive();rlSetTexture(0);EndShaderMode();rlEnableDepthMask();EndBlendMode();
}
}
