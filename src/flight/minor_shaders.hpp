#pragma once
namespace hyperion::flight {
// Static, direction-anchored material data. Alpha is relief, never opacity.
inline constexpr const char* minorBakeFragment=R"GLSL(#version 330
out vec4 finalColor;
uniform vec2 resolution;uniform float seed,hue,ice,metal,kind;
uniform vec4 basins[12];uniform float basinDepth[12];
float hash(vec3 p){p=fract(p*.1031);p+=dot(p,p.yzx+33.33);return fract((p.x+p.y)*p.z);}
vec3 random3(vec3 p){return vec3(hash(p),hash(p+19.17),hash(p+47.63));}
float noise(vec3 p){vec3 i=floor(p),f=fract(p);f=f*f*(3.-2.*f);return mix(mix(mix(hash(i),hash(i+vec3(1,0,0)),f.x),mix(hash(i+vec3(0,1,0)),hash(i+vec3(1,1,0)),f.x),f.y),mix(mix(hash(i+vec3(0,0,1)),hash(i+vec3(1,0,1)),f.x),mix(hash(i+vec3(0,1,1)),hash(i+vec3(1,1,1)),f.x),f.y),f.z);}
float filtered(vec3 p){return mix(.5,noise(p),1.-smoothstep(.35,1.4,length(fwidth(p))));}
vec3 pigment(float h){return pow(.5+.5*cos(6.283185*(h+vec3(0,.68,.34))),vec3(1.25));}
vec3 smallImpacts(vec3 n){
 float scale=kind<.5?19.:11.;vec3 cell=floor(n*scale);float height=0.,cavity=0.,apron=0.;
 for(int z=-1;z<=1;z++)for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++){
  vec3 id=cell+vec3(x,y,z),r=random3(id+seed),c=id+r;
  if(abs(length(c)-scale)>.8||r.z<(kind<.5?.48:.87))continue;
  float d=length(n-normalize(c))*scale/(.22+r.y*.46);
  if(d>1.7)continue;
  float bowl=1.-smoothstep(.1,.88,d),rim=exp(-pow((d-.98)*9.,2.));
  height+=rim*.09-bowl*.10;cavity=max(cavity,bowl);apron=max(apron,rim);
 }
 return vec3(height,cavity,apron);
}
void main(){
 vec2 uv=gl_FragCoord.xy/resolution;float phi=uv.x*6.283185-3.14159265,theta=(uv.y-.5/resolution.y)*(resolution.y/(resolution.y-1.))*3.14159265;
 vec3 n=vec3(sin(theta)*cos(phi),cos(theta),sin(theta)*sin(phi));
 vec3 p=n*5.1+seed;float province=noise(p),warp=noise(p*2.7+13.);
 float rubble=filtered(n*89.+seed),grain=filtered(n*137.+seed*.37),fine=filtered(n*311.+71.);
 float layer=dot(n,normalize(vec3(.4+sin(seed),.7,.2+cos(seed))))*173.+province*23.+warp*5.;
 float bedding=.5+.5*sin(layer),vein=1.-smoothstep(.014,.065,abs(noise(n*31.+warp*2.+seed)-.5));
 float fracture=1.-smoothstep(.007,.036,abs(noise(n*61.+seed*.71)-.5));
 float floorMask=0.,rim=0.,ejecta=0.,terrace=0.;
 for(int i=0;i<12;i++){
  float u=(1.-dot(n,basins[i].xyz))/basins[i].w;
  if(u>3.)continue;
  float bowl=max(0.,1.-u),edge=exp(-pow((u-.94)*10.,2.));
  floorMask=max(floorMask,bowl);rim=max(rim,edge);
  vec3 axis=basins[i].xyz,east=normalize(cross(axis,abs(axis.y)>.9?vec3(1,0,0):vec3(0,1,0))),north=cross(axis,east);
  float angle=atan(dot(n,north),dot(n,east)),raysPhase=angle*(17.+float(i))+warp*2.;
  float rayResolved=1.-smoothstep(.5,2.,fwidth(raysPhase));
  float rays=.36+.64*pow(.5+.5*sin(raysPhase),4.)*rayResolved;
  float steps=sin(u*41.+warp*.8)*(1.-smoothstep(.45,1.8,fwidth(u*41.)));
  terrace=max(terrace,(.5+.5*steps)*smoothstep(.25,.47,u)*(1.-smoothstep(.72,.94,u)));
  ejecta=max(ejecta,exp(-pow((u-1.12)*2.9,2.))*rays*clamp(basinDepth[i]*18.,.2,1.));
 }
 vec3 impact=vec3(0.);if(kind<1.5)impact=smallImpacts(n)*(1.-floorMask*.72);
 if(kind>.5&&kind<1.5)impact*=.58;
 float age=smoothstep(.25,.7,noise(n*7.9+seed*.13));if(kind<.5)impact*=.45+age*.55;
 float pit=kind>1.5?smoothstep(.53,.72,noise(n*37.+warp*2.+seed))*(.5+noise(n*13.+seed)*.5):0.;
 float rubbleFace=pow(abs(noise(n*43.+seed)-.5)*2.,.65);
 float grains=filtered(n*487.+seed*.29),breccia=smoothstep(.56,.77,filtered(n*193.+seed));
 float dust=smoothstep(.26,.72,province*.5+rubble*.5)*(1.-floorMask*.45);
 float frost=smoothstep(.25,.78,province*.5+warp*.35+grain*.15)*ice;
 vec3 basalt=mix(vec3(.024,.028,.043),vec3(.16,.125,.095),province);
 vec3 ore=mix(vec3(.055,.065,.10),vec3(.22,.19,.16),warp);
 vec3 rock=mix(basalt,ore,metal*.55);
 rock*=.70+rubble*.30+grain*.18+bedding*.11;if(kind>.5&&kind<1.5)rock*=.65+rubbleFace*.65;
 rock=mix(rock,vec3(.115,.095,.078)*(.7+grain*.5),dust*.38);
 rock=mix(rock,vec3(.20,.31,.36)*(.70+grain*.22),frost*.82);
 vec3 spectral=pigment(hue+province*.61+warp*.18);
 rock=mix(rock,rock*.58+spectral*.19,.35+vein*.38);
 rock=mix(rock,rock*.65+spectral*.045+vec3(.022,.018,.014),breccia*(1.-dust*.55)*.32);
 rock*=.94+grains*.12;rock*=1.-floorMask*.27-impact.y*.20-fracture*(.12+ice*.23)-pit*.45;
 rock+=spectral*terrace*.013;
 rock+=mix(vec3(.065,.055,.045),spectral*.085,.48)*(rim*.65+ejecta*.40+impact.z*.30);
 rock+=pigment(hue+.47+province*.14)*vein*(.018+ice*.025);
 if(kind>1.5)rock=mix(rock,rock*.5+vec3(.12,.22,.25),smoothstep(.58,.75,rubble)*frost*.40);
 float relief=.5+impact.x+(rubble-.5)*.035+(grain-.5)*.014+(fine-.5)*.004;
 relief+=(bedding-.5)*.010+(terrace-.25)*.008+(grains-.5)*.004-fracture*(.015+ice*.035)+vein*.006-pit*.075;
 if(kind>.5&&kind<1.5)relief+=(rubbleFace-.5)*.055;
 finalColor=vec4(max(rock,vec3(.003)),clamp(relief,.06,.94));
})GLSL";
inline constexpr const char* minorFragment=R"GLSL(#version 330
in vec3 worldPosition,normal,localPosition;out vec4 finalColor;
uniform sampler2D materialTexture,detailTexture,grainTexture;
uniform vec3 lightDirection,eye;uniform float hue,ice,metal,visibility,radius,detailBlend;
vec3 pigment(float h){return .5+.5*cos(6.283185*(h+vec3(0,.68,.34)));}
vec4 field(sampler2D tex,vec3 p){
 vec2 size=vec2(textureSize(tex,0)),uv=vec2(atan(p.z,p.x)/6.283185+.5,acos(clamp(p.y,-1.,1.))/3.14159265);
 vec2 dx=dFdx(uv),dy=dFdy(uv);dx.x-=round(dx.x);dy.x-=round(dy.x);
 uv.y=uv.y*(1.-1./size.y)+.5/size.y;dx.y*=1.-1./size.y;dy.y*=1.-1./size.y;
 return textureGrad(tex,uv,dx,dy);
}
vec3 reliefNormal(vec3 n,float h){
 vec3 dx=dFdx(localPosition),dy=dFdy(localPosition),a=cross(dy,n),b=cross(n,dx);float det=dot(dx,a);
 vec3 gradient=sign(det)*(dFdx(h)*a+dFdy(h)*b)*radius*.052/max(abs(det),1e-9);
 float g=length(gradient);gradient*=min(1.,.85/max(g,.001));return normalize(n-gradient);
}
vec3 smoothFieldNormal(vec3 p,vec3 geometric,vec3 fallback){
 vec2 size=vec2(textureSize(detailTexture,0)),uv=vec2(atan(p.z,p.x)/6.283185+.5,acos(clamp(p.y,-1.,1.))/3.14159265);
 vec2 dx=dFdx(uv),dy=dFdy(uv);dx.x-=round(dx.x);dy.x-=round(dy.x);
 uv.y=uv.y*(1.-1./size.y)+.5/size.y;dx.y*=1.-1./size.y;dy.y*=1.-1./size.y;
 float blend=(1.-smoothstep(.18,.60,max(length(dx*size),length(dy*size))))*detailBlend*(1.-smoothstep(.985,.999,abs(p.y)));
 if(blend<.001)return fallback;
 // Central slopes interpolate continuously across enlarged height texels.
 // Use the same gradients as the colour lookup, including longitude wrapping.
 vec2 texel=1./size;
 float du=(textureGrad(detailTexture,uv+vec2(texel.x,0),dx,dy).a-textureGrad(detailTexture,uv-vec2(texel.x,0),dx,dy).a)*size.x*.5;
 float dv=(textureGrad(detailTexture,uv+vec2(0,texel.y),dx,dy).a-textureGrad(detailTexture,uv-vec2(0,texel.y),dx,dy).a)*size.y*.5;
 vec3 east=normalize(vec3(-p.z,0,p.x)),south=cross(p,east);
 vec3 gradient=(east*du/max(length(p.xz)*6.283185,.05)+south*dv/3.14159265)*.052;
 gradient-=geometric*dot(gradient,geometric);gradient*=min(1.,.85/max(length(gradient),.001));
 return normalize(mix(fallback,normalize(geometric-gradient),blend));
}
void main(){
 vec3 geometric=normalize(normal),p=normalize(localPosition),v=normalize(eye-worldPosition),l=normalize(lightDirection);
 vec4 material=field(materialTexture,p);if(detailBlend>.001)material=mix(material,field(detailTexture,p),detailBlend);
 // Close grains are anchored in body-local space, independent of the
 // spherical atlas. Explicit mip gradients avoid pole seams and shimmer.
 vec3 q=localPosition*.29,dx=dFdx(q),dy=dFdy(q),w=pow(abs(geometric),vec3(4.));w/=max(dot(w,vec3(1.)),.001);
 float close=1.-smoothstep(.045,.20,max(length(dx),length(dy)));vec4 grain=vec4(.5,0.,.5,.5);
 if(close>.001)grain=mix(grain,textureGrad(grainTexture,q.yz,dx.yz,dy.yz)*w.x+textureGrad(grainTexture,q.zx,dx.zx,dy.zx)*w.y+textureGrad(grainTexture,q.xy,dx.xy,dy.xy)*w.z,close);
 float pebble=smoothstep(.51,.76,grain.r)*(1.-grain.g)*close;
 material.rgb*=.93+grain.a*.14;material.rgb=mix(material.rgb,material.rgb*.72+pigment(hue+grain.b*.13)*.025,pebble*.24);
 vec3 n=reliefNormal(geometric,material.a);
 if(detailBlend>.001)n=smoothFieldNormal(p,geometric,n);
 float microHeight=((grain.r-.5)*.014-grain.g*.003)/max(radius*.052,.001)*close;
 n=reliefNormal(n,microHeight);float variance=min(.20,max(dot(dFdx(n),dFdx(n)),dot(dFdy(n),dFdy(n))));
 float rough=clamp(mix(.90,.65,ice)*(1.-metal*.08)+(grain.a-.5)*close*.12,.57,.99);rough=pow(min(1.,pow(rough,4.)+variance),.25);
 float nl=max(dot(n,l),0.),nv=max(dot(n,v),.001);vec3 h=normalize(l+v);
 float a=rough*rough,a2=a*a,nh=max(dot(n,h),0.),vh=max(dot(v,h),0.),den=nh*nh*(a2-1.)+1.;
 float distribution=a2/max(3.14159265*den*den,.00001),k=(rough+1.)*(rough+1.)/8.;
 float masking=nv/(nv*(1.-k)+k)*nl/(nl*(1.-k)+k);
 vec3 f0=mix(vec3(.025),material.rgb*.35,metal*.35),f=f0+(1.-f0)*pow(1.-vh,5.);
 vec3 spec=f*distribution*masking/max(4.*nv*max(nl,.001),.001);
 float cavity=mix(.70,1.,smoothstep(.36,.53,material.a));
 vec3 direct=((1.-f)*material.rgb/3.14159265+spec)*nl*3.3;
 vec3 ambient=material.rgb*vec3(.045,.055,.085)*cavity;
 float edge=pow(1.-nv,4.);vec3 opal=pigment(hue+dot(p,v)*.12)*edge*.012*max(0.,dot(geometric,l));
 finalColor=vec4(direct*cavity+ambient+opal,visibility);
})GLSL";
}
