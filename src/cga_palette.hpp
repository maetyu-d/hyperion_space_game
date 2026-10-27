#pragma once
#include <array>
#include <cstddef>
#include <string_view>
namespace hyperion {
inline constexpr std::array<std::array<unsigned char,3>,4> cgaPalette{{{0,0,0},{85,255,255},{255,85,255},{255,255,255}}};
inline int cgaPaletteIndex(unsigned char r,unsigned char g,unsigned char b){for(std::size_t i=0;i<cgaPalette.size();i++)if(cgaPalette[i]==std::array<unsigned char,3>{r,g,b})return int(i);return -1;}
enum class WirePalette { CGA, EGA };
inline constexpr const char* wirePaletteName(WirePalette p){return p==WirePalette::EGA?"ega":"cga";}
inline bool parseWirePalette(std::string_view name,WirePalette& p){if(name=="cga")p=WirePalette::CGA;else if(name=="ega")p=WirePalette::EGA;else return false;return true;}
inline constexpr WirePalette nextWirePalette(WirePalette p){return p==WirePalette::CGA?WirePalette::EGA:WirePalette::CGA;}
inline constexpr std::array<std::array<unsigned char,3>,16> egaPalette{{{0,0,0},{0,0,170},{0,170,0},{0,170,170},{170,0,0},{170,0,170},{170,85,0},{170,170,170},{85,85,85},{85,85,255},{85,255,85},{85,255,255},{255,85,85},{255,85,255},{255,255,85},{255,255,255}}};
inline int wirePaletteIndex(WirePalette p,unsigned char r,unsigned char g,unsigned char b){if(p==WirePalette::CGA)return cgaPaletteIndex(r,g,b);for(std::size_t i=0;i<egaPalette.size();i++)if(egaPalette[i]==std::array<unsigned char,3>{r,g,b})return int(i);return -1;}
// Output colours are the four RGBI swatches. Ordered coverage supplies edge
// antialiasing without creating extra colours or temporal noise.
inline constexpr const char* vectorPaletteResolveFragment=R"GLSL(#version 330
in vec2 fragTexCoord;out vec4 finalColor;uniform sampler2D texture0;uniform float ega;
const vec3 palette[4]=vec3[4](vec3(0.),vec3(85.,255.,255.)/255.,vec3(255.,85.,255.)/255.,vec3(1.));
const vec3 egaColours[16]=vec3[16](vec3(0),vec3(0,0,170),vec3(0,170,0),vec3(0,170,170),vec3(170,0,0),vec3(170,0,170),vec3(170,85,0),vec3(170),vec3(85),vec3(85,85,255),vec3(85,255,85),vec3(85,255,255),vec3(255,85,85),vec3(255,85,255),vec3(255,255,85),vec3(255));
void main(){
 vec3 c=clamp(texture(texture0,fragTexCoord).rgb,0.,1.);c=pow(c,vec3(2.2));
 if(ega>.5){float error=1e10;int closest=0;for(int i=0;i<16;i++){vec3 colour=pow(egaColours[i]/255.,vec3(2.2)),delta=c-colour;float e=dot(delta,delta);if(e<error){error=e;closest=i;}}finalColor=vec4(egaColours[closest]/255.,1.);return;}
 vec3 linearPalette[4];for(int i=0;i<4;i++)linearPalette[i]=pow(palette[i],vec3(2.2));
 float error=1e10,amount=0.;int first=0,second=0;
 for(int a=0;a<4;a++)for(int b=a;b<4;b++){
  vec3 delta=linearPalette[b]-linearPalette[a];float t=clamp(dot(c-linearPalette[a],delta)/max(dot(delta,delta),.00001),0.,1.);
  vec3 residual=c-mix(linearPalette[a],linearPalette[b],t);float e=dot(residual,residual)+t*(1.-t)*.015;
  if(e<error){error=e;first=a;second=b;amount=t;}
 }
 if(first==0)amount=smoothstep(.025,.60,amount);else amount=smoothstep(.15,.85,amount);
 const float ordered[16]=float[16](0.,8.,2.,10.,12.,4.,14.,6.,3.,11.,1.,9.,15.,7.,13.,5.);ivec2 p=ivec2(gl_FragCoord.xy);
 float threshold=(ordered[(p.y%4)*4+p.x%4]+.5)/16.;finalColor=vec4(palette[amount>threshold?second:first],1.);
})GLSL";
}
