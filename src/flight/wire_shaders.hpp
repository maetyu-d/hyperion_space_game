#pragma once
// Analytic lines are filtered in screen space, including their disappearance
// below a pixel. They do not rely on driver-dependent polygon line widths.
#define HYPERION_VECTOR_GLSL R"GLSL(
uniform float wireframe,wirePixelScale;
float vectorLine(float coordinate){float footprint=fwidth(coordinate);float d=abs(fract(coordinate+.5)-.5)/max(footprint,.000001);return (1.-smoothstep(.55*wirePixelScale,1.25*wirePixelScale,d))*(1.-smoothstep(.25,.65,footprint));}
vec3 vectorSurface(vec3 p,vec3 normal,vec3 view,float radius,float seed,float closePass){
 vec3 n=normalize(p);float longitude=atan(n.z,n.x)/6.283185,latitude=acos(clamp(n.y,-1.,1.))/3.14159265;
 float polar=smoothstep(.025,.10,length(n.xz));
 float orbital=max(vectorLine(longitude*36.)*polar,vectorLine(latitude*18.));
 // Continuous, world-anchored nested survey grids retain a readable scale
 // through descent. Tiny unresolved lines fade instead of sparkling.
 vec2 survey=vec2(longitude*6.283185*radius,latitude*3.14159265*radius);
 float footprint=max(length(dFdx(survey)),length(dFdy(survey)));
 float lod=log(max(.125,footprint*32.))/log(4.),level=floor(lod),blend=fract(lod),spacing=pow(4.,level);
 vec2 fine=survey/spacing,coarse=fine*.25;
 float fineGrid=max(vectorLine(fine.x)*polar,vectorLine(fine.y)),coarseGrid=max(vectorLine(coarse.x)*polar,vectorLine(coarse.y));
 float closeGrid=max(fineGrid*(1.-smoothstep(.25,.8,blend)),coarseGrid*(.25+.75*smoothstep(0.,.65,blend)));
 float elevation=(length(p)-radius)/(spacing*.35),contours=vectorLine(elevation)*smoothstep(.002,.04,fwidth(elevation));
 float detail=1.-smoothstep(.0001,.0007,max(length(dFdx(n)),length(dFdy(n))));
 float line=mix(orbital,closeGrid,detail);float nv=abs(dot(normalize(normal),normalize(view)));float rim=1.-smoothstep(0.,max(.001,fwidth(nv)*1.7*wirePixelScale),nv);
 vec3 cyan=vec3(.12,.87,1.18),violet=vec3(.76,.24,1.06),gold=vec3(1.10,.64,.22);
 vec3 colour=mix(cyan,violet,.22+.18*sin(seed*.31+n.y*2.));
 vec3 fill=vec3(.00045,.0012,.0022)+vec3(.001,.002,.003)*max(0.,dot(normalize(normal),vec3(.6,.7,-.4)));
 return fill+colour*line*.42+mix(gold,cyan,.60)*contours*detail*.30+mix(cyan,vec3(.72,1.,1.1),.6)*rim*.85;
}
)GLSL"
#define HYPERION_WIRE_EDGE_GLSL R"GLSL(
noperspective in vec3 vectorBary;flat in vec3 vectorFeatures;
uniform float wirePixelScale;
float vectorEdge(){vec3 distances=vectorBary/max(fwidth(vectorBary),vec3(.00001));distances=mix(vec3(100000.),distances,vectorFeatures);return min(distances.x,min(distances.y,distances.z));}
vec3 vectorInk(vec3 n,vec3 view,vec3 tint){
 float distance=vectorEdge()/max(1.,wirePixelScale),edge=1.-smoothstep(.55,1.25,distance),core=1.-smoothstep(.0,.48,distance),halo=exp(-distance*distance*.32)*.075;
 float resolution=1.-smoothstep(.45,1.3,max(fwidth(vectorBary.x),max(fwidth(vectorBary.y),fwidth(vectorBary.z))));
 float nv=abs(dot(normalize(n),normalize(view))),rim=1.-smoothstep(0.,max(.001,fwidth(nv)*1.4*wirePixelScale),nv);
 return vec3(.0005,.0012,.0022)+tint*((edge*.78+halo)*resolution+rim*.45)+vec3(.70,.92,1.)*core*.18*resolution;
}
)GLSL"
namespace hyperion::flight {
inline constexpr const char* wireVertex=R"GLSL(#version 330
in vec3 vertexPosition,vertexNormal;in vec2 vertexTexCoord2;in vec4 vertexTangent;
uniform mat4 mvp,matModel,matNormal;
out vec3 worldPosition,normal,localPosition,vectorLocalNormal;noperspective out vec3 vectorBary;flat out vec3 vectorFeatures;
void main(){vectorLocalNormal=vertexNormal;worldPosition=(matModel*vec4(vertexPosition,1)).xyz;normal=normalize(mat3(matNormal)*vertexNormal);localPosition=vertexPosition;vectorBary=vec3(vertexTexCoord2,1.-vertexTexCoord2.x-vertexTexCoord2.y);vectorFeatures=vertexTangent.xyz;gl_Position=mvp*vec4(vertexPosition,1);}
)GLSL";
inline constexpr const char* wireFragment=R"GLSL(#version 330
in vec3 worldPosition,normal,localPosition,vectorLocalNormal;out vec4 finalColor;
uniform vec3 eye,wireTint;uniform float wireKind;
)GLSL" HYPERION_WIRE_EDGE_GLSL R"GLSL(
void main(){
 gl_FragDepth=gl_FragCoord.z*(abs(wireKind-2.)<.1?.0001:1.);vec3 c=vectorInk(normal,eye-worldPosition,wireTint);
 if(wireKind>3.5){
  vec3 axis=abs(vectorLocalNormal);vec2 uv=axis.y>.65?localPosition.xz:axis.x>axis.z?localPosition.zy:localPosition.xy;
  vec2 cell=uv/vec2(10.,5.),q=abs(fract(cell)-.5)-vec2(.34,.24);float box=max(q.x,q.y);
  float d=abs(box)/max(fwidth(box),.00001),resolved=1.-smoothstep(.035,.14,max(length(dFdx(cell)),length(dFdy(cell))));
  c+=wireTint*(1.-smoothstep(.45*wirePixelScale,1.15*wirePixelScale,d))*resolved*.10;
 }
 finalColor=vec4(c,1.);
}
)GLSL";
}
