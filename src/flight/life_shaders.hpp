#pragma once
#include "wire_shaders.hpp"
namespace hyperion::flight {
inline constexpr const char* lifeVertex=R"GLSL(#version 330
in vec2 vertexTexCoord2;in vec4 vertexTangent;noperspective out vec3 vectorBary;flat out vec3 vectorFeatures;
in vec3 vertexPosition;in vec3 vertexNormal;in vec4 vertexColor;
uniform mat4 mvp,matModel,matNormal;
uniform float clock,phase,gait,activity,rooted,feeding,bodyPlan,resting;uniform vec3 shape;
out vec3 localPos,worldPos,normal;out vec4 material;
void main(){vec3 p=vertexPosition;float t=clock*gait+phase,part=vertexColor.b*12.56637,flex=vertexColor.g,motion=mix(activity,.025,resting);
 if(rooted>.5){p.x+=sin(t*.55+p.y*.9)*p.y*p.y*.018;p.z+=cos(t*.4+p.y)*p.y*.024;p.x+=sin(t*.8+part)*flex*.022;}
 else{if(bodyPlan<.5){float pulse=sin(t*1.6);p.xz*=1.+pulse*.025;p.x+=sin(t*1.9+part+p.y)*flex*.055;p.z+=cos(t*1.6+part)*flex*.04;}
  else if(bodyPlan<1.5){p.x+=sin(t*(1.8+motion)+p.z*2.8)*(.035+.1*flex)*motion;}
  else if(bodyPlan<3.5){p.z+=sin(t*(3.+motion*3.)+part)*flex*.105*motion;p.y+=max(0.,sin(t*(3.+motion*3.)+part))*flex*.065*motion;}
  else if(bodyPlan<4.5){p.x+=sin(t*1.2+part)*.025;p.z+=sin(t*3.+part)*flex*.06;}
  else{p.z+=sin(t*1.3+part)*flex*.055*motion;p.y+=sin(t+part)*flex*.018;}
  p.y*=1.-resting*.18;p.y+=sin(t*.8)*.009*min(1.,p.y);p.y-=feeding*max(0.,-p.z-.6)*(.035+.02*sin(t*5.));}
 vectorBary=vec3(vertexTexCoord2,1.-vertexTexCoord2.x-vertexTexCoord2.y);vectorFeatures=vertexTangent.xyz;localPos=p;p*=shape;worldPos=(matModel*vec4(p,1)).xyz;vec3 n=vertexNormal/shape;if(rooted<.5)n.y/=1.-resting*.18;normal=normalize((matNormal*vec4(n,0)).xyz);material=vertexColor;gl_Position=mvp*vec4(p,1);}
)GLSL";
inline constexpr const char* lifeFragment=R"GLSL(#version 330
in vec3 localPos,worldPos,normal;in vec4 material;uniform vec3 sun,eye;uniform float pigment,phase,genes,clock,rooted;uniform vec2 coverageRange;uniform float lowGraphics;
uniform float wireframe;
out vec4 finalColor;
)GLSL" HYPERION_WIRE_EDGE_GLSL R"GLSL(
vec3 palette(float h){return .11+.89*(.5+.5*cos(6.283185*(h+vec3(0.,.67,.34))));}
void main(){
 // Fixed ordered coverage keeps opaque depth and needs no temporal noise.
 // The two LODs occupy complementary intervals, so they never double-blend.
 const float bayer[16]=float[16](0.,8.,2.,10.,12.,4.,14.,6.,3.,11.,1.,9.,15.,7.,13.,5.);
 ivec2 pixel=ivec2(gl_FragCoord.xy);float coverage=(bayer[(pixel.y%4)*4+pixel.x%4]+.5)/16.;
 if(coverage<coverageRange.x||coverage>=coverageRange.y)discard;
 if(wireframe>.5){finalColor=vec4(vectorInk(normal,eye-worldPos,mix(vec3(.18,.86,.94),palette(pigment+.1),.38)),1.);return;}
 vec3 n=normalize(normal);if(!gl_FrontFacing)n=-n;vec3 view=normalize(eye-worldPos),light=normalize(sun),p=localPos;
 float role=material.r,shell=1.-smoothstep(.025,.09,abs(role-.47)),membrane=1.-smoothstep(.03,.1,abs(role-.667));
 float frequency=11.+genes*8.,bands=sin(p.z*frequency+sin(p.x*8.+genes*19.)*1.1),cells=sin(p.x*16.+p.z*5.)*cos(p.z*13.-p.y*7.);float rings=sin(length(p.xz)*23.+p.y*9.);
 float pattern=floor(genes*4.),signal=pattern<1.?cells:pattern<2.?bands:pattern<3.?sin(p.x*12.+sin(p.y*8.)+p.z*13.):rings;
 float footprint=length(fwidth(p)),detail=1.-smoothstep(.3,1.3,footprint*22.),aa=max(.03,fwidth(signal));
 float vein=(1.-smoothstep(.055,.055+aa,abs(signal)))*detail,mark=mix(.5,smoothstep(-.1-aa,-.1+aa,signal),detail);
 float relief=(shell*.055+.018)*(signal*.5+.5)*detail;vec3 dx=dFdx(worldPos),dy=dFdy(worldPos),r1=cross(dy,n),r2=cross(n,dx);float det=dot(dx,r1);vec3 grad=(dFdx(relief)*r1+dFdy(relief)*r2)*sign(det)/max(abs(det),.00001);n=normalize(n-grad*.2);
 float tone=.5+.5*sin(p.y*3.+p.z*1.5+genes*11.);vec3 base=mix(palette(pigment),palette(pigment+.19),tone*.58);base*=mix(.44,.85,mark);base=mix(base,base*vec3(.76,.9,1.08),shell*.3);
 float micro=sin(p.x*43.+p.z*19.)*cos(p.y*29.-p.z*17.);base*=1.+micro*.06*(1.-smoothstep(.2,1.,footprint*37.));
 float diffuse=max(0.,dot(n,light)),rim=pow(1.-abs(dot(n,view)),3.),back=max(0.,dot(-n,light));float pulse=.84+.16*sin(clock*.16+phase+p.z*2.);
 vec3 colour;
 if(lowGraphics>.5){colour=base*(.38+.72*diffuse)+palette(pigment+.3)*(vein*.13*pulse+rim*(.055+membrane*.13));colour+=base*membrane*back*.33;}else{
 float wrapped=clamp((dot(n,light)+.35)/1.35,0.,1.);
 vec3 skyBounce=mix(vec3(.10,.075,.11),vec3(.20,.29,.36),clamp(n.y*.5+.5,0.,1.));
 colour=base*(skyBounce+.88*diffuse)+palette(pigment+.3)*(vein*.13*pulse+rim*(.055+membrane*.13));
 colour+=base*(membrane*.43+rooted*.08)*wrapped*(1.-diffuse);
 colour+=palette(pigment+.12)*base*membrane*back*.27;
 }
 if(role>.79&&role<.9)colour=mix(base*.5,palette(pigment+.28)*.9,.64+.12*pulse);
 if(role>.9&&role<.98)colour=mix(vec3(.7,.9,.82),palette(pigment+.18),.3)*(.62+.35*diffuse);
 if(role>.985)colour=vec3(.006,.012,.018)+vec3(.025)*diffuse;
 finalColor=vec4(colour,1.);}
)GLSL";
}
