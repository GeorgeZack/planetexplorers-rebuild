#include "pe_adventure_npc_view.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

void pe_adventure_npc_view_unload(PeAdventureNpcView *v) {
    if(!v)return;
    if(v->portrait.id)UnloadRenderTexture(v->portrait);
    pe_actor_scene_view_unload(&v->actor);memset(v,0,sizeof *v);
}
bool pe_adventure_npc_view_load_role(PeAdventureNpcView *v,const char *root,int role,char *error,size_t cap) {
    if(!v||!root||(role!=1&&role!=2)||!IsWindowReady()||v->actor.loaded||v->portrait.id)return false;
    char path[4096];int n=snprintf(path,sizeof path,"%s/adventure-%s/recording.peas",root,role==1?"welcome":"supplier");
    if(n<0||(size_t)n>=sizeof path){if(error&&cap)snprintf(error,cap,"Adventure NPC asset path too long");return false;}
    return pe_adventure_npc_view_load_scene(v,path,error,cap);
}
bool pe_adventure_npc_view_load_scene(PeAdventureNpcView *v,const char *path,char *error,size_t cap){
    if(!v||!path||!IsWindowReady()||v->actor.loaded||v->portrait.id)return false;
    if(!pe_actor_scene_view_load(&v->actor,path,error,cap))return false;
    v->idle=pe_actor_scene_view_find_clip(&v->actor,"NormalIdle");
    const PeActorClip *clip=pe_actor_scene_view_clip_at(&v->actor,v->idle);
    Vector3 lo,hi;
    if(!clip||!clip->loop||!isfinite(clip->duration)||clip->duration<=0||
       !pe_actor_scene_view_bounds(&v->actor,&lo,&hi)) {
        if(error&&cap)snprintf(error,cap,"Adventure NPC requires a recovered looping NormalIdle scene");
        goto fail;
    }
    v->label_height=fmaxf(1.75f,hi.y)+.25f;
    v->portrait=LoadRenderTexture(256,256);
    if(!v->portrait.id){if(error&&cap)snprintf(error,cap,"Adventure NPC portrait allocation failed");goto fail;}
    SetTextureFilter(v->portrait.texture,TEXTURE_FILTER_BILINEAR);
    float head=hi.y-.22f;
    Camera3D camera={{0,head+.025f,1.35f},{0,head,0},{0,1,0},28,CAMERA_PERSPECTIVE};
    BeginTextureMode(v->portrait);ClearBackground(BLANK);BeginMode3D(camera);
    bool ok=pe_actor_scene_view_draw_clip(&v->actor,v->idle,0,(Vector3){0},0,1,WHITE);
    EndMode3D();EndTextureMode();
    if(!ok){if(error&&cap)snprintf(error,cap,"Adventure NPC portrait rendering failed");goto fail;}
    return true;
fail:pe_adventure_npc_view_unload(v);return false;
}
bool pe_adventure_npc_view_load(PeAdventureNpcView *v,const char *root,char *error,size_t cap){
    return pe_adventure_npc_view_load_role(v,root,2,error,cap);
}
bool pe_adventure_npc_view_draw(PeAdventureNpcView *v,PeVec3 p,float yaw,double seconds) {
    if(!v||!isfinite(p.x)||!isfinite(p.y)||!isfinite(p.z)||!isfinite(yaw)||!isfinite(seconds)||seconds<0)return false;
    return pe_actor_scene_view_draw_clip(&v->actor,v->idle,seconds,(Vector3){p.x,p.y,p.z},yaw,1,WHITE);
}
