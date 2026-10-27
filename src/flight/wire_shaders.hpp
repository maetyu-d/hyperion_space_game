#pragma once
#define HYPERION_CGA_INK_GLSL R"GLSL(
const vec3 vectorCyan=vec3(.09,1.,1.),vectorMagenta=vec3(1.,.09,1.),vectorWhite=vec3(1.);
)GLSL"
#define HYPERION_VECTOR_GLSL HYPERION_CGA_INK_GLSL R"GLSL(
uniform float wireframe,wirePixelScale,wireEGA;
float vectorLine(float coordinate){float footprint=fwidth(coordinate);float d=abs(fract(coordinate+.5)-.5)/max(footprint,.000001);return (1.-smoothstep(.45*wirePixelScale,1.05*wirePixelScale,d))*(1.-smoothstep(.22,.55,footprint));}
vec3 vectorSurface(vec3 p,vec3 normal,vec3 view,float radius,float seed,float closePass){
 vec3 n=normalize(p);float longitude=atan(n.z,n.x)/6.283185,latitude=acos(clamp(n.y,-1.,1.))/3.14159265;
 float polar=smoothstep(.025,.10,length(n.xz));float orbital=max(vectorLine(longitude*24.)*polar,vectorLine(latitude*12.));
 vec2 survey=vec2(longitude*6.283185*radius,latitude*3.14159265*radius);
 float footprint=max(length(dFdx(survey)),length(dFdy(survey))),lod=log(max(.125,footprint*40.*wirePixelScale))/log(4.),blend=fract(lod),spacing=pow(4.,floor(lod));
 vec2 fine=survey/spacing,coarse=fine*.25;
 float fineGrid=max(vectorLine(fine.x)*polar,vectorLine(fine.y)),coarseGrid=max(vectorLine(coarse.x)*polar,vectorLine(coarse.y));
 float closeGrid=max(fineGrid*(1.-smoothstep(.25,.8,blend)),coarseGrid*(.25+.75*smoothstep(0.,.65,blend)));
 float slope=1.-abs(dot(normalize(normal),n));float diagonals=vectorLine(fine.x+fine.y)*smoothstep(.02,.13,slope)*(1.-smoothstep(.18,.6,blend))*.35;
 float elevation=(length(p)-radius)/(spacing*.35),contours=vectorLine(elevation)*smoothstep(.002,.04,fwidth(elevation));
 float detail=1.-smoothstep(.0001,.0007,max(length(dFdx(n)),length(dFdy(n))));
 float nv=abs(dot(normalize(normal),normalize(view))),rim=1.-smoothstep(0.,max(.001,fwidth(nv)*1.4*wirePixelScale),nv);
 return (wireEGA>.5?vec3(.06,.30,1.):vectorCyan)*(mix(orbital,closeGrid,detail)*.60+diagonals*detail*.25)+(wireEGA>.5?vec3(1.,.65,.09):vectorMagenta)*contours*detail*.40+vectorWhite*rim*.90;
}
)GLSL"
#define HYPERION_WIRE_VERTEX_GLSL R"GLSL(
in vec2 vertexTexCoord,vertexTexCoord2;in vec4 vertexTangent;
noperspective out vec3 vectorBary;flat out vec3 vectorFeatures,vectorAdjacentA,vectorAdjacentB,vectorAdjacentC;
vec3 vectorDecode(vec2 p){vec3 n=vec3(p,1.-abs(p.x)-abs(p.y));if(n.z<0.)n.xy=(1.-abs(n.yx))*vec2(n.x<0.?-1.:1.,n.y<0.?-1.:1.);return normalize(n);}
void vectorGeometry(mat4 normalMatrix,vec3 shape){
 int corner=gl_VertexID%3;vectorBary=vec3(corner==0?1.:0.,corner==1?1.:0.,corner==2?1.:0.);
 int mask=int(vertexTangent.z+.5);vectorFeatures=vec3((mask&1)!=0?1.:0.,(mask&2)!=0?1.:0.,(mask&4)!=0?1.:0.);
 vectorAdjacentA=normalize(mat3(normalMatrix)*(vectorDecode(vertexTexCoord)/shape));vectorAdjacentB=normalize(mat3(normalMatrix)*(vectorDecode(vertexTexCoord2)/shape));vectorAdjacentC=normalize(mat3(normalMatrix)*(vectorDecode(vertexTangent.xy)/shape));
}
)GLSL"
#define HYPERION_WIRE_EDGE_GLSL HYPERION_CGA_INK_GLSL R"GLSL(
noperspective in vec3 vectorBary;flat in vec3 vectorFeatures,vectorAdjacentA,vectorAdjacentB,vectorAdjacentC;
uniform float wirePixelScale,wireEGA;
vec3 vectorInk(vec3 n,vec3 view,vec3 tint){
 view=normalize(view);vec3 facing=vec3(dot(vectorAdjacentA,view),dot(vectorAdjacentB,view),dot(vectorAdjacentC,view));
 vec3 silhouettes=1.-smoothstep(vec3(-.008),vec3(.008),facing),enabled=max(vectorFeatures,silhouettes);
 vec3 d=vectorBary/max(fwidth(vectorBary),vec3(.00001))/max(1.,wirePixelScale);
 vec3 creases=(1.-smoothstep(vec3(.38),vec3(.94),d))*vectorFeatures;
 vec3 outline=(1.-smoothstep(vec3(.48),vec3(1.25),d))*silhouettes;
 float edge=max(creases.x,max(creases.y,creases.z)),rim=max(outline.x,max(outline.y,outline.z));
 float resolution=1.-smoothstep(.45,1.3,max(fwidth(vectorBary.x),max(fwidth(vectorBary.y),fwidth(vectorBary.z))));
 return max(tint*edge*.78,vectorWhite*rim)*resolution;
}
float vectorBox(vec2 p,vec2 halfSize){vec2 q=abs(p)-halfSize;float d=max(q.x,q.y);return 1.-smoothstep(.4*wirePixelScale,1.05*wirePixelScale,abs(d)/max(fwidth(d),.00001));}
float vectorRule(float coordinate){float d=abs(fract(coordinate+.5)-.5)/max(fwidth(coordinate),.00001);return (1.-smoothstep(.35*wirePixelScale,.95*wirePixelScale,d))*(1.-smoothstep(.12,.35,fwidth(coordinate)));}
)GLSL"
namespace hyperion::flight {
inline constexpr const char* wireVertex=R"GLSL(#version 330
in vec3 vertexPosition,vertexNormal;in vec4 vertexColor;
uniform mat4 mvp,matModel,matNormal;uniform vec3 eye;uniform float wireKind,wireGroundBlend;
out vec3 worldPosition,normal,localPosition,vectorLocalNormal;flat out float vectorRole;
)GLSL" HYPERION_WIRE_VERTEX_GLSL R"GLSL(
void main(){
 vec3 p=vertexPosition;if(abs(wireKind-6.)<.1){float range=length(eye-(matModel*vec4(p,1)).xyz),fade=(1.-smoothstep(95.,120.,range))*wireGroundBlend;p=normalize(p)*mix(vertexTangent.w-.16,length(p),fade);}
 vectorGeometry(matNormal,vec3(1.));vectorRole=floor(vertexColor.a*255./32.+.001);vectorLocalNormal=vertexNormal;worldPosition=(matModel*vec4(p,1)).xyz;normal=normalize(mat3(matNormal)*vertexNormal);localPosition=p;gl_Position=mvp*vec4(p,1);
}
)GLSL";
inline constexpr const char* wireFragment=R"GLSL(#version 330
in vec3 worldPosition,normal,localPosition,vectorLocalNormal;flat in float vectorRole;out vec4 finalColor;
uniform vec3 eye,wireTint;uniform float wireKind;
)GLSL" HYPERION_WIRE_EDGE_GLSL R"GLSL(
void main(){
 gl_FragDepth=gl_FragCoord.z*(abs(wireKind-2.)<.1?.0001:1.);
 bool ship=abs(wireKind-5.)<.1,building=abs(wireKind-4.)<.1;vec3 ink=vectorRole==3.||vectorRole==2.?vectorMagenta:vectorCyan;
 if(wireEGA>.5){if(ship)ink=vectorRole==3.||vectorRole==2.?vec3(1.,.3,.03):mix(vectorCyan,wireTint,.75);else if(building)ink=vectorRole==3.||vectorRole==2.?vec3(1.,.65,.09):vec3(.08,.68,.85);}
 if(abs(wireKind-2.)<.1)ink=wireEGA>.5?vec3(.14,.24,.70):vectorCyan;vec3 c=vectorInk(normal,eye-worldPosition,ink);
 if(ship||building){
  vec3 axis=abs(vectorLocalNormal);vec2 uv=axis.y>.65?localPosition.xz:axis.x>axis.z?localPosition.zy:localPosition.xy;
  vec2 size=ship?vec2(.65,.85):vec2(7.,4.5),cell=uv/size,id=floor(cell),q=fract(cell)-.5;
  float footprint=max(length(dFdx(cell)),length(dFdy(cell))),resolved=1.-smoothstep(.025,.13,footprint);
  float signature=fract(sin(dot(id,vec2(17.31,53.17)))*43758.5453),panel=vectorBox(q,ship?vec2(.36,.33):vec2(.32,.26));
  bool facade=axis.y<.65,roof=axis.y>.65&&localPosition.y>3.;
  if(ship){
   if(vectorRole==1.){float glass=vectorBox(q,vec2(.42,.40));c=max(c,vectorCyan*glass*resolved*.55);}
   else if(vectorRole==3.||vectorRole==2.){float vanes=vectorRule(uv.x*9.);c=max(c,vectorMagenta*vanes*resolved*.75);}
   else{float hatch=panel*step(.62,signature),rib=vectorRule(uv.y/1.2)*.24;c=max(c,vectorCyan*max(hatch*.42,rib)*resolved);if(signature>.88)c=max(c,vectorMagenta*panel*resolved*.45);}
  }else if(facade){
   float lit=step(.28,signature),mullion=vectorRule(cell.x*2.)*step(abs(q.y),.24)*step(abs(q.x),.32);
   c=max(c,vectorCyan*panel*resolved*.55);c=max(c,(wireEGA>.5?vec3(1.,.65,.09):vectorWhite)*mullion*resolved*.32*lit);if(signature>.82)c=max(c,vectorMagenta*panel*resolved*.65);
  }else if(roof){float hatch=panel*step(.74,signature);c=max(c,vectorCyan*hatch*resolved*.33);}
 }
 finalColor=vec4(c,1.);
}
)GLSL";
}
