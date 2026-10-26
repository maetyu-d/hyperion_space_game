#pragma once
namespace hyperion {
constexpr const char* sceneFragment=R"GLSL(#version 330
in vec2 fragTexCoord;
out vec4 finalColor;
uniform sampler2D texture0,terrainTexture;
uniform vec2 resolution,viewSize,camera,terrainOrigin;
uniform float time,seed,depth,water,sea,zoom,terrainSpan,delta,persistence;
uniform int mode;
mat2 rot(float a){return mat2(cos(a),-sin(a),sin(a),cos(a));}
float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
float noise(vec2 p){vec2 i=floor(p),f=fract(p);f=f*f*(3.-2.*f);return mix(mix(hash(i),hash(i+vec2(1,0)),f.x),mix(hash(i+vec2(0,1)),hash(i+1.),f.x),f.y);}
vec3 spectrum(float h){vec3 c=.5+.5*cos(6.283185*(h+vec3(0.,.68,.34)));return pow(c,vec3(1.35));}
float thread(float p,float w){float a=max(fwidth(p)*.8,.002);return 1.-smoothstep(w,w+a,abs(sin(p)));}
vec3 silk(vec2 p,float phase,float motion){
 float t=time*motion+seed;
 p=rot(.1*sin(t*.3+phase))*p;
 vec2 q=p+.21*vec2(sin(p.y*2.8+t*.31),cos(p.x*3.1-t*.27));
 q+=.07*vec2(sin(q.y*7.1-t*.18),cos(q.x*6.8+t*.22));
 float r=length(q-vec2(.15*sin(t*.1),.2*cos(t*.13)));
 float a=atan(q.y+.001,q.x+.001);
 float fold=q.y*3.7+sin(q.x*3.4+t*.17)*1.7+.38*sin(q.y*5.-q.x*2.+t*.23);
 fold+=.55*sin(a*3.+r*4.2-t*.2);
 float cross=q.x*4.2+sin(q.y*3.6-t*.21)*1.4;
 float wave=sin(fold),sheet=pow(.5+.5*wave,1.9),rear=pow(.5+.5*sin(cross),3.);
 vec3 front=spectrum(fold*.18+r*.09+phase+time*.004);
 vec3 back=spectrum(cross*.15+phase+.36-time*.003);
 float fine=thread(fold*17.+.6*sin(cross*2.),.035);
 float lace=thread(fold*51.+sin(cross*3.1),.018);
 float veins=thread(cross*19.+.8*sin(fold*4.),.025);
 float crest=thread(fold+.15,.055);
 float shade=.28+.72*pow(.5+.5*cos(fold+.8),2.);
 vec3 c=front*sheet*shade*.90+back*rear*.34;
 c+=front*(fine*.90+lace*.20)+back*veins*.23;
 c+=mix(front,vec3(.75,1.,.92),.3)*crest*1.1;
 c+=front*pow(fine*veins,2.)*.4;
 return c*1.08;
}
void main(){
 vec2 uv=gl_FragCoord.xy/resolution,pixel=vec2(uv.x,1.-uv.y)*viewSize;
 vec2 p=(pixel/viewSize-.5)*vec2(viewSize.x/viewSize.y,1.)*2.;
 float phase=fract(seed*.0371);vec3 c=vec3(.004,.009,.025);vec2 velocity=vec2(0.);
 if(mode==0||mode==1){
  vec2 q=p*.9+camera*.0015;
  float cloud=noise(q*1.4+seed)+.5*noise(q*3.7+seed);
  float nebula=smoothstep(.30,1.15,cloud);
  c+=silk(q,phase,.14)*nebula*(mode==0?.85:.32);
  c+=spectrum(phase+.2)*pow(nebula,3.)*.07;
  velocity=vec2(sin(q.y*2.+time*.03),cos(q.x*2.-time*.02))*.00013;
 }else if(mode==2){
  vec2 world=camera+(pixel-viewSize*.5)/zoom;
  vec2 map=(world-terrainOrigin)/terrainSpan;
  float h=texture(terrainTexture,map).r;
  float dx=texture(terrainTexture,map+vec2(1./512.,0)).r-h;
  float dy=texture(terrainTexture,map+vec2(0,1./512.)).r-h;
  float contour=thread((h-sea)*370.,.035);
  float wet=1.-smoothstep(sea-.003,sea+.004,h);
  float shore=exp(-abs(h-sea)*210.);
  vec2 q=world*.003;
  vec3 flow=silk(q,phase,.13);
  vec3 ocean=mix(vec3(.005,.018,.065),vec3(.012,.19,.24),clamp((h-sea+.19)*4.,0.,1.));
  ocean+=flow*(.22+shore*.35);
  vec3 land=mix(vec3(.10,.065,.16),vec3(.20,.30,.27),clamp((h-sea)*4.+.2,0.,1.));
  land+=spectrum(h*5.+phase)*.2+flow*.25;
  float lighting=clamp(.7+(dx-dy)*33.,.28,1.5);
  c=mix(land*lighting,ocean,wet);
  c+=spectrum(h*4.+phase+.2)*contour*.16;
  c+=mix(vec3(.20,.9,.8),spectrum(phase+.6),.4)*shore*.68;
  float ripple=thread((h-sea)*155.-time*.25,.035)*exp(-abs(h-sea)*25.)*wet;
  c+=vec3(.12,.65,.63)*ripple*.27;
  velocity=vec2(.00004,0.);
 }else{
  vec2 q=p*vec2(.85,.74)+vec2(camera.x*.006,depth*.00085);
  float down=clamp(depth/6000.,0.,1.);
  vec3 currents=silk(q,phase+.13,.12);
  float shaft=pow(.5+.5*sin(p.x*3.2+p.y*.37+sin(p.x*2.-time*.05)),12.);
  float photic=exp(-depth*.0018);
  c+=currents*(1.35+.28*down);
  c+=vec3(.01,.25,.30)*shaft*(1.-pixel.y/viewSize.y)*photic*.4;
  c+=vec3(.002,.022,.045)*(1.-down);
  float veil=smoothstep(-.5,1.2,p.y+sin(p.x*2.3+seed)*.22);
  c=mix(c,c*spectrum(phase+.7)*.75,veil*.35);
  velocity=vec2(sin(q.y*3.+time*.05)*.00010,-.00009);
 }
 // A short, bounded image return gives moving pigment echoes without hiding
 // terrain or allowing persistent exposure to build up.
 vec2 source=clamp(uv+velocity,vec2(.001),vec2(.999));
 vec3 history=texture(texture0,source).rgb;
 float retain=pow(clamp(persistence,0.,.85),delta*60.);
 c=mix(c,history,retain*.34);
 c=c/(1.+c*.32);
 finalColor=vec4(c,1.);
}
)GLSL";
constexpr const char* orbFragment=R"GLSL(#version 330
out vec4 finalColor;
uniform vec2 resolution,viewSize,center;
uniform float radius,time,seed,water;
uniform int stellar;
vec3 spectrum(float h){return pow(.5+.5*cos(6.283185*(h+vec3(0.,.68,.34))),vec3(1.25));}
float wave(vec2 q){float t=time*.035;return sin(q.x*7.+sin(q.y*5.+t)*2.)+.52*sin(q.y*11.-q.x*4.-t)+.24*sin(q.x*19.+q.y*9.);}
void main(){
 vec2 pixel=vec2(gl_FragCoord.x/resolution.x,1.-gl_FragCoord.y/resolution.y)*viewSize;
 vec2 p=(pixel-center)/radius;float r=length(p),edge=fwidth(r)*1.3;
 if(r>1.22)discard;
 float atmosphere=exp(-abs(r-1.)*29.);
 vec3 c=spectrum(seed*.003+time*.003+.5)*atmosphere*.44;
 float opacity=atmosphere*.6;
 if(r<1.+edge){
  float z=sqrt(max(0.,1.-r*r));vec3 normal=normalize(vec3(p,z));
  vec2 q=vec2(atan(normal.x,normal.z)*1.3,asin(normal.y)*1.4)+seed*.001;
  float f=wave(q),g=wave(q*1.7+2.1);
  vec3 pigment=spectrum(f*.14+seed*.003+time*.002);
  float waterMask=smoothstep(.2-water,1.-water,f*.35+.5);
  pigment=mix(mix(vec3(.015,.13,.22),pigment,.73),pigment*.75+vec3(.12,.17,.09),waterMask*.6);
  float light=.14+.86*max(0.,dot(normal,normalize(vec3(-.65,-.45,1.))));
  float thread=1.-smoothstep(.02,max(.025,fwidth(f*20.)*.85),abs(sin(f*20.+g*.3)));
  c=pigment*light+pigment*thread*.38;
  c+=vec3(.3,.63,.65)*pow(1.-z,4.)*.5;
  if(stellar==1){c=spectrum(f*.07+seed*.003)*(.7+thread*.7)+vec3(.6,.28,.045)*(.5+z*.3);}
  float pearl=pow(max(0.,dot(reflect(normalize(vec3(.65,.45,-1.)),normal),vec3(0,0,1))),40.);
  if(stellar==0)c+=vec3(.75,.95,.9)*pearl*.8;
  c=c/(1.+c*.3);opacity=1.-smoothstep(1.-edge,1.+edge,r);
 }
 finalColor=vec4(c,opacity);
}
)GLSL";
constexpr const char* blurFragment=R"GLSL(#version 330
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec2 resolution,direction;
uniform float extractLight;
void main(){vec2 uv=gl_FragCoord.xy/resolution;vec3 c=texture(texture0,uv).rgb*.227027;
 c+=texture(texture0,uv+direction*1.384615).rgb*.316216;c+=texture(texture0,uv-direction*1.384615).rgb*.316216;
 c+=texture(texture0,uv+direction*3.230769).rgb*.070270;c+=texture(texture0,uv-direction*3.230769).rgb*.070270;
 if(extractLight>.5)c=max(c-vec3(.18),vec3(0.));finalColor=vec4(c,1.);}
)GLSL";
constexpr const char* postFragment=R"GLSL(#version 330
in vec2 fragTexCoord;
out vec4 finalColor;
uniform sampler2D texture0,bloomTexture;
void main(){vec2 uv=fragTexCoord;vec3 c=texture(texture0,uv).rgb+texture(bloomTexture,uv).rgb*.32;
 float vignette=1.-.20*pow(length((uv-.5)*vec2(1.1,1.)),1.7);c*=vignette;
 finalColor=vec4(c,1.);}
)GLSL";
}
