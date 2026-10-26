#pragma once
// Liquid detail is a shading field, anchored in planet-local metres. It never
// displaces the surveyed ground or changes the ship's landing surface.
#define PRISM_WATER_GLSL R"GLSL(
struct LiquidSurface {vec3 normal;float roughness,caustic,foam,wet,coverage;};
LiquidSurface liquidSurface(vec3 n,vec3 dx,vec3 dy,float elevation,float sea,float land,float ice,float coastPixel){
 float fluid=(kind<1.5||(kind>3.5&&kind<4.5))&&water>.02?1.:0.;
 float wet=(1.-smoothstep(.002,.025,abs(elevation-sea-.006)))*fluid*(1.-ice);
 float coverage=(1.-land)*(1.-ice)*fluid;
 LiquidSurface result=LiquidSurface(n,.20,0.,0.,wet,coverage);
 if(groundPass>1.5){result.wet=0.;result.coverage=0.;return result;}
 if(fluid<.5||(coverage<.001&&wet<.001))return result;
 vec3 p=n*radius;
 const vec3 a=vec3(.350,.058,.273),b=vec3(-.502,.324,1.538),c=vec3(4.248,.531,-3.139);
 vec3 phase=vec3(dot(p,a)-time*.94+seed*.41,dot(p,b)-time*1.41+seed*.71,dot(p,c)-time*2.37+seed*1.13);
 vec3 pixel=abs(vec3(dot(dx,a),dot(dx,b),dot(dx,c)))+abs(vec3(dot(dy,a),dot(dy,b),dot(dy,c)));
 vec3 resolved=1.-smoothstep(vec3(.45),vec3(1.65),pixel);
 if(all(equal(resolved,vec3(0.)))&&wet<.001){result.roughness=.24;return result;}
 vec3 s=sin(phase),slope=cos(phase)*resolved;
 vec3 gradient=a*(slope.x*.28)+b*(slope.y*.055)+c*(slope.z*.012);
 gradient-=n*dot(gradient,n);result.normal=normalize(n-gradient);
 // Unresolved ripples widen the highlight instead of becoming point glints.
 result.roughness=.18+dot(1.-resolved,vec3(.028,.020,.012));
 float crossing=1.-clamp(abs(s.x*.70+s.y*.70),0.,1.);
 result.caustic=pow(crossing,9.)*resolved.y;
 float washPhase=phase.x*.23-time*.57;
 float wash=sin(washPhase)*.0018*(1.-smoothstep(.5,1.8,pixel.x*.23));
 float edge=1.-smoothstep(.0012,.004+max(coastPixel*1.5,.00035),abs(elevation-sea-wash));
 float breaking=.35+.65*max(0.,s.x*resolved.x),lace=1.;
 if(wet>.001){
  float grainResolved=1.-smoothstep(.3,1.2,max(length(dx),length(dy))*1.8);
  float grain=mix(.5,noise(p*1.8+vec3(time*.08,0.,-time*.04)),grainResolved);
  lace=.38+.62*smoothstep(.25,.70,grain);
  result.wet=(1.-smoothstep(.002,.025,abs(elevation-sea-.006-wash)))*fluid*(1.-ice);
 }
 float whitecaps=smoothstep(.77,.97,s.x*.68+s.y*.23+s.z*.09)*resolved.x;
 result.foam=(edge*breaking*lace+whitecaps*coverage*.065)*fluid*(1.-ice);
 return result;
}
vec3 liquidReflection(vec3 N,vec3 V,vec3 radial,vec3 L,vec3 flowingColour,float daylight){
 vec3 r=reflect(-V,N);float horizon=exp(-abs(dot(r,radial))*3.8);
 vec3 air=mix(vec3(.018,.045,.10),vec3(.12,.21,.26),horizon);
 air+=vec3(.30,.105,.035)*pow(max(dot(r,L),0.),8.)*.35;
 vec3 spectral=flowingColour*.25+pigment(seed*.031+r.z*.22+r.y*.17)*.018;
 return mix(vec3(.008,.014,.035)+spectral,air*daylight+spectral*.15,clamp(atmosphere,0.,1.));
}
)GLSL"
