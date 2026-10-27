#include "renderer.hpp"
#include "biosphere.hpp"
#include "flight_scene.hpp"
#include "shaders.hpp"
#include "rlgl.h"
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <unordered_map>
namespace hyperion {
namespace {
constexpr Color ink{5,10,18,255},panelInk{8,15,24,246},pearl{234,244,231,255},muted{132,163,170,255},lineColor{59,88,97,255},mint{153,255,218,255};
Color alpha(Color c,float a){c.a=static_cast<unsigned char>(clamp(a,0.f,1.f)*255);return c;}
Color hue(float phase,float sat=.8f,float val=1){return ColorFromHSV(phase*360,sat,val);}
Vector2 v(float x,float y){return {x,y};}
Vector2 polar(Vector2 p,float a,float r){return {p.x+std::cos(a)*r,p.y+std::sin(a)*r};}
std::string num(double n,int places=0){std::ostringstream s;s<<std::fixed<<std::setprecision(places)<<n;return s.str();}
void uniform(Shader s,const char* name,float value){int loc=GetShaderLocation(s,name);if(loc>=0)SetShaderValue(s,loc,&value,SHADER_UNIFORM_FLOAT);}
void integer(Shader s,const char* name,int value){int loc=GetShaderLocation(s,name);if(loc>=0)SetShaderValue(s,loc,&value,SHADER_UNIFORM_INT);}
void vector(Shader s,const char* name,Vector2 value){int loc=GetShaderLocation(s,name);if(loc>=0)SetShaderValue(s,loc,&value,SHADER_UNIFORM_VEC2);}
void texture(Shader s,const char* name,Texture2D t){int loc=GetShaderLocation(s,name);if(loc>=0)SetShaderValueTexture(s,loc,t);}
void line(Vector2 a,Vector2 b,Color c,float width=1){DrawLineEx(a,b,width,c);}
void ring(Vector2 p,float r,Color c,float width=1){DrawRing(p,std::max(0.f,r-width*.5f),r+width*.5f,0,360,std::max(48,int(r*.8)),c);}
void glow(Vector2 p,float r,Color color,float strength=.3){DrawCircleGradient(int(p.x),int(p.y),r,alpha(color,strength),alpha(color,0));}
Color contactColor(const std::string& k){return k=="life"||k=="flora"?mint:k=="mineral"?Color{255,198,126,255}:k=="vent"?Color{255,132,118,255}:k=="signal"?Color{132,197,255,255}:Color{218,158,255,255};}
void polygon(const std::vector<Vector2>& points,Color fill,Color rim){
 if(points.size()<3)return;std::vector<Vector2> poly=points;double area=0;
 for(size_t i=0;i<poly.size();i++){auto a=poly[i],b=poly[(i+1)%poly.size()];area+=a.x*b.y-b.x*a.y;}
 if(area>0)std::reverse(poly.begin(),poly.end());
 auto cross=[](Vector2 a,Vector2 b,Vector2 c){return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);};
 std::vector<int> indices;for(size_t i=0;i<poly.size();i++)indices.push_back(int(i));
 for(int attempts=0;indices.size()>2&&attempts<100;attempts++){
  bool found=false;for(size_t i=0;i<indices.size();i++){
   Vector2 a=poly[indices[(i+indices.size()-1)%indices.size()]],b=poly[indices[i]],c=poly[indices[(i+1)%indices.size()]];
   if(cross(a,b,c)>=-.001f)continue;bool inside=false;
   for(auto index:indices){auto p=poly[index];if((p.x==a.x&&p.y==a.y)||(p.x==b.x&&p.y==b.y)||(p.x==c.x&&p.y==c.y))continue;
    if(cross(a,b,p)<0&&cross(b,c,p)<0&&cross(c,a,p)<0){inside=true;break;}}
   if(inside)continue;DrawTriangle(a,b,c,fill);indices.erase(indices.begin()+i);found=true;break;
  }if(!found)break;
 }
 for(size_t i=0;i<points.size();i++){line(points[i],points[(i+1)%points.size()],{3,9,17,255},4.5f);line(points[i],points[(i+1)%points.size()],rim,1.25f);}
}
}
bool Renderer::init(GraphicsQuality initialQuality){auto assets=std::filesystem::path(GetApplicationDirectory())/"assets/fonts";
 sans=LoadFontEx((assets/"Karla-Regular.ttf").c_str(),64,nullptr,0);mono=LoadFontEx((assets/"Cousine-Regular.ttf").c_str(),32,nullptr,0);
 SetTextureFilter(sans.texture,TEXTURE_FILTER_BILINEAR);SetTextureFilter(mono.texture,TEXTURE_FILTER_BILINEAR);
 scene=LoadShaderFromMemory(nullptr,sceneFragment);orb=LoadShaderFromMemory(nullptr,orbFragment);blur=LoadShaderFromMemory(nullptr,blurFragment);post=LoadShaderFromMemory(nullptr,postFragment);targets();return flightGraphics.init(initialQuality)&&ready();}
bool Renderer::ready()const{return scene.id>0&&orb.id>0&&blur.id>0&&post.id>0&&scene.id!=rlGetShaderIdDefault()&&orb.id!=rlGetShaderIdDefault()&&blur.id!=rlGetShaderIdDefault()&&post.id!=rlGetShaderIdDefault()&&IsRenderTextureValid(history[0])&&IsRenderTextureValid(history[1])&&IsRenderTextureValid(bloom[0])&&IsRenderTextureValid(bloom[1])&&IsRenderTextureValid(composite);}
void Renderer::close(){flightGraphics.shutdown();for(auto& t:history)if(t.id)UnloadRenderTexture(t);for(auto& t:bloom)if(t.id)UnloadRenderTexture(t);if(terrain.id)UnloadTexture(terrain);if(composite.id)UnloadRenderTexture(composite);if(handoffFrame.id)UnloadRenderTexture(handoffFrame);UnloadShader(scene);UnloadShader(orb);UnloadShader(blur);UnloadShader(post);UnloadFont(sans);UnloadFont(mono);}
void Renderer::targets(){int x=GetRenderWidth(),y=GetRenderHeight();if(x==width&&y==height)return;width=x;height=y;presentedMode=-1;
 for(auto& t:history){if(t.id)UnloadRenderTexture(t);t=LoadRenderTexture(width,height);SetTextureFilter(t.texture,TEXTURE_FILTER_BILINEAR);BeginTextureMode(t);ClearBackground(ink);EndTextureMode();}
 for(auto& t:bloom){if(t.id)UnloadRenderTexture(t);t=LoadRenderTexture(std::max(1,width/3),std::max(1,height/3));SetTextureFilter(t.texture,TEXTURE_FILTER_BILINEAR);}current=0;if(composite.id)UnloadRenderTexture(composite);composite=LoadRenderTexture(width,height);SetTextureFilter(composite.texture,TEXTURE_FILTER_BILINEAR);}
void Renderer::updateTerrain(){if(game->s.mode!=Mode::Surface)return;Planet p=game->activePlanet();
 double ox=std::floor(game->s.surface.x/256)*256-terrainSpan/2,oy=std::floor(game->s.surface.y/256)*256-terrainSpan/2;
 std::string key=game->s.seed+":"+p.id+":"+std::to_string(p.generation)+":"+num(ox)+":"+num(oy);if(key==terrainKey)return;
 std::vector<float> values(512*512);for(int y=0;y<512;y++)for(int x=0;x<512;x++)values[size_t(y)*512+x]=float(game->universe.elevation(p,ox+(x+.5)*terrainSpan/512,oy+(y+.5)*terrainSpan/512));
 if(terrain.id)UnloadTexture(terrain);Image im{values.data(),512,512,1,PIXELFORMAT_UNCOMPRESSED_R32};terrain=LoadTextureFromImage(im);SetTextureFilter(terrain,TEXTURE_FILTER_BILINEAR);SetTextureWrap(terrain,TEXTURE_WRAP_CLAMP);terrainKey=key;terrainOrigin={ox,oy};}
void Renderer::background(double dt){int next=1-current;State& s=game->s;Vector2 camera{float(s.galaxy.x),float(s.galaxy.y)};Planet p;
 if(s.mode==Mode::Surface){camera={float(s.surface.x),float(s.surface.y)};p=game->activePlanet();}else if(s.mode==Mode::Ocean){camera={float(s.oceanX),float(s.oceanAtY)};p=game->activePlanet();}else if(s.mode==Mode::System)p=game->planet();
 uniform(scene,"time",clock);uniform(scene,"seed",float((p.seed?p.seed:game->universe.seed())%10000));uniform(scene,"delta",float(dt));
 uniform(scene,"persistence",ui->reduced?.15f:.68f);integer(scene,"mode",int(s.mode));vector(scene,"resolution",v(float(width),float(height)));vector(scene,"viewSize",v(w,h));vector(scene,"camera",camera);
 uniform(scene,"depth",float(s.depth));uniform(scene,"water",float(p.water));uniform(scene,"sea",float(game->universe.seaLevel(p)));uniform(scene,"zoom",float(ui->surfaceZoom));
 vector(scene,"terrainOrigin",v(float(terrainOrigin.x),float(terrainOrigin.y)));uniform(scene,"terrainSpan",terrainSpan);
 BeginTextureMode(history[next]);ClearBackground(ink);BeginShaderMode(scene);if(terrain.id)texture(scene,"terrainTexture",terrain);
 DrawTexturePro(history[current].texture,{0,0,float(width),float(height)},{0,0,float(width),float(height)},{0,0},0,WHITE);EndShaderMode();EndTextureMode();current=next;
 for(int i=0;i<2;i++){Texture2D src=i==0?history[current].texture:bloom[0].texture;
 BeginTextureMode(bloom[i]);ClearBackground(BLACK);vector(blur,"resolution",v(float(bloom[i].texture.width),float(bloom[i].texture.height)));
 vector(blur,"direction",i==0?v(2.f/src.width,0):v(0,2.f/src.height));uniform(blur,"extractLight",i==0?1.f:0.f);BeginShaderMode(blur);
 DrawTexturePro(src,{0,0,float(src.width),float(src.height)},{0,0,float(bloom[i].texture.width),float(bloom[i].texture.height)},{0,0},0,WHITE);EndShaderMode();EndTextureMode();}
}
void Renderer::text(const std::string& s,float x,float y,float size,Color c,bool code){if(panelContent&&(y+size<UILayout::contentTop||y>UILayout::contentBottom(h)))return;DrawTextEx(code?mono:sans,s.c_str(),{x,y},size,code?.1f:.15f,c);}
void Renderer::right(const std::string& s,float x,float y,float size,Color c,bool code){Vector2 sz=MeasureTextEx(code?mono:sans,s.c_str(),size,code?.1f:.15f);text(s,x-sz.x,y,size,c,code);}
std::string Renderer::elide(std::string value,float maxWidth,float size,bool code)const{
 auto measure=[&](const std::string& str){return MeasureTextEx(code?mono:sans,str.c_str(),size,code?.1f:.15f).x;};
 if(measure(value)<=maxWidth)return value;
 while(!value.empty()&&measure(value+"...")>maxWidth){size_t at=value.size()-1;while(at>0&&(static_cast<unsigned char>(value[at])&0xc0)==0x80)--at;value.resize(at);}return value.empty()?"":value+"...";
}
void Renderer::fitted(const std::string& value,float x,float y,float maxWidth,float size,Color colour,bool code){text(elide(value,maxWidth,size,code),x,y,size,colour,code);}
void Renderer::wrapped(const std::string& source,float x,float& y,float max,float size,Color c,bool code){std::istringstream in(source);std::string word,line;while(in>>word){auto candidate=line.empty()?word:line+" "+word;
 if(MeasureTextEx(code?mono:sans,candidate.c_str(),size,.15f).x>max&&!line.empty()){text(line,x,y,size,c,code);y+=size*1.35f;line=word;}else line=candidate;}if(!line.empty()){text(line,x,y,size,c,code);y+=size*1.35f;}}
bool Renderer::disclosure(const std::string& label,const std::string& key,float x,float& y,float width){bool open=ui->expanded.count(key);button(std::string(open?"-  ":"+  ")+label,{x,y,width,30},"disclosure",key);y+=38;return open;}
bool Renderer::button(const std::string& label,Rectangle r,const std::string& action,const std::string& arg,bool enabled,bool primary){
 if(panelContent&&(r.y+r.height<UILayout::contentTop||r.y>UILayout::contentBottom(h)))return false;
 if(ui->panel&&!panelDrawing&&!modalDrawing&&r.x+r.width>UILayout::panelLeft(w)&&r.y>UILayout::header)return false;
 bool hover=CheckCollisionPointRec(uiMouse(),r),pressed=ui->pointerPressed&&CheckCollisionPointRec(ui->pressPosition,r),modalBlock=!modalDrawing&&((ui->help||ui->settings)||((ui->menu||ui->sections)&&action!="menu"&&action!="sections"&&action!="panel"));
 static const std::set<std::string> presentation={"look","graphics","menu","sections","disclosure","selectGood","selectBuilding","selectFaction","filterDestinations","researchPage","clean","pause","flightHUD","orders","divePage","oceanClose","planHours","reportOlder","reportNewer","missionBoard"};
 enabled=enabled&&ui->handoff.ready()&&(!ui->paused||Game::planningAction(action)||presentation.count(action));
 bool visible=r.y>=0&&r.y+r.height<=h;
 if(panelContent&&(r.y<UILayout::contentTop||r.y+r.height>UILayout::contentBottom(h)))visible=false;
 if(ui->auditInterface&&visible&&enabled&&!modalBlock)ui->auditButtons[action+"|"+arg]=r;
 Color fill=primary?alpha(mint,enabled?.90f:.13f):Color{8,19,27,185};if(hover&&enabled)fill=primary?pearl:Color{30,51,56,245};
 const bool flightToolbar=game->s.mode==Mode::Flight&&r.y==14&&r.height==32;
 if(flightToolbar)fill=primary?alpha(mint,.10f):hover?Color{30,51,56,190}:Color{0,0,0,0};
 DrawRectangleRounded(r,.08f,3,fill);if(primary||hover)DrawRectangleRoundedLinesEx(r,.08f,3,1,alpha(primary?mint:lineColor,enabled?.55f:.2f));
 float size=12;auto title=elide(label,r.width-16,size,true);float tw=MeasureTextEx(mono,title.c_str(),size,.1f).x;
 text(title,r.x+(r.width-tw)*.5f,r.y+(r.height-size)*.5f,size,enabled?(primary?(flightToolbar?mint:ink):pearl):alpha(muted,.44f),true);
 if(visible&&pressed&&!modalBlock){ui->clickConsumed=true;ui->pointerCaptured=true;}
 if(visible&&hover&&!modalBlock&&ui->handoff.ready()){auto hint=game->orderHint(action,arg);if(!hint.empty())ui->orderHint=hint;}
 if(enabled&&visible&&pressed&&!modalBlock){ui->action=action;ui->arg=arg;ui->clickConsumed=true;ui->pointerCaptured=true;return true;}return false;
}
void Renderer::orbDraw(Vector2 center,float radius,uint32_t seed,float water,bool stellar){vector(orb,"resolution",v(float(width),float(height)));vector(orb,"viewSize",v(w,h));vector(orb,"center",center);uniform(orb,"radius",radius);uniform(orb,"seed",float(seed%10000));uniform(orb,"time",clock);uniform(orb,"water",water);integer(orb,"stellar",stellar?1:0);
 BeginShaderMode(orb);DrawRectangleRec({center.x-radius*1.23f,center.y-radius*1.23f,radius*2.46f,radius*2.46f},WHITE);EndShaderMode();}
void Renderer::planet(const Planet& p,Vector2 at,float r,bool selected){if(selected){glow(at,r*1.7f,mint,.13f);ring(at,r+10,alpha(mint,.65f));line({at.x+r+12,at.y},{at.x+r+34,at.y},alpha(mint,.7f));}orbDraw(at,r,p.seed,float(p.water),false);}
void Renderer::galaxy(){State& s=game->s;auto screen=[&](V2 p){return v(w*.5f+float((p.x-s.galaxy.x)*s.galaxyZoom),h*.5f+float((p.y-s.galaxy.y)*s.galaxyZoom));};
 Star here=game->currentStar(),selected=game->selectedStar();Vector2 origin=screen(here.pos);float range=float(game->range()*s.galaxyZoom);
 ring(origin,range,alpha(mint,.20f));ring(origin,range+2,alpha(mint,.06f));
 // Distant star layers use fixed coordinates, so panning keeps a stable atlas.
 Random dust{game->universe.seed()};for(int i=0;i<280;i++){float x=float(dust()),y=float(dust()),z=float(dust());float px=std::fmod(x*w-float(s.galaxy.x)*.07f+w*100,w),py=std::fmod(y*h-float(s.galaxy.y)*.07f+h*100,h);DrawCircleV({px,py},z>.96f?1.5f:.7f,alpha(hue(float(dust()),.25f),.16f+z*.35f));}
 double halfX=w/2/s.galaxyZoom+30,halfY=h/2/s.galaxyZoom+30;std::vector<Star> visible;
 for(int cx=int(std::floor((s.galaxy.x-halfX)/182));cx<=int(std::ceil((s.galaxy.x+halfX)/182));cx++)for(int cy=int(std::floor((s.galaxy.y-halfY)/182));cy<=int(std::ceil((s.galaxy.y+halfY)/182));cy++)for(auto& star:game->universe.cell(cx,cy))visible.push_back(star);
 for(auto& star:visible){Vector2 p=screen(star.pos);bool chosen=star.id==selected.id,home=star.id==here.id;float r=float(2.1+star.size*1.4);Color color=star.type=="M"?Color{255,135,144,255}:star.type=="K"?Color{255,179,127,255}:star.type=="G"?Color{255,224,161,255}:Color{166,209,255,255};
 if(distance(star.pos,here.pos)<=game->range()&&!home&&s.galaxyZoom>.42)line(origin,p,alpha(mint,.09f));
 glow(p,r*(chosen?12:6),color,chosen?.43f:.23f);DrawCircleV(p,r,color);DrawCircleV(p,r*.32f,pearl);
 if(chosen){ring(p,r+12,mint,1.3f);ring(p,r+17,alpha(mint,.19f));line({p.x-r-22,p.y},{p.x-r-14,p.y},mint);line({p.x+r+14,p.y},{p.x+r+22,p.y},mint);}
 if(home)ring(p,r+7,pearl,1.2f);
 bool mouse=CheckCollisionPointCircle(uiMouse(),p,15);if(chosen||home||mouse||(s.galaxyZoom>.9&&r>4.1)){text(star.name,p.x+12,p.y-20,chosen?17:12,chosen?pearl:alpha(pearl,.72f),false);if(chosen)text(home?"NOMA / VESSEL PRESENT":num(distance(star.pos,here.pos))+" LIGHT-YEARS",p.x+12,p.y+3,10,mint,true);}
 hotspots.push_back({p,std::max(15.f,r+5),"star",star.id,0,{}});
 }
 if(selected.id!=here.id){Vector2 dst=screen(selected.pos);line(origin,dst,alpha(mint,.6f),1.2f);Vector2 mid{(origin.x+dst.x)*.5f,(origin.y+dst.y)*.5f};glow(mid,25,mint,.15f);DrawCircleV(mid,2,mint);}
 text("APERTURE / "+num(game->range())+" LY",origin.x+8,origin.y-range-18,10,alpha(mint,.7f),true);
}
void Renderer::system(){State& s=game->s;Star star=game->universe.star(s.viewStar);auto planets=game->universe.planets(s.viewStar);float edge=UILayout::worldRight(w,ui->panel),glyph=clamp(edge/1000.f,.6f,1.f);bool compact=edge<900;Vector2 center{edge*.46f,h*.47f};float reach=std::min(edge*.36f,h*.42f),first=reach*.26f;
 for(int i=0;i<180;i++){double x=game->universe.rnd(i,1),y=game->universe.rnd(i,2);DrawCircleV(v(float(x*w),float(y*h)),.65f,alpha(pearl,.19f));}
 for(size_t i=0;i<planets.size();i++){float r=first+(reach-first)*float(i+1)/float(planets.size());float yr=r*.64f;
 Vector2 prev{center.x+r,center.y};for(int j=1;j<=160;j++){float a=float(tau*j/160);Vector2 next{center.x+std::cos(a)*r,center.y+std::sin(a)*yr};line(prev,next,alpha(i==size_t(s.selectedPlanet)?mint:pearl,i==size_t(s.selectedPlanet)?.28f:.10f));prev=next;}}
 glow(center,115,Color{255,163,87,255},.25f);orbDraw(center,47*glyph,star.seed,.0f,true);text(star.name,center.x-28,center.y+47*glyph+12,11,pearl,true);
 for(size_t i=0;i<planets.size();i++){const auto& p=planets[i];float r=first+(reach-first)*float(i+1)/float(planets.size());float a=float(p.phase+s.hours*.015/std::pow(p.orbit,.6));Vector2 pos{center.x+std::cos(a)*r,center.y+std::sin(a)*r*.64f};float radius=float(10+p.size*5.5)*glyph;bool selected=int(i)==s.selectedPlanet&&!game->minorSelection();if(selected)radius*=1.25f;
 planet(p,pos,radius,selected);if(selected||!compact||CheckCollisionPointCircle(uiMouse(),pos,radius+10))fitted(p.name,pos.x+radius+15,pos.y-15,edge-pos.x-radius-29,selected?18:12,selected?pearl:alpha(pearl,.7f));if(selected)fitted(p.dominant+" / "+num(p.water*100)+"% WATER",pos.x+radius+15,pos.y+8,edge-pos.x-radius-29,10,mint,true);
 for(const auto& moon:game->universe.minorBodies(s.viewStar))if(moon.parent==int(i)){auto orbit=minorOrbit(moon,s.hours);float angle=float(std::atan2(orbit.z,orbit.x));Vector2 at=polar(pos,angle,radius+10+float(moon.chartSlot)*5);bool focus=game->selectedMinor==moon.id;DrawCircleV(at,focus?4.f:2.f,moon.ice>.5?Color{153,218,255,220}:Color{209,176,245,200});if(focus)ring(at,7,alpha(mint,.8f));hotspots.push_back({at,7,"minor",moon.id,0,{}});}
 hotspots.push_back({pos,std::max(24.f,radius+10),"planet",p.id,int(i),{}});
 }
 for(const auto& b:game->universe.minorBodies(s.viewStar))if(b.parent<0&&objectIntegrity(s.economy.worldDamage,"body:"+b.id)>0){auto orbit=minorOrbit(b,s.hours);Vector2 at;bool comet=b.kind==MinorKind::Comet;
  if(comet){at={center.x+float(orbit.x/b.orbit)*reach*.66f,center.y+float(orbit.z/b.orbit)*reach*.43f};auto tail=polar(at,float(std::atan2(at.y-center.y,at.x-center.x)),18*glyph);line(at,tail,alpha(mint,.45f),2*glyph);}
  else{float r=first+(reach-first)*float(b.chartSlot+1)/float(planets.size()),a=float(std::atan2(orbit.z,orbit.x));at={center.x+std::cos(a)*r,center.y+std::sin(a)*r*.64f};}
  bool focus=game->selectedMinor==b.id;Color colour=comet?mint:Color{186,166,227,180};DrawCircleV(at,focus?4.f:comet?2.5f:1.5f,colour);if(focus)ring(at,8,alpha(mint,.8f));hotspots.push_back({at,comet?8.f:5.f,"minor",b.id,0,{}});
  if(focus||CheckCollisionPointCircle(uiMouse(),at,8))fitted(b.name+" / "+(objectIntegrity(s.economy.worldDamage,"body:"+b.id)<=0?"DEBRIS":minorKindName(b.kind)),at.x+10,at.y-18,std::max(0.f,edge-at.x-20),12,pearl);
 }
}
void Renderer::craft(Vector2 p,float scale,float angle,bool submarine){
 glow(p,45*scale,mint,.20f);Vector2 wake=polar(p,angle+float(pi),22*scale);line(wake,polar(wake,angle+float(pi),25*scale),alpha(mint,.15f),7*scale);line(wake,p,alpha(mint,.8f),2*scale);
 auto local=[&](float x,float y){return v(p.x+(x*std::cos(angle)-y*std::sin(angle))*scale,p.y+(x*std::sin(angle)+y*std::cos(angle))*scale);};
 std::vector<Vector2> points;
 if(submarine){points={local(23,0),local(11,8),local(-9,10),local(-19,5),local(-23,12),local(-21,0),local(-23,-12),local(-19,-5),local(-9,-10),local(11,-8)};}
 else{points={local(24,0),local(5,9),local(-16,19),local(-10,3),local(-18,0),local(-10,-3),local(-16,-19),local(5,-9)};}
 polygon(points,Color{164,200,197,255},pearl);
 for(int i=0;i<4;i++){auto a=local(float(12-i*5),float(-4+i%2)),b=local(float(12-i*5),float(4-i%2));line(a,b,hue(.48f+i*.12f,.4f),1.6f*scale);}
 DrawCircleV(local(8,0),3*scale,pearl);DrawCircleV(local(8,0),1.5f*scale,mint);
 ring(p,30*scale,alpha(mint,.2f));float fraction=float(game->s.battery/100);DrawRing(p,29*scale,30.4f*scale,-90,-90+fraction*360,80,alpha(mint,.8f));
}
void Renderer::contact(const Contact& o,Vector2 p,float scale,bool ocean){Color color=contactColor(o.kind);bool done=game->s.scanned.count(o.id);float phase=float(o.n%628)/100,age=clock*.5f+phase;
 float presence=1;const Ecosystem* habitat=nullptr;if(o.kind=="life"||o.kind=="flora"){habitat=contactHabitat(game->s.economy,game->universe,game->activePlanet(),o);if(habitat){presence=float(populationPresence(*habitat,o));scale*=.65f+.35f*std::sqrt(presence);color=alpha(color,.2f+.8f*presence);}}
 if(done)color=alpha(color,.5f);glow(p,38*scale,color,.22f);
 if(o.kind=="life"||o.kind=="flora"){
  auto bio=phenotype(o);scale*=float(bio.size);float body=(ocean?13.f:8.f)*scale;age*=float(bio.gait);
  Color pigment=alpha(hue(float(bio.hue),.64f,.66f),.18f+.82f*presence),rim=alpha(pearl,(done?.5f:1.f)*(.18f+.82f*presence));
  auto segment=[&](Vector2 q,float a,float radius,int lobes){std::vector<Vector2> shape;for(int k=0;k<lobes;k++)shape.push_back(polar(q,a+float(tau*k/lobes),radius));polygon(shape,pigment,rim);};
  if(bio.plan==0){
   for(int k=0;k<bio.limbs;k++){float a=float(tau*k/bio.limbs)+phase*.2f+std::sin(age)*.08f;Vector2 tip=polar(p,a,body*(1.65f+.14f*std::sin(age+k)));polygon({polar(p,a-.5f,body*.45f),polar(tip,a-1.5f,body*.24f),tip,polar(tip,a+1.5f,body*.24f),polar(p,a+.5f,body*.45f),p},hue(float(bio.hue)+k*.04f,.62f,.62f),rim);}
  }else if(bio.plan==1){
   Vector2 last=p;for(int k=0;k<bio.segments;k++){Vector2 q{p.x+(k-bio.segments/2.f)*body*.6f,p.y+std::sin(age-k*.6f)*body*.45f};line(last,q,pigment,body*.35f);segment(q,float(age*.1f),body*(.55f-k*.035f),5);last=q;}
  }else if(bio.plan==2){
   for(int side:{-1,1})for(int k=0;k<bio.limbs;k++){Vector2 root{p.x,p.y+body-k*body*.35f},tip{p.x+side*body*(1.6f-k*.12f),root.y-body*(.6f+.15f*std::sin(age+k))};polygon({root,{tip.x,tip.y-body*.22f},tip,{root.x+side*body*.4f,root.y+body*.15f}},pigment,rim);}
   line({p.x,p.y+body*1.2f},{p.x,p.y-body},color,2*scale);
  }else if(bio.plan==3){
   for(int k=0;k<bio.segments;k++){Vector2 q{p.x+(k-bio.segments/2.f)*body*.38f,p.y};segment(q,float(pi/6),body*(.75f-std::abs(k-bio.segments/2.f)*.07f),6);for(int side:{-1,1}){Vector2 knee{q.x+std::sin(age+k)*body*.3f,q.y+side*body};line(q,knee,color,1.7f*scale);line(knee,{knee.x-body*.3f,knee.y+side*body*.25f},rim,scale);}}
  }else if(bio.plan==4){
   for(int k=0;k<bio.limbs+2;k++){float a=k*2.39996f+std::sin(age+k)*.15f;Vector2 q=polar(p,a,body*(.5f+std::sqrt(float(k))*.44f));glow(q,body*.6f,pigment,.12f);segment(q,a,body*.42f,5);DrawCircleV(q,scale,color);}
  }else{
   for(int k=0;k<bio.limbs;k++){float a=float(tau*k/bio.limbs)-float(pi/2);Vector2 q=polar(p,a,body*1.8f);line(p,q,pigment,2*scale);for(int j=1;j<4;j++){Vector2 root{p.x+(q.x-p.x)*j/4,p.y+(q.y-p.y)*j/4};for(int side:{-1,1}){Vector2 tip=polar(root,a+side*.7f,body*.6f);line(root,tip,rim,scale);DrawCircleV(tip,1.8f*scale,color);}}}
  }
  DrawCircleV(p,body*.22f,color);DrawCircleV({p.x-2*scale,p.y-2*scale},body*.10f,rim);
  if(ocean&&bio.plan==0){for(int k=0;k<bio.limbs;k++){Vector2 last{p.x+(k-bio.limbs/2.f)*4*scale,p.y+body*.7f};for(int j=1;j<12;j++){Vector2 next{last.x+std::sin(age*1.4f-j*.26f+k)*scale*1.3f,p.y+body*.7f+j*4*scale};line(last,next,alpha(color,(1-j/13.f)*.7f),std::max(.6f,1.4f*scale));last=next;}}}
 }else if(o.kind=="wreck"){
  float a=scale*1.7f;auto pt=[&](float x,float y){return Vector2{p.x+x*a,p.y+y*a};};Color plate{29,41,51,255},rim=hue(float(o.n%100)/100,.52f,.85f);
  polygon({pt(-30,8),pt(-26,-7),pt(-9,-12),pt(4,-8),pt(12,-1),pt(25,-4),pt(34,7),pt(20,13),pt(-7,11)},plate,rim);
  polygon({pt(-4,-8),pt(2,-19),pt(12,-18),pt(16,-3),pt(9,2)},Color{46,45,61,255},alpha(mint,.7f));
  for(int k=0;k<7;k++){float x=-24+k*8;line(pt(x,-4),pt(x-3,7),alpha(rim,.48f),a*.7f);DrawCircleV(pt(x,1),1.2f*a,k%2?mint:Color{210,136,255,255});}
  for(int k=0;k<5;k++){float x=-19+k*10;line(pt(x,9),pt(x-5,15+k%3),Color{78,106,100,220},a);DrawCircleV(pt(x-5,15+k%3),a*1.5f,hue(.44f+k*.09f,.65f,.75f));}
  line(pt(5,-5),pt(13,3),Color{3,9,17,255},2*a);line(pt(13,3),pt(4,10),Color{3,9,17,255},2*a);glow(pt(23,-6),8*a,rim,.25f);
 }else if(o.kind=="mineral"){
  for(int k=0;k<3;k++){Vector2 q=v(p.x+(k-1)*10*scale,p.y+5*scale);float size=(10+(o.n>>(k*3))%13)*scale;polygon({{q.x,q.y-size},{q.x+6*scale,q.y-2*scale},{q.x,q.y+9*scale},{q.x-5*scale,q.y}},hue(.68f+k*.12f,.6f,.68f),pearl);line({q.x,q.y-size},{q.x,q.y+8*scale},color,1.4f);}
 }else if(o.kind=="signal"){
  for(int i=0;i<3;i++)ring(p,(9+i*8+std::sin(age)*2)*scale,alpha(color,.85f-i*.21f));DrawCircleV(p,3*scale,pearl);
  for(int i=0;i<4;i++){float a=float(i*pi/2)+age*.18f;line(polar(p,a,30*scale),polar(p,a,36*scale),color,1.3f);}
 }else if(o.kind=="relic"){
  for(int j=0;j<3;j++){float a=float(pi/3)+j*.45f;Vector2 q=v(p.x+j*8*scale,p.y-j*7*scale);std::vector<Vector2> shape;for(int k=0;k<4;k++)shape.push_back(polar(q,a+float(k*pi/2),(17-j*3)*scale));polygon(shape,Color{39,33,66,255},color);}
 }else{
  polygon({{p.x-14*scale,p.y+10*scale},{p.x-7*scale,p.y-14*scale},{p.x+4*scale,p.y-24*scale},{p.x+13*scale,p.y+10*scale}},Color{73,48,66,255},color);
  for(int j=0;j<14;j++){float a=clock*.3f+j;Vector2 q=v(p.x+std::sin(a)*j*.9f*scale,p.y-(28+j*6)*scale);glow(q,(6+j*.3f)*scale,color,(1-j/15.f)*.13f);}
 }
 if(o.feature==1||o.feature==2){for(int k=0;k<3;k++){float x=p.x+(k-1)*13*scale;line({x-7*scale,p.y+10*scale},{x-5*scale,p.y-18*scale},color,3*scale);line({x-5*scale,p.y-18*scale},{x+6*scale,p.y-13*scale},pearl,2*scale);line({x+6*scale,p.y-13*scale},{x+9*scale,p.y+10*scale},color,3*scale);}}
 if(o.feature==8){polygon({{p.x-25*scale,p.y+10*scale},{p.x-16*scale,p.y-8*scale},{p.x+19*scale,p.y-3*scale},{p.x+28*scale,p.y+14*scale}},Color{47,40,65,255},color);for(int k=0;k<3;k++)line({p.x+(k-1)*11*scale,p.y-3*scale},{p.x+(k-1)*11*scale,p.y+10*scale},pearl,scale);}
 if(done){text("+",p.x+24*scale,p.y-25*scale,13,mint,true);}else{ring(p,(ocean?44.f:26.f)*scale,alpha(color,.16f));}
 bool hover=CheckCollisionPointCircle(uiMouse(),p,34*scale);
 if(hover){text(o.name,p.x+24,p.y-27,14,pearl);text(o.kind+(habitat?" / "+num(presence*100)+"% ABUNDANCE":std::string{})+(done?" / ARCHIVED":" / SET COURSE"),p.x+24,p.y-8,9,color,true);}
 hotspots.push_back({p,std::max(18.f,32*scale),"contact",o.id,0,o});
}
void Renderer::surface(){State& s=game->s;Planet p=game->activePlanet();float z=float(ui->surfaceZoom);auto project=[&](V2 q){return v(w*.5f+float(q.x-s.surface.x)*z,h*.5f+float(q.y-s.surface.y)*z);};
 double rangeX=w*.5/z+60,rangeY=h*.5/z+60;
 for(int cx=int(std::floor((s.surface.x-rangeX)/86));cx<=int(std::floor((s.surface.x+rangeX)/86));cx++)for(int cy=int(std::floor((s.surface.y-rangeY)/86));cy<=int(std::floor((s.surface.y+rangeY)/86));cy++)for(auto& o:game->universe.surfaceObjects(p,cx,cy)){if(o.kind=="life"||o.kind=="flora"||objectIntegrity(s.economy.worldDamage,"field:"+o.id)<=0)continue;Vector2 pos=project(o.pos);contact(o,pos,.73f,false);}
 drawWildlife(false);
 if(distance(s.surface,s.waypoint)>4){Vector2 q=project(s.waypoint);line(v(w*.5f,h*.5f),q,alpha(mint,.32f));ring(q,12,mint);line(v(q.x-18,q.y),v(q.x+18,q.y),alpha(mint,.5f));}
 auto move=game->planMove();if(move.valid){auto next=project(move.position);line(v(w*.5f,h*.5f),next,mint,2);ring(next,5,mint);fitted("ENTER / 1 step / "+std::to_string(move.turns)+" turns / Shift+Enter follows route",28,h-144,UILayout::worldRight(w,ui->panel)-56,10,mint,true);}
 if(game->radar>=0){float radius=float((34+s.upgrades[1]*14)*z*game->radar);ring(v(w*.5f,h*.5f),radius,alpha(mint,float(1-game->radar/1.3)),2);}
 craft(v(w*.5f,h*.5f),.8f,float(game->heading),false);
 const auto& report=game->diveReport;if(report.valid&&report.star==s.activeStar&&report.planet==s.activePlanet&&game->lastTurn.valid&&std::abs(game->lastTurn.endHour-s.hours)<1e-7&&report.ended>game->lastTurn.startHour+1e-7&&report.ended<=s.hours+1e-7&&!ui->clean){
  float rw=std::min(570.f,UILayout::worldRight(w,ui->panel)-76);DrawRectangleRec({58,112,rw,76},Color{4,15,25,239});
  fitted(report.complete?"SURVEY RECOVERED":report.emergency?"EMERGENCY RECOVERY":"TRANSECT ENDED",72,123,rw-28,11,report.complete?mint:Color{255,174,138,255},true);
  fitted("Studies "+std::to_string(report.studies)+(game->rescuedCrew()?" / crew aboard "+std::to_string(game->rescuedCrew()):" / samples "+std::to_string(report.collections))+" / "+num(report.hours,1)+" h / R debrief",72,145,rw-28,11,pearl);
  std::string cargo;int shown=0;for(int g=0;g<goodCount&&shown<3;g++)if(std::abs(report.delta[g])>=.05){if(!cargo.empty())cargo+=" / ";cargo+=(report.delta[g]>0?"+":"")+num(report.delta[g])+" "+goods[g].name;++shown;}
  fitted(cargo.empty()?"Cargo hold unchanged":cargo,72,166,rw-28,10,mint,true);
 }

 // Sparse coordinate ticks stay attached to the same landscape as the contacts.
 for(int x=int(std::floor((s.surface.x-rangeX)/100))*100;x<s.surface.x+rangeX;x+=100){float px=project({double(x),s.surface.y}).x;line(v(px,h-170),v(px,h-164),alpha(mint,.4f));text(num(x),px+4,h-165,9,alpha(mint,.6f),true);}
}
void Renderer::ocean(){
 State& s=game->s;Planet p=game->activePlanet();auto brief=game->diveBrief();auto order=game->nextDiveOrder();float z=float(ui->oceanZoom),cx=oceanX(),cy=oceanY(),edge=UILayout::worldRight(w,ui->panel),bottom=h-84;
 auto project=[&](double x,double depth){return Vector2{cx+float(x-s.oceanX)*3.4f*z,cy+float(depth-s.depth)*.35f*z};};
 BeginScissorMode(0,int(UILayout::header*height/h),int(edge*width/w),int((bottom-UILayout::header)*height/h));
 // Keep the spectral currents as a quiet backdrop to a readable sonar chart.
 DrawRectangleRec({0,UILayout::header,edge,bottom-UILayout::header},Color{3,17,30,201});
 for(int k=0;k<4;k++){Vector2 last{};for(float x=0;x<=edge+8;x+=8){float y=cy+(k-1.5f)*95*z+std::sin(x*.008f+clock*.06f+k*1.8f)*16;Vector2 next{x,y};if(x>0)line(last,next,alpha(hue(.47f+k*.15f,.65f),.17f),1.3f);last=next;}}
 for(int d=int(std::floor((s.depth-(cy-70)/(.35*z))/100))*100;d<s.depth+(bottom-cy)/(.35*z);d+=100){if(d<0)continue;float y=project(s.oceanX,d).y;line({10,y},{28,y},Color{77,129,143,95});if(d%200==0){line({50,y},{edge-16,y},Color{46,94,113,23});text(num(d),6,y-14,9,muted,true);}}
 float surface=project(s.oceanX,0).y;if(surface>60&&surface<bottom){DrawRectangleRec({0,60,edge,surface-60},Color{5,15,25,195});line({50,surface},{edge,surface},alpha(mint,.6f));fitted("SURFACE",56,surface-18,100,9,mint,true);}
 double left=s.oceanX-cx/(3.4*z)-4,rightX=s.oceanX+(edge-cx)/(3.4*z)+4;Vector2 previous=project(left,game->universe.floorDepth(p,left,s.oceanAtY));
 for(double x=left+4;x<=rightX+4;x+=4){auto next=project(x,game->universe.floorDepth(p,x,s.oceanAtY));DrawTriangle(previous,{previous.x,bottom+4},{next.x,bottom+4},Color{8,19,30,255});DrawTriangle(previous,{next.x,bottom+4},next,Color{8,19,30,255});line(previous,next,Color{120,192,190,185},1.5f);previous=next;}
 float limit=project(s.oceanX,game->maxDepth()).y;if(limit>70&&limit<bottom){for(float x=50;x<edge-16;x+=18)line({x,limit},{x+8,limit},Color{244,157,123,130});fitted("CERTIFIED LIMIT / "+num(game->maxDepth())+" m",56,limit-18,edge-72,9,Color{255,174,138,255},true);}
 // Flow arrows describe the same local currents used by movement quotes.
 for(int k=0;k<5;k++){double x=s.oceanX+(k-2)*36;auto water=game->oceanWater({x,s.oceanAtY},s.depth);auto at=project(x,s.depth+160);float dx=float(water.current*22);if(std::abs(dx)<3||at.x<55||at.x>edge-30||at.y>bottom-25)continue;Color c=alpha(hue(water.shelter>.4?.47f:.62f,.5f),.35f);line({at.x-dx*.5f,at.y},{at.x+dx*.5f,at.y},c,1);float sign=dx>0?1.f:-1.f;line({at.x+dx*.5f,at.y},{at.x+dx*.5f-sign*4,at.y-3},c,1);line({at.x+dx*.5f,at.y},{at.x+dx*.5f-sign*4,at.y+3},c,1);}
 drawWildlife(true);
 const float rx=float(brief.range*.68*z),ry=float(brief.range*.35*z);DrawEllipseLines(int(cx),int(cy),rx,ry,Color{109,199,200,38});
 if(game->radar>=0)DrawEllipseLines(int(cx),int(cy),rx*float(game->radar*game->diveState.pingRange/brief.range),ry*float(game->radar*game->diveState.pingRange/brief.range),alpha(mint,float(1-game->radar/1.3)*.65f));
 auto contacts=game->oceanContacts();std::stable_sort(contacts.begin(),contacts.end(),[&](const auto& a,const auto& b){return std::hypot((a.pos.x-s.oceanX)*5,a.depth-s.depth)<std::hypot((b.pos.x-s.oceanX)*5,b.depth-s.depth);});int drawn=0;
 for(const auto& c:contacts){auto q=project(c.pos.x,c.depth);if(q.x<55||q.x>edge-20||q.y<70||q.y>bottom-10)continue;if(drawn++>=8&&c.id!=brief.goal.id&&c.id!=game->focusedContact)continue;
  if(c.kind=="life"||c.kind=="flora"){DrawCircleLines(int(q.x),int(q.y),11,mint);DrawCircleV(q,3,mint);if(c.id==brief.goal.id||c.id==game->focusedContact)text("BIO",q.x+15,q.y-4,8,mint,true);hotspots.push_back({q,16,"contact",c.id,0,c});}
  else contact(c,q,c.id==brief.goal.id?.58f:.43f,true);
  if(c.kind=="wreck"&&c.id==brief.goal.id)if(auto* wreck=findOceanWreck(s.economy,c.id)){std::string label=wreck->crew?std::to_string(wreck->crew)+" crew / "+num(std::max(0.,wreck->supportUntil-s.hours))+"h life support":"Cache / "+num(stockTotal(wreck->remaining))+" supplies";fitted(label,clamp(q.x+40,56.f,edge-210),clamp(q.y+21,94.f,bottom-18),190,10,mint,true);}
 }
 Vector2 home=project(game->diveState.origin.x,0);if(home.x>55&&home.x<edge-20&&home.y>70&&home.y<bottom){line({home.x,home.y},{home.x,home.y+22},mint,1.6f);DrawCircleV({home.x,home.y+5},4,mint);fitted("LAUNCH BUOY",home.x+9,home.y+2,130,9,mint,true);}
 if((brief.phase==3||game->diveState.returning)&&(home.x<55||home.x>edge-20||home.y<70||home.y>bottom)){
  Vector2 mark{clamp(home.x,66.f,edge-24),clamp(home.y,94.f,bottom-22)};ring(mark,9,mint);line({mark.x,mark.y-16},{mark.x,mark.y+16},alpha(mint,.7f));fitted("BUOY / "+std::to_string(brief.homeTurns)+" MOVES",clamp(mark.x-60,56.f,edge-150),mark.y+19,138,9,mint,true);
 }
 auto plan=game->planMove();if(plan.valid){auto q=project(s.oceanTargetX,s.targetDepth);line({cx,cy},q,Color{153,255,218,80});auto next=project(plan.position.x,plan.depth);line({cx,cy},next,mint,2);ring(next,5,mint);if(q.x>50&&q.x<edge-20&&q.y>70&&q.y<bottom)ring(q,9,mint);if(order.action=="oceanAdvance"&&(plan.power>0||plan.wear>.01))fitted(num(plan.power)+" power / "+num(plan.wear,1)+" hull",clamp(next.x+10,56.f,edge-160),clamp(next.y-29,94.f,bottom-18),145,9,Color{255,198,139,255},true);}
 if(brief.hasGoal&&!game->diveState.returning){auto q=project(brief.goal.pos.x,brief.goal.depth);Vector2 mark{clamp(q.x,66.f,edge-24),clamp(q.y,94.f,bottom-20)};ring(mark,15,brief.studied?mint:Color{239,204,140,255},1.5f);if(!brief.known)text("?",mark.x-3,mark.y-5,10,pearl,true);}
 craft({cx,cy},.7f,0,true);

 EndScissorMode();
}
void Renderer::heading(const std::string& title,float x,float y){fitted(title,x,y,w-x-24,10,mint,true);line(v(x,y+18),v(w-24,y+18),alpha(lineColor,.3f));}
void Renderer::row(const std::string& name,const std::string& value,float x,float& y,float width){fitted(name,x,y,width*.55f,13,muted);right(elide(value,width*.44f,13),x+width,y,13,pearl);line(v(x,y+22),v(x+width,y+22),alpha(lineColor,.18f));y+=29;}
void Renderer::notification(){
 ui->noticeBottom=70;if(game->toastTime<=0)return;
 float maxWidth=std::min(game->s.mode==Mode::Flight?520.f:680.f,UILayout::worldRight(w,ui->panel)-48);
 std::istringstream words(game->toast);std::string word,line;int lines=1;
 while(words>>word){auto candidate=line.empty()?word:line+" "+word;if(MeasureTextEx(mono,candidate.c_str(),11,.1f).x>maxWidth&&!line.empty()){++lines;line=word;}else line=candidate;}
 float ty=74;if(ui->pointerPressed&&CheckCollisionPointRec(ui->pressPosition,{18,ty-8,maxWidth+20,lines*15.f+16})){ui->clickConsumed=true;ui->pointerCaptured=true;}ui->noticeBottom=ty+lines*15+8;DrawRectangleRounded({18,ty-8,maxWidth+20,lines*15.f+16},.06f,3,alpha(ink,.94f));
 wrapped(game->toast,28,ty,maxWidth,11,game->toastPriority==Notice::Critical?Color{255,174,138,255}:mint,true);
}
void Renderer::menus(){
 if(!ui->menu&&!ui->sections)return;modalDrawing=true;
 Rectangle box{};
 if(ui->sections&&ui->panel){
  box={UILayout::panelLeft(w)+12,102,UILayout::panelWidth-24,158};DrawRectangleRec(box,panelInk);DrawRectangleLinesEx(box,1,alpha(lineColor,.7f));
  const char* labels[]={"Inspect","Atlas","Research","Log","Outposts","Market","Factions","History"};
  for(int i=0;i<8;i++)button(labels[i],{box.x+8+(i%2)*178,box.y+8+(i/2)*36,170,30},"tab",std::to_string(i),true,ui->tab==i);
 }else if(ui->menu){
  struct Entry{std::string title,action,arg;};std::vector<Entry> entries={{game->s.mode==Mode::Flight&&game->s.flight.docked?"Market / E":"Market","tab","5"},{"Atlas","tab","1"},{"Research","tab","2"},{"Factions","tab","6"},{"History","tab","7"},{"Mission log","tab","3"}};
  if(game->hasExpedition()&&(game->s.mode==Mode::System||game->s.mode==Mode::Galaxy)){entries.push_back({"Resume survey / L","expedition",""});entries.push_back({"Ascend / U","launch",""});}
  if(game->turnBased())entries.push_back({game->s.mode==Mode::Ocean?"Dive sites / R":"Turn planner / R","orders",""});
  if(game->s.mode==Mode::Flight)entries.push_back({ui->enhancedHUD?"Flight guidance off / F8":"Flight guidance on / F8","flightHUD",""});
  entries.push_back({"Controls / H","help",""});entries.push_back({"Settings","settings",""});entries.push_back({ui->paused?"Resume / P":"Pause / P","pause",""});entries.push_back({"Hide interface / F9","clean",""});
  box={w-270,54,252,float(entries.size()*34+16)};DrawRectangleRec(box,panelInk);DrawRectangleLinesEx(box,1,alpha(lineColor,.7f));
  for(size_t i=0;i<entries.size();i++)button(entries[i].title,{box.x+8,box.y+8+float(i)*34,236,30},entries[i].action,entries[i].arg);
 }
 if(ui->pointerPressed&&!ui->clickConsumed&&!CheckCollisionPointRec(ui->pressPosition,box)){ui->action="dismissMenu";ui->clickConsumed=true;ui->pointerCaptured=true;}
 modalDrawing=false;
}
void Renderer::overlay(){if(game->s.mode==Mode::Ocean){oceanOverlay();return;}State& s=game->s;float rightEdge=UILayout::worldRight(w,ui->panel),viewWidth=rightEdge-48;
 DrawRectangleRec({0,0,w,UILayout::header},alpha(ink,.92f));line({0,UILayout::header},{w,UILayout::header},alpha(lineColor,.3f));
 if(s.mode==Mode::Flight){
  const Vector2 mark{30,29};
  line({mark.x,mark.y-8},{mark.x+6,mark.y+4},mint,1.5f);line({mark.x+6,mark.y+4},{mark.x-6,mark.y+4},Color{156,193,255,255},1.5f);line({mark.x-6,mark.y+4},{mark.x,mark.y-8},Color{202,166,255,255},1.5f);
  text("HYPERION",46,16,20,pearl);
  line({24,UILayout::header},{138,UILayout::header},alpha(mint,.45f));
 }else text("HYPERION",24,16,20,pearl);
 fitted(ui->paused?"PAUSED / P":"D"+num(std::floor(s.hours/24)+1)+" "+num(std::floor(std::fmod(s.hours,24)))+":00"+(game->turnBased()?" / T"+std::to_string(s.turns)+" / ORDERS":""),190,23,w-648,10,ui->paused?pearl:muted,true);
 float bx=w-438;
 button(s.mode==Mode::Flight?"Strategy / O":game->hasExpedition()&&game->expeditionLayer()==Mode::Ocean?(game->diveReturnNeeded()?"Return / K":"Recover / K"):"Cockpit / K",{bx,14,108,32},s.mode==Mode::Flight?"strategy":game->diveReturnNeeded()?(s.mode==Mode::Ocean?"oceanReturn":"expedition"):"cockpit");
 button(s.mode==Mode::Flight?"Ports / I":"Inspect / I",{bx+114,14,108,32},"panel","0",true,ui->panel&&ui->tab==0);
 button("Outposts / B",{bx+228,14,120,32},"panel","4",true,ui->panel&&ui->tab==4);
 button("Menu",{bx+354,14,60,32},"menu","",true,ui->menu);
 notification();
 if(s.mode==Mode::Flight)flightHUD();
 else{
  DrawRectangleGradientV(0,int(h-UILayout::footer),int(rightEdge),int(UILayout::footer),alpha(ink,.72f),alpha(ink,.96f));
  std::string name,meta;struct Action{std::string label,action,arg;bool enabled=true,primary=false;};std::vector<Action> actions;
  if(s.mode==Mode::Galaxy){auto star=game->selectedStar();name=star.name;meta=star.type+"-class / "+std::to_string(star.numPlanets)+" planets / "+(star.id==s.currentStar?"Vessel present":num(distance(star.pos,game->currentStar().pos))+" ly");
   if(star.id!=s.currentStar)actions.push_back({"Jump / J","jump",star.id,distance(star.pos,game->currentStar().pos)<=game->range()&&game->fuelCost(distance(star.pos,game->currentStar().pos))<=s.fuel});
   actions.push_back({"System / Enter","system",star.id});actions.push_back({"Refuel / F","refuel","",s.fuel<s.maxFuel&&game->inOrbit()});
  }else if(s.mode==Mode::System){if(auto* minor=game->minorSelection()){name=minor->name;meta=std::string(minorKindName(minor->kind))+" / "+minor->composition;actions.push_back({"Plot fly-by / Enter","plotMinor","",s.viewStar==s.currentStar});actions.push_back({"Cockpit / K","cockpit","",s.viewStar==s.currentStar});}else{auto p=game->planet();name=p.name;meta=p.dominant+" / "+num(p.temp)+" K / "+num(p.water*100)+"% water / "+p.bio;
   if(s.viewStar==s.currentStar){actions.push_back({game->hasExpedition()&&s.activeStar==s.viewStar&&s.activePlanet==s.selectedPlanet?"Resume / Enter":"Land / Enter","land","",!p.giant});actions.push_back({"Scan / Space","orbitScan","",!s.scanned.count("ORB:"+p.id)&&game->inOrbit()});}
   else actions.push_back({"Jump here","jump",s.viewStar,distance(game->currentStar().pos,game->universe.star(s.viewStar).pos)<=game->range()&&game->fuelCost(distance(game->currentStar().pos,game->universe.star(s.viewStar).pos))<=s.fuel});
   }
   actions.push_back({"Atlas / G","galaxy",""});
  }else if(s.mode==Mode::Surface){auto p=game->activePlanet();name=p.name;meta=(p.generation>=2?game->universe.region(p,s.surface).name+" / ":std::string{})+game->universe.terrain(p,s.surface)+" / "+num(s.surface.x)+" : "+num(s.surface.y)+" / Battery "+num(s.battery)+"%";
   actions={{"Scan 1h / Space","scan","",s.battery>=12},{"Move 1h / Enter","moveWaypoint","",game->planMove().valid},{"Dive / E","dive","",p.water>=.2},{"Map / O","system",""}};
  }else{name="SUBMERSIBLE / "+num(s.depth)+" m";meta="Hull "+num(s.hull)+"% / Battery "+num(s.battery)+"% / Certified "+num(game->maxDepth())+" m";
   auto next=game->nextDiveOrder();actions={{"Sonar / Space","scan","",s.battery>=12},{next.label+" / Enter","oceanProceed","",next.available,true},{s.depth>80?"Ascend / E":"Recover / E",s.depth>80?"oceanAscend":"surface",""}};
  }
  fitted(name,24,h-99,viewWidth-215,22,pearl);right("Fuel "+num(s.fuel)+" / "+num(s.maxFuel)+"   "+num(s.economy.credits)+" cr",rightEdge-24,h-94,10,muted,true);
  fitted(meta,24,h-69,viewWidth,11,muted,true);
  if(!ui->panel)actions.push_back({s.mode==Mode::Ocean?"Dive ops / R":"Orders / R","orders",""});
  if(!ui->panel)actions.push_back({turnOutlook.warnings?"Resolve 1h ! / N":"Resolve 1h / N","endTurn","",!ui->paused,s.mode!=Mode::Ocean});
  float gap=6,cell=(viewWidth-gap*float(actions.size()-1))/float(actions.size());
  for(size_t i=0;i<actions.size();i++){auto& a=actions[i];button(a.label,{24+float(i)*(cell+gap),h-42,cell,30},a.action,a.arg,a.enabled,a.primary);}
 }
 if(ui->panel)panel();menus();
 if(!ui->orderHint.empty()&&!ui->menu&&!ui->sections&&!ui->help&&!ui->settings){float y=h-UILayout::footer-64;float width=std::min(760.f,rightEdge-48);DrawRectangleRec({18,y-8,width+20,60},alpha(ink,.96f));wrapped(ui->orderHint,28,y,width,11,mint,true);}
 if(ui->help||ui->settings)modal();
 if(!ui->saveWarning.empty()){float sy=h-UILayout::footer-54;DrawRectangleRec({18,sy-8,rightEdge-36,52},alpha(ink,.95f));wrapped("SAVE / "+ui->saveWarning,28,sy,rightEdge-56,11,Color{255,174,138,255},true);}
}
void Renderer::panel(){panelDrawing=true;float left=UILayout::panelLeft(w),x=left+20,contentWidth=UILayout::panelWidth-40;
 DrawRectangleRec({left,UILayout::header,UILayout::panelWidth,h-UILayout::header},panelInk);line({left,UILayout::header},{left,h},alpha(mint,.18f));
 const char* labels[]={"Inspect","Atlas","Research","Log","Outposts","Market","Factions","History"};
 fitted(ui->tab==0&&game->s.mode==Mode::Flight?"Destinations":labels[ui->tab],x,79,contentWidth-144,19,pearl);
 button("Sections",{w-150,74,98,30},"sections","",true,ui->sections);button("X",{w-44,74,28,30},"panel",std::to_string(ui->tab));
 BeginScissorMode(int((left+1)*width/w),int(UILayout::contentTop*height/h),int((UILayout::panelWidth-2)*width/w),int((UILayout::contentBottom(h)-UILayout::contentTop)*height/h));float y=126-ui->scroll,top=y;panelContent=true;
 State& s=game->s;
 if(game->turnBased()&&ui->tab==0&&disclosure(game->s.mode==Mode::Ocean?"Dive operations / R":"Next moves / R","play-turn",x,y,contentWidth))nextMoves(x,y,contentWidth);
 if(game->turnBased()&&ui->tab==0&&disclosure("Time / forecast","turn-planner",x,y,contentWidth))turnPlanner(x,y,contentWidth);
 if(game->turnBased()&&ui->tab==0&&disclosure("Before next turn"+(turnOutlook.warnings?" / "+std::to_string(turnOutlook.warnings)+(turnOutlook.warnings==1?" risk":" risks"):""),"before-turn",x,y,contentWidth)){
  row("Battery after waiting 1 h",num(turnOutlook.battery,1)+"%",x,y,contentWidth);
  if(turnOutlook.items.empty()){wrapped("No immediate alerts. The world waits for your next order.",x,y,contentWidth,12,muted);y+=12;}
  std::set<std::string> linkedSites;
  for(const auto& item:turnOutlook.items){wrapped(item.text,x,y,contentWidth,12,item.priority==Notice::Status?muted:Color{255,174,138,255});y+=10;
   if(!item.settlement.empty()&&findSettlement(s.economy,item.settlement)&&linkedSites.insert(item.settlement).second){button("Inspect settlement",{x,y,contentWidth,27},"reviewSettlement",item.settlement);y+=36;}}
  y+=10;
 }
 if(game->turnBased()&&game->lastTurn.valid&&disclosure("Recent resolutions / "+std::to_string(game->recentTurns.size()),"turn-report",x,y,contentWidth))turnReport(x,y,contentWidth);
 if(ui->tab>=4){economyPanel(x,y,contentWidth);}
 else if(ui->tab==0){
  if(s.mode==Mode::Flight){flightPanel(x,y,contentWidth);}
  else if(s.mode==Mode::Galaxy){Star star=game->selectedStar();double d=distance(star.pos,game->currentStar().pos);text(star.name,x,y,34,pearl);y+=44;wrapped(star.type+"-CLASS STAR / "+std::to_string(star.numPlanets)+" MAJOR BODIES",x,y,contentWidth,11,mint,true);y+=22;
   row("Distance",num(d)+" ly",x,y,contentWidth);row("Drive aperture",num(game->range())+" ly",x,y,contentWidth);row("Transit fuel",std::to_string(game->fuelCost(d))+" units",x,y,contentWidth);row("Luminosity",num(star.luminosity,2)+" solar",x,y,contentWidth);row("Stellar age",num(star.age,1)+" Gyr",x,y,contentWidth);y+=18;
   heading("NEARBY STARS",x,y);y+=42;Star here=game->currentStar();std::vector<Star> nearby;
   for(int cx=int(std::floor(here.pos.x/182))-3;cx<=int(std::floor(here.pos.x/182))+3;cx++)for(int cy=int(std::floor(here.pos.y/182))-3;cy<=int(std::floor(here.pos.y/182))+3;cy++)for(auto& q:game->universe.cell(cx,cy))if(q.id!=here.id)nearby.push_back(q);
   std::sort(nearby.begin(),nearby.end(),[&](const Star& a,const Star& b){return distance(a.pos,here.pos)<distance(b.pos,here.pos);});
   for(size_t i=0;i<std::min(size_t(10),nearby.size());i++){button(nearby[i].name+" / "+num(distance(nearby[i].pos,here.pos))+" ly",{x,y,contentWidth,34},"selectStar",nearby[i].id);y+=42;}
  }else if(s.mode==Mode::System){if(s.flight.valid&&!s.flight.deployed&&s.viewStar==s.currentStar)combatPanel(x,y,contentWidth);Planet p=game->planet();const MinorBody* minor=nullptr;for(const auto& b:game->universe.minorBodies(s.viewStar))if(b.id==game->selectedMinor)minor=&b;
   if(minor){bool destroyed=objectIntegrity(s.economy.worldDamage,"body:"+minor->id)<=0;fitted(minor->name,x,y,contentWidth,26,pearl);y+=40;wrapped(minorDescription(*minor),x,y,contentWidth,14,mint);y+=18;if(destroyed){wrapped("Fractured / debris and finite salvage remain in orbit",x,y,contentWidth,13,Color{255,178,125,255});y+=12;}row("Survey",s.scanned.count(minor->id)?"Catalogued":"Uncharted",x,y,contentWidth);if(minor->parent>=0)row("Parent",game->universe.planet(s.viewStar,minor->parent).name,x,y,contentWidth);
    button("Set fly-by destination",{x,y,contentWidth,32},"plotMinor","",s.viewStar==s.currentStar&&!destroyed);y+=42;wrapped("K takes the helm. I opens destinations; J flies to a safe stand-off point. Survey within 600 km at low speed.",x,y,contentWidth,12,muted);y+=22;
   }else{text(p.name,x,y,34,pearl);y+=46;wrapped(p.dominant+" / "+p.formation,x,y,contentWidth,14,mint);y+=20;
   row("Temperature",num(p.temp)+" K",x,y,contentWidth);row("Gravity",num(p.gravity,2)+" g",x,y,contentWidth);row("Ocean coverage",num(p.water*100)+"%",x,y,contentWidth);row("Atmosphere",num(p.pressure,2)+" atm",x,y,contentWidth);row("Biology",p.bio,x,y,contentWidth);row("Moons",std::to_string(p.moonCount),x,y,contentWidth);if(p.generation>=2)row("Architecture",game->universe.profile(p.starId).layout,x,y,contentWidth);y+=15;
   wrapped(atmosphere(p)+". "+(p.giant?"Turbulent pressure cells and layered condensates. Remote sensing is available.":"Coordinate-generated terrain and discoveries. Land to begin a surface expedition."),x,y,contentWidth,15,muted);y+=25;
   }
   heading("SYSTEM BODIES",x,y);y+=42;for(auto& q:game->universe.planets(s.viewStar)){button(q.name+" / "+q.type,{x,y,contentWidth,35},"planet",std::to_string(q.index),true,q.index==s.selectedPlanet);y+=43;}
   if(disclosure("Moons / asteroids / comets","minor-bodies",x,y,contentWidth))for(const auto& b:game->universe.minorBodies(s.viewStar)){button(b.name+" / "+(objectIntegrity(s.economy.worldDamage,"body:"+b.id)<=0?"DEBRIS":minorKindName(b.kind)),{x,y,contentWidth,30},"minorBody",b.id,true,b.id==game->selectedMinor);y+=37;}
  }else{Planet p=game->activePlanet();text(p.name,x,y,34,pearl);y+=48;
   if(s.mode==Mode::Ocean){row("Zone",oceanZone(s.depth),x,y,contentWidth);row("Pressure",num(1+s.depth/10)+" atm",x,y,contentWidth);row("Seafloor",num(game->universe.floorDepth(p,s.oceanX,s.oceanAtY))+" m",x,y,contentWidth);row("Certified depth",num(game->maxDepth())+" m",x,y,contentWidth);row("Hull integrity",num(s.hull)+"%",x,y,contentWidth);}
   else{row("Terrain",game->universe.terrain(p,s.surface),x,y,contentWidth);row("Temperature",num(p.temp)+" K",x,y,contentWidth);row("Wind",num(p.wind)+" m/s",x,y,contentWidth);}
   if(p.generation>=2&&disclosure("Regional survey","site-geology",x,y,contentWidth)){auto r=game->universe.region(p,s.mode==Mode::Ocean?V2{s.oceanX,s.oceanAtY}:s.surface,s.mode==Mode::Ocean?s.depth:0);wrapped(r.name+" / "+r.biome,x,y,contentWidth,13,mint);y+=10;row("Ore / crystal potential",num(r.ore*100)+"% / "+num(r.crystal*100)+"%",x,y,contentWidth);row("Habitat / geothermal",num(r.biomass*100)+"% / "+num(r.geothermal*100)+"%",x,y,contentWidth);}
   row("Battery",num(s.battery)+"%",x,y,contentWidth);y+=22;heading("CONTACTS / SET A WAYPOINT",x,y);y+=44;
   auto contacts=game->targets();std::sort(contacts.begin(),contacts.end(),[&](const Contact& a,const Contact& b){return s.mode==Mode::Ocean?std::hypot((a.pos.x-s.oceanX)*5,a.depth-s.depth)<std::hypot((b.pos.x-s.oceanX)*5,b.depth-s.depth):distance(a.pos,s.surface)<distance(b.pos,s.surface);});
   int count=0;for(auto& o:contacts){if(s.scanned.count(o.id))continue;button(o.kind+" / "+o.name,{x,y,contentWidth,36},"target",o.id);y+=44;if(++count==12)break;}if(count==0)wrapped("No uncharted contacts within passive sensor range. Traverse into a new region.",x,y,contentWidth,15,muted);
  }
 }else if(ui->tab==1){text("Survey record",x,y,24,pearl);y+=38;row("Stars visited",std::to_string(s.visitedStars.size()),x,y,contentWidth);row("Planetfalls",std::to_string(s.visitedPlanets.size()),x,y,contentWidth);row("Discoveries",std::to_string(s.discovered.size()),x,y,contentWidth);row("Distance travelled",num(s.totalDistance)+" ly",x,y,contentWidth);y+=20;
  heading("NACRE TRANSMISSION",x,y);y+=40;row("Recorded signatures",std::to_string(s.signals)+" / 12",x,y,contentWidth);DrawRectangleRec({x,y,contentWidth,3},lineColor);DrawRectangleRec({x,y,contentWidth*float(std::min(1.,s.signals/12.)),3},mint);y+=31;
  heading("DISCOVERIES",x,y);y+=44;
  if(s.discovered.empty())wrapped("The atlas is waiting. An orbital scan or focused Study begins your record.",x,y,contentWidth,16,muted);
  for(auto& d:s.discovered){text(d.planet+" / "+d.kind,x,y,10,contactColor(d.kind),true);y+=23;wrapped(d.title,x,y,contentWidth,19,pearl);y+=8;wrapped(d.description,x,y,contentWidth,14,muted);y+=23;line(v(x,y-9),v(x+contentWidth,y-9),alpha(lineColor,.5f));}
 }else if(ui->tab==2){
  button("Ship",{x,y,(contentWidth-6)/2,30},"researchPage","0",true,ui->researchPage==0);button("Outposts",{x+(contentWidth+6)/2,y,(contentWidth-6)/2,30},"researchPage","1",true,ui->researchPage==1);y+=44;
  if(ui->researchPage==1)civicResearchPanel(x,y,contentWidth);
  else{
   fitted("Alloy "+num(s.resources[0])+" / Crystal "+num(s.resources[1])+" / Bio "+num(s.resources[2])+" / Data "+num(s.resources[3]),x,y,contentWidth,10,mint,true);y+=32;
   for(int i=0;i<4;i++){auto& def=upgradeDefs[i];auto cost=game->upgradeCost(i);fitted(def.name,x,y,contentWidth-45,18,pearl);right(std::to_string(s.upgrades[i])+"/"+std::to_string(def.max),x+contentWidth,y+3,11,mint,true);y+=29;
    wrapped(def.desc,x,y,contentWidth,13,muted);y+=10;
    if(s.upgrades[i]<def.max){wrapped(std::to_string(cost[0])+" alloy / "+std::to_string(cost[1])+" crystal / "+std::to_string(cost[2])+" bio / "+std::to_string(cost[3])+" data",x,y,contentWidth,11,mint,true);y+=10;button("Fabricate upgrade",{x,y,contentWidth,32},"upgrade",def.key,game->canUpgrade(i),game->canUpgrade(i));y+=44;}else{ text("Certified",x,y,11,mint,true);y+=30;}
   }
  }
 }else{for(auto& e:s.log){text(num(e.time,1)+" H / NOMA",x,y,10,mint,true);y+=22;wrapped(e.msg,x,y,contentWidth,16,pearl);y+=29;line(v(x,y-14),v(x+contentWidth,y-14),alpha(lineColor,.5f));}}
 ui->panelHeight=y-top;EndScissorMode();panelContent=false;float available=UILayout::contentBottom(h)-126;ui->scroll=clamp(ui->scroll,0.f,std::max(0.f,ui->panelHeight-available));
 line({left,h-48},{w,h-48},alpha(lineColor,.25f));
 if(game->turnBased())button("Orders / R",{x,h-39,122,28},"orders");else text(ui->paused?"PAUSED / P":"FLIGHT LIVE / P PAUSES",x,h-29,10,muted,true);
 if(game->turnBased())button(turnOutlook.warnings?"Resolve 1h ! / N":"Resolve 1h / N",{w-164,h-39,144,28},"endTurn","",!ui->paused,true);panelDrawing=false;
 if(ui->panelHeight>available){float thumb=std::max(25.f,available*available/ui->panelHeight),progress=ui->scroll/std::max(1.f,ui->panelHeight-available);DrawRectangleRec({w-5,126+progress*(available-thumb),2,thumb},alpha(mint,.5f));}
}
void Renderer::modal(){modalDrawing=true;DrawRectangle(0,0,int(w),int(h),alpha(ink,.72f));float mw=std::min(820.f,w-60),mh=ui->settings?574.f:std::min(h-40,690.f);float x=(w-mw)/2,y=(h-mh)/2;
 DrawRectangleRounded({x,y,mw,mh},.025f,6,panelInk);DrawRectangleRoundedLinesEx({x,y,mw,mh},.025f,6,1,alpha(mint,.4f));float tx=x+34,ty=y+31;
 text(ui->settings?"DISPLAY / EXPEDITION":"NOMA / FIELD MANUAL",tx,ty,11,mint,true);ty+=36;text(ui->settings?"Your expedition":"Flight and survey",tx,ty,26,pearl);ty+=44;
 if(ui->settings){float choiceWidth=(mw-80)/3;
  for(int i=0;i<3;i++){const auto quality=std::array<GraphicsQuality,3>{GraphicsQuality::Low,GraphicsQuality::High,GraphicsQuality::Extreme}[i];const char* title=i==0?"GRAPHICS: LOW":i==1?"GRAPHICS: HIGH":"GRAPHICS: EXTREME";button(title,{tx+i*(choiceWidth+6),ty,choiceWidth,32},"graphics",graphicsQualityName(quality),true,ui->graphicsQuality==quality);}
  ty+=44;for(int i=0;i<3;i++){auto look=std::array<FlightLook,3>{FlightLook::Standard,FlightLook::Psychedelic,FlightLook::Wireframe}[i];button(i==0?"LOOK: STANDARD":i==1?"LOOK: PSYCHEDELIC":"LOOK: WIREFRAME",{tx+i*(choiceWidth+6),ty,choiceWidth,32},"look",flightLookName(look),true,ui->flightLook()==look);}ty+=44;wrapped("A seed defines a repeatable universe. Creating another universe first exports a backup of this expedition.",tx,ty,mw-68,16,muted);ty+=18;
  DrawRectangleRounded({tx,ty,mw-68,45},.05f,4,Color{16,30,39,255});text(ui->seedInput+((int(clock*2)%2)?"|":""),tx+12,ty+13,16,pearl,true);ty+=61;
  button("CREATE NEW UNIVERSE",{tx,ty,mw-68,40},"new","",!ui->seedInput.empty(),true);ty+=61;
  wrapped("Drop a native JSON save onto this window to import it. Export creates a portable file beside the automatic save.",tx,ty,mw-68,15,muted);ty+=15;
  button("EXPORT SAVE",{tx,ty,172,36},"export");button(ui->sound?"SOUND ON / F10":"SOUND OFF / F10",{tx+184,ty,185,36},"sound");
  button(ui->enhancedHUD?"FLIGHT GUIDANCE: ON / F8":"FLIGHT GUIDANCE: OFF / F8",{tx,ty+47,mw-68,32},"flightHUD");
 }else{
  const std::vector<std::pair<std::string,std::string>> lines=game->s.mode==Mode::Flight?std::vector<std::pair<std::string,std::string>>{
   {"FLIGHT","W/S throttle, arrows steer, Q/E roll, A/D strafe, R/F vertical thrust. Shift boosts; hold Option/Alt for fine thrust and steering. C cruises, X matches assist to forward speed, Backspace stops."},
   {"NAVIGATE","I lists ports; Tab cycles; J flies and docks automatically. Steering takes control. Enter launches or docks nearby below 55 km/s."},
   {"LANDING","Enter requests nearby landing assist. G lowers gear. Keep descent under 6 km/s, drift under 10 and tilt under 35 degrees; the landing display checks the ground slope. L deploys after touchdown."},
   {"COMMERCE","E opens the docked exchange. Port service uses real local supplies. Courier pouches pay reserved escrow. Civilian kills carry fines and faction penalties."},
   {"COMBAT","Space / left mouse fires. 1/2/3/4 selects pulse, kinetic, plasma or resonance; U cycles subsystem focus. T cycles hostiles. Keep the target in the reticle to lock, Z launches a seeker, Y releases countermeasures. I opens ship systems and orders."},
   {"STRATEGY","O or M opens system strategy. B opens settlements, markets, factions and history. Outside the cockpit, R opens the turn planner; N ends a turn and the world waits between orders. K resumes real time."}
  }:std::vector<std::pair<std::string,std::string>>{
   {"GALAXY","Drag to pan. Scroll to zoom. Select a star; J jumps, Enter opens its system."},
   {"CHARTS","O/G opens charts freely. L resumes a deployed survey. U recovers after ascent for one turn. Select a planet; Enter lands. Space scans and F scoops from orbit."},
   {"SURFACE","WASD commits one step per turn. Click plans; Enter moves. Space scans. E deploys the submersible. K returns to your ship at this site."},
   {"OCEAN","Enter performs the labelled next action. C samples deliberately. Space pings; Shift+Space listens quietly. R opens Sites / Contacts / Details. I opens Details. K returns; E ascends / recovers."},
   {"SOCIETIES","B opens outposts, markets, factions, patrols and pirate vendettas. Factions offers safe-passage deals and warrant restitution. Freight routes offer funded escorts. N resolves an hour. R opens field choices, objectives and forecasts. Shift+Enter follows a plotted field route. Shift+N waits up to six hours. I opens details. Research fabricates upgrades. Atlas holds discoveries."}
  };
  for(auto& item:lines){text(item.first,tx,ty,10,mint,true);float lineY=ty;wrapped(item.second,tx+84,lineY,mw-152,14,pearl);ty=std::max(ty+43,lineY+15);}
  wrapped("F8 flight HUD / P pause / F9 hide UI / F10 sound / F11 fullscreen / F12 capture / K switch mode / O strategy / Cmd or Ctrl+Q quit",tx,ty,mw-68,12,muted,true);
 }
 button("CLOSE / ESC",{x+mw-171,y+mh-53,137,33},"close");modalDrawing=false;
}
void Renderer::draw(Game& g,UI& state,double dt){g.ensureEconomy();game=&g;ui=&state;auto viewport=uiViewport();w=viewport.width;h=viewport.height;clock+=float(dt);ui->orderHint.clear();if(ui->auditInterface)ui->auditButtons.clear();turnOutlook=g.outlook();if(state.reportHour!=g.lastTurn.endHour){state.reportHour=g.lastTurn.endHour;state.reportIndex=0;}
 std::string lifeWorld=g.s.seed+"|"+g.s.activeStar+"|"+std::to_string(g.s.activePlanet)+"|"+std::to_string(int(g.s.mode));if(lifeKey!=lifeWorld||g.s.hours<lifeTarget){lifeKey=lifeWorld;lifeHours=lifeFrom=lifeTarget=g.s.hours;lifeBlend=1;}
 if(g.s.mode==Mode::Flight)lifeHours=lifeFrom=lifeTarget=g.s.hours;else{if(g.s.hours!=lifeTarget){lifeFrom=lifeHours;lifeTarget=g.s.hours;lifeBlend=0;}lifeBlend=std::min(1.,lifeBlend+dt/.55);double blend=lifeBlend*lifeBlend*(3-2*lifeBlend);lifeHours=lifeFrom+(lifeTarget-lifeFrom)*blend;}
 targets();updateTerrain();hotspots.clear();
 if(presentedMode>=0&&presentedMode!=int(g.s.mode)){
  if(handoffFrame.id&&(handoffFrame.texture.width!=width||handoffFrame.texture.height!=height)){UnloadRenderTexture(handoffFrame);handoffFrame={};}
  if(!handoffFrame.id)handoffFrame=LoadRenderTexture(width,height);
  BeginTextureMode(handoffFrame);ClearBackground(ink);DrawTexturePro(composite.texture,{0,0,float(width),-float(height)},{0,0,float(width),float(height)},{0,0},0,WHITE);EndTextureMode();
 }
 presentedMode=int(g.s.mode);
 if(g.s.mode==Mode::Flight){g.ensureFlight();flightPresentation=flightScene(g);if(auditHideWildlife)flightPresentation.organisms.clear();flightGraphics.prepare(g.s.flight,flightPresentation,state.reduced,!state.clean,state.graphicsQuality,state.psychedelic,state.wireframe);}else background(dt);BeginTextureMode(composite);ClearBackground(ink);rlPushMatrix();rlScalef(width/w,height/h,1);
 if(g.s.mode==Mode::Flight)flightGraphics.present(w,h);else{BeginShaderMode(post);texture(post,"bloomTexture",bloom[1].texture);DrawTexturePro(history[current].texture,{0,0,float(width),-float(height)},{0,0,w,h},{0,0},0,WHITE);EndShaderMode();}
 switch(g.s.mode){case Mode::Galaxy:galaxy();break;case Mode::System:system();break;case Mode::Surface:surface();break;case Mode::Ocean:ocean();break;case Mode::Flight:break;}
 if(g.s.mode!=Mode::Flight)colonyMarkers();
 if(handoffFrame.id&&ui->handoff.remaining>0){
  BeginScissorMode(0,int(UILayout::header*height/h),int(UILayout::worldRight(w,ui->panel)*width/w),int((h-UILayout::header-UILayout::footer)*height/h));
  DrawTexturePro(handoffFrame.texture,{0,0,float(width),-float(height)},{0,0,w,h},{0,0},0,alpha(WHITE,ui->handoff.opacity()));EndScissorMode();
 }
 if(!ui->clean)overlay();else if(ui->help||ui->settings)modal();
 if(!ui->handoff.ready()){
  std::string title=g.turnBased()?(g.s.mode==Mode::Surface||g.s.mode==Mode::Ocean?"EXPEDITION / WORLD FROZEN":"STRATEGY / WORLD FROZEN"):"COCKPIT / TAKING THE HELM";
  std::string hint=ui->handoff.release?"Release held controls to continue":!g.transferNotice.empty()?g.transferNotice:g.turnBased()?"Orders advance time / K resumes flight":"Same ship / same position / real time resumes";
  float edge=UILayout::worldRight(w,ui->panel);DrawRectangleRec({edge*.5f-218,112,436,58},alpha(ink,.95f));
  fitted(title,edge*.5f-202,121,404,12,mint,true);fitted(hint,edge*.5f-202,144,404,10,muted,true);
 }
 rlPopMatrix();EndTextureMode();
 BeginDrawing();ClearBackground(ink);DrawTexturePro(composite.texture,{0,0,float(width),-float(height)},{0,0,float(GetScreenWidth()),float(GetScreenHeight())},{0,0},0,WHITE);EndDrawing();}
bool Renderer::screenshot(const std::string& path,bool centreCrop){auto p=std::filesystem::path(path);if(p.has_parent_path())std::filesystem::create_directories(p.parent_path());Image im=LoadImageFromTexture(composite.texture);if(centreCrop){int side=std::min({640,im.width,im.height});ImageCrop(&im,{float((im.width-side)/2),float((im.height-side)/2),float(side),float(side)});}ImageFlipVertical(&im);bool result=ExportImage(im,path.c_str());UnloadImage(im);return result;}

bool Renderer::makeIcon(const std::string& path){
 float oldW=w,oldH=h;int oldWidth=width,oldHeight=height;w=h=1024;width=height=1024;
 RenderTexture2D target=LoadRenderTexture(1024,1024);BeginTextureMode(target);ClearBackground(BLANK);
 DrawRectangleRounded({24,24,976,976},.23f,24,Color{6,14,27,255});
 for(int i=0;i<20;i++){float r=220+i*14;ring({512,512},r,alpha(hue(.48f+i*.024f,.75f),.10f),2);}
 glow({480,448},430,mint,.20f);orbDraw({512,475},320,4781,.82f,false);
 Vector2 last{};for(int i=0;i<=240;i++){float a=float(tau*i/240);Vector2 p{512+std::cos(a)*425,520+std::sin(a)*148};
  if(i>0&&(i>120||i<25)){line(last,p,hue(.47f+i*.0017f,.45f),5);line({last.x,last.y+5},{p.x,p.y+5},alpha(mint,.2f),2);}last=p;}
 std::vector<Vector2> craft{{754,649},{691,676},{631,734},{648,675},{615,659},{653,650},{671,590},{699,641}};
 polygon(craft,Color{182,229,217,255},pearl);DrawCircleV({701,651},12,pearl);glow({701,651},42,mint,.3f);
 EndTextureMode();Image image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);auto p=std::filesystem::path(path);if(p.has_parent_path())std::filesystem::create_directories(p.parent_path());bool ok=ExportImage(image,path.c_str());UnloadImage(image);UnloadRenderTexture(target);w=oldW;h=oldH;width=oldWidth;height=oldHeight;return ok;
}
void Renderer::pointer(Game& g,UI& state,Vector2 mouse,bool dbl){if(!state.handoff.ready())return;if(state.panel&&mouse.x>UILayout::panelLeft(w))return;if(state.help||state.settings||state.menu||state.sections||state.paused||(!state.clean&&(mouse.y<UILayout::header+8||mouse.y>h-(g.s.mode==Mode::Ocean?84:UILayout::footer))))return;
 Hotspot* chosen=nullptr;float closest=1e9;for(auto& hit:hotspots){float d=std::hypot(hit.p.x-mouse.x,hit.p.y-mouse.y);if(d<hit.radius&&d<closest){chosen=&hit;closest=d;}}
 if(chosen){if(chosen->type=="star"){g.s.selectedStar=chosen->id;if(dbl)g.system(chosen->id);}else if(chosen->type=="planet"){g.s.selectedPlanet=chosen->index;g.selectedMinor.clear();}else if(chosen->type=="minor"){g.selectedMinor=chosen->id;state.panel=true;state.tab=0;state.scroll=0;}else if(chosen->type=="settlement"){state.settlement=chosen->id;state.panel=true;state.tab=4;state.scroll=0;}else if(g.s.mode==Mode::Ocean){g.issueOrder("oceanSelect",chosen->object.id);state.panel=false;state.divePage=0;state.scroll=0;}else g.waypoint(chosen->object);return;}
 if(g.s.mode==Mode::Surface){g.focusedContact.clear();g.s.waypoint={g.s.surface.x+(mouse.x-w*.5)/state.surfaceZoom,g.s.surface.y+(mouse.y-h*.5)/state.surfaceZoom};g.notify("Waypoint planned / Enter commits one movement turn");}
 else if(g.s.mode==Mode::Ocean){g.focusedContact.clear();g.diveState.returning=false;g.s.oceanTargetX=g.s.oceanX+(mouse.x-UILayout::worldRight(w,state.panel)*.48)/(3.4*state.oceanZoom);g.s.targetDepth=clamp(g.s.depth+(mouse.y-oceanY())/(.35*state.oceanZoom),20.,std::min(g.maxDepth(),g.universe.floorDepth(g.activePlanet(),g.s.oceanTargetX,g.s.oceanAtY)-40));g.notify("Dive waypoint planned / Enter commits one movement turn");}
}
}
