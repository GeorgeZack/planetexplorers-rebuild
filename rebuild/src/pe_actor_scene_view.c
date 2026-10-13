#include "pe_actor_scene_view.h"
#include "pe_material_view.h"
#include <rlgl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void set_error(char *error,size_t cap,const char *text){if(error&&cap)snprintf(error,cap,"%s",text);}
static bool empty(const PeActorSceneView *v){return v&&!v->manifest&&!v->parts&&!v->images&&!v->count&&!v->loaded&&!v->material_shader.id;}
void pe_actor_scene_view_unload(PeActorSceneView *v){
    if(!v)return;
    for(size_t i=0;i<v->count;i++){
        if(v->parts)pe_actor_view_unload(&v->parts[i]);
        if(v->images&&v->images[i].data)UnloadImage(v->images[i]);
    }
    if(v->material_shader.id&&v->material_shader.id!=rlGetShaderIdDefault())UnloadShader(v->material_shader);
    free(v->parts);free(v->images);pe_actor_scene_manifest_free(v->manifest);memset(v,0,sizeof *v);
}
size_t pe_actor_scene_view_part_count(const PeActorSceneView *v){return v?v->count:0;}
size_t pe_actor_scene_view_clip_count(const PeActorSceneView *v){return v?pe_actor_scene_clip_count(v->manifest):0;}
const PeActorClip *pe_actor_scene_view_clip_at(const PeActorSceneView *v,size_t clip){return v?pe_actor_scene_clip_at(v->manifest,clip):NULL;}
size_t pe_actor_scene_view_find_clip(const PeActorSceneView *v,const char *name){return v?pe_actor_scene_find_clip(v->manifest,name):PE_ACTOR_BIND_POSE;}
const PeActorClip *pe_actor_scene_view_clip(const PeActorSceneView *v){return pe_actor_scene_view_clip_at(v,0);}
bool pe_actor_scene_view_bounds(const PeActorSceneView *v,Vector3 *min,Vector3 *max){
    float lo[3],hi[3];if(!v||!min||!max||!pe_actor_scene_bounds(v->manifest,lo,hi))return false;
    *min=(Vector3){lo[0],lo[1],lo[2]};*max=(Vector3){hi[0],hi[1],hi[2]};return true;
}
bool pe_actor_scene_view_prepare(PeActorSceneView *v,const char *path,char *error,size_t cap){
    set_error(error,cap,"");if(!empty(v)){set_error(error,cap,"actor scene prepare requires an empty view");return false;}
    PeActorSceneView next={0};next.manifest=pe_actor_scene_manifest_load(path,error,cap);if(!next.manifest)return false;
    next.count=pe_actor_scene_part_count(next.manifest);next.parts=calloc(next.count,sizeof *next.parts);next.images=calloc(next.count,sizeof *next.images);
    if(!next.parts||!next.images){set_error(error,cap,"actor scene view allocation failed");goto fail;}
    uint64_t pixels=0;
    for(size_t i=0;i<next.count;i++){
        size_t bytes=0;const unsigned char *png=pe_actor_scene_png(next.manifest,i,&bytes);
        next.images[i]=LoadImageFromMemory(".png",png,(int)bytes);Image image=next.images[i];
        if(!image.data||image.width<=0||image.height<=0||image.width>8192||image.height>8192||(uint64_t)image.width*image.height>16777216){
            set_error(error,cap,"required actor scene PNG failed bounded decoding");goto fail;
        }
        pixels+=(uint64_t)image.width*image.height;
        if(pixels>64u*1024u*1024u){set_error(error,cap,"decoded scene texture budget exceeded");goto fail;}
    }
    *v=next;return true;
fail:pe_actor_scene_view_unload(&next);return false;
}
bool pe_actor_scene_view_upload(PeActorSceneView *v,char *error,size_t cap){
    set_error(error,cap,"");
    if(!v||!v->manifest||!v->parts||!v->images||!v->count||v->loaded){set_error(error,cap,"actor scene upload requires a prepared view");return false;}
    if(!IsWindowReady()){set_error(error,cap,"actor scene upload requires a graphics context");pe_actor_scene_view_unload(v);return false;}
    bool qualified=false;
    for(size_t i=0;i<v->count;i++)qualified|=pe_actor_scene_part(v->manifest,i)->material.alpha_mode!=PE_ACTOR_ALPHA_LEGACY;
    int cutoff=-1,alpha=-1;
    if(qualified){
        if(!pe_material_shader_load(&v->material_shader,&cutoff,&alpha)){
            set_error(error,cap,"source actor alpha shader unavailable");pe_actor_scene_view_unload(v);return false;
        }
    }
    for(size_t i=0;i<v->count;i++){
        if(!pe_actor_view_adopt(&v->parts[i],pe_actor_scene_take_actor(v->manifest,i),&v->images[i],error,cap)){
            pe_actor_scene_view_unload(v);return false;
        }
        v->parts[i].material=pe_actor_scene_part(v->manifest,i)->material;
        if(v->parts[i].material.alpha_mode!=PE_ACTOR_ALPHA_LEGACY){
            v->parts[i].model.materials[0].shader=v->material_shader;
            v->parts[i].cutoff_location=cutoff;v->parts[i].alpha_location=alpha;
        }
        UnloadImage(v->images[i]);v->images[i]=(Image){0};
    }
    free(v->images);v->images=NULL;v->loaded=true;return true;
}
bool pe_actor_scene_view_load(PeActorSceneView *v,const char *path,char *error,size_t cap){
    set_error(error,cap,"");
    if(!empty(v)||!IsWindowReady()){set_error(error,cap,"actor scene load requires an empty view and graphics context");return false;}
    return pe_actor_scene_view_prepare(v,path,error,cap)&&pe_actor_scene_view_upload(v,error,cap);
}
bool pe_actor_scene_view_draw_clip(PeActorSceneView *v,size_t clip,double seconds,Vector3 pos,float yaw,float scale,Color tint){
    if(!v||!v->loaded||!pe_actor_scene_view_clip_at(v,clip)||!isfinite(seconds)||seconds<0||!isfinite(pos.x)||!isfinite(pos.y)||!isfinite(pos.z)||!isfinite(yaw)||!isfinite(scale)||scale<=0||scale>1000)return false;
    if(v->ik_bound&&v->ik_active){PeActorBlendSample sample={clip,seconds,1};return pe_actor_scene_view_draw_blend(v,&sample,1,pos,yaw,scale,tint);}
    for(size_t i=0;i<v->count;i++)if(!pe_actor_view_draw(&v->parts[i],clip,seconds,pos,yaw,scale,tint))return false;
    return true;
}
bool pe_actor_scene_view_bind_ik(PeActorSceneView *v,const PeIkDrive *drive,const char *group){
    if(!v||!v->loaded||!v->parts||!v->count)return false;
    bool bound=drive&&group;
    for(size_t i=0;i<v->count;i++){
        v->parts[i].ik_driver=bound?pe_ik_drive_drivers(drive,group,i,v->parts[i].actor):NULL;
        bound=bound&&v->parts[i].ik_driver;
    }
    if(!bound)for(size_t i=0;i<v->count;i++)v->parts[i].ik_driver=NULL;
    v->ik_bound=bound;v->ik_active=false;return bound;
}
void pe_actor_scene_view_set_ik(PeActorSceneView *v,const PeIkDriveFrame *frame){
    if(!v)return;
    v->ik_active=frame&&v->ik_bound;if(v->ik_active)v->ik=*frame;
}
bool pe_actor_scene_view_draw_blend(PeActorSceneView *v,const PeActorBlendSample *samples,size_t count,Vector3 pos,float yaw,float scale,Color tint){
    if(!v||!v->loaded||!v->parts||!v->count||!samples||!count||count>PE_ACTOR_BLEND_MAX||
       !isfinite(pos.x)||!isfinite(pos.y)||!isfinite(pos.z)||!isfinite(yaw)||!isfinite(scale)||scale<=0||scale>1000)return false;
    for(size_t i=0;i<count;i++)if(samples[i].clip!=PE_ACTOR_BIND_POSE&&!pe_actor_scene_view_clip_at(v,samples[i].clip))return false;
    for(size_t i=0;i<v->count;i++){
        PeActorView *p=&v->parts[i];
        if(!p->loaded||!p->pose||p->model.meshCount<1||!p->model.meshes||!p->model.meshes[0].vertices||!p->model.meshes[0].normals||
           !pe_actor_blend_valid(p->actor,samples,count))return false;
    }
    const float *delta=v->ik_bound&&v->ik_active?v->ik.delta[0]:NULL;
    for(size_t i=0;i<v->count;i++)if(!pe_actor_view_draw_blend_driven(&v->parts[i],samples,count,delta,pos,yaw,scale,tint))return false;
    return true;
}
bool pe_actor_scene_view_draw(PeActorSceneView *v,double seconds,Vector3 pos,float yaw,float scale,Color tint){
    return pe_actor_scene_view_draw_clip(v,0,seconds,pos,yaw,scale,tint);
}
