#pragma once
namespace hyperion::flight {
// Shared, periodic material features. Bake once, then filter three planar
// samples at runtime instead of searching neighbouring cells in every pixel.
inline constexpr const char* detailBakeFragment=R"GLSL(#version 330
out vec4 finalColor;
uniform sampler2D noiseTexture;uniform vec2 resolution;
vec3 cellRandom(vec2 p){return textureLod(noiseTexture,(mod(p,16.)+.5)/256.,0.).rgb;}
void main(){
 vec2 uv=gl_FragCoord.xy/resolution,p=uv*16.,cell=floor(p),f=fract(p);
 float nearest=10.,second=10.,tint=.5;
 for(int y=-2;y<=2;y++)for(int x=-2;x<=2;x++){
  vec2 offset=vec2(x,y);vec3 r=cellRandom(cell+offset);if(r.b<.21)continue;vec2 delta=offset+.01+r.rg*.98-f;float d=dot(delta,delta);
  if(d<nearest){second=nearest;nearest=d;tint=r.b;}else second=min(second,d);
 }
 float edge=sqrt(second)-sqrt(nearest),crack=1.-smoothstep(.010,.047,edge);
 float dome=pow(clamp(1.-sqrt(nearest)*1.05,0.,1.),.8);
 float grain=textureLod(noiseTexture,uv,0.).r*.54+textureLod(noiseTexture,uv*2.,0.).g*.29+textureLod(noiseTexture,uv*4.,0.).b*.17;
 float chipped=smoothstep(.60,.83,grain)*crack;
 float height=.22+dome*.53-crack*(.11+grain*.05)+(grain-.5)*.065-chipped*.018;
 finalColor=vec4(height,crack,tint,grain);
})GLSL";
}

#define PRISM_SURFACE_DETAIL_GLSL R"GLSL(
#ifdef TERRAIN_DETAIL
uniform sampler2D detailTexture;
struct LocalMaterial {vec3 colour,glow;float height,roughness,cavity,vein;};
vec4 detailField(vec3 p,vec3 N,vec3 dx,vec3 dy){
 vec3 w=pow(abs(N),vec3(4.));w/=max(dot(w,vec3(1.)),.0001);
 return textureGrad(detailTexture,p.yz,dx.yz,dy.yz)*w.x+
        textureGrad(detailTexture,p.zx,dx.zx,dy.zx)*w.y+
        textureGrad(detailTexture,p.xy,dx.xy,dy.xy)*w.z;
}
LocalMaterial closeMaterial(vec3 p,vec3 radial,vec3 faceNormal,vec3 dx,vec3 dy,vec3 geological,vec3 flow){
 float footprint=max(length(dx),length(dy)),fine=1.-smoothstep(.035,.28,footprint);
 float strata=noise(p*.14),deposit=noise(p*.047+31.),warp=noise(p*.31+11.);
 // Sand is a continuous granular material. It needs wind ripples and one
 // filtered 3D grain sample, rather than three fractured-rock projections.
 if(kind>.5&&kind<1.5){
  float grit=mix(.5,noise(p*14.+7.),fine),phase=dot(p,vec3(.64,.17,.49))*4.8+strata*7.+warp*2.;
  float resolved=1.-smoothstep(.45,1.8,fwidth(phase)),ripple=mix(.375,pow(.5+.5*sin(phase),2.),resolved),crust=smoothstep(.57,.75,deposit);
  float secondaryPhase=phase*2.31+warp*3.,secondaryResolved=1.-smoothstep(.45,1.8,fwidth(secondaryPhase));
  float rippleFine=sin(secondaryPhase)*secondaryResolved;
  float crack=(1.-smoothstep(.012,.065,abs(strata-.5)))*crust;
  vec3 colour=mix(vec3(.18,.080,.032),vec3(.39,.25,.105),strata)*(.72+ripple*.22+grit*.22);
  colour=mix(colour,vec3(.31,.30,.22)*(.7+warp*.3),crust*.50);
  colour*=1.+rippleFine*.035;
  colour=colour*.85+geological*.12+pigment(seed*.031+strata*.55+warp*.08)*.086+flow*.15;
  return LocalMaterial(colour,vec3(0.),(ripple-.375)*.095+rippleFine*.008+(grit-.5)*.010+crust*(warp-.5)*.045-crack*.018,.78+grit*.14,1.-crack*.22,0.);
 }
 vec3 coords=p/12.+vec3(strata,warp,deposit)*.30+seed*vec3(.17,.31,.23);
 vec4 field=detailField(coords,faceNormal,dFdx(coords),dFdy(coords));
 // The packed grain channel has finer spatial frequencies than the stone
 // height. Reuse that filtered sample for inclusions instead of another
 // three-way triplanar lookup in the already substantial ground shader.
 float grit=mix(.5,field.a,fine),crack=field.g,stone=field.r,grainTone=field.b;
 float inclusions=smoothstep(.57,.76,field.a)*(1.-field.g)*fine;
 float slope=1.-clamp(dot(faceNormal,radial),0.,1.),dust=smoothstep(.25,.70,deposit)*(1.-smoothstep(.12,.65,slope));
 LocalMaterial m=LocalMaterial(vec3(0.),vec3(0.),0.,.75,1.,0.);float organic=0.,bedding=.5;
 // Uniform material branches avoid evaluating soil layers and mineral colour
 // work that ice, basalt and chemical crust replace in full.
 if(kind<.5||kind>4.5){
  float layer=dot(p,normalize(vec3(.4+sin(seed),.7,.3+cos(seed))))*3.4+strata*7.;
  float layerResolved=1.-smoothstep(.6,2.,fwidth(layer));bedding=mix(.5,.5+.5*sin(layer),layerResolved);
  m.vein=thread(strata*24.+warp*.8,.035)*(1.-smoothstep(.12,.75,footprint));
  vec3 mineral=mix(vec3(.095,.080,.060),vec3(.27,.23,.17),strata);
  mineral=mix(mineral,geological*(.55+stone*.75),.42);
  float gravel=smoothstep(.38,.68,warp)*(1.-dust*.75);
  mineral*=.73+stone*gravel*.20+grit*.23+bedding*.12;
  mineral=mix(mineral,mineral*.68+geological*.26+vec3(.026,.021,.012),inclusions*gravel*.35);mineral*=1.-crack*gravel*.13;
  mineral=mix(mineral,vec3(.20,.15,.09)*(.65+grit*.6),dust*.24);
  m.colour=mineral;m.height=(stone-.5)*gravel*.07+(strata-.5)*.08+(grit-.5)*.012+(field.a-.5)*gravel*.012;
  m.roughness=.74+grit*.18;m.cavity=1.-crack*gravel*.13;
  organic=life*smoothstep(.42,.69,deposit)*(1.-smoothstep(.2,.7,slope));
  m.colour=mix(m.colour,m.colour*.45+vec3(.018,.065,.039)+pigment(seed*.031+grainTone*.22)*.065,organic*.65);
 }
 if(kind>1.5&&kind<2.5){
  float frost=smoothstep(.42,.72,deposit+grit*.13),stress=abs(noise(p*.32+warp*2.)-.5);
  float seam=max(crack*smoothstep(.38,.68,warp)*.55,1.-smoothstep(.012,.055,stress))*(1.-frost*.45);
  m.colour=mix(vec3(.045,.18,.23),vec3(.39,.64,.70),.3+deposit*.45+stone*.15);
  m.colour=mix(m.colour,vec3(.48,.67,.70)*(.8+grit*.25),frost*.62);
  m.colour=mix(m.colour,vec3(.006,.025,.060),seam*.78);
  m.height=(deposit-.5)*.09-seam*.095+(grit-.5)*frost*.006;
  float bubbles=inclusions*frost;m.colour*=1.-bubbles*.07;
  m.height+=(field.a-.5)*frost*.009;m.roughness=mix(.25+grit*.17,.58,frost*.78);m.cavity=1.-seam*.38;
  m.glow=pigment(seed*.031+grainTone*.23+.42)*seam*.035;m.vein=seam*.7;
 }
 if(kind>2.5&&kind<3.5){
  float ash=smoothstep(.54,.76,deposit),rim=smoothstep(.16,.58,stone);crack*=smoothstep(.25,.63,warp+grainTone*.15);
  m.colour=mix(vec3(.011,.016,.024),vec3(.065,.040,.033),grainTone)*(.65+rim*.65+grit*.18);
  m.colour=mix(m.colour,vec3(.14,.115,.105)*(.65+grit*.35),ash*.55);m.colour*=1.-crack*.65;
  m.colour=mix(m.colour,m.colour*.68+pigment(seed*.031+field.b*.18)*.027,inclusions*(1.-ash)*.28);
  m.height=rim*.064-crack*.060+(grit-.5)*.018+(field.a-.5)*(1.-ash)*.016;m.roughness=.72+ash*.17;m.cavity=1.-crack*.45;
  m.glow=mix(vec3(.85,.065,.008),pigment(seed*.031+grainTone*.25)*.4,.28)*crack*(1.-ash*.8)*.18;m.vein=crack*.5;
 }
 if(kind>3.5&&kind<4.5){
  float salt=smoothstep(.48,.71,deposit),damp=smoothstep(.58,.78,warp)*(1.-salt);crack*=salt*.8+.15;
  m.colour=mix(vec3(.060,.085,.018),vec3(.28,.26,.075),stone*.6+grainTone*.4);
  m.colour=mix(m.colour,vec3(.34,.40,.22)*(.7+grit*.3),salt*.7);m.colour*=1.-crack*.32;
  m.colour+=pigment(seed*.031+field.b*.11)*inclusions*salt*.016;
  m.height=(stone-.5)*.11-crack*.035+(grit-.5)*salt*.025+(field.a-.5)*salt*.009;m.roughness=mix(.82,.23,damp);
  m.cavity=1.-crack*.35;m.glow=pigment(seed*.031+grainTone*.3+.6)*crack*.026;m.vein=crack*.45;
 }
 // The stable material supplies relief; the live pigment remains visible in
 // its body colour, mineral seams and the grazing opalescent sheen.
 m.colour=m.colour*.85+geological*.12+pigment(seed*.031+strata*.55+grainTone*.08)*.086+flow*.15;
 m.colour+=pigment(seed*.031+bedding*.15)*m.vein*.035;
 m.glow+=pigment(seed*.031+grainTone*.2)*organic*crack*.012;
 return m;
}
#endif
)GLSL"
