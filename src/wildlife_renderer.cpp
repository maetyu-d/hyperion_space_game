#include "renderer.hpp"
#include "biosphere.hpp"
namespace hyperion {
namespace {
Color tint(double phase,float value=.85f){return ColorFromHSV(float(phase*360),.6f,value);}
Color fade(Color c,float a){c.a=static_cast<unsigned char>(clamp(a,0.f,1.f)*255);return c;}
}
void Renderer::lifeBody(const LifeAgent& a,Vector2 p,float scale){
 auto bio=phenotype(a.contact);float angle=float(a.flora&&a.marine?0:a.heading),co=std::cos(angle),si=std::sin(angle),t=float(std::fmod(lifeHours*55,100000.)*bio.gait+a.phase),s=scale*float(a.size);
 float rest=a.behaviour==Resting?.76f:1.f,width=.82f+.06f*float(a.contact.n%7),length=.8f+.05f*bio.segments,motion=float(a.activity);
 auto point=[&](float x,float y){x*=length;y*=width*rest;return Vector2{p.x+(x*co-y*si)*s,p.y+(x*si+y*co)*s};};
 bool detailed=scale>1.05f;float presence=float(.4+.6*a.presence);Color body=fade(tint(bio.hue,.62f),presence),edge=fade(tint(bio.hue+.2,.92f),presence),light=fade(tint(bio.hue+.1,.86f),presence),dark=fade(tint(bio.hue+.05,.25f),presence);
 auto tri=[&](Vector2 x,Vector2 y,Vector2 z,Color c){if((y.x-x.x)*(z.y-x.y)-(y.y-x.y)*(z.x-x.x)>0)std::swap(y,z);DrawTriangle(x,y,z,c);};
 auto line=[&](Vector2 x,Vector2 y,Color c,float w=1){DrawLineEx(x,y,std::max(.5f,w*s),c);};
 auto oval=[&](float x,float y,float rx,float ry,Color c){int n=detailed?20:std::max(rx,ry)*s<1.5f?6:8;auto centre=point(x,y);for(int j=0;j<n;j++){float u=float(tau*j/n),v=float(tau*(j+1)/n);tri(centre,point(x+std::cos(u)*rx,y+std::sin(u)*ry),point(x+std::cos(v)*rx,y+std::sin(v)*ry),c);}};
 auto shell=[&](float x,float y,float rx,float ry){oval(x,y,rx,ry,dark);oval(x-.25f,y-.3f,rx*.91f,ry*.88f,body);if(detailed)oval(x-.8f,y-.7f,rx*.63f,ry*.6f,fade(light,presence*.56f));};
 auto curve=[&](float ax,float ay,float mx,float my,float bx,float by,float w,Color c){auto previous=point(ax,ay);int n=detailed?8:3;for(int j=1;j<=n;j++){float f=float(j)/n;auto next=point((1-f)*(1-f)*ax+2*f*(1-f)*mx+f*f*bx,(1-f)*(1-f)*ay+2*f*(1-f)*my+f*f*by);line(previous,next,c,w*(1-f*.65f));previous=next;}};
 auto leaf=[&](float ax,float ay,float bx,float by,float radius){float dx=bx-ax,dy=by-ay,d=std::max(.01f,std::hypot(dx,dy)),nx=-dy/d,ny=dx/d;Vector2 oldL=point(ax,ay),oldR=oldL;int n=detailed?6:3;
  for(int j=1;j<=n;j++){float f=float(j)/n,w=std::sin(f*float(tau*.5))*radius,x=ax+dx*f+nx*std::sin(f*float(tau*.5))*.8f,y=ay+dy*f+ny*std::sin(f*float(tau*.5))*.8f;auto l=point(x+nx*w,y+ny*w),r=point(x-nx*w,y-ny*w);if(j>1)tri(oldL,l,oldR,fade(body,.82f*presence));if(j<n)tri(oldR,l,r,fade(light,.57f*presence));if(detailed){line(oldL,l,fade(edge,.72f),.5f);line(oldR,r,fade(edge,.72f),.5f);}oldL=l;oldR=r;}
  curve(ax,ay,(ax+bx)*.5f+nx,(ay+by)*.5f+ny,bx,by,.5f,edge);
 };
 auto eye=[&](float x,float y,float r){oval(x,y,r*1.25f,r,dark);if(detailed)oval(x,y,r,r*.8f,Color{192,235,214,255});oval(x+.15f,y,r*.7f,r*.68f,edge);oval(x+.35f,y,r*.27f,r*.38f,Color{3,8,17,255});if(detailed)oval(x+.1f,y-r*.25f,r*.13f,r*.13f,Color{238,255,247,255});};
 DrawCircleGradient(int(p.x),int(p.y),19*s,fade(edge,.055f),fade(edge,0));
 if(!a.marine&&!a.flora)DrawEllipse(int(p.x+2*s),int(p.y+4*s),12*s,4*s,Color{2,9,17,65});
 if(a.flora){
  int branches=std::min(detailed?8:6,3+bio.limbs);for(int j=0;j<branches;j++){float phase=float(tau*j/branches),bend=std::sin(t*.45f+j)*.65f,x=std::cos(phase)*(8+j%3),y=std::sin(phase)*(8+j%3);curve(0,0,x*.55f+bend,y*.3f,x,y,1.5f,dark);
   if(bio.plan==0)leaf(x*.15f,y*.15f,x*1.25f+bend,y*1.25f,2.8f);
   else if(bio.plan==1){leaf(x*.1f,y*.1f,x*1.3f,y*1.3f,1.4f);for(int side:{-1,1})leaf(x*.5f,y*.5f,x*.9f-y*.3f*side,y*.9f+x*.3f*side,1.1f);}
   else if(bio.plan==2){shell(x,y,3.7f,2.8f);oval(x+.3f,y-.4f,2.8f,1.7f,dark);oval(x+.4f,y-.1f,1.2f,.8f,edge);}
   else if(bio.plan==3){for(int k=1;k<=4;k++){float f=k*.25f;shell(x*f,y*f,2.1f-f,.9f);}}
   else if(bio.plan==4){shell(x,y,3.6f,3.3f);for(int k=0;k<(detailed?5:3);k++){float a=float(tau*k/(detailed?5:3));curve(x+std::cos(a)*2,y+std::sin(a)*2,x+std::cos(a)*4,y+std::sin(a)*4,x+std::cos(a)*5.2f,y+std::sin(a)*5.2f,.5f,edge);}}
   else{for(int k=1;k<=3;k++){float f=k*.25f;line(point(x*f-y*.2f,y*f+x*.2f),point(x*f+y*.2f,y*f-x*.2f),edge,.6f);}leaf(x*.3f,y*.3f,x,y,1.1f);}
  }shell(0,0,2.8f,2.4f);return;
 }
 if(bio.plan==0){float pulse=1+std::sin(t*1.6f)*.035f;for(int j=0;j<bio.limbs;j++){float a=float(tau*j/bio.limbs);curve(std::cos(a)*5,std::sin(a)*5,std::cos(a)*10+std::sin(t+j),std::sin(a)*10,std::cos(a)*13,std::sin(a)*13,1.2f,body);}shell(0,0,8.7f*pulse,7.8f*pulse);oval(-.7f,-1,6.5f,5.5f,fade(edge,.19f));for(int j=0;j<5;j++){float a=float(tau*j/5);curve(0,0,std::cos(a)*4,std::sin(a)*4,std::cos(a)*7,std::sin(a)*7,.45f,edge);}oval(0,0,2,1.7f,light);}
 else if(bio.plan==1){for(int side:{-1,1})for(int j=0;j<4;j++){float x=-8+j*4;leaf(x,side*2,x-5,side*(6+std::sin(t+j)*motion),2.2f);}curve(10,0,0,-std::sin(t)*motion,-13,0,6,body);for(int j=0;j<5;j++){float x=7-j*4;shell(x,std::sin(t-j*.6f)*motion,3.7f-j*.35f,3.2f-j*.35f);}eye(8,-1.4f,1.2f);}
 else if(bio.plan==2){for(int side:{-1,1})for(int j=0;j<3;j++){float x=-6+j*5,swing=std::sin(t*5+j+side*2)*motion;curve(x,side*2,x-1+swing,side*6,x-3+swing,side*10,1.8f,dark);curve(x,side*2,x-1+swing,side*6,x-3+swing,side*10,.65f,edge);}shell(-1,0,10,5);for(int side:{-1,1})leaf(-1,side*2,-6,side*9,2.8f);shell(9,0,4.2f,3);for(int side:{-1,1}){curve(10,side*1.8f,13,side*4,15,side*3,.7f,edge);eye(11,side*1.5f,.9f);}}
 else if(bio.plan==3){for(int j=0;j<5;j++){float x=7-j*3.7f;for(int side:{-1,1}){float f=std::sin(t*5+j+side)*motion;curve(x,side*2,x+f,side*6,x-3+f,side*10,1.3f,body);}shell(x,0,3.8f,4.5f);curve(x,-3,x+1,0,x,3,.55f,edge);}for(int side:{-1,1}){curve(8,side*1.5f,11,side*5,15,side*4,.6f,edge);eye(9,side*1.6f,.7f);}}
 else if(bio.plan==4){for(int j=0;j<6;j++){float x=std::cos(j*2.4f)*9,y=std::sin(j*2.4f)*6+std::sin(t*.9f+j)*.7f;leaf(x-2,y,x-7,y+std::sin(t*3+j),1.8f);shell(x,y,3.7f,1.7f);eye(x+2,y-.5f,.5f);}}
 else{for(int side:{-1,1})for(int j=0;j<(detailed?5:4);j++){float x=-5+j*2.5f,sway=std::sin(t*1.3f+j)*motion;curve(x,side*2,x-1,side*6,x-3+sway,side*11,.9f,edge);for(int k=1;k<(detailed?4:3);k++){float f=k*.23f;leaf(x-f*2,side*(2+f*8),x-3-f*3+sway,side*(4+f*8),.9f);}}shell(0,0,6.7f,3.5f);eye(5,-1,.8f);}
 if(bio.plan!=4)for(int j=0;j<4;j++){float x=-4+j*2.4f;if(a.contact.n%2){oval(x,std::sin(j*2.f+a.contact.n%13)*1.7f,.8f,.5f,fade(edge,.56f));oval(x,std::sin(j*2.f+a.contact.n%13)*1.7f,.3f,.23f,dark);}else curve(x,-1.8f,x+.7f,0,x,1.8f,.5f,fade(edge,.55f));}
 if(a.behaviour==Grazing||a.behaviour==GardenForaging||a.behaviour==Filtering||a.behaviour==Recycling){float jaw=1+std::sin(t*6)*.5f;line(point(10,-jaw),point(12,0),edge,.6f);line(point(10,jaw),point(12,0),light,.6f);for(int j=0;j<3;j++){float f=std::fmod(t*.4f+j*.33f,1.f);oval(17-f*6,std::sin(j*3.1f)*2.6f,.4f,.4f,edge);}}
}
void Renderer::drawWildlife(bool ocean){
 auto p=game->activePlanet();auto& s=game->s;LifeView view;view.centre=ocean?V2{s.oceanX,s.oceanAtY}:s.surface;view.observer=view.centre;view.observerDepth=s.depth;view.alert=ocean?game->diveAlert():.65;view.ocean=ocean;view.depth=s.depth;view.hours=lifeHours;view.radius=ocean?95:std::min(480.,double(w*.55/ui->surfaceZoom+60));view.vertical=ocean?480:std::min(1600.,double(h/.35/ui->oceanZoom*.6));view.limit=ocean?(ui->reduced?16:24):(ui->reduced?140:210);
 auto animals=visibleLife(s.economy,game->universe,p,view);float z=float(ocean?ui->oceanZoom:ui->surfaceZoom);
 auto project=[&](const LifeAgent& a){return ocean?Vector2{oceanX()+float(a.position.x-s.oceanX)*3.4f*z,oceanY()+float(a.depth-s.depth)*.35f*z}:Vector2{w*.5f+float(a.position.x-s.surface.x)*z,h*.5f+float(a.position.y-s.surface.y)*z};};
 // Rooted food first, then swimming/crawling animals and feeding cues.
 for(int rooted=1;rooted>=0;rooted--)for(const auto& animal:animals)if(int(animal.flora)==rooted){auto q=project(animal);if(q.x<-35||q.y<65||q.x>w+35||q.y>h+35)continue;float scale=(ocean?.42f:.8f)*std::sqrt(z);lifeBody(animal,q,scale);
  if(CheckCollisionPointCircle(uiMouse(),q,16*scale)){text(animal.contact.name,q.x+20,q.y-23,12,{234,244,231,255});text(lifeBehaviourName(animal.behaviour),q.x+20,q.y-6,9,{153,255,218,255},true);}
  hotspots.push_back({q,16*scale,"contact",animal.contact.id,0,animal.contact});
 }
}
}
