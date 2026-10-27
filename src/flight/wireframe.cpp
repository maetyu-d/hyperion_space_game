#include "renderer.hpp"
#include "wire_geometry.hpp"
#include <cstring>
namespace hyperion::flight {
void Renderer::clearWireModels(){for(auto& [key,entry]:wireModels)UnloadModel(entry.model);wireModels.clear();}
void Renderer::drawVectorModel(const Model& source,Vector3 at,float scale,int kind,Color tint){
 if(!wireframe){DrawModel(source,at,scale,tint);return;}
 for(int m=0;m<source.meshCount;m++){
  const auto& input=source.meshes[m];uint64_t key=1469598103934665603ULL;
  auto hash=[&](const void* data,size_t bytes){auto* p=static_cast<const unsigned char*>(data);if(p)for(size_t i=0;i<bytes;i++){key^=p[i];key*=1099511628211ULL;}};
  hash(input.vertices,size_t(input.vertexCount)*3*sizeof(float));hash(input.normals,size_t(input.vertexCount)*3*sizeof(float));hash(input.texcoords,size_t(input.vertexCount)*2*sizeof(float));hash(input.colors,size_t(input.vertexCount)*4);hash(input.indices,input.indices?size_t(input.triangleCount)*3*sizeof(unsigned short):0);
  auto found=wireModels.find(key);
  if(found==wireModels.end()){
   auto topology=wireTopology(input.vertices,input.vertexCount,input.indices,input.triangleCount);Mesh mesh{};mesh.vertexCount=input.triangleCount*3;mesh.triangleCount=input.triangleCount;
   mesh.vertices=static_cast<float*>(MemAlloc(mesh.vertexCount*3*sizeof(float)));mesh.normals=static_cast<float*>(MemAlloc(mesh.vertexCount*3*sizeof(float)));mesh.colors=static_cast<unsigned char*>(MemAlloc(mesh.vertexCount*4));mesh.texcoords=static_cast<float*>(MemAlloc(mesh.vertexCount*2*sizeof(float)));mesh.texcoords2=static_cast<float*>(MemAlloc(mesh.vertexCount*2*sizeof(float)));mesh.tangents=static_cast<float*>(MemAlloc(mesh.vertexCount*4*sizeof(float)));
   int edges=0;for(int t=0;t<input.triangleCount;t++)for(int k=0;k<3;k++){
    int dest=t*3+k,index=input.indices?input.indices[dest]:dest;
    std::memcpy(mesh.vertices+dest*3,input.vertices+index*3,3*sizeof(float));
    if(input.normals)std::memcpy(mesh.normals+dest*3,input.normals+index*3,3*sizeof(float));else{mesh.normals[dest*3]=0;mesh.normals[dest*3+1]=1;mesh.normals[dest*3+2]=0;}
    for(int c=0;c<4;c++)mesh.colors[dest*4+c]=input.colors?input.colors[index*4+c]:255;
    auto a=wireOctEncode(topology[t].adjacent[0]),b=wireOctEncode(topology[t].adjacent[1]),c=wireOctEncode(topology[t].adjacent[2]);
    for(int j=0;j<2;j++){mesh.texcoords[dest*2+j]=a[j];mesh.texcoords2[dest*2+j]=b[j];mesh.tangents[dest*4+j]=c[j];}
    int mask=0;for(int j=0;j<3;j++)if(topology[t].features[j]>.5)mask|=1<<j;
    mesh.tangents[dest*4+2]=float(mask);mesh.tangents[dest*4+3]=input.texcoords?input.texcoords[index*2]:0;
    if(k==0)for(float edge:topology[t].features)edges+=edge>.5;
   }
   UploadMesh(&mesh,false);auto model=LoadModelFromMesh(mesh);found=wireModels.emplace(key,VectorAsset{model,wireSerial,edges}).first;
  }
  auto& asset=found->second;asset.used=wireSerial;asset.model.transform=source.transform;
  Shader shader=kind==3?lifeShader:wireShader;asset.model.materials[0].shader=shader;
  auto setFloat=[&](const char* name,float value){int loc=GetShaderLocation(shader,name);if(loc>=0)SetShaderValue(shader,loc,&value,SHADER_UNIFORM_FLOAT);};
  setFloat("wirePixelScale",std::max(1.f,float(scene.texture.width)/std::max(1,GetRenderWidth())));setFloat("wireKind",float(kind));setFloat("wireGroundBlend",groundBlend);
  Vector3 eye=kind==2?Vector3{}:camera.position;int loc=GetShaderLocation(shader,"eye");SetShaderValue(shader,loc,&eye,SHADER_UNIFORM_VEC3);
  Vector3 ink{.09f,1.f,1.f};
  loc=GetShaderLocation(shader,"wireTint");if(loc>=0)SetShaderValue(shader,loc,&ink,SHADER_UNIFORM_VEC3);
  DrawModel(asset.model,at,scale,WHITE);wireDraws++;wireTriangles+=input.triangleCount;wireEdges+=asset.edges;
 }
}
}
