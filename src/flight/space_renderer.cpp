#include "renderer.hpp"
#include "space_shaders.hpp"
#include "rlgl.h"
#include "raymath.h"
#include <stdexcept>
namespace hyperion::flight {
Shader Renderer::spaceShader(const char* vertex,const char* fragment,const char* position){auto source=spaceVertex(vertex,position);auto shader=LoadShaderFromMemory(source.c_str(),fragment);if(!IsShaderValid(shader)||shader.id==rlGetShaderIdDefault())throw std::runtime_error("Curved-space shader initialization failed");return shader;}
void Renderer::prepareSpace(const Game& g,bool cockpit){
 auto eye=g.ship.position+(cockpit?g.ship.up*3:V3{});uint32_t seed=g.planets.empty()?0:g.planets[0].worldSeed;
 ambient=spaceView(eye,seed);if(!ambientEnabled)ambient.strength=0;
 // Local harbours and surfaces retain the same readable near geometry. A
 // physical fold has a locally flat seam, and its destination inherits it.
 ambient.strength*=spaceSmooth(60,260,g.altitude());
 for(const auto& gate:g.folds)ambient.strength*=spaceSmooth(250,950,length(gate.position-eye));
 for(const auto& region:g.nilRegions)ambient.strength*=1-nilWeight(region,eye);
 if(ambientOverride>=0)ambient.strength=ambientOverride;
}
void Renderer::applySpace(Shader shader,bool enabled){
 auto scalar=[&](const char* name,float value){int loc=GetShaderLocation(shader,name);if(loc>=0)SetShaderValue(shader,loc,&value,SHADER_UNIFORM_FLOAT);};
 auto vector=[&](const char* name,V3 v){Vector3 value{float(v.x),float(v.y),float(v.z)};int loc=GetShaderLocation(shader,name);if(loc>=0)SetShaderValue(shader,loc,&value,SHADER_UNIFORM_VEC3);};
 scalar("hxStrength",enabled?float(ambient.strength):0);scalar("hxRadius",float(ambient.radius));vector("hxEye",ambient.eye);vector("hxX",ambient.x);vector("hxY",ambient.y);vector("hxZ",ambient.z);
 int loc=GetShaderLocation(shader,"hxWorldToClip");if(loc>=0)SetShaderValueMatrix(shader,loc,MatrixMultiply(rlGetMatrixModelview(),rlGetMatrixProjection()));
}
void Renderer::beginSpace(){
 for(auto shader:{world,groundShader,planetDepth,atmo,cloud,ringShader,aurora,structure,minorShader,wireShader,spacePrimitive})applySpace(shader);
 auto look=unit(V3{camera.target.x-camera.position.x,camera.target.y-camera.position.y,camera.target.z-camera.position.z});Vector3 forward{float(look.x),float(look.y),float(look.z)};
 for(auto shader:{planetDepth,minorShader}){int loc=GetShaderLocation(shader,"viewForward");if(loc>=0)SetShaderValue(shader,loc,&forward,SHADER_UNIFORM_VEC3);loc=GetShaderLocation(shader,"eye");if(loc>=0)SetShaderValue(shader,loc,&camera.position,SHADER_UNIFORM_VEC3);}
 resumeSpace();
}
void Renderer::resumeSpace(){BeginShaderMode(spacePrimitive);}
bool Renderer::inFront(V3 p,double radius)const{auto eye=V3{camera.position.x,camera.position.y,camera.position.z};auto look=unit(V3{camera.target.x,camera.target.y,camera.target.z}-eye);return dot(visualPosition(p)-eye,look)>-radius;}
}
