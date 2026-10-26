#pragma once
#include "../worldgen_glsl.hpp"
#include "surface_detail.hpp"
#include "lighting.hpp"
#include "water.hpp"
#include "architecture.hpp"
#include "weather.hpp"
// The same bounded, direction-anchored silk field colours space and materials.
// It has no longitude seam and never moves geography or flight instruments.
#define PRISM_PIGMENT_GLSL R"GLSL(
vec3 pigment(float h){return pow(.5+.5*cos(6.283185*(h+vec3(0.,.68,.34))),vec3(1.35));}
float thread(float p,float width){float span=fwidth(p),aa=clamp(span*.8,.002,.75),resolved=1.-smoothstep(.65,1.75,span);float line=1.-smoothstep(width,width+aa,abs(sin(p)));float coverage=2.*asin(clamp(width,0.,1.))/3.14159265;return mix(coverage,line,resolved);}
vec3 silk(vec3 direction,float phase,float seconds){
 float t=seconds*.045;vec3 p=direction*3.2;
 vec3 motion=vec3(sin(t*.31),cos(t*.27),sin(t*.19+1.7));
 vec3 q=p+.20*sin(p.yzx*2.8+motion*.7+phase);
 q+=.065*sin(q.zxy*6.8-motion*.5);
 float fold=q.y*3.7+sin(q.x*3.4+motion.x*.5)*1.7+.38*sin(q.z*5.-q.y*2.+motion.y*.5);
 float crossFold=q.z*4.2+sin(q.y*3.6-motion.z*.5)*1.4;
 vec3 front=pigment(fold*.18+phase+sin(t*.11)*.045),back=pigment(crossFold*.15+phase+.36+sin(t*.13+2.)*.035);
 float foldResolved=1.-smoothstep(1.,3.,fwidth(fold)),crossResolved=1.-smoothstep(1.,3.,fwidth(crossFold));float sheet=mix(.384,pow(.5+.5*sin(fold),1.9),foldResolved),rear=mix(.3125,pow(.5+.5*sin(crossFold),3.),crossResolved);
 float fine=thread(fold*17.+.6*sin(crossFold*2.),.035),lace=thread(fold*43.+sin(crossFold*3.1),.018);
 float crest=thread(fold+.15,.055),shade=mix(.55,.28+.72*pow(.5+.5*cos(fold+.8),2.),foldResolved);
 // Lift the broad pigment sheets before the fine fibres: colour stays rich
 // at distance without making subpixel threads brighter or less stable.
 return front*(.07+sheet*shade*.66+fine*.28+lace*.09)+back*rear*.26+mix(front,vec3(.35,1.,.82),.2)*crest*.42;
}
)GLSL"
namespace hyperion::flight {
inline constexpr const char* skyFragment=R"GLSL(#version 330
in vec2 fragTexCoord;
out vec4 finalColor;
uniform vec2 resolution;
uniform vec3 forward,up,right,radial,lightDirection;
uniform float time,air,seed,altitude,storm,lightning,lowGraphics;
float hash(vec3 p){p=fract(p*.1031);p+=dot(p,p.yzx+33.33);return fract((p.x+p.y)*p.z);}
uniform sampler2D noiseTexture;
float noise(vec3 p){vec3 cell=floor(p),f=fract(p);f=f*f*(3.-2.*f);vec2 uv=cell.xy+vec2(37.,17.)*cell.z+f.xy;vec2 rg=textureLod(noiseTexture,(uv+.5)/256.,0.).rg;return mix(rg.r,rg.g,f.z);}
float dust(vec3 p){return noise(p)*.57+noise(p*2.03+7.1)*.29+noise(p*4.09+13.4)*.14;}
)GLSL" PRISM_PIGMENT_GLSL R"GLSL(
vec3 stars(vec3 d,float density,float layer){
 // Cubical projection avoids the pinched star-field poles of a latitude atlas.
 vec3 axis=abs(d);vec2 uv;float face;
 if(axis.x>axis.y&&axis.x>axis.z){uv=d.yz/axis.x;face=d.x>0.?0.:1.;}
 else if(axis.y>axis.z){uv=d.xz/axis.y;face=d.y>0.?2.:3.;}
 else{uv=d.xy/axis.z;face=d.z>0.?4.:5.;}
 vec2 grid=(uv*.5+.5)*density,cell=floor(grid),q=fract(grid);layer+=face*17.;
 float h=hash(vec3(cell,layer)),bright=step(.989,h);
 vec2 centre=vec2(hash(vec3(cell,layer+2.)),hash(vec3(cell,layer+4.)))*.7+.15;
 vec2 delta=q-centre;
 float aa=clamp(length(fwidth(grid))*.43,.018,.16),r=length(delta);
 float point=(1.-smoothstep(aa*.15,aa*1.1,r))*step(.94,h);
 float halo=exp(-r*r*115.)*bright*.22;
 float spike=(exp(-abs(delta.x)*190.)*exp(-abs(delta.y)*12.)+exp(-abs(delta.y)*190.)*exp(-abs(delta.x)*12.))*bright*.12;
 vec3 temperature=mix(vec3(.49,.69,1.),vec3(1.,.78,.47),hash(vec3(cell,layer+8.)));
 return temperature*(point*(.4+bright*1.4)+halo+spike);
}
void main(){
 vec2 uv=gl_FragCoord.xy/resolution*2.-1.;uv.x*=resolution.x/resolution.y;
 vec3 d=normalize(forward+right*uv.x*.7002+up*uv.y*.7002);
 vec3 q=d*3.1+vec3(seed*.013,0,seed*.007);
 float wisps=dust(q*1.7),band=exp(-pow((d.y*.75+d.x*.31+d.z*.12+.09+sin(d.z*3.)*.1)*3.8,2.));
 float cloud=pow(max(0.,wisps-.23),2.1)*band;
 vec3 c=vec3(.004,.007,.016);
 float farMist=dust(q+19.),nearDust=noise(q*3.1+3.);
 c+=mix(vec3(.018,.12,.18),vec3(.20,.032,.14),smoothstep(.38,.68,farMist))*cloud*5.4;
 float darkLane=smoothstep(.39,.62,nearDust);c*=1.-darkLane*band*.68;
 float illuminatedEdge=smoothstep(.46,.66,wisps)*(1.-smoothstep(.55,.80,darkLane))*band;
 c+=mix(vec3(.015,.055,.09),pigment(seed*.031+d.y*.23+d.z*.18)*.055,.55)*illuminatedEdge;
 // Fine spectral fibres remain as an accent through the quiet nebular volume.
 float phase=q.x*2.+sin(q.y*2.7)+cos(q.z*2.3);
 float fibre=1.-smoothstep(.015,max(.016,fwidth(phase*23.)),abs(sin(phase*23.)));
 c+=pigment(phase*.18+seed*.031)*fibre*cloud*.65;
 c+=silk(d,fract(seed*.0371),time)*(cloud*2.55+band*.065);
 if(lowGraphics<.5){
 // Nested, direction-anchored volumes give the silk a dark foreground and
 // luminous depth. No screen-space noise or camera-relative star motion.
 // Reuse the existing gas and dust samples: layering costs no extra noise
 // lookups at full framebuffer resolution.
 float cavity=smoothstep(.44,.72,nearDust)*band;
 float emission=smoothstep(.34,.76,farMist)*band;
 vec3 gasColour=mix(vec3(.025,.13,.22),pigment(seed*.031+farMist*.48+d.z*.12)*.28,.65);
 c*=exp(-cavity*1.65);
 c+=gasColour*emission*(.18+wisps*.72)*(1.-cavity*.55);
 float rim=smoothstep(.38,.57,nearDust)*(1.-smoothstep(.57,.72,nearDust))*emission;
 c+=pigment(seed*.031+farMist*.36+.12)*rim*.095;
 }
 c+=vec3(.09,.12,.20)*pow(band,2.)*wisps*.035;
 c+=(stars(d,115.,1.)+stars(d,245.,11.)*.40)*exp(-air*10.*smoothstep(-.12,.18,dot(radial,lightDirection)));
 float horizon=dot(d,radial),sun=max(0.,dot(d,lightDirection));
 float sunElevation=dot(radial,lightDirection);
 float dusk=1.-smoothstep(.02,.40,abs(sunElevation));
 vec3 zenith=mix(vec3(.025,.075,.19),vec3(.08,.035,.14),dusk*.5);
 vec3 horizonColour=mix(vec3(.29,.46,.55),vec3(.66,.19,.095),dusk);
 vec3 aerial=mix(horizonColour,zenith,smoothstep(-.12,.72,horizon));
 aerial+=vec3(.85,.38,.14)*pow(sun,14.)*(.14+dusk*.24);
 aerial+=pigment(seed*.031+horizon*.16)*exp(-abs(horizon)*11.)*.024;
 if(lowGraphics>.5){
 aerial=mix(vec3(.11,.22,.34),vec3(.018,.06,.13),smoothstep(-.1,.8,horizon));
 aerial+=vec3(.65,.26,.10)*pow(sun,8.)*.35;
 aerial+=vec3(.16,.27,.38)*exp(-abs(horizon)*8.);
 }
 aerial*=mix(.035,1.,smoothstep(-.12,.18,dot(radial,lightDirection)));
 aerial*=1.-storm*.38;
 c=mix(c,aerial+c*.012,air);
 c+=vec3(.45,.32,.72)*lightning*air*.16;
 // Below the weather shell, local magnetic curtains live in the sky field.
 float night=1.-smoothstep(-.24,.12,dot(radial,lightDirection));
 float polar=smoothstep(.36,.77,abs(radial.y));
 float arc=horizon-(.30+.11*sin(d.x*6.+d.z*4.+sin(time*.017)));
 float curtain=exp(-arc*arc*160.)*(.35+.65*pow(.5+.5*sin(d.x*41.+d.z*37.+sin(time*.031)*2.),2.));
 c+=pigment(d.x*.42+d.z*.35+seed*.031+arc*1.6)*curtain*night*polar*air*(1.-smoothstep(50.,140.,altitude))*.48;
 c+=vec3(1.,.82,.57)*(pow(sun,5400.)*3.2+pow(sun,140.)*.24+pow(sun,14.)*.028)*(1.-air*.35);
 finalColor=vec4(c,1.);
})GLSL";
inline constexpr const char* worldVertex=R"GLSL(#version 330
in vec3 vertexPosition,vertexNormal;
in vec2 vertexTexCoord;
in vec4 vertexColor;
uniform mat4 mvp,matModel,matNormal;
uniform float groundPass,groundBlend;uniform vec3 eye;
out vec3 worldPosition,normal,localPosition;out vec2 modelUV;
out vec4 tint;
void main(){
 vec3 position=vertexPosition;
#ifdef TERRAIN_DETAIL
 if(groundPass>.5&&groundPass<1.5)position=normalize(position)*mix(vertexTexCoord.x,length(position),groundBlend);
 if(groundPass>1.5){float range=length(eye-(matModel*vec4(position,1)).xyz),fade=(1.-smoothstep(95.,120.,range))*groundBlend;position=normalize(position)*mix(vertexTexCoord.x-.16,length(position),fade);}
#endif
 worldPosition=(matModel*vec4(position,1)).xyz;normal=normalize(mat3(matNormal)*vertexNormal);localPosition=position;modelUV=vertexTexCoord;tint=vertexColor;gl_Position=mvp*vec4(position,1);
#ifdef CLAMP_NEAR
 if(gl_Position.w>0.)gl_Position.z=max(gl_Position.z,-gl_Position.w);
#endif
}
)GLSL";
inline constexpr const char* planetDepthFragment=R"GLSL(#version 330
in vec3 localPosition;out vec4 finalColor;
uniform vec3 patchNormal;uniform float patchCos,groundPass;
void main(){if(patchCos>0.){float cap=dot(normalize(localPosition),patchNormal)-patchCos;if(groundPass<.5&&cap>0.)discard;if(groundPass>.5&&groundPass<1.5&&cap<0.)discard;}finalColor=vec4(1./gl_FragCoord.w);}
)GLSL";
inline constexpr const char* worldFragment=R"GLSL(#version 330
in vec3 worldPosition,normal,localPosition;
in vec2 modelUV;
in vec4 tint;
out vec4 finalColor;
uniform vec3 centre,eye,lightDirection;
uniform float time,seed,metal,water,life,kind,atmosphere,radius,temperature,ringed,lowGraphics;
uniform vec3 patchNormal;uniform float patchCos,groundPass;
uniform vec3 gearPosition;uniform float groundClearance;
uniform sampler2D noiseTexture,surfaceTexture,geologyTexture;uniform float useSurface,useGeology;
float hash(vec3 p){p=fract(p*.1031);p+=dot(p,p.yzx+33.33);return fract((p.x+p.y)*p.z);}
float noise(vec3 p){vec3 cell=floor(p),f=fract(p);f=f*f*(3.-2.*f);vec2 uv=cell.xy+vec2(37.,17.)*cell.z+f.xy;vec2 rg=textureLod(noiseTexture,(uv+.5)/256.,0.).rg;return mix(rg.r,rg.g,f.z);}
float fbm(vec3 p){float v=0.,a=.52;for(int i=0;i<6;i++){v+=a*noise(p);p=p*2.03+vec3(7.1,13.7,19.3);a*=.49;}return v;}
vec4 surface(vec3 n){
 vec2 uv=vec2(atan(n.z,n.x)/6.283185,acos(clamp(n.y,-1.,1.))/3.14159265),size=vec2(textureSize(surfaceTexture,0));
 uv.y=uv.y*((size.y-1.)/size.y)+.5/size.y;vec2 dx=dFdx(uv),dy=dFdy(uv);dx.x-=round(dx.x);dy.x-=round(dy.x);
 vec4 value=textureGrad(surfaceTexture,uv,dx,dy);
#ifdef TERRAIN_DETAIL
 // Mix the four cached elevation values in float precision near the ground.
 // Half-float filtering can otherwise expose contour steps in narrow surf.
 float closeGrid=1.-smoothstep(.65,1.3,max(length(dx*size),length(dy*size)));
 if(closeGrid>.001&&useSurface>.5){
  vec2 p=uv*size-.5,f=fract(p);ivec2 base=ivec2(floor(p)),s=ivec2(size);
  int x=(base.x%s.x+s.x)%s.x,y=clamp(base.y,0,s.y-1),nextX=(x+1)%s.x,nextY=min(y+1,s.y-1);
  float a=texelFetch(surfaceTexture,ivec2(x,y),0).r,b=texelFetch(surfaceTexture,ivec2(nextX,y),0).r;
  float c=texelFetch(surfaceTexture,ivec2(x,nextY),0).r,d=texelFetch(surfaceTexture,ivec2(nextX,nextY),0).r;
  value.r=mix(value.r,mix(mix(a,b,f.x),mix(c,d,f.x),f.y),closeGrid);
 }
#endif
 return value;
}
vec4 geology(vec3 n){vec2 uv=vec2(atan(n.z,n.x)/6.283185,acos(clamp(n.y,-1.,1.))/3.14159265);float rows=float(textureSize(geologyTexture,0).y);uv.y=uv.y*((rows-1.)/rows)+.5/rows;vec2 dx=dFdx(uv),dy=dFdy(uv);dx.x-=round(dx.x);dy.x-=round(dy.x);return textureGrad(geologyTexture,uv,dx,dy);}
)GLSL" PRISM_PIGMENT_GLSL PRISM_SURFACE_DETAIL_GLSL PRISM_RING_LIGHT_GLSL PRISM_NEAR_SUN_GLSL PRISM_WATER_GLSL R"GLSL(
vec3 spectrum(float x){return .5+.5*cos(6.283185*(x+vec3(0,.67,.33)));}
vec3 bump(vec3 n,float height,float strength){
#ifdef TERRAIN_DETAIL
 vec3 dx=dFdx(localPosition),dy=dFdy(localPosition);
#else
 vec3 dx=dFdx(worldPosition),dy=dFdy(worldPosition);
#endif
 vec3 r1=cross(dy,n),r2=cross(n,dx);float det=dot(dx,r1);return normalize(abs(det)*n-sign(det)*(dFdx(height)*r1+dFdy(height)*r2)*strength);}
vec3 brdf(vec3 albedo,vec3 N,vec3 V,vec3 L,float rough,float metallic){
 vec3 H=normalize(V+L);float nv=max(dot(N,V),.001),nl=max(dot(N,L),0.),nh=max(dot(N,H),0.),vh=max(dot(V,H),0.);
 float a=rough*rough,a2=a*a,denom=nh*nh*(a2-1.)+1.;float D=a2/max(3.14159*denom*denom,.00001);
 float k=(rough+1.)*(rough+1.)/8.;float G=nv/(nv*(1.-k)+k)*nl/(nl*(1.-k)+k);
 vec3 F0=mix(vec3(.035),albedo,metallic),F=F0+(1.-F0)*pow(1.-vh,5.);
 return ((1.-F)*(1.-metallic)*albedo/3.14159+F*D*G/max(4.*nv*max(nl,.001),.001))*nl*3.8;
}
float clouds(vec3 n){float t=time*(.0009+sin(n.y*7.)*.00018);n.xz=mat2(cos(t),-sin(t),sin(t),cos(t))*n.xz;vec3 q=n*7.5+seed*.07;q+=sin(q.yzx*.8+sin(q.zxy*.7))*1.2;float field=useSurface>.5?surface(n).a:fbm(q);return smoothstep(.43,.67,field)*clamp(.22+water*.74,.2,.95);}
void main(){
 vec3 N=normalize(normal),V=normalize(eye-worldPosition),L=normalize(lightDirection);float light=dot(N,L),day=smoothstep(-.10,.18,light),fresnel=pow(1.-max(0.,dot(N,V)),4.);
 if(metal>.5){
  vec2 uv=modelUV*vec2(8.,12.);vec2 cell=fract(uv),edge=min(cell,1.-cell);float seams=1.-smoothstep(.012,.035,min(edge.x,edge.y));
  float grain=noise(localPosition*65.+seed),panelNoise=hash(vec3(floor(uv),seed));
  vec3 base=vec3(.11,.16,.20)*(.86+panelNoise*.16)*(1.-seams*.68);
  float rough=.42+panelNoise*.13;
  if(metal>1.5&&metal<2.5){base=mix(vec3(.009,.025,.066),vec3(.032,.075,.14),grain)*(1.-seams*.8);rough=.20;}
  base*=.70+pigment(seed*.071+dot(N,V)*.38)*.60;
  N=bump(N,grain*.025+seams*.05,.38);
  vec3 c=brdf(base,N,V,L,rough,.75)+base*vec3(.20,.29,.37)+spectrum(seed*.09)*fresnel*.035;
  if(metal>2.5){float window=step(.16,cell.x)*step(cell.x,.71)*step(.28,cell.y)*step(cell.y,.69)*step(.36,panelNoise);c+=mix(vec3(.12,.8,.9),vec3(1.,.50,.16),step(.55,panelNoise))*window*(.13+(1.-day)*1.1);}
  c+=pigment(seed*.09+dot(N,V)*.6)*(fresnel*.17+seams*.014)*(.3+day*.7);finalColor=vec4(c,1.);return;
 }
 vec3 n=normalize(localPosition),q=n*3.2+vec3(seed*.17,seed*.073,seed*.11);
 float pixelAngle=max(length(dFdx(n)),length(dFdy(n))),normalResolved=1.-smoothstep(.006,.035,pixelAngle);
 if(patchCos>0.){float cap=dot(n,patchNormal)-patchCos;if(groundPass<.5&&cap>0.)discard;if(groundPass>.5&&groundPass<1.5&&cap<0.)discard;}
 vec4 data=surface(n);float elevation=useSurface>.5?data.r:fbm(q+noise(q*1.9)*.6),detail=useSurface>.5?data.g:fbm(q*27.);
 float sea=mix(.28,.72,water),land=smoothstep(sea-.008,sea+.013,elevation),latitude=abs(n.y);
 float ridge=useSurface>.5?data.b:pow(1.-abs(noise(q*23.)*2.-1.),2.);
 float mountains=smoothstep(sea+.055,sea+.23,elevation),snowline=clamp(.79+(temperature-285.)*.0027,.18,.96),ice=smoothstep(snowline,snowline+.14,latitude+(elevation-.5)*.24);
 vec3 ocean=mix(vec3(.004,.019,.055),vec3(.012,.22,.23),pow(smoothstep(sea-.16,sea,elevation),2.));
 vec3 soil=mix(vec3(.16,.105,.052),vec3(.042,.15,.080),clamp(life,0.,1.));
 vec3 ground=mix(soil,vec3(.24,.22,.20),mountains);ground*=.68+detail*.52+ridge*mountains*.2;
 float sand=exp(-pow((elevation-sea-.017)*65.,2.));ground=mix(ground,vec3(.33,.29,.18),sand*.65);
 if(kind>.5&&kind<1.5){ground=mix(vec3(.14,.055,.023),vec3(.47,.26,.11),elevation)*(.7+detail*.65);ice*=.18;}
 if(kind>1.5&&kind<2.5){ground=mix(vec3(.07,.16,.24),vec3(.52,.72,.80),elevation)*(.7+detail*.5);ocean=vec3(.008,.04,.10);ice=max(ice,.45);}
 if(kind>2.5&&kind<3.5){ground=mix(vec3(.018,.021,.025),vec3(.13,.063,.035),elevation)*(.6+detail*.7);ice=0.;}
 if(kind>3.5&&kind<4.5){ground=mix(vec3(.055,.095,.027),vec3(.33,.30,.10),elevation);ocean=vec3(.018,.075,.035);}
 vec4 geo=useGeology>.5?geology(n):vec4(ground,.5);
 ground=mix(ground,geo.rgb,.72);
 float shelf=smoothstep(sea-.10,sea-.008,elevation),reef=shelf*life*smoothstep(.42,.65,detail);
 ocean=mix(ocean,vec3(.018,.25,.25),shelf*.45);ocean+=vec3(.015,.10,.065)*reef;
 // Reintroduce the expedition renderer's mineral pigments and flowing oceans.
 // Illumination, roughness, relief and biome masks still give them volume.
 vec3 flowColour=silk(n,fract(seed*.0371),time);
 vec3 mineral=pigment(elevation*3.7+seed*.031+sin(n.x*13.+sin(n.z*6.))*.13);
 ground=mix(ground,ground*.42+mineral*.27+flowColour*.28,.56);
 ocean=ocean*.72+flowColour*.215+pigment(elevation*2.3+seed*.031)*.012;
 vec3 iceColour=vec3(.62,.76,.80);if(kind>1.5&&kind<2.5&&useGeology>.5)iceColour=mix(iceColour,geo.rgb*1.18,.55);
 vec3 albedo=mix(ocean,ground,land);albedo=mix(albedo,iceColour,ice);
 LiquidSurface liquid=liquidSurface(n,dFdx(n*radius),dFdy(n*radius),elevation,sea,land,ice,fwidth(elevation));
 if(liquid.coverage>.001){
  float depth=max(sea-elevation,0.)*180.,shallow=exp(-depth*.11);
  vec3 absorption=kind>3.5?vec3(.18,.045,.13):vec3(.22,.075,.05);
  vec3 transmission=exp(-depth*absorption),bed=mix(vec3(.055,.17,.14),geo.rgb*.60,.45);
  if(kind>3.5)bed=mix(bed,vec3(.13,.22,.045),.55);
  vec3 below=mix(ocean,bed,transmission);
  albedo=mix(albedo,mix(albedo,below,.48)+vec3(.07,.13,.11)*liquid.caustic*shallow*day,liquid.coverage);
 }
 float nearDetail=0.;
#ifdef TERRAIN_DETAIL
 nearDetail=(1.-smoothstep(48.,140.,length(eye-worldPosition)))*step(kind,4.5);
#endif
 vec3 grit=localPosition+seed*.13;float microHeight=0.,microVein=0.,cavity=1.,roughness=mix(.13,.78,land);vec3 microGlow=vec3(0.),sheenColour=vec3(0.);
#ifdef TERRAIN_DETAIL
 vec3 localDx=dFdx(localPosition),localDy=dFdy(localPosition);
 if(nearDetail>.005&&(land>.005||groundPass>1.5)){
  LocalMaterial m=closeMaterial(grit,n,N,localDx,localDy,geo.rgb,flowColour);
  if(groundPass>1.5){land=1.;albedo=mix(m.colour,albedo,.30)*tint.rgb;}else albedo=mix(albedo,mix(m.colour,albedo,.55),nearDetail*land*.90);
  microHeight=m.height*nearDetail;microVein=m.vein;cavity=mix(1.,m.cavity,nearDetail*land);
  roughness=mix(roughness,m.roughness,nearDetail*land);microGlow=m.glow;
  sheenColour=pigment(seed*.031+m.height*1.2+dot(N,V)*.36)*(.035+m.vein*.065);
  float wet=liquid.wet;
  albedo*=1.-wet*nearDetail*land*.32;roughness=mix(roughness,.25,wet*nearDetail*.72);
 }
#endif
 // Keep the height field continuous across the land mask. Multiplying height
 // by coverage created a false cliff and stippled normals at shorelines.
 float materialHeight=elevation+ridge*mountains*.12+(geo.a-.5)*.22;
 cavity*=1.-(1.-smoothstep(.25,.46,geo.a))*land*.14;
 roughness=clamp(roughness+(geo.a-.5)*land*.16,.12,.94);
 N=bump(N,materialHeight,mix(.6,9.,land)*normalResolved*mix(1.,.24,nearDetail));
 if(nearDetail>.005)N=bump(N,microHeight,land*1.6);
 // Submerged terrain relief must give way before the broad colour blend
 // reaches pure water; otherwise half-float geology gradients form terraces
 // in a grazing reflection right at the shoreline.
 float liquidNormal=smoothstep(.005,.32,liquid.coverage);
 N=normalize(mix(N,liquid.normal,liquidNormal));roughness=mix(roughness,liquid.roughness,liquidNormal);
 if(kind>4.5){
  vec3 flow=q*3.;flow.x+=sin(n.y*25.)*.6;float turbulence=useSurface>.5?data.b:fbm(flow*3.);
  float bands=sin(n.y*75.+noise(q*2.)*6.5+sin(n.x*11.)*1.3+turbulence*4.2);
  vec3 a=pigment(seed*.031+.02)*.22+.003,b=pigment(seed*.031+.23)*.44+.006;
  albedo=mix(a,b,smoothstep(-.8,.8,bands)*.72+turbulence*.28)+flowColour*.12;land=1.;ice=0.;N=normalize(normal);
  vec2 st=vec2(atan(n.z,n.x)-(.65+sin(seed)*.5),(n.y+.23)*3.4);float r=length(st),mask=exp(-r*r*38.);
  albedo=mix(albedo,pigment(r*3.7+seed*.031+.5)*(.20+.24*(.5+.5*sin(r*120.+atan(st.y,st.x+.00001)*2.))),mask*.7);
  if(useGeology>.5)albedo=mix(albedo,geo.rgb+flowColour*.10,.62);roughness=.82;
 }
 float shadow=1.;if(kind<4.5&&atmosphere>.12)shadow=1.-clouds(normalize(n+L*(30./radius)))*.48*day;
 shadow*=ringVisibility(localPosition,L,radius,ringed,seed);
#ifdef TERRAIN_DETAIL
 shadow*=nearSunVisibility(localPosition,normalize(normal),L);
#endif
 // Broaden a highlight by the normal variation within a pixel. Fine water
 // and rock relief then contributes a smooth reflection, rather than glints.
 vec3 ndx=dFdx(N),ndy=dFdy(N);float variance=min(.25,max(dot(ndx,ndx),dot(ndy,ndy))*.5);
 roughness=max(pow(min(1.,pow(roughness,4.)+variance),.25),mix(.13,.52,smoothstep(.002,.025,pixelAngle)));
 vec3 c=brdf(albedo,N,V,L,roughness,0.)*shadow*cavity;
 float skyFacing=clamp(dot(N,n)*.5+.5,0.,1.);
 float airAmount=clamp(atmosphere,0.,1.),sunElevation=dot(n,L);
 float dusk=1.-smoothstep(.03,.36,abs(sunElevation));
 if(lowGraphics>.5){
 vec3 skyTint=vec3(.045,.075,.105)*(1.+clamp(atmosphere,0.,1.)*.22)+pigment(seed*.031+n.z*.18)*clamp(atmosphere,0.,1.)*.008;
 c+=albedo*mix(vec3(.044,.033,.026),skyTint,skyFacing)*cavity;
 }else{
 vec3 skyTint=mix(vec3(.025,.044,.075),vec3(.07,.13,.22),airAmount)*(.25+.75*day);
 skyTint+=pigment(seed*.031+n.z*.18)*airAmount*(.012+dusk*.015)*day;
 vec3 groundBounce=mix(vec3(.035,.022,.016),albedo*.11,airAmount)*(.2+.8*day);
 c+=albedo*mix(groundBounce,skyTint,skyFacing)*cavity;
 // Broad diffuse retroreflection reveals dusty relief without turning rock
 // into polished metal. Its footprint is already filtered by nearDetail.
 float backscatter=pow(clamp(dot(V,L),0.,1.),4.);
 c+=albedo*land*nearDetail*backscatter*.055*day*shadow*cavity;
 }
 c+=sheenColour*fresnel*nearDetail*land;
 c+=microGlow*nearDetail*land+pigment(seed*.031+fresnel*.4)*microVein*nearDetail*land*(.018+fresnel*.16);
 if(nearDetail>.005&&groundClearance<40.){vec3 foot=worldPosition-gearPosition;foot-=n*dot(foot,n);float r=3.2+max(0.,groundClearance)*.22;float contactShadow=exp(-dot(foot,foot)/(r*r))*(1.-smoothstep(1.,38.,groundClearance));c*=1.-contactShadow*.35;}
 c+=vec3(.008,.028,.045)*(1.-land)*day*.35;
 float trace=thread(elevation*420.+detail,.015);
 c+=pigment(elevation*3.+seed*.031)*trace*land*.072*day;
 c+=pigment(elevation*2.3+seed*.031+fresnel*.25)*fresnel*.11*day;
 float shore=exp(-abs(elevation-sea)*190.);
 if(liquid.coverage>.001){
  float waterFresnel=.02+.98*pow(1.-clamp(dot(N,V),0.,1.),5.);
  vec3 reflected=liquidReflection(N,V,n,L,flowColour,smoothstep(-.15,.18,dot(n,L)));
  c=mix(c,c*.3+reflected,liquid.coverage*waterFresnel*.82);
 }
 c+=mix(vec3(.30,.40,.37),mineral*.30+.12,.18)*liquid.foam*(.08+.58*day)*shadow;
 if(kind>1.5&&kind<2.5)c+=pigment(seed*.031+geo.a*.6)*(1.-smoothstep(.27,.40,geo.a))*(.012+fresnel*.05)*day;
 float currents=thread(elevation*165.+detail*2.+sin(time*.035)*.6,.035);
 c+=mix(vec3(.015,.28,.23),pigment(elevation*4.+seed*.031),.6)*life*(1.-day)*(shore*.35+currents*(1.-land)*.042);
 if(kind>2.5&&kind<3.5){float lavaFootprint=max(length(dFdx(q*12.)),length(dFdy(q*12.)));float lava=mix(.028,pow(max(0.,1.-abs(noise(q*12.)-.5)*18.),5.),1.-smoothstep(.35,1.4,lavaFootprint))*smoothstep(.48,.64,elevation);float rift=(1.-smoothstep(.31,.41,geo.a))*smoothstep(.43,.61,elevation);c+=mix(vec3(2.3,.16,.009),pigment(seed*.031+.1)*1.3,.18)*(lava+rift*.16);}
 float eyeHeight=length(eye-centre)-radius;
 if(lowGraphics>.5){
 float haze=(1.-exp(-length(eye-worldPosition)*.0024*clamp(atmosphere,0.,1.4)))*(1.-smoothstep(60.,200.,eyeHeight));
 vec3 airColour=mix(vec3(.012,.022,.045),vec3(.16,.25,.34),day)+vec3(.12,.055,.025)*pow(1.-abs(dot(n,L)),7.)*day;
 c=mix(c,airColour,clamp(haze,0.,.64));finalColor=vec4(c,1.); }else{
 float airPath=length(eye-worldPosition)*.0020*clamp(atmosphere,0.,1.4)*(1.-smoothstep(60.,200.,eyeHeight));
 vec3 transmission=exp(-vec3(.72,1.05,1.55)*airPath);
 vec3 airColour=mix(vec3(.006,.012,.03),mix(vec3(.19,.32,.46),vec3(.52,.19,.12),dusk*.65),day);
 airColour+=pigment(seed*.031+n.z*.18)*dusk*day*.018;
 c=c*transmission+airColour*(1.-transmission);finalColor=vec4(c,1.); }
})GLSL";
inline constexpr const char* dustFragment=R"GLSL(#version 330
in vec2 fragTexCoord;in vec4 fragColor;out vec4 finalColor;
void main(){vec2 p=fragTexCoord*2.-1.;float r=dot(p,p);if(r>1.)discard;float density=pow(1.-r,2.);finalColor=vec4(fragColor.rgb*.65,fragColor.a*density);}
)GLSL";
inline constexpr const char* surfaceBakeFragment=R"GLSL(#version 330
out vec4 finalColor;
uniform sampler2D noiseTexture;uniform vec2 resolution;uniform float seed,kind,water;
)GLSL" HYPERION_WORLDGEN_GLSL R"GLSL(
float noise(vec3 p){vec3 cell=floor(p),f=fract(p);f=f*f*(3.-2.*f);vec2 uv=cell.xy+vec2(37.,17.)*cell.z+f.xy;vec2 rg=textureLod(noiseTexture,(uv+.5)/256.,0.).rg;return mix(rg.r,rg.g,f.z);}
float fbm(vec3 p){float v=0.,a=.52;for(int i=0;i<6;i++){v+=a*noise(p);p=p*2.03+vec3(7.1,13.7,19.3);a*=.49;}return v;}
float field(vec3 q){return noise(q)*.52+noise(q*2.07+13.)*.25+noise(q*4.13+9.)*.13+noise(q*8.29+21.)*.07+noise(q*16.61+7.)*.03;}
void main(){
 vec2 uv=gl_FragCoord.xy/resolution;float phi=uv.x*6.283185,theta=(uv.y-.5/resolution.y)*(resolution.y/(resolution.y-1.))*3.14159265;
 vec3 n=vec3(sin(theta)*cos(phi),cos(theta),sin(theta)*sin(phi)),q=n*3.2+vec3(seed*.17,seed*.073,seed*.11);
 float elevation=fbm(q+noise(q*1.9)*.6),detail=fbm(q*27.),ridge=pow(1.-abs(noise(q*23.)*2.-1.),2.);
 if(generation>1.5&&kind<4.5)elevation=pnElevation(n)+(.28+.44*water)-(.36+.285*water);
 if(kind>4.5){vec3 flow=q*3.;flow.x+=sin(n.y*25.)*.6;ridge=fbm(flow*3.);}
 // Broad fronts, folded wind lanes and two seeded cyclonic systems. The field
 // is spherical, so neither the longitude seam nor the poles repeat a tile.
 vec3 cloud=n*7.5+seed*.07;cloud+=sin(cloud.yzx*.8+sin(cloud.zxy*.7))*1.2;
 float weather=field(cloud);
 for(int i=0;i<2;i++){
  float phase=seed*1.73+float(i)*3.1;vec3 axis=normalize(vec3(sin(phase),sin(phase*1.31)*.65,cos(phase)));
  vec3 east=normalize(cross(axis,vec3(0,1,0))),north=cross(axis,east);
  vec2 p=vec2(dot(n,east),dot(n,north));float r=length(p),angle=atan(p.y,p.x),facing=smoothstep(.78,.95,dot(n,axis));
  float arms=.5+.5*sin(angle*2.+r*33.+noise(n*48.+seed)*1.4);
  float storm=.46+arms*.17+noise(n*90.+seed)*.05;
  storm-=exp(-r*r*1700.)*.21;weather=mix(weather,storm,facing*(1.-smoothstep(.26,.48,r))*.85);
 }
 finalColor=vec4(elevation,detail,ridge,weather);
})GLSL";
inline constexpr const char* structureFragment=R"GLSL(#version 330
#define TERRAIN_DETAIL
in vec3 worldPosition,normal,localPosition;
in vec2 modelUV;in vec4 tint;out vec4 finalColor;
uniform mat4 matModel;
uniform vec3 eye,lightDirection,accent,shadowCentre,skyRadial;
uniform sampler2D buildingTexture,buildingDetailTexture;
uniform vec4 buildingDetailMean;
uniform float time,condition,pollution,ecology,powerOn,activity[14],structureAir,materialStyle,craftScale,craftIdentity,serviceWear;
)GLSL" PRISM_PIGMENT_GLSL PRISM_NEAR_SUN_GLSL PRISM_ARCHITECTURE_GLSL;
inline constexpr const char* cloudFragment=R"GLSL(#version 330
in vec3 worldPosition,normal;
out vec4 finalColor;
uniform float lowGraphics;
uniform vec3 centre,eye,lightDirection;
uniform float time,seed,water,kind,radius,atmosphere,sampleCount,ringed,weatherHours,weatherSeed;
uniform vec3 viewForward;
uniform vec2 resolution;
uniform sampler2D noiseTexture,surfaceTexture,opaqueDepth;uniform float useSurface;
float noise(vec3 p){vec3 cell=floor(p),f=fract(p);f=f*f*(3.-2.*f);vec2 uv=cell.xy+vec2(37.,17.)*cell.z+f.xy;vec2 rg=textureLod(noiseTexture,(uv+.5)/256.,0.).rg;return mix(rg.r,rg.g,f.z);}
vec2 sphere(vec3 o,vec3 d,float r){float b=dot(o,d);vec3 perpendicular=cross(o,d);float h=r*r-dot(perpendicular,perpendicular);if(h<0.)return vec2(-1e8);h=sqrt(h);return vec2(-b-h,-b+h);}
vec3 wind(vec3 n,float height){float t=time*(.0009+sin(n.y*7.)*.00018+height*.00025);n.xz=mat2(cos(t),-sin(t),sin(t),cos(t))*n.xz;return n;}
float weather(vec3 n,float lod){
 if(useSurface<.5){vec3 q=n*7.5+seed*.07;q+=sin(q.yzx*.8+sin(q.zxy*.7))*1.2;return noise(q)*.67+noise(q*2.07+13.)*.33;}
 vec2 uv=vec2(atan(n.z,n.x)/6.283185,acos(clamp(n.y,-1.,1.))/3.14159265);float rows=float(textureSize(surfaceTexture,0).y);uv.y=uv.y*((rows-1.)/rows)+.5/rows;
 return textureLod(surfaceTexture,uv,lod).a;
}
)GLSL" PRISM_PIGMENT_GLSL PRISM_RING_LIGHT_GLSL HYPERION_WEATHER_GLSL R"GLSL(
void main(){
 vec3 o=eye-centre,d=normalize(worldPosition-eye),L=normalize(lightDirection);
 // The proxy supplies coverage only. Stop the volume against the exact
 // opaque surface already drawn with the cockpit's world projection.
 if(gl_FrontFacing)discard;
 vec2 span=sphere(o,d,radius+48.);float start=max(0.,span.x),end=span.y;
 float opaque=texture(opaqueDepth,gl_FragCoord.xy/resolution).r;
 if(opaque>0.)end=min(end,opaque/max(dot(d,viewForward),.0001));
 vec2 ground=sphere(o,d,radius+2.);if(ground.x>0.)end=min(end,ground.x);if(end<=start)discard;
 // Skip the empty space beneath the weather, retaining both tangent limbs.
 vec2 inner=sphere(o,d,radius+20.);float aEnd=end,bStart=end;
 if(inner.y>start&&inner.x<end){aEnd=clamp(inner.x,start,end);bStart=clamp(inner.y,start,end);}
 float first=aEnd-start,total=first+end-bStart;if(total<=.001)discard;
 float ringLight=ringVisibility(o+d*start,L,radius,ringed,seed);
 float count=sampleCount,stepSize=total/count;
 float jitter=fract(52.9829189*fract(dot(floor(gl_FragCoord.xy),vec2(.06711056,.00583715))));
 float transmission=1.,mu=dot(d,L),forward=pow(max(0.,mu),12.);vec3 sum=vec3(0.);
 // Broad storm fronts vary across the ray much more slowly than billows.
 // Sample their occupied midpoint once; fine cloud density still resolves
 // per step, and the exact terrain/depth clipping remains unchanged.
 float middle=total*.5,weatherDistance=middle<first?start+middle:bStart+middle-first;
 float storm=stormField(normalize(o+d*weatherDistance),weatherSeed,weatherHours,atmosphere,water);
 float dischargePulse=pow(smoothstep(.996,1.,sin(weatherHours*55.*.14+weatherSeed)),2.)*pow(storm,4.);
 // Palette endpoints are constant along the ray; interpolate their sheen
 // rather than evaluating three cosines and powers at every volume step.
 vec3 opalLow=pigment(seed*.031+mu*.15),opalMid=pigment(seed*.031+mu*.15+.18),opalHigh=pigment(seed*.031+mu*.15+.36);
 for(int i=0;i<24;i++){
  if(float(i)>=count||transmission<.012)break;
  float along=(float(i)+.45+jitter*.10)*stepSize,t=along<first?start+along:bStart+along-first;
  vec3 p=o+d*t,n=normalize(p);float height=length(p)-radius,h=(height-22.)/18.;
  float lod=clamp(log2(max(1.,t*1.4004/resolution.y/radius*float(textureSize(surfaceTexture,0).y)/3.14159)),0.,8.);
  vec3 wn=wind(n,0.),q=wn*(radius*.095)+seed*.43+n*(height*.12);
  float cover=0.,billow=.5,density=0.,footprint=max(stepSize,t*1.4004/resolution.y),resolved=1.-smoothstep(4.,22.,footprint);
  if(height>22.&&height<43.){
   cover=smoothstep(.43,.67,weather(wn,lod)+storm*.13)*clamp(.22+water*.74,.2,.95);
   if(cover>.025){
    billow=resolved>.005?mix(.5,noise(q),resolved):.5;
    float detail=resolved>.05?mix(.5,noise(q*2.07+19.),resolved):.5;
    h=(height-22.)/(11.+cover*8.+billow*4.);
    float profile=smoothstep(0.,.13,h)*(1.-smoothstep(.48,1.,h));
    float erosion=(1.-billow)*.39+(1.-detail)*.12;
    density=max(0.,cover-erosion)*1.9*profile;
   }
  }
  // A separate, faster wind carries fine ice filaments above the banks.
  if(height>40.&&height<47.){
   float high=(height-40.)/7.;vec3 cirrus=wind(n,1.);float highCover=weather(cirrus,lod+1.);
   float fibreResolved=1.-smoothstep(.3,1.5,footprint*1.8);
   float fibre=fibreResolved>.005?mix(.45,smoothstep(.36,.7,noise(cirrus*vec3(radius*.45,radius*1.8,radius*.45)+seed*.71)),fibreResolved):.45;
   density+=smoothstep(0.,.2,high)*(1.-smoothstep(.5,1.,high))*smoothstep(.49,.64,highCover)*fibre*.11;
  }
  density*=clamp(atmosphere*.8,.0,1.);if(density<.002)continue;
  float sun=dot(n,L),day=smoothstep(-.15,.16,sun),sunHeight=max(.12,sun);
  float top=clamp((40.-height)/18.,0.,1.);
  float lightBillow=resolved>.005?mix(.5,noise(q+L*.65),resolved):.5;float gradient=clamp(.5+(billow-lightBillow)*2.4,0.,1.);
  float selfShadow=exp(-cover*top*1.2/sunHeight)*(.22+.78*gradient);
  // Powder scattering brightens the thin banks, while dense cores retain
  // their shadow. This reuses the existing density field and light sample.
  float powder=1.-exp(-density*stepSize*.28);
  float silver=pow(1.-clamp(density,0.,1.),2.)*forward;
  vec3 ambient=mix(vec3(.014,.025,.048),vec3(.21,.29,.40),day)*(.58+.42*clamp(h,0.,1.))*(.72+billow*.5);
  vec3 sunColour=mix(vec3(1.12,.29,.12),vec3(1.05,.96,.82),smoothstep(.02,.45,sun));
  vec3 colour;
  if(lowGraphics>.5){
  colour=ambient+sunColour*day*selfShadow*ringLight*(.65+max(0.,sun)*.8+forward*2.2);
  }else{
  colour=ambient+sunColour*day*selfShadow*ringLight*(.60+max(0.,sun)*.8+forward*1.7+powder*.34);
  colour+=sunColour*day*ringLight*silver*.19*(.3+.7*selfShadow);
  }
  colour*=1.-storm*.22;
  float discharge=dischargePulse;
  colour+=opalHigh*discharge*.28;
  float rim=pow(1.-clamp(density,0.,1.),3.);float phase=clamp(h*.17+billow*.13,0.,.36);vec3 opal=phase<.18?mix(opalLow,opalMid,phase/.18):mix(opalMid,opalHigh,(phase-.18)/.18);
  colour*=mix(vec3(1.),.68+opal*.70,.33);colour+=opal*day*(rim*forward*.30+pow(max(0.,1.-abs(sun)*3.),3.)*.06);
  if(kind>3.5)colour=mix(colour,colour*vec3(.80,1.,.48),.35);
  float opacity=1.-exp(-density*stepSize*.14);sum+=transmission*colour*opacity;transmission*=1.-opacity;
 }
 finalColor=vec4(sum,1.-transmission);
})GLSL";
// Full-resolution, depth-aware reconstruction keeps the low-resolution volume
// from blending sky samples over foreground terrain and ridge silhouettes.
inline constexpr const char* cloudResolveFragment=R"GLSL(#version 330
out vec4 finalColor;
uniform sampler2D texture0,opaqueDepth;
uniform vec3 eye,localCentre,viewForward,viewRight,viewUp;
uniform float localRadius,emissionPass;
void main(){
 ivec2 size=textureSize(opaqueDepth,0),pixel=ivec2(gl_FragCoord.xy),small=textureSize(texture0,0);
 vec2 uv=gl_FragCoord.xy/vec2(size),screen=uv*2.-1.,p=uv*vec2(small)-.5,f=fract(p);ivec2 base=ivec2(floor(p));
 vec4 a=texelFetch(texture0,clamp(base,ivec2(0),small-1),0),b=texelFetch(texture0,clamp(base+ivec2(1,0),ivec2(0),small-1),0);
 vec4 c=texelFetch(texture0,clamp(base+ivec2(0,1),ivec2(0),small-1),0),e=texelFetch(texture0,clamp(base+ivec2(1,1),ivec2(0),small-1),0);
 vec4 peak=max(max(a,b),max(c,e));float signal=emissionPass>.5?max(peak.r,max(peak.g,peak.b)):peak.a;
 if(signal<=0.){finalColor=vec4(0.);return;}
 float depth=texelFetch(opaqueDepth,pixel,0).r;if(depth<=0.)depth=60000.;
 vec3 o=eye-localCentre;
 // A pilot below the weather cannot see any cloud before leaving the clear
 // inner shell. Evaluate this at full resolution, including thin ridges that
 // can fall between all four low-resolution cloud samples.
 float inner=localRadius+20.;
 if(dot(o,o)<inner*inner){
  vec3 d=normalize(viewForward+viewRight*screen.x*(float(size.x)/float(size.y))*.7002075+viewUp*screen.y*.7002075);
  float b=dot(o,d),h=inner*inner-dot(cross(o,d),cross(o,d));
  float entry=(-b+sqrt(max(0.,h)))*dot(d,viewForward);
  if(depth<entry){finalColor=vec4(0.);return;}
 }
 vec4 sum=vec4(0.);float total=0.;
 for(int y=0;y<2;y++)for(int x=0;x<2;x++){
  ivec2 q=clamp(base+ivec2(x,y),ivec2(0),small-1);
  ivec2 opaquePixel=clamp(ivec2((vec2(q)+.5)*vec2(size)/vec2(small)),ivec2(0),size-1);
  float neighbour=texelFetch(opaqueDepth,opaquePixel,0).r;if(neighbour<=0.)neighbour=60000.;float delta=abs(neighbour-depth)/max(.25,depth*.015);
  float weight=(x==0?1.-f.x:f.x)*(y==0?1.-f.y:f.y)*exp(-delta*delta);
  sum+=(y==0?(x==0?a:b):(x==0?c:e))*weight;total+=weight;
 }
 finalColor=total>1e-5?sum/total:vec4(0.);
})GLSL";
inline constexpr const char* atmoFragment=R"GLSL(#version 330
in vec3 worldPosition,normal;
in vec4 tint;
out vec4 finalColor;
uniform vec3 centre,eye,lightDirection;
uniform vec3 viewForward;uniform vec2 resolution;uniform sampler2D opaqueDepth;
uniform float seed,atmosphere,kind,radius;
vec2 sphere(vec3 o,vec3 d,float r){float b=dot(o,d);vec3 perpendicular=cross(o,d);float h=r*r-dot(perpendicular,perpendicular);if(h<0.)return vec2(-1e8);h=sqrt(h);return vec2(-b-h,-b+h);}
)GLSL" PRISM_PIGMENT_GLSL R"GLSL(
void main(){
 if(!gl_FrontFacing)discard;
 vec3 o=eye-centre,d=normalize(worldPosition-eye),L=normalize(lightDirection);float outer=radius+40.;vec2 span=sphere(o,d,outer);
 float start=max(0.,span.x),end=span.y;vec2 ground=sphere(o,d,radius+5.);if(ground.x>0.)end=min(end,ground.x);
 float opaque=texture(opaqueDepth,gl_FragCoord.xy/resolution).r;
 if(opaque>0.)end=min(end,opaque/max(dot(d,viewForward),.0001));if(end<=start)discard;
 float stepSize=(end-start)/8.,optical=0.;vec3 sum=vec3(0.);vec3 beta=vec3(.0058,.0135,.029)*clamp(atmosphere,.12,1.4);
 if(kind>3.5&&kind<4.5)beta=vec3(.018,.026,.008);if(kind>4.5)beta=vec3(.022,.015,.009);
 float mu=dot(d,L),rayleigh=.05968*(1.+mu*mu),mie=.012*pow(max(.02,1.-.76*mu),-1.5);
 for(int i=0;i<8;i++){
  vec3 p=o+d*(start+(float(i)+.5)*stepSize);float h=max(0.,length(p)-radius),density=exp(-h/9.);
  optical+=density*stepSize;vec2 sunSpan=sphere(p,L,outer);vec2 hit=sphere(p,L,radius+2.);float sunlight=hit.x>0.?0.:1.;
  float sunOptical=0.;for(int j=0;j<3;j++){vec3 sp=p+L*sunSpan.y*(float(j)+.5)/3.;sunOptical+=exp(-max(0.,length(sp)-radius)/9.)*sunSpan.y/3.;}
  vec3 transmission=exp(-beta*(optical+sunOptical));sum+=transmission*density*stepSize*sunlight;
 }
 vec3 scattering=sum*(beta*rayleigh+vec3(.0016)*mie)*7.;
 scattering*=mix(vec3(1.),.28+pigment(dot(normalize(worldPosition-centre),vec3(.3,.8,.4))*.7+seed*.031+mu*.12)*1.35,.34);
 float opacity=clamp(1.-exp(-dot(beta,vec3(.333))*optical),0.,.92);
 finalColor=vec4(scattering*(opacity/max(opacity,.001)),opacity);
})GLSL";
inline constexpr const char* ringFragment=R"GLSL(#version 330
in vec3 worldPosition,normal,localPosition;out vec4 finalColor;
uniform vec3 centre,eye,lightDirection;uniform float radius,seed,time;
)GLSL" PRISM_PIGMENT_GLSL PRISM_RING_LIGHT_GLSL R"GLSL(
void main(){
 float r=length(localPosition),x=(r-1.29)/.48;vec2 field=ringField(r,fwidth(r),seed);
 vec3 p=worldPosition-centre,L=normalize(lightDirection),V=normalize(eye-worldPosition),N=normalize(normal);
 float along=dot(p,L),distance=length(cross(p,L)),penumbra=radius*.009+fwidth(distance);
 float eclipse=(1.-smoothstep(radius-penumbra,radius+penumbra,distance))*(1.-step(0.,along));
 float sunlight=mix(1.,.10,eclipse),grazing=pow(1.-abs(dot(N,V)),3.),forward=pow(max(0.,dot(-V,L)),8.);
 vec3 base=mix(vec3(.105,.16,.17),vec3(.30,.265,.20),smoothstep(.34,.75,field.y));
 base=mix(base,.025+pigment(x*.88+seed*.031+sin(time*.012)*.018+grazing*.045)*.36,.78);
 vec3 c=base*(.23+abs(dot(N,L))*.86+forward*.45)*sunlight+base*grazing*.10;
 c+=pigment(seed*.031+x*.24)*forward*field.x*.035*sunlight;
 finalColor=vec4(c,field.x);
})GLSL";
inline constexpr const char* auroraFragment=R"GLSL(#version 330
in vec3 worldPosition,normal;
out vec4 finalColor;
uniform vec3 centre,eye,lightDirection;
uniform float seed,time,atmosphere,radius,strength;
uniform vec3 viewForward;uniform vec2 resolution;uniform sampler2D opaqueDepth;
)GLSL" PRISM_PIGMENT_GLSL R"GLSL(
vec2 sphere(vec3 o,vec3 d,float r){float b=dot(o,d);vec3 perpendicular=cross(o,d);float h=r*r-dot(perpendicular,perpendicular);if(h<0.)return vec2(-1e8);h=sqrt(h);return vec2(-b-h,-b+h);}
void main(){
 vec3 n=normalize(worldPosition-centre),axis=normalize(vec3(sin(seed)*.20,1.,cos(seed)*.20));
 float angularPixel=max(length(dFdx(n)),length(dFdy(n)));float latitude=abs(dot(n,axis));if(latitude<.30||latitude>.99)discard;
 if(dot(n,lightDirection)>.25)discard;
 vec3 o=eye-centre,d=normalize(worldPosition-eye);vec2 span=sphere(o,d,radius+70.);
 float start=max(0.,span.x),end=span.y;vec2 ground=sphere(o,d,radius+18.);if(ground.x>0.)end=min(end,ground.x);
 float opaque=texture(opaqueDepth,gl_FragCoord.xy/resolution).r;if(opaque>0.)end=min(end,opaque/max(dot(d,viewForward),.0001));if(end<=start)discard;
 float stepSize=(end-start)/6.,drift=sin(time*.027+seed);vec3 sum=vec3(0.);
 for(int i=0;i<6;i++){
  vec3 p=o+d*(start+(float(i)+.5)*stepSize);float height=clamp((length(p)-radius-18.)/52.,0.,1.);n=normalize(p);
  float night=1.-smoothstep(-.25,.15,dot(n,lightDirection));
  float wave=sin(n.x*10.+n.z*7.+drift*.7)+.42*sin(n.z*21.-n.x*9.-drift*.4);
  float oval=abs(dot(n,axis))-(.70+wave*.045+height*.012),width=.017+.010*(.5+.5*sin(n.x*16.-n.z*11.));
  if(abs(oval)>.12||night<.005)continue;
  float filteredWidth=sqrt(width*width+angularPixel*angularPixel*.5);float curtain=exp(-oval*oval/(filteredWidth*filteredWidth))*width/filteredWidth;
  float rays=mix(.470625,.23+.77*pow(.5+.5*sin(n.x*129.+n.z*101.+wave*3.+drift+height*.8),3.),1.-smoothstep(.65,2.6,angularPixel*164.));
  float veil=exp(-pow((oval+.02)*28.,2.))*.10;
  float envelope=pow(max(0.,sin(height*3.14159)),.7);
  vec3 colour=pigment(height*.30+seed*.031+n.z*.18+.07*drift+oval*1.2);
  sum+=colour*(curtain*rays+veil)*night*envelope*stepSize*.020;
 }
 sum*=1.45/(1.+max(sum.r,max(sum.g,sum.b))*.5);
 finalColor=vec4(sum*clamp(atmosphere,.0,1.)*strength,1.);
})GLSL";
inline constexpr const char* blurFragment=R"GLSL(#version 330
in vec2 fragTexCoord;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec2 direction;
uniform float extractLight;
vec3 sampleLight(vec2 p){vec3 c=texture(texture0,p).rgb;return extractLight>.5?max(c-.72,vec3(0.)):c;}
void main(){vec3 c=sampleLight(fragTexCoord)*.227027;c+=(sampleLight(fragTexCoord+direction*1.384615)+sampleLight(fragTexCoord-direction*1.384615))*.316216;c+=(sampleLight(fragTexCoord+direction*3.230769)+sampleLight(fragTexCoord-direction*3.230769))*.070270;finalColor=vec4(c,1.);}
)GLSL";
inline constexpr const char* postFragment=R"GLSL(#version 330
in vec2 fragTexCoord;
out vec4 finalColor;
uniform sampler2D texture0,bloomTexture;
uniform vec2 texel;
uniform float hit,effects,sunVisibility,lowGraphics;uniform vec2 sunPosition;
float luma(vec3 c){return dot(c,vec3(.299,.587,.114));}
void main(){
 vec2 uv=fragTexCoord;vec3 c=texture(texture0,uv).rgb;
 vec3 a=texture(texture0,uv+vec2(-1,-1)*texel).rgb,b=texture(texture0,uv+vec2(1,-1)*texel).rgb;
 vec3 d=texture(texture0,uv+vec2(-1,1)*texel).rgb,e=texture(texture0,uv+vec2(1,1)*texel).rgb;
 float lo=min(luma(c),min(min(luma(a),luma(b)),min(luma(d),luma(e))));
 float hi=max(luma(c),max(max(luma(a),luma(b)),max(luma(d),luma(e))));
 if(hi-lo>max(.045,hi*.18)){
  vec2 edge=vec2(-(luma(a)+luma(b)-luma(d)-luma(e)),luma(a)+luma(d)-luma(b)-luma(e));
  edge=clamp(edge/(min(abs(edge.x),abs(edge.y))+max(.012,(luma(a)+luma(b)+luma(d)+luma(e))*.03125)),-4.,4.)*texel;
  vec3 filtered=(texture(texture0,uv+edge*.167).rgb+texture(texture0,uv-edge*.167).rgb)*.5;
  c=mix(c,filtered,.68);
 }
 if(lowGraphics<.5){
 // Recover broad material contrast from the taps already used for AA.
 // Skip silhouettes and tiny highlights so sharpening cannot make stars,
 // mineral glints or the horizon sparkle. The HUD is drawn afterwards.
 vec3 neighbourhood=(a+b+d+e)*.25;
 float clarity=(1.-smoothstep(.12,.32,(hi-lo)/max(hi,.02)))*smoothstep(.015,.08,luma(c));
 c=max(vec3(0.),c+(c-neighbourhood)*clarity*.24);
 }
 c+=texture(bloomTexture,uv).rgb*mix(.14,.38,effects);
 vec2 v=(uv-.5)*2.;vec2 sun=uv-sunPosition;
 float streak=exp(-abs(sun.y)*950.)*exp(-abs(sun.x)*3.5);
 vec2 ghost=uv-(.5-(sunPosition-.5)*.38);float halo=exp(-abs(length(ghost)-.13)*190.);
 c+=vec3(.30,.18,.065)*(streak*.09+halo*.018)*sunVisibility*effects;
 c*=1.-.10*dot(v,v);c+=vec3(.30,.012,.035)*hit*dot(v,v)*effects;
 // Linear HDR is preserved until the final exposure and ACES display transform.
 c=max(c*1.30,vec3(0.));
 float peak=max(c.r,max(c.g,c.b)),mappedPeak=(peak*(2.51*peak+.03))/(peak*(2.43*peak+.59)+.14);
 vec3 film=(c*(2.51*c+.03))/(c*(2.43*c+.59)+.14);
 // Keep rich pigment hues through the highlight rolloff instead of whitening
 // every bright colour independently. The HUD is composed after this pass.
 c=clamp(mix(film,c/max(peak,.0001)*mappedPeak,.48),0.,1.);
 c=pow(c,vec3(1./2.2));
 finalColor=vec4(c,1.);
})GLSL";
inline constexpr const char* cockpitFragment=R"GLSL(#version 330
in vec3 worldPosition,normal,localPosition;
in vec2 modelUV;
in vec4 tint;
out vec4 finalColor;
uniform vec3 lightDirection;
uniform float time;
uniform sampler2D noiseTexture;
float noise(vec3 p){vec3 cell=floor(p),f=fract(p);f=f*f*(3.-2.*f);vec2 rg=textureLod(noiseTexture,(cell.xy+vec2(37.,17.)*cell.z+f.xy+.5)/256.,0.).rg;return mix(rg.r,rg.g,f.z);}
)GLSL" PRISM_PIGMENT_GLSL R"GLSL(
void main(){
 // Reserve a narrow depth interval for the cabin so it can share the world's
 // HDR framebuffer while retaining its own beveled-surface depth ordering.
 gl_FragDepth=gl_FragCoord.z*.0001;
 vec3 N=normalize(normal),V=normalize(-worldPosition),L=normalize(lightDirection),H=normalize(L+V);float nv=max(dot(N,V),0.),nl=max(dot(N,L),0.);
 if(tint.a>.5){finalColor=vec4(tint.rgb*2.8,1.);return;}
 float grain=noise(localPosition*430.),brush=.98+.02*sin(localPosition.x*1900.);
 vec3 base=pow(tint.rgb,vec3(2.2))*(.96+grain*.07)*brush;
 float sheen=pow(max(dot(N,H),0.),48.),fresnel=pow(1.-nv,4.);
 vec3 c=base*(vec3(.15,.24,.34)+vec3(1.,.87,.68)*nl*1.3);
 c+=vec3(.48,.66,.73)*sheen*.65+vec3(.055,.14,.18)*fresnel;
 c+=base*vec3(.10,.25,.23)*max(0.,-N.y);
 c+=pigment(nv*.65+localPosition.y*.13+.04*sin(time*.017))*fresnel*.055;
 finalColor=vec4(c,1.);
})GLSL";

}
#undef PRISM_PIGMENT_GLSL

#undef PRISM_SURFACE_DETAIL_GLSL

#undef PRISM_RING_LIGHT_GLSL
#undef PRISM_NEAR_SUN_GLSL

#undef PRISM_WATER_GLSL

#undef PRISM_ARCHITECTURE_GLSL
