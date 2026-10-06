/* GPU ownership/failure tests; actual shader pixels are checked on desktop. */
#include "pe_terrain_material_view.h"
#include "../generated/pe_terrain_materials.h"
#include "../generated/pe_torch_source.h"
#include <rlgl.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
static int failure,loads,textures,shaders,sets;
static int light_count,light_uploads;
static float light_positions[PE_TERRAIN_POINT_LIGHTS][3];
static bool window=true;
bool IsWindowReady(void){return window;}
unsigned int rlGetTextureIdDefault(void){return 1;}
unsigned int rlGetShaderIdDefault(void){return 1;}
Texture2D LoadTexture(const char *path) {
    char suffix[40];snprintf(suffix,sizeof suffix,"/terrain/diffuse%d.png",loads);assert(strstr(path,suffix));
    loads++;if(failure==loads)return(Texture2D){0};textures++;
    return(Texture2D){(unsigned)(loads+1),failure==7?512:pe_terrain_diffuse_size[loads-1],pe_terrain_diffuse_size[loads-1],1,7};
}
void GenTextureMipmaps(Texture2D *t){if(failure!=8)t->mipmaps=12;}
void SetTextureFilter(Texture2D t,int filter){assert(t.id>1&&filter==TEXTURE_FILTER_TRILINEAR);}
void SetTextureWrap(Texture2D t,int wrap){assert(t.id>1&&wrap==TEXTURE_WRAP_CLAMP);}
void UnloadTexture(Texture2D t){assert(t.id>1);textures--;}
Shader LoadShaderFromMemory(const char *vs,const char *fs){
    assert(strstr(vs,"vertexTexCoord2")&&strstr(fs,"p.zy")&&strstr(fs,"textureLod"));
    if(failure==9)return(Shader){.id=1};
    shaders++;return(Shader){.id=2,.locs=calloc(32,sizeof(int))};
}
int GetShaderLocation(Shader s,const char *name){
    assert(s.id==2);
    if(failure==10||(failure==11&&!strcmp(name,"terrainSettings"))||(failure==12&&!strcmp(name,"pointLightPositions[0]")))return -1;
    if(!strcmp(name,"pointLightCount"))return 3;
    if(!strcmp(name,"pointLightPositions[0]"))return 4;
    if(!strcmp(name,"pointLightColor"))return 5;
    if(!strcmp(name,"pointLightSettings"))return 6;
    return 2;
}
void SetShaderValue(Shader s,int location,const void *v,int type){
    assert(s.id==2&&v);const float *f=v;
    if(location==3){assert(type==SHADER_UNIFORM_INT);light_count=*(const int*)v;assert(light_count>=0&&light_count<=PE_TERRAIN_POINT_LIGHTS);}
    else if(location==5){assert(type==SHADER_UNIFORM_VEC3&&f[0]==PE_TORCH_LIGHT_R&&f[1]==PE_TORCH_LIGHT_G&&f[2]==PE_TORCH_LIGHT_B);}
    else if(location==6){assert(type==SHADER_UNIFORM_VEC2&&f[0]==PE_TORCH_LIGHT_RANGE&&f[1]==PE_TORCH_LIGHT_INTENSITY);}
    else assert(location==2&&type==SHADER_UNIFORM_VEC3);
    sets++;
}
void SetShaderValueV(Shader s,int location,const void *v,int type,int count){
    assert(s.id==2&&location==4&&v&&type==SHADER_UNIFORM_VEC3&&count==light_count&&count>0);
    memcpy(light_positions,v,(size_t)count*sizeof light_positions[0]);light_uploads++;
}
void UnloadShader(Shader s){assert(s.id==2);free(s.locs);shaders--;}
int main(void){
    char error[200];PeTerrainMaterialView view={0};
    window=false;assert(!pe_terrain_material_view_load(&view,"assets",error,sizeof error));window=true;
    for(failure=1;failure<=12;failure++) {
        loads=0;assert(!pe_terrain_material_view_load(&view,"assets",error,sizeof error));
        assert(error[0]&&!view.loaded&&!textures&&!shaders);
        pe_terrain_material_view_unload(&view);assert(!textures&&!shaders);
    }
    failure=loads=sets=0;assert(pe_terrain_material_view_load(&view,"assets",error,sizeof error));
    assert(textures==6&&shaders==1&&sets==4&&light_count==0);
    assert(!pe_terrain_material_view_load(&view,"assets",error,sizeof error));
    MaterialMap maps[12]={0};Material material={.maps=maps};Model model={.materials=&material,.materialCount=1};
    assert(pe_terrain_material_view_bind(&view,&model)&&model.materials[0].shader.id==2);
    for(int i=0;i<6;i++)assert(maps[i].texture.id==(unsigned)i+2);
    PeWorld *world=calloc(1,sizeof *world);assert(world);world->source=1;world->origin_x=-64;
    pe_terrain_material_view_frame(&view,world,(Vector3){3,4,5});assert(sets==6);free(world);
    PeVec3 lights[12];for(int i=0;i<12;i++)lights[i]=(PeVec3){(float)(12-i),2,3};
    lights[0].x=NAN;lights[1].x=INFINITY;
    pe_terrain_material_view_lights(&view,lights,12,(PeVec3){0,2,3});
    assert(light_count==8&&light_uploads==1);
    for(int i=0;i<8;i++)assert(light_positions[i][0]==i+1&&light_positions[i][1]==2&&light_positions[i][2]==3);
    pe_terrain_material_view_lights(&view,lights,12,(PeVec3){9,2,3});
    assert(light_count==8&&light_uploads==2&&light_positions[0][0]==9&&light_positions[1][0]==10&&light_positions[2][0]==8);
    pe_terrain_material_view_lights(&view,NULL,0,(PeVec3){0});assert(!light_count&&light_uploads==2);
    pe_terrain_material_view_lights(&view,lights,12,(PeVec3){NAN,0,0});assert(!light_count&&light_uploads==2);
    pe_terrain_material_view_unload(&view);assert(!textures&&!shaders&&!view.loaded);
    assert(!pe_terrain_material_view_bind(&view,&model));
    pe_terrain_material_view_lights(&view,lights,12,(PeVec3){0});assert(light_uploads==2);
    puts("Terrain GPU ownership, staged failure cleanup and nearest-eight source Torch lights PASS");return 0;
}
