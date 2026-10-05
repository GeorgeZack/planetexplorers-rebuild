/* Standalone mocked-GPU test; never opens a graphics context. */
#define _POSIX_C_SOURCE 200809L
#include "pe_sandbox_world_view.h"
#include "pe_reference.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static bool window=true;
static int failure,models,textures,shaders,draws;
static Vector3 positions[PE_MAX_TREES];
static Mesh *shared_mesh;
bool IsWindowReady(void){return window;}
unsigned int rlGetShaderIdDefault(void){return 1;}
unsigned int rlGetTextureIdDefault(void){return 1;}
bool pe_reference_load(PeReferenceModel *r,const char *path,char *error,size_t cap){
    (void)error;(void)cap;assert(strstr(path,"/sandbox-world/tree.obj"));
    if(failure==1)return false;
    r->loaded=true;r->triangles=4216;
    r->bounds=(BoundingBox){{-1,0,0},{1,4,0}};
    Model *m=&r->model;m->meshCount=1;m->materialCount=1;
    m->meshes=calloc(1,sizeof *m->meshes);m->materials=calloc(1,sizeof *m->materials);m->meshMaterial=calloc(1,sizeof *m->meshMaterial);
    assert(m->meshes&&m->materials&&m->meshMaterial);models++;
    Mesh *mesh=m->meshes;mesh->vertexCount=3;mesh->triangleCount=4216;
    float vertices[]={-1,0,0,1,0,0,0,4,0};
    mesh->vertices=malloc(sizeof vertices);memcpy(mesh->vertices,vertices,sizeof vertices);
    mesh->normals=calloc(9,sizeof(float));mesh->texcoords=calloc(6,sizeof(float));
    mesh->indices=calloc(4216*3,sizeof(unsigned short));mesh->vboId=calloc(9,sizeof(unsigned));
    assert(mesh->vertices&&mesh->normals&&mesh->texcoords&&mesh->indices&&mesh->vboId);
    for(int i=0;i<4216*3;i++)mesh->indices[i]=(unsigned short)(i%3);
    for(int i=0;i<3;i++)mesh->vboId[i]=(unsigned)(i+2);
    mesh->vboId[6]=8;
    if(failure!=3){m->materials[0].maps=calloc(12,sizeof(MaterialMap));assert(m->materials[0].maps);}
    if(m->materials[0].maps){m->materials[0].maps[MATERIAL_MAP_DIFFUSE].texture=(Texture2D){10,1024,1024,1,7};textures++;}
    if(failure==2)mesh->vboId[0]=0;
    if(failure==4){m->materials[0].maps[MATERIAL_MAP_DIFFUSE].texture.id=1;textures--;}
    if(failure==7)mesh->indices[0]=3;
    if(failure==8)mesh->vertices[0]=NAN;
    return true;
}
void UnloadTexture(Texture2D t){assert(t.id==10);textures--;}
void UnloadModel(Model m){
    for(int i=0;i<m.meshCount;i++){Mesh mesh=m.meshes[i];free(mesh.vertices);free(mesh.normals);free(mesh.texcoords);free(mesh.indices);free(mesh.vboId);}
    for(int i=0;i<m.materialCount;i++)free(m.materials[i].maps);
    free(m.meshes);free(m.materials);free(m.meshMaterial);models--;
}
Shader LoadShaderFromMemory(const char *vs,const char *fs){
    assert(!vs&&strstr(fs,"if(texel.a<cutoff)discard"));
    if(failure==5)return(Shader){.id=1};
    shaders++;return(Shader){.id=20};
}
int GetShaderLocation(Shader s,const char *name){assert(!strcmp(name,"cutoff"));return s.id==20&&failure!=6?4:-1;}
void UnloadShader(Shader s){assert(s.id==20);shaders--;}
void SetShaderValue(Shader s,int loc,const void *value,int type){assert(s.id==20&&loc==4&&type==SHADER_UNIFORM_FLOAT&&*(const float*)value==.25f);}
void DrawModel(Model m,Vector3 p,float scale,Color tint){
    assert(scale==1&&tint.r==255&&tint.a==255&&draws<PE_MAX_TREES);
    if(draws)assert(shared_mesh==m.meshes);
    shared_mesh=m.meshes;positions[draws++]=p;
}
static void ray(PeSandboxWorldView *v,PeGame *g,PeVec3 o,PeVec3 d,float range,int want,float want_distance){
    int index=50;float distance=-1;assert(pe_sandbox_world_view_ray_tree(v,g,o,d,range,&index,&distance));
    assert(index==want&&fabsf(distance-want_distance)<1e-5f);
}
static void trunk(PeSandboxWorldView *v,PeGame *g,PeVec3 o,PeVec3 d,float range,int want,float want_distance){
    int index=50;float distance=-1;assert(pe_sandbox_world_view_ray_trunk(v,g,o,d,range,&index,&distance));
    assert(index==want&&fabsf(distance-want_distance)<1e-5f);
}
static void write_collider(const char *path,int malformed){
    unsigned char b[36]={'P','E','S','C',1};
    float values[]={-.1f,3,-.2f,1,7};
    if(malformed==4)values[3]=.85f; /* Well-formed capsule, wrong source identity. */
    for(int i=0;i<5;i++){uint32_t bits;memcpy(&bits,&values[i],4);for(int j=0;j<4;j++)b[8+4*i+j]=(unsigned char)(bits>>(8*j));}
    b[28]=1;b[32]=1;if(malformed==1)b[28]=2;
    FILE *f=fopen(path,"wb");assert(f);assert(fwrite(b,1,malformed==2?35:36,f)==(malformed==2?35u:36u));
    if(malformed==3)assert(fputc(0,f)!=EOF);
    assert(!fclose(f));
}
int main(void){
    char root[]="./pe-world-test-XXXXXX",folder[256],collider[256];assert(mkdtemp(root));
    snprintf(folder,sizeof folder,"%s/sandbox-world",root);assert(!mkdir(folder,0700));
    snprintf(collider,sizeof collider,"%s/sandbox-world/tree-collider.pesc",root);
    char error[256];window=false;assert(!pe_sandbox_world_view_load("assets",error,sizeof error));window=true;
    assert(!pe_sandbox_world_view_load(NULL,error,sizeof error));
    assert(!pe_sandbox_world_view_load(root,error,sizeof error)&&models==0);
    for(int malformed=1;malformed<=4;malformed++){write_collider(collider,malformed);assert(!pe_sandbox_world_view_load(root,error,sizeof error)&&models==0);}
    write_collider(collider,0);
    for(failure=1;failure<=8;failure++){
        assert(!pe_sandbox_world_view_load(root,error,sizeof error));assert(models==0&&textures==0&&shaders==0);
    }
    failure=0;PeSandboxWorldView *v=pe_sandbox_world_view_load(root,error,sizeof error);assert(v);
    assert(models==1&&textures==1&&shaders==1);
    PeGame *g=calloc(1,sizeof *g),*before=malloc(sizeof *before);assert(g&&before);
    g->tree_count=3;g->trees[0]=(PeTree){{10,2,10},8,255};g->trees[1]=(PeTree){{10,2,8},1,20};g->trees[2]=(PeTree){{9,2,10},0,0};
    memcpy(before,g,sizeof *g);assert(pe_sandbox_world_view_draw_trees(v,g));assert(draws==2&&models==1);
    assert(positions[0].x==10&&positions[0].y==2&&positions[0].z==10&&positions[1].z==8);
    assert(!memcmp(g,before,sizeof *g));
    ray(v,g,(PeVec3){10,3,6},(PeVec3){0,0,5},100,1,2);
    ray(v,g,(PeVec3){10,3,12},(PeVec3){0,0,-.5f},2,0,2);
    ray(v,g,(PeVec3){10,3,6},(PeVec3){0,0,1},1.999f,-1,1.999f);
    ray(v,g,(PeVec3){10,3,6},(PeVec3){1,0,0},100,-1,100);
    ray(v,g,(PeVec3){10,3,8},(PeVec3){0,0,1},0,1,0);
    /* Source trunk, unlike leaf mesh: radius1 at local(-.1,3,-.2).
       Caps are curved; tests cover lateral, vertical and inside-origin paths. */
    trunk(v,g,(PeVec3){9.9f,3,4.8f},(PeVec3){0,0,5},2,1,2);
    trunk(v,g,(PeVec3){9.9f,12,9.8f},(PeVec3){0,-2,0},100,0,3.5f);
    trunk(v,g,(PeVec3){9.9f,0,9.8f},(PeVec3){0,2,0},100,0,1.5f);
    trunk(v,g,(PeVec3){10.4f,12,9.8f},(PeVec3){0,-1,0},100,0,4.5f-sqrtf(.75f));
    trunk(v,g,(PeVec3){9.9f,5,9.8f},(PeVec3){0,0,1},100,-1,100);
    /* Depletion changes rendering and both ray paths immediately; reloaded
       live data reappears without rebuilding/uploading or changing state. */
    g->trees[1].remaining=0;g->trees[1].health=0;draws=0;
    assert(pe_sandbox_world_view_draw_trees(v,g)&&draws==1);
    ray(v,g,(PeVec3){10,3,6},(PeVec3){0,0,1},100,0,4);
    trunk(v,g,(PeVec3){9.9f,3,4.8f},(PeVec3){0,0,5},100,0,4);
    memcpy(g,before,sizeof *g);draws=0;assert(pe_sandbox_world_view_draw_trees(v,g)&&draws==2&&models==1);
    int index=88;float distance=99;
    assert(!pe_sandbox_world_view_ray_tree(v,g,(PeVec3){0},(PeVec3){0},10,&index,&distance)&&index==88&&distance==99);
    assert(!pe_sandbox_world_view_ray_trunk(v,g,(PeVec3){0},(PeVec3){NAN,0,1},10,&index,&distance)&&index==88&&distance==99);
    g->trees[2].position.y=NAN;draws=0;
    assert(!pe_sandbox_world_view_draw_trees(v,g)&&draws==0);
    assert(!pe_sandbox_world_view_ray_tree(v,g,(PeVec3){0},(PeVec3){0,0,1},10,&index,&distance)&&index==88&&distance==99);
    g->tree_count=PE_MAX_TREES+1;assert(!pe_sandbox_world_view_draw_trees(v,g));
    free(before);free(g);pe_sandbox_world_view_unload(v);pe_sandbox_world_view_unload(NULL);
    assert(models==0&&textures==0&&shaders==0);
    assert(!unlink(collider)&&!rmdir(folder)&&!rmdir(root));
    puts("Sandbox world view ownership, depletion, source placement and triangle ray tests passed");return 0;
}
