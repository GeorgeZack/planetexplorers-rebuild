/* CPU-only drawing-boundary regression: real actor, view and scene blend code,
 * intercepted submissions; no window, GL context or installed assets. */
#define _POSIX_C_SOURCE 200809L
#include "pe_actor_scene_view.h"
#include <rlgl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int failures,draws,updates;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"blend view:%d: %s\n",__LINE__,#x);failures++;}}while(0)
/* These fixtures intentionally retain legacy materials. Link the shared draw
 * boundary, but fail if blending starts requiring shader/GL-state operations.
 * Qualified material values and cull restoration have separate GPU checks. */
static void unexpected_material_api(const char *name){
    fprintf(stderr,"blend view: legacy draw called %s\n",name);failures++;
}
void SetShaderValue(Shader shader,int location,const void *value,int type){
    (void)shader;(void)location;(void)value;(void)type;unexpected_material_api("SetShaderValue");
}
void rlDrawRenderBatchActive(void){unexpected_material_api("rlDrawRenderBatchActive");}
void rlEnableBackfaceCulling(void){unexpected_material_api("rlEnableBackfaceCulling");}
void rlDisableBackfaceCulling(void){unexpected_material_api("rlDisableBackfaceCulling");}
void *rlGetProcAddress(const char *name){(void)name;unexpected_material_api("rlGetProcAddress");return NULL;}
static void u32(unsigned char*p,uint32_t v){for(int i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}
static void f32(unsigned char*p,float v){uint32_t bits;memcpy(&bits,&v,4);u32(p,bits);}
static void matrix(unsigned char*p,float x){const float m[12]={1,0,0,x,0,1,0,0,0,0,1,0};for(int i=0;i<12;i++)f32(p+4*i,m[i]);}
static PeActor *fixture(void){
    unsigned char bytes[396]={0};memcpy(bytes,"PEAC",4);u32(bytes+4,1);u32(bytes+8,3);u32(bytes+12,3);u32(bytes+16,1);u32(bytes+20,1);
    for(int i=0;i<3;i++){f32(bytes+24+i*56,(float)i);f32(bytes+24+i*56+16,1);f32(bytes+24+i*56+32,1);}
    u32(bytes+196,1);u32(bytes+200,2);matrix(bytes+204,0);memcpy(bytes+252,"pose",5);f32(bytes+284,1);u32(bytes+288,2);f32(bytes+292,1);
    matrix(bytes+300,0);matrix(bytes+348,4);
    char path[]="/tmp/pe-blend-view-XXXXXX";int fd=mkstemp(path);if(fd<0)return NULL;
    FILE*f=fdopen(fd,"wb");if(!f){close(fd);unlink(path);return NULL;}
    bool ok=fwrite(bytes,1,sizeof bytes,f)==sizeof bytes;ok=fclose(f)==0&&ok;char error[128];
    PeActor*a=ok?pe_actor_load(path,error,sizeof error):NULL;unlink(path);return a;
}
/* Manifest ordering itself is covered by test_actor_scene; this fixture models
 * its validated one-clip metadata so the real scene draw dispatcher is tested. */
const PeActorClip *pe_actor_scene_clip_at(const PeActorSceneManifest*m,size_t clip){
    static const PeActorClip c={.name="pose",.fps=1,.duration=1,.frames=2};return m&&clip==0?&c:NULL;
}
/* One staged group whose parts all follow solver bone 2; the second part can lose its entry. */
static bool second_part_staged=true;
const unsigned char *pe_ik_drive_drivers(const PeIkDrive *drive,const char *group,size_t part,const PeActor *actor){
    static const unsigned char bone[1]={2};
    return drive&&actor&&!strcmp(group,"player-test")&&(part==0||(part==1&&second_part_staged))?bone:NULL;
}
void UpdateMeshBuffer(Mesh mesh,int index,const void*data,int size,int offset){
    CHECK(data&&size==9*(int)sizeof(float)&&offset==0&&(index==0||index==2));
    CHECK(mesh.vertexCount==3);updates++;
}
void DrawModelEx(Model model,Vector3 pos,Vector3 axis,float yaw,Vector3 scale,Color tint){
    (void)pos;(void)axis;(void)yaw;(void)scale;(void)tint;
    CHECK(model.meshCount==1);draws++;
}
int main(void){
    PeActor*a=fixture();CHECK(a);if(!a)return failures;
    PeActorVertex poses[2][3];memset(poses,0x5a,sizeof poses);
    float vertices[2][9]={{0}},normals[2][9]={{0}};
    Mesh meshes[2]={{.vertexCount=3,.vertices=vertices[0],.normals=normals[0]},
                    {.vertexCount=3,.vertices=vertices[1],.normals=normals[1]}};
    PeActorView parts[2]={{.actor=a,.pose=poses[0],.loaded=true,.model={.meshCount=1,.meshes=&meshes[0]}},
                        {.actor=a,.pose=poses[1],.loaded=true,.model={.meshCount=1,.meshes=&meshes[1]}}};
    int marker=0;PeActorSceneView scene={.manifest=(PeActorSceneManifest*)&marker,.parts=parts,.count=2,.loaded=true};
    PeActorBlendSample blend[]={{0,0,1},{0,1,3}};
    CHECK(pe_actor_scene_view_draw_blend(&scene,blend,2,(Vector3){0},0,1,WHITE));
    CHECK(draws==2&&updates==4&&fabsf(vertices[0][0]-3)<1e-6f&&vertices[1][0]==vertices[0][0]);
    PeActorVertex before[2][3];memcpy(before,poses,sizeof before);float saved_vertices[2][9];memcpy(saved_vertices,vertices,sizeof vertices);
    draws=updates=0;parts[1].loaded=false;
    CHECK(!pe_actor_scene_view_draw_blend(&scene,blend,2,(Vector3){0},0,1,WHITE));
    CHECK(draws==0&&updates==0&&!memcmp(before,poses,sizeof poses)&&!memcmp(saved_vertices,vertices,sizeof vertices));parts[1].loaded=true;
    parts[1].actor=NULL;CHECK(!pe_actor_scene_view_draw_blend(&scene,blend,2,(Vector3){0},0,1,WHITE));parts[1].actor=a;
    blend[1]=(PeActorBlendSample){9,0,0};CHECK(!pe_actor_scene_view_draw_blend(&scene,blend,2,(Vector3){0},0,1,WHITE));
    blend[1]=(PeActorBlendSample){0,NAN,0};CHECK(!pe_actor_scene_view_draw_blend(&scene,blend,2,(Vector3){0},0,1,WHITE));
    CHECK(draws==0&&updates==0&&!memcmp(before,poses,sizeof poses)&&!memcmp(saved_vertices,vertices,sizeof vertices));
    blend[1]=(PeActorBlendSample){0,1,3};CHECK(!pe_actor_scene_view_draw_blend(&scene,blend,2,(Vector3){NAN,0,0},0,1,WHITE));
    CHECK(!pe_actor_scene_view_draw_blend(&scene,blend,PE_ACTOR_BLEND_MAX+1,(Vector3){0},0,1,WHITE));
    CHECK(!pe_actor_view_draw_blend(&parts[0],blend,2,(Vector3){0},0,-1,WHITE));
    CHECK(draws==0&&updates==0&&!memcmp(before,poses,sizeof poses));
    blend[0].weight=0;blend[1].weight=1;draws=updates=0;
    CHECK(pe_actor_view_draw_blend(&parts[0],blend,2,(Vector3){0},0,1,WHITE));PeActorVertex exact[3];CHECK(pe_actor_pose(a,0,1,exact,3));
    CHECK(draws==1&&updates==2&&!memcmp(exact,poses[0],sizeof exact));
    /* Solved-skeleton presentation: bound parts move by their driver bone's motion, in blends and single clips. */
    const PeIkDrive *drive=(const PeIkDrive*)&marker;static PeIkDriveFrame frame;
    for(unsigned b=0;b<PE_FBBIK_MAX_BONES;b++)frame.delta[b][0]=frame.delta[b][5]=frame.delta[b][10]=1;
    frame.delta[2][3]=10;blend[0].weight=1;blend[1].weight=3;
    pe_actor_scene_view_set_ik(&scene,&frame);CHECK(!scene.ik_active);                       /* nothing bound yet */
    scene.loaded=false;CHECK(!pe_actor_scene_view_bind_ik(&scene,drive,"player-test"));scene.loaded=true;
    CHECK(!pe_actor_scene_view_bind_ik(&scene,drive,"player-other")&&!pe_actor_scene_view_bind_ik(&scene,NULL,"player-test")&&!scene.ik_bound);
    second_part_staged=false;CHECK(!pe_actor_scene_view_bind_ik(&scene,drive,"player-test")&&!parts[0].ik_driver&&!parts[1].ik_driver);
    pe_actor_scene_view_set_ik(&scene,&frame);CHECK(pe_actor_scene_view_draw_blend(&scene,blend,2,(Vector3){0},0,1,WHITE)&&fabsf(vertices[0][0]-3)<1e-6f);
    second_part_staged=true;CHECK(pe_actor_scene_view_bind_ik(&scene,drive,"player-test")&&scene.ik_bound&&!scene.ik_active&&parts[0].ik_driver&&parts[1].ik_driver);
    CHECK(pe_actor_scene_view_draw_blend(&scene,blend,2,(Vector3){0},0,1,WHITE)&&fabsf(vertices[0][0]-3)<1e-6f);   /* bound but no frame */
    pe_actor_scene_view_set_ik(&scene,&frame);frame.delta[2][3]=99;                                              /* the view keeps its own copy */
    draws=updates=0;CHECK(pe_actor_scene_view_draw_blend(&scene,blend,2,(Vector3){0},0,1,WHITE));
    CHECK(draws==2&&updates==4&&fabsf(vertices[0][0]-13)<1e-5f&&fabsf(vertices[1][3]-14)<1e-5f&&fabsf(normals[0][1]-1)<1e-6f);
    CHECK(pe_actor_scene_view_draw_clip(&scene,0,1,(Vector3){0},0,1,WHITE)&&fabsf(vertices[0][0]-14)<1e-5f&&fabsf(vertices[1][6]-16)<1e-5f);
    CHECK(!pe_actor_scene_view_draw_clip(&scene,1,0,(Vector3){0},0,1,WHITE)&&!pe_actor_scene_view_draw_clip(&scene,0,NAN,(Vector3){0},0,1,WHITE));
    /* A motion the skin cannot take falls back to the baked pose instead of failing the draw. */
    memset(frame.delta[2],0,sizeof frame.delta[2]);pe_actor_scene_view_set_ik(&scene,&frame);
    CHECK(pe_actor_scene_view_draw_blend(&scene,blend,2,(Vector3){0},0,1,WHITE)&&fabsf(vertices[0][0]-3)<1e-6f);
    pe_actor_scene_view_set_ik(&scene,NULL);CHECK(!scene.ik_active&&scene.ik_bound);
    CHECK(pe_actor_scene_view_draw_clip(&scene,0,1,(Vector3){0},0,1,WHITE)&&fabsf(vertices[0][0]-4)<1e-6f);
    pe_actor_free(a);return failures?1:0;
}
