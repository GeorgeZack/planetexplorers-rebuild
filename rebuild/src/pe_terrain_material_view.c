#include "pe_terrain_material_view.h"
#include "../generated/pe_terrain_materials.h"
#include <rlgl.h>
#include <stdio.h>
#include <string.h>

static const char *vertex_shader=
"#version 330\n"
"in vec3 vertexPosition;in vec3 vertexNormal;in vec2 vertexTexCoord;in vec2 vertexTexCoord2;in vec4 vertexColor;\n"
"uniform mat4 mvp;out vec3 terrainPosition;out vec3 terrainNormal;out vec3 blendWeights;flat out vec3 materialIds;\n"
"void main(){terrainPosition=vertexPosition;terrainNormal=vertexNormal;blendWeights=vertexColor.rgb;"
"materialIds=vec3(vertexTexCoord,vertexTexCoord2.x);gl_Position=mvp*vec4(vertexPosition,1.0);}\n";
static const char *fragment_shader=
"#version 330\n"
"in vec3 terrainPosition;in vec3 terrainNormal;in vec3 blendWeights;flat in vec3 materialIds;out vec4 finalColor;\n"
"uniform sampler2D diffuse0,diffuse1,diffuse2,diffuse3,diffuse4,diffuse5;\n"
"uniform vec3 terrainOrigin,cameraPosition,terrainSettings;\n"
"vec3 atlas(int a,vec2 uv,float lod){"
"if(a==0)return textureLod(diffuse0,uv,lod).rgb;if(a==1)return textureLod(diffuse1,uv,lod).rgb;"
"if(a==2)return textureLod(diffuse2,uv,lod).rgb;if(a==3)return textureLod(diffuse3,uv,lod).rgb;"
"if(a==4)return textureLod(diffuse4,uv,lod).rgb;return textureLod(diffuse5,uv,lod).rgb;}\n"
"vec3 samplePlane(int a,vec2 base,float tile,float repeatSize,float inset,vec2 p,float lod){"
"vec2 uv=base+(inset+fract(p/repeatSize)*(1.0-2.0*inset))*tile;return atlas(a,uv,lod);}\n"
"vec3 material(float id,vec3 p,vec3 weights,float lod){"
"if(id<1.0||id>84.0)return vec3(1.0,0.0,1.0);"
"int index=int(id)-1;int a=index/16;int slot=index%16;int grid=4;"
"if(index>=68){a=5;slot=index-68;}else if(index>=64){a=4;slot=index-64;grid=2;}"
"float tile=1.0/float(grid);vec2 base=vec2(slot%grid,slot/grid)*tile;"
"float repeatSize=grid==2?terrainSettings.z:terrainSettings.y;"
"float inset=terrainSettings.x*(grid==2?0.001953125:0.00390625);"
"return samplePlane(a,base,tile,repeatSize,inset,p.xy,lod)*weights.z+"
"samplePlane(a,base,tile,repeatSize,inset,p.zy,lod)*weights.x+"
"samplePlane(a,base,tile,repeatSize,inset,p.zx,lod)*weights.y;}\n"
"void main(){vec3 n=normalize(terrainNormal);vec3 weights=max((normalize(abs(n))-0.2)*7.0,vec3(0.01));"
"weights/=weights.x+weights.y+weights.z;vec3 p=terrainPosition+terrainOrigin;"
"vec3 delta=terrainPosition-cameraPosition;float lod=max(0.0,0.73*log(max(0.04*dot(delta,delta),0.000001)));"
"vec3 color=material(materialIds.x,p,weights,lod);if(materialIds.y!=materialIds.x||materialIds.z!=materialIds.x)color=color*blendWeights.x+"
"material(materialIds.y,p,weights,lod)*blendWeights.y+material(materialIds.z,p,weights,lod)*blendWeights.z;"
"float light=0.59+0.41*max(0.0,dot(n,vec3(-0.35,0.85,0.4)));finalColor=vec4(color*light,1.0);}\n";

void pe_terrain_material_view_unload(PeTerrainMaterialView *v) {
    if(!v)return;
    for(int i=0;i<6;i++)if(v->diffuse[i].id&&v->diffuse[i].id!=rlGetTextureIdDefault())UnloadTexture(v->diffuse[i]);
    if(v->shader.id&&v->shader.id!=rlGetShaderIdDefault())UnloadShader(v->shader);
    memset(v,0,sizeof *v);
}
bool pe_terrain_material_view_load(PeTerrainMaterialView *v,const char *root,char *error,size_t capacity) {
    if(!v||!root||!IsWindowReady()||v->loaded||v->shader.id)return false;
    const char *reason="Terrain texture/shader load failed";
    for(int i=0;i<6;i++) {
        char path[4096];int length=snprintf(path,sizeof path,"%s/terrain/diffuse%d.png",root,i);
        if(length<0||(size_t)length>=sizeof path){reason="Terrain asset path too long";goto fail;}
        v->diffuse[i]=LoadTexture(path);
        if(!v->diffuse[i].id||v->diffuse[i].id==rlGetTextureIdDefault()||v->diffuse[i].width!=pe_terrain_diffuse_size[i]||v->diffuse[i].height!=pe_terrain_diffuse_size[i]) {
            reason="Terrain requires six recovered diffuse atlases at their original sizes; run export_terrain_assets.py";goto fail;
        }
        GenTextureMipmaps(&v->diffuse[i]);
        if(v->diffuse[i].mipmaps<2){reason="Terrain atlas mipmap generation failed";goto fail;}
        SetTextureFilter(v->diffuse[i],TEXTURE_FILTER_TRILINEAR);
        SetTextureWrap(v->diffuse[i],TEXTURE_WRAP_CLAMP);
    }
    v->shader=LoadShaderFromMemory(vertex_shader,fragment_shader);
    if(!v->shader.id||v->shader.id==rlGetShaderIdDefault()||!v->shader.locs){reason="Terrain shader compilation failed";goto fail;}
    for(int i=0;i<6;i++) {
        char name[24];snprintf(name,sizeof name,"diffuse%d",i);
        int location=GetShaderLocation(v->shader,name);
        if(location<0){reason="Terrain atlas sampler missing";goto fail;}
        v->shader.locs[SHADER_LOC_MAP_ALBEDO+i]=location;
    }
    v->origin_location=GetShaderLocation(v->shader,"terrainOrigin");
    v->camera_location=GetShaderLocation(v->shader,"cameraPosition");
    int settings=GetShaderLocation(v->shader,"terrainSettings");
    if(v->origin_location<0||v->camera_location<0||settings<0){reason="Terrain shader parameters missing";goto fail;}
    const float values[]={PE_TERRAIN_INDENT,PE_TERRAIN_REPETITION,PE_TERRAIN_REPETITION_LARGE};
    SetShaderValue(v->shader,settings,values,SHADER_UNIFORM_VEC3);v->loaded=true;return true;
fail:
    if(error&&capacity)snprintf(error,capacity,"%s",reason);
    pe_terrain_material_view_unload(v);return false;
}
bool pe_terrain_material_view_bind(const PeTerrainMaterialView *v,Model *model) {
    if(!v||!v->loaded||!model||model->materialCount<1||!model->materials||!model->materials[0].maps)return false;
    Material *m=&model->materials[0];m->shader=v->shader;
    for(int i=0;i<6;i++)m->maps[i].texture=v->diffuse[i];
    return true;
}
void pe_terrain_material_view_frame(PeTerrainMaterialView *v,const PeWorld *w,Vector3 camera) {
    if(!v||!v->loaded||!w)return;
    float origin[3]={0};
    if(w->source==1){origin[0]=(float)w->origin_x;origin[1]=(float)w->origin_y;origin[2]=(float)w->origin_z;}
    SetShaderValue(v->shader,v->origin_location,origin,SHADER_UNIFORM_VEC3);
    SetShaderValue(v->shader,v->camera_location,&camera,SHADER_UNIFORM_VEC3);
}
