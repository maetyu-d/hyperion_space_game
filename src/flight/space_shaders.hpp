#pragma once
#include <string>
#include <stdexcept>
namespace hyperion::flight {
inline constexpr const char* spaceGLSL=R"GLSL(
uniform vec3 hxEye,hxX,hxY,hxZ;uniform float hxRadius,hxStrength;uniform mat4 hxWorldToClip;
vec3 hxLocal(vec3 v){return vec3(dot(v,hxX),dot(v,hxY),dot(v,hxZ));}
vec3 hxWorld(vec3 v){return hxX*v.x+hxY*v.y+hxZ*v.z;}
vec3 hxLog(vec3 q){
 float az=abs(q.z),r2=dot(q.xy,q.xy);if(az<.0000001)return q;
 float lo=0.,hi=min(az,6.2801853);
 for(int i=0;i<18;i++){float a=(lo+hi)*.5,h=a*.5;float term=a<.02?a/12.+a*a*a/360.:(a-sin(a))/(8.*sin(h)*sin(h));if(a+r2*term<az)lo=a;else hi=a;}
 float a=(lo+hi)*.5*sign(q.z),h=a*.5,c=abs(h)<.001?1.-h*h/3.:h/tan(h);
 return vec3(c*q.x+h*q.y,c*q.y-h*q.x,a);
}
vec3 hxPoint(vec3 point){
 if(hxStrength<=0.)return point;vec3 d=point-hxEye;float blend=hxStrength*smoothstep(300.,2200.,length(d));if(blend<=0.)return point;
 vec3 p=hxLocal(hxEye)/hxRadius,q=hxLocal(d)/hxRadius;q.z-=.5*(p.x*q.y-p.y*q.x);
 return hxEye+mix(d,hxWorld(hxLog(q))*hxRadius,blend);
}
vec3 hxSky(vec3 direction){
 if(hxStrength<=0.)return direction;vec3 v=hxLocal(direction),p=hxLocal(hxEye)/hxRadius,q;float t=3.8,w=v.z;
 if(abs(w)<.00001)q=v*t;
 else{float a=w*t;q=vec3((v.x*sin(a)+v.y*(cos(a)-1.))/w,(v.x*(1.-cos(a))+v.y*sin(a))/w,a+dot(v.xy,v.xy)*(a-sin(a))/(2.*w*w));}
 q.z+=.5*(p.x*q.y-p.y*q.x);vec3 result=mix(direction,normalize(hxWorld(q)),hxStrength);return length(result)>.000001?normalize(result):direction;
}
)GLSL";
inline std::string spaceVertex(const char* source,const char* position){
 std::string s=source;auto at=s.find("void main()");if(at==std::string::npos)throw std::runtime_error("Missing curved-space vertex entry");s.replace(at,11,"void hxFlat()");
 s.insert(s.find('\n')+1,spaceGLSL);
 s+="\nvoid main(){hxFlat();if(hxStrength>0.&&length("+std::string(position)+"-hxEye)>300.)gl_Position=hxWorldToClip*vec4(hxPoint("+std::string(position)+"),1.);";
 if(s.find("#define CLAMP_NEAR")!=std::string::npos)s+="if(gl_Position.w>0.)gl_Position.z=max(gl_Position.z,-gl_Position.w);";
 return s+"}\n";
}
inline constexpr const char* spacePrimitiveVertex=R"GLSL(#version 330
in vec3 vertexPosition;in vec2 vertexTexCoord;in vec4 vertexColor;uniform mat4 mvp;
out vec2 fragTexCoord;out vec4 fragColor;
void main(){fragTexCoord=vertexTexCoord;fragColor=vertexColor;gl_Position=mvp*vec4(vertexPosition,1.);}
)GLSL";
inline constexpr const char* spacePrimitiveFragment=R"GLSL(#version 330
in vec2 fragTexCoord;in vec4 fragColor;uniform sampler2D texture0;uniform vec4 colDiffuse;out vec4 finalColor;
void main(){finalColor=texture(texture0,fragTexCoord)*colDiffuse*fragColor;}
)GLSL";
}
