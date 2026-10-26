#pragma once
// Same integer noise and geographic field as worldgen.hpp. Only evaluated when
// baking a planet's cached texture; the flight fragment budget is unchanged.
#define HYPERION_WORLDGEN_GLSL R"GLSL(
uniform vec4 worldParams;
uniform float generation;
uint pnHash(uint seed,ivec3 p){uint h=seed^uint(p.x)*374761393u^uint(p.y)*668265263u^uint(p.z)*2246822519u;h=(h^(h>>13))*1274126177u;return h^(h>>16);}
float pnNoise(uint seed,vec3 p){ivec3 cell=ivec3(floor(p));vec3 f=fract(p);f=f*f*f*(f*(f*6.-15.)+10.);float sum=0.;
 for(int z=0;z<2;z++)for(int y=0;y<2;y++)for(int x=0;x<2;x++){vec3 w=mix(1.-f,f,vec3(x,y,z));sum+=float(pnHash(seed,cell+ivec3(x,y,z))>>8)/16777216.*w.x*w.y*w.z;}return sum;}
vec3 pnScale(vec3 n,float scale,float shift){return n*scale+vec3(shift,shift*.71,-shift*.39);}
float pnElevation(vec3 n){
 uint seed=uint(worldParams.x)+uint(worldParams.y)*65536u;int form=int(worldParams.z);float tectonic=worldParams.w;
 float scale=form==1?5.2:form==2?2.4:form==3?2.8:form==6?3.8:2.6;
 vec3 q=pnScale(n,scale,13.7);float warp=pnNoise(seed^0x719a35u,pnScale(n,3.1,9.));q+=warp*vec3(.9,.5,-.7);
 float broad=pnNoise(seed,q)*.68+pnNoise(seed^0x91a23u,pnScale(q,2.03,7.1))*.32;
 float detail=pnNoise(seed^0x35fa19u,pnScale(n,16.3,11.)),ridge=1.-abs(pnNoise(seed^0x156addu,pnScale(n,7.7,3.))*2.-1.);
 float e=.5+(broad-.5)*.85+(detail-.5)*.075+(ridge-.65)*(.07+tectonic*.12);
 if(form==1)e=.52+(broad-.5)*.62+(detail-.5)*.14+(ridge-.65)*.06;
 if(form==2)e=.53+(broad-.5)*.62+(detail-.5)*.028+(ridge-.65)*.025;
 if(form==3)e-=pow(clamp(1.-ridge,0.,1.),2.)*.17;
 if(form==4)e+=(ridge-.65)*.18+(detail-.5)*.06;
 if(form==5)e=.54+(broad-.5)*.72+(detail-.5)*.045-abs(n.y)*.025;
 if(form==6){float t=clamp((broad-.3)/.4,0.,1.);t=t*t*(3.-2.*t);e=.36+t*.3+(detail-.5)*.065+(ridge-.65)*.05;}
 if(form==7)e=.48+(broad-.5)*.56+(ridge-.65)*.22-(detail-.5)*.08;
 if(water<.15)e=max(e,.36+.285*water+.025);
 return clamp(e,.06,.94);
}
)GLSL"
