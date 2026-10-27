#pragma once
namespace hyperion::flight {
// Depth is stored in an R32F colour target, keeping the comparison portable.
inline constexpr const char* sunDepthFragment=R"GLSL(#version 330
out vec4 finalColor;
void main(){finalColor=vec4(gl_FragCoord.z,0.,0.,1.);}
)GLSL";
}

#define PRISM_RING_LIGHT_GLSL R"GLSL(
vec2 ringField(float r,float pixel,float phase){
 float x=(r-1.29)/.48,aa=max(pixel/.48,.0004);
 float bands=.5+.22*sin(x*73.+phase)*(1.-smoothstep(.65,2.6,aa*73.))+
 .14*sin(x*211.)*(1.-smoothstep(.65,2.6,aa*211.))+.09*sin(x*617.)*(1.-smoothstep(.65,2.6,aa*617.));
 float gap=(1.-smoothstep(.44-aa,.45+aa,x))+smoothstep(.493-aa,.505+aa,x);
 gap*=1.-(smoothstep(.79-aa,.798+aa,x)-smoothstep(.808-aa,.818+aa,x))*.85;
 float edge=smoothstep(0.,aa*2.,x)*(1.-smoothstep(1.-aa*2.,1.,x));
 return vec2(clamp(.35+bands*.48,0.,.88)*gap*edge,bands);
}
float ringVisibility(vec3 p,vec3 L,float radius,float ringed,float phase){
 if(ringed<.5)return 1.;
 vec3 axis=normalize(vec3(0.,-1.,.32));float travel=-dot(p,axis)/dot(L,axis);
 float r=length(p+L*travel)/radius;
 float pixel=max(fwidth(r),.002+abs(travel)/radius*.006);
 return 1.-ringField(r,pixel,phase).x*step(0.,travel)*.92;
}
)GLSL"

#define PRISM_NEAR_SUN_GLSL R"GLSL(
#ifdef TERRAIN_DETAIL
uniform sampler2D sunShadowTexture;
uniform mat4 sunShadowMatrix;
uniform float useSunShadow;
float nearSunVisibility(vec3 p,vec3 N,vec3 L){
 if(useSunShadow<.01)return 1.;
 vec4 clip=sunShadowMatrix*vec4(p,1.);vec3 q=clip.xyz/clip.w*.5+.5;
 // Compare each tap against the receiver plane at that texel. A single
 // centre depth produces diagonal self-shadow acne on sloping terrain.
 vec3 dx=dFdx(q),dy=dFdy(q);float det=dx.x*dy.y-dx.y*dy.x;
 vec2 gradient=abs(det)>1e-16?vec2(dx.z*dy.y-dy.z*dx.y,dy.z*dx.x-dx.z*dy.x)/det:vec2(0.);
 if(q.z<=0.||q.z>=1.||min(q.x,q.y)<=.002||max(q.x,q.y)>=.998)return 1.;
 vec2 size=vec2(textureSize(sunShadowTexture,0)),pixel=q.xy*size-.5,base=(floor(pixel)+.5)/size,f=fract(pixel);
 float depth=q.z-max(.00006,.00016*(1.-clamp(dot(N,L),0.,1.)));
 vec2 d=1./size;
 float a=step(depth+dot(gradient,base-q.xy),texture(sunShadowTexture,base).r);
 float b=step(depth+dot(gradient,base+vec2(d.x,0.)-q.xy),texture(sunShadowTexture,base+vec2(d.x,0.)).r);
 float c=step(depth+dot(gradient,base+vec2(0.,d.y)-q.xy),texture(sunShadowTexture,base+vec2(0.,d.y)).r);
 float e=step(depth+dot(gradient,base+d-q.xy),texture(sunShadowTexture,base+d).r);
 float visibility=mix(mix(a,b,f.x),mix(c,e,f.x),f.y);
 if(extremeQuality>.5){
  float sum=0.,total=0.;
  for(int y=-2;y<=2;y++)for(int x=-2;x<=2;x++){
   vec2 tap=(floor((q.xy+vec2(x,y)*d)*size)+.5)/size;float weight=exp(-float(x*x+y*y)*.48);
   sum+=step(depth+dot(gradient,tap-q.xy),texture(sunShadowTexture,tap).r)*weight;total+=weight;
  }
  visibility=sum/total;
 }
 float edge=smoothstep(.015,.085,min(min(q.x,q.y),min(1.-q.x,1.-q.y)));
 return mix(1.,visibility,useSunShadow*edge);
}
#endif
)GLSL"
