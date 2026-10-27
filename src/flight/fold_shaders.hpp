#pragma once
namespace hyperion::flight {
inline constexpr const char* foldWindowFragment=R"GLSL(#version 330
uniform sampler2D texture0;uniform vec2 resolution;out vec4 finalColor;
void main(){finalColor=vec4(texture(texture0,gl_FragCoord.xy/resolution).rgb,1.);}
)GLSL";
inline constexpr const char* nilFlightFragment=R"GLSL(#version 330
uniform vec2 resolution;
uniform vec3 observer,centre,forward,up,right,worldRight,worldUp,worldAxis,sun;
uniform float radius,weight,wireframe,ega,water,seed,psychedelic;
uniform sampler2D surfaceMap,geologyMap;
out vec4 finalColor;
vec3 rayPoint(vec3 v,float t){
 float w=v.z;vec3 q;
 if(abs(w)<.00001)q=vec3(v.xy*t,w*t);
 else{float s=sin(w*t),c=cos(w*t);q=vec3((v.x*s+v.y*(c-1.))/w,(v.x*(1.-c)+v.y*s)/w,w*t+dot(v.xy,v.xy)*(w*t-s)/(2.*w*w));}
 return observer+q+vec3(0.,0.,.5*(observer.x*q.y-observer.y*q.x));
}
vec3 velocity(vec3 v,float t){float s=sin(v.z*t),c=cos(v.z*t);return vec3(v.x*c-v.y*s,v.x*s+v.y*c,v.z);}
float surfaceDistance(vec3 v,float t){return length(rayPoint(v,t)-centre)-radius;}
vec3 pigment(float h){return pow(.5+.5*cos(6.283185*(h+vec3(0.,.68,.34))),vec3(1.35));}
float grid(float a,float steps){float s=a*steps;return 1.-smoothstep(.015,max(.025,min(.10,fwidth(s)*.7)),abs(fract(s+.5)-.5));}
void main(){
 vec2 uv=gl_FragCoord.xy/resolution*2.-1.;uv.x*=resolution.x/resolution.y;
 vec3 ray=normalize(forward+right*uv.x*.7002075+up*uv.y*.7002075);
 if(ray.z*(centre.z-observer.z)<=0.)discard;
 float t=0.,previous=0.;bool hit=false;
 for(int i=0;i<620;i++){
  vec3 p=rayPoint(ray,t);float d=length(p-centre)-radius;
  if(d<0.){hit=true;break;}
  if(t>length(centre-observer)+18.)break;
  vec3 v=velocity(ray,t);float speed=length(vec3(v.xy,v.z+.5*(p.x*v.y-p.y*v.x)));
  previous=t;t+=clamp(d*.32/max(1.,speed),.012,.105);
 }
 if(!hit)discard;
 float lo=previous,hi=t;for(int i=0;i<8;i++){float m=(lo+hi)*.5;if(surfaceDistance(ray,m)<0.)hi=m;else lo=m;}t=(lo+hi)*.5;
 vec3 p=rayPoint(ray,t),q=p-centre,n=normalize(q),v=velocity(ray,t);
 vec3 worldNormal=normalize(worldRight*n.x+worldUp*n.y+worldAxis*n.z);
 vec2 st=vec2(atan(worldNormal.z,worldNormal.x)/6.283185,acos(clamp(worldNormal.y,-1.,1.))/3.14159265);
 vec4 field=textureLod(surfaceMap,st,1.);vec3 geology=textureLod(geologyMap,st,1.).rgb;
 float sea=mix(.28,.72,water),land=smoothstep(sea-.008,sea+.013,field.r);
 vec3 normalFrame=normalize(vec3(q.x-p.y*.5*q.z,q.y+p.x*.5*q.z,q.z));
 float rim=1.-smoothstep(.06,.20,abs(dot(normalFrame,v)));
 float coast=field.r-sea,coastWidth=max(.0015,fwidth(coast));
 vec3 colour;
 if(wireframe>.5){
  float lines=max(grid(st.x,24.),grid(st.y,16.));colour=(ega>.5?vec3(.2,.45,1.):vec3(.333,1.,1.))*lines*.7;
  colour=max(colour,(ega>.5?vec3(.2,1.,.25):vec3(1.,.333,1.))*(1.-smoothstep(coastWidth*.5,coastWidth*1.4,abs(coast))));
  colour=max(colour,vec3(rim));
 }else{
  vec3 ocean=mix(vec3(.004,.019,.055),vec3(.012,.22,.23),pow(smoothstep(sea-.16,sea,field.r),2.));
  vec3 albedo=mix(ocean,max(geology,vec3(.015)),land);
  float day=max(0.,dot(worldNormal,sun));colour=albedo*(.12+day*.85);
  colour+=pigment(seed*.031+field.r*.7+worldNormal.z*.13)*rim*.23;
  if(psychedelic>.5)colour+=pigment(field.r*1.3+seed*.031+st.x*.4)*(.10+.12*rim);
 }
 finalColor=vec4(colour,weight);
}
)GLSL";
}
