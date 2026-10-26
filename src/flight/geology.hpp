#pragma once
namespace hyperion::flight {
// Static materials are evaluated once per system, in planet directions. RGB is
// linear rock colour; alpha is a continuous relief field for material normals.
inline constexpr const char* geologyBakeFragment=R"GLSL(#version 330
out vec4 finalColor;
uniform sampler2D noiseTexture;
uniform vec2 resolution;
uniform float seed,kind,water,life,atmosphere,temperature;
float noise(vec3 p){vec3 cell=floor(p),f=fract(p);f=f*f*(3.-2.*f);vec2 uv=cell.xy+vec2(37.,17.)*cell.z+f.xy;vec2 rg=textureLod(noiseTexture,(uv+.5)/256.,0.).rg;return mix(rg.r,rg.g,f.z);}
float fbm(vec3 p){float value=0.,a=.54;for(int i=0;i<5;i++){value+=a*noise(p);p=p*2.03+vec3(7.1,13.7,19.3);a*=.48;}return value;}
vec3 random3(vec3 p){p=vec3(dot(p,vec3(127.1,311.7,74.7)),dot(p,vec3(269.5,183.3,246.1)),dot(p,vec3(113.5,271.9,124.6)));return fract(sin(p+seed)*43758.5453);}
vec3 pigment(float h){return pow(.5+.5*cos(6.283185*(h+vec3(0.,.68,.34))),vec3(1.35));}
vec3 impacts(vec3 n,float scale){
 vec3 cell=floor(n*scale);float relief=0.,ejecta=0.,floorMask=0.;
 // Project cell centres onto the sphere. This avoids a repeated crater tile,
 // longitude seams and any special case at the geographic poles.
 for(int z=-1;z<=1;z++)for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++){
  vec3 id=cell+vec3(x,y,z),j=random3(id);vec3 centre=id+j;
  if(abs(length(centre)-scale)>.72||j.z<.30)continue;
  float size=.29+j.y*.36,r=length(n-normalize(centre))*scale/size;
  if(r>1.65)continue;
  float bowl=(1.-smoothstep(.12,.85,r)),rim=exp(-pow((r-1.)*8.,2.));
  float apron=exp(-pow((r-1.13)*3.5,2.))*(.5+.5*sin(atan(n.z-centre.z/scale,n.x-centre.x/scale)*17.+j.x*9.));
  relief+=rim*.23-bowl*.18+apron*.025;floorMask=max(floorMask,bowl);ejecta=max(ejecta,rim*.7+apron*.22);
 }
 return vec3(relief,ejecta,floorMask);
}
void main(){
 vec2 uv=gl_FragCoord.xy/resolution;float phi=uv.x*6.283185,theta=(uv.y-.5/resolution.y)*(resolution.y/(resolution.y-1.))*3.14159265;
 vec3 n=vec3(sin(theta)*cos(phi),cos(theta),sin(theta)*sin(phi));
 vec3 p=n*5.2+vec3(seed*.17,seed*.073,seed*.11);
 vec3 warp=vec3(noise(p+7.),noise(p+19.),noise(p+37.))-.5;
 float province=fbm(p+warp*.9),sediment=fbm(p*8.+warp*2.);
 float folded=.5+.5*sin(dot(n,normalize(vec3(.4+sin(seed),.8,.3+cos(seed))))*105.+province*17.+sediment*3.);
 float fault=pow(max(0.,1.-abs(noise(p*7.+warp)-.5)*13.),3.);
 float ridges=pow(1.-abs(fbm(p*4.+warp)*2.-1.),3.);
 float wet=clamp(water*.55+life*.4+atmosphere*.06,0.,1.);
 vec3 colour=mix(vec3(.17,.095,.047),vec3(.32,.27,.19),smoothstep(.26,.73,province));
 colour=mix(colour,mix(vec3(.028,.10,.061),vec3(.12,.19,.066),sediment),wet*smoothstep(.30,.64,sediment)*(1.-abs(n.y)*.5));
 float relief=.5+(ridges-.5)*.15+(folded-.5)*.035-fault*.075;
 vec3 crater=vec3(0.);
 if(kind<4.5){crater=impacts(n,5.)+impacts(n,12.)*.5;crater*=clamp(1.-atmosphere*.45-water*.55,.08,1.);}
 relief+=crater.x;colour*=.75+sediment*.45+folded*.09;
 colour=mix(colour,colour*.55+vec3(.16,.14,.11),clamp(crater.y,0.,.8));colour*=1.-crater.z*.23;
 if(kind>.5&&kind<1.5){
  float dunePhase=dot(n,vec3(97.,19.,63.))+province*20.+noise(p*5.)*3.;float dunes=.5+.5*sin(dunePhase);
  colour=mix(vec3(.12,.045,.022),vec3(.52,.29,.095),smoothstep(.23,.75,province))*(.70+sediment*.4+dunes*.18);
  colour=mix(colour,vec3(.40,.36,.26),smoothstep(.64,.78,sediment)*.45);
  relief+=(dunes-.5)*.065+crater.x*.3;
 }
 if(kind>1.5&&kind<2.5){
  float fracture=abs(noise(p*10.+warp*3.)-.5),crack=1.-smoothstep(.008,.045,fracture);
  colour=mix(vec3(.20,.40,.48),vec3(.65,.79,.83),smoothstep(.24,.72,province))*(.8+sediment*.3);
  colour=mix(colour,vec3(.008,.055,.13),crack*.75);colour+=pigment(seed*.031+province)*crack*.035;
  relief-=crack*.14;colour*=1.-crater.z*.15;
 }
 if(kind>2.5&&kind<3.5){
  colour=mix(vec3(.014,.019,.027),vec3(.16,.052,.022),smoothstep(.25,.78,province))*(.72+sediment*.5+folded*.13);
  colour=mix(colour,vec3(.26,.20,.16),smoothstep(.64,.77,sediment)*.45);
  relief-=fault*.13;colour*=1.-fault*.45;
 }
 if(kind>3.5&&kind<4.5){
  colour=mix(vec3(.045,.085,.022),vec3(.37,.30,.07),smoothstep(.24,.75,province));
  colour=mix(colour,vec3(.33,.42,.25),smoothstep(.57,.76,sediment)*.6);relief-=fault*.06;
 }
 // Keep excavated floors and brighter ejecta visible after choosing the
 // surface class, including on desert and basalt worlds.
 vec3 rimColour=(kind>1.5&&kind<2.5)?vec3(.15,.22,.25):vec3(.13,.11,.085);
 colour=colour*(1.-crater.z*.32)+rimColour*crater.y*.55;
 // Fine bedding and broken grains live in the mipmapped field rather than
 // being redrawn as subpixel, sparkling highlights at distance.
 float grainResolved=1.-smoothstep(.4,1.5,length(fwidth(n*149.)));
 float grit=mix(.5,noise(n*149.+seed*.37),grainResolved);
 float beddingPhase=dot(n,normalize(vec3(.4+sin(seed),.8,.3+cos(seed))))*487.+province*39.;
 float beddingResolved=1.-smoothstep(.6,2.,fwidth(beddingPhase));
 float lamina=mix(.5,.5+.5*sin(beddingPhase),beddingResolved);
 colour*=.84+grit*.23+lamina*.09;
 colour=mix(colour,colour*.65+vec3(.075,.055,.035),fault*grit*.12);
 float mineralGrain=mix(.5,noise(n*317.+seed*.53),1.-smoothstep(.4,1.5,length(fwidth(n*317.))));
 float inclusions=smoothstep(.61,.78,mineralGrain)*(1.-fault*.5);
 colour=mix(colour,colour*.64+pigment(seed*.031+sediment*.23)*.075,inclusions*.27);
 relief+=(grit-.5)*.013+(lamina-.5)*.009+(mineralGrain-.5)*.004;
 // A small mineral tint is fixed to the geology; the live silk remains an
 // additional layer, so the landscape has stable features under the motion.
 colour=mix(colour,colour*.55+pigment(province*1.8+seed*.031)*.22,.28);
 if(kind>4.5){
  float shear=sin(n.y*23.)*.55+noise(p*3.)*.5;
  vec3 flow=n;flow.xz=mat2(cos(shear),-sin(shear),sin(shear),cos(shear))*flow.xz;
  float turbulence=fbm(flow*24.+seed),latitude=n.y;
  for(int i=0;i<3;i++){
   float a=seed*.51+float(i)*2.09,y=sin(seed+float(i)*4.)*.65;
   vec3 axis=normalize(vec3(cos(a),y,sin(a)));float distance=length(n-axis);
   vec3 east=normalize(cross(axis,vec3(0,1,0))),north=cross(axis,east);
   float angle=atan(dot(n,north),dot(n,east));float spiral=sin(distance*97.-angle*3.+turbulence*2.);
   latitude+=spiral*exp(-distance*distance*70.)*.065;
  }
  float jets=latitude*85.+turbulence*7.+sin(latitude*17.)*2.;
  float bands=.5+.5*sin(jets),filaments=pow(.5+.5*sin(jets*3.+sediment*5.),5.);
  colour=mix(pigment(seed*.031+.04)*.19,pigment(seed*.031+.30)*.50,bands*.78+turbulence*.22);
  colour+=pigment(seed*.031+.58)*filaments*.09;relief=.5+(turbulence-.5)*.2;
 }
 finalColor=vec4(colour,clamp(relief,.04,.96));
})GLSL";
}
