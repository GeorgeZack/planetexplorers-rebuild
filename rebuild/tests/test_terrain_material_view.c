/* GPU ownership/failure tests; actual shader pixels are checked on desktop. */
#include "pe_terrain_material_view.h"
#include "../generated/pe_terrain_materials.h"
#include <rlgl.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static int failure,loads,textures,shaders,sets;
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
int GetShaderLocation(Shader s,const char *name){assert(s.id==2);return failure==10||(failure==11&&!strcmp(name,"terrainSettings"))?-1:2;}
void SetShaderValue(Shader s,int location,const void *v,int type){assert(s.id==2&&location==2&&v&&type==SHADER_UNIFORM_VEC3);sets++;}
void UnloadShader(Shader s){assert(s.id==2);free(s.locs);shaders--;}
int main(void){
    char error[200];PeTerrainMaterialView view={0};
    window=false;assert(!pe_terrain_material_view_load(&view,"assets",error,sizeof error));window=true;
    for(failure=1;failure<=11;failure++) {
        loads=0;assert(!pe_terrain_material_view_load(&view,"assets",error,sizeof error));
        assert(error[0]&&!view.loaded&&!textures&&!shaders);
        pe_terrain_material_view_unload(&view);assert(!textures&&!shaders);
    }
    failure=loads=sets=0;assert(pe_terrain_material_view_load(&view,"assets",error,sizeof error));
    assert(textures==6&&shaders==1&&sets==1);
    assert(!pe_terrain_material_view_load(&view,"assets",error,sizeof error));
    MaterialMap maps[12]={0};Material material={.maps=maps};Model model={.materials=&material,.materialCount=1};
    assert(pe_terrain_material_view_bind(&view,&model)&&model.materials[0].shader.id==2);
    for(int i=0;i<6;i++)assert(maps[i].texture.id==(unsigned)i+2);
    PeWorld *world=calloc(1,sizeof *world);assert(world);world->source=1;world->origin_x=-64;
    pe_terrain_material_view_frame(&view,world,(Vector3){3,4,5});assert(sets==3);free(world);
    pe_terrain_material_view_unload(&view);assert(!textures&&!shaders&&!view.loaded);
    assert(!pe_terrain_material_view_bind(&view,&model));
    puts("Terrain GPU resource ownership and staged failure cleanup PASS");return 0;
}
