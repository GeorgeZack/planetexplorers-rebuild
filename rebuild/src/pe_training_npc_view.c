#include "pe_training_npc_view.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

void pe_training_npc_view_unload(PeTrainingNpcView *view) {
    if(!view)return;
    for(size_t i=0;i<PE_TRAINING_NPC_COUNT;i++)pe_actor_scene_view_unload(&view->actors[i]);
    memset(view,0,sizeof *view);
}
bool pe_training_npc_view_load(PeTrainingNpcView *view,const char *dir,
    const PeSceneCollision *room,char *error,size_t capacity) {
    if(!view||!dir||!room||view->count)return false;
    char paths[PE_TRAINING_NPC_COUNT][4096];size_t present=0;
    for(size_t i=0;i<PE_TRAINING_NPC_COUNT;i++) {
        const PeTrainingNpc *npc=pe_training_npc_at(i);
        int n=snprintf(paths[i],sizeof paths[i],"%s/npcs/%d/recording.peas",dir,npc->id);
        if(n<0||(size_t)n>=sizeof paths[i]) {
            if(error&&capacity)snprintf(error,capacity,"NPC asset path too long");
            return false;
        }
        present+=FileExists(paths[i]);
    }
    if(!present)return true;
    if(present!=PE_TRAINING_NPC_COUNT) {
        if(error&&capacity)snprintf(error,capacity,"Tutorial NPC assets are incomplete (%zu/%u)",present,PE_TRAINING_NPC_COUNT);
        return false;
    }
    for(size_t i=0;i<PE_TRAINING_NPC_COUNT;i++) {
        const PeTrainingNpc *npc=pe_training_npc_at(i);
        if(!pe_actor_scene_view_load(&view->actors[i],paths[i],error,capacity))goto fail;
        view->idle_clips[i]=pe_actor_scene_view_find_clip(&view->actors[i],"NormalIdle");
        const PeActorClip *clip=pe_actor_scene_view_clip_at(&view->actors[i],view->idle_clips[i]);
        if(!clip||!clip->loop||!isfinite(clip->duration)||clip->duration<=0) {
            if(error&&capacity)snprintf(error,capacity,"NPC %d requires its recovered looping NormalIdle clip",npc->id);
            goto fail;
        }
        if(!pe_training_npc_ground(npc,room,&view->positions[i])) {
            if(error&&capacity)snprintf(error,capacity,"NPC %d has no clear floor near its source spawn",npc->id);
            goto fail;
        }
        Vector3 minimum,maximum;
        if(!pe_actor_scene_view_bounds(&view->actors[i],&minimum,&maximum))goto fail;
        view->label_heights[i]=fmaxf(npc->height,maximum.y)+.25f;
        view->count++;
    }
    return true;
fail:
    pe_training_npc_view_unload(view);return false;
}
bool pe_training_npc_view_draw(PeTrainingNpcView *view) {
    if(!view||!isfinite(view->seconds)||view->seconds<0)return false;
    for(size_t i=0;i<view->count;i++) {
        const PeTrainingNpc *npc=pe_training_npc_at(i);PeVec3 p=view->positions[i];
        if(!pe_actor_scene_view_draw_clip(&view->actors[i],view->idle_clips[i],view->seconds,
            (Vector3){p.x,p.y,p.z},npc->yaw,1,WHITE))return false;
    }
    return true;
}
bool pe_training_npc_view_position(const PeTrainingNpcView *view,int id,PeVec3 *position) {
    if(!view||!position)return false;
    for(size_t i=0;i<view->count;i++)if(pe_training_npc_at(i)->id==id){*position=view->positions[i];return true;}
    return false;
}
