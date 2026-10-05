#include "pe_training_terrain.h"
#include "../generated/pe_training_mound.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
_Static_assert(PE_TRAINING_MOUND_COUNT==PE_TRAINING_TERRAIN_SAMPLES,"mound schema");

bool pe_training_terrain_valid(const PeWorld *w){
    return w&&w->source==1&&w->seed==PE_TRAINING_TERRAIN_SEED&&w->revision&&
        pe_world_width(w)==PE_WORLD_X&&pe_world_size_y(w)==PE_WORLD_Y&&pe_world_depth(w)==PE_WORLD_Z&&
        w->origin_x==PE_TRAINING_TERRAIN_ORIGIN_X&&w->origin_y==PE_TRAINING_TERRAIN_ORIGIN_Y&&w->origin_z==PE_TRAINING_TERRAIN_ORIGIN_Z;
}
static size_t index_of(PeTrainingTerrainSample s){
    return ((size_t)(s.z-PE_TRAINING_TERRAIN_ORIGIN_Z)*PE_WORLD_Y+(size_t)(s.y-PE_TRAINING_TERRAIN_ORIGIN_Y))*PE_WORLD_X+(size_t)(s.x-PE_TRAINING_TERRAIN_ORIGIN_X);
}
void pe_training_terrain_init(PeWorld *w){
    if(!w)return;
    memset(w,0,sizeof *w);w->source=1;w->seed=PE_TRAINING_TERRAIN_SEED;w->revision=1;
    w->origin_x=PE_TRAINING_TERRAIN_ORIGIN_X;w->origin_y=PE_TRAINING_TERRAIN_ORIGIN_Y;w->origin_z=PE_TRAINING_TERRAIN_ORIGIN_Z;
    for(size_t i=0;i<PE_TRAINING_MOUND_COUNT;i++)w->voxels[index_of(pe_training_mound[i])]=(PeVoxel){pe_training_mound[i].volume,1};
}
static bool update(PeWorld *w,bool clear){
    if(!pe_training_terrain_valid(w))return false;
    bool changed=false;
    for(size_t i=0;i<PE_TRAINING_MOUND_COUNT;i++){
        PeVoxel target=clear?(PeVoxel){0,0}:(PeVoxel){pe_training_mound[i].volume,1};
        PeVoxel old=w->voxels[index_of(pe_training_mound[i])];
        if(old.volume!=target.volume||old.material!=target.material){changed=true;break;}
    }
    if(!changed)return true;
    if(w->revision==UINT32_MAX)return false;
    for(size_t i=0;i<PE_TRAINING_MOUND_COUNT;i++)w->voxels[index_of(pe_training_mound[i])]=clear?(PeVoxel){0,0}:(PeVoxel){pe_training_mound[i].volume,1};
    w->revision++;return true;
}
bool pe_training_terrain_apply(PeWorld *w){return update(w,false);}
bool pe_training_terrain_clear(PeWorld *w){return update(w,true);}
static bool finite3(PeVec3 p){return isfinite(p.x)&&isfinite(p.y)&&isfinite(p.z);}
PeVec3 pe_training_terrain_to_local(const PeWorld *w,PeVec3 p){
    if(!pe_training_terrain_valid(w)||!finite3(p))return (PeVec3){NAN,NAN,NAN};
    return (PeVec3){p.x-w->origin_x,p.y-w->origin_y,p.z-w->origin_z};
}
PeVec3 pe_training_terrain_to_world(const PeWorld *w,PeVec3 p){
    if(!pe_training_terrain_valid(w)||!finite3(p))return (PeVec3){NAN,NAN,NAN};
    return (PeVec3){p.x+w->origin_x,p.y+w->origin_y,p.z+w->origin_z};
}
PeHit pe_training_terrain_raycast(const PeWorld *w,PeVec3 p,PeVec3 direction,float range){
    if(!pe_training_terrain_valid(w))return (PeHit){0};
    PeHit hit=pe_world_raycast(w,pe_training_terrain_to_local(w,p),direction,range);
    if(hit.hit)hit.position=pe_training_terrain_to_world(w,hit.position);
    return hit;
}
PeHit pe_training_terrain_dig_target(const PeWorld *w,PeVec3 p,PeVec3 direction,PeVec3 player,float reach){
    if(!pe_training_terrain_valid(w))return (PeHit){0};
    PeHit hit=pe_world_dig_target(w,pe_training_terrain_to_local(w,p),direction,pe_training_terrain_to_local(w,player),reach);
    if(hit.hit)hit.position=pe_training_terrain_to_world(w,hit.position);
    return hit;
}

bool pe_training_terrain_collision(const PeWorld *w,PeSceneCollision **snapshot,char *error,size_t cap){
    if(!snapshot||!pe_training_terrain_valid(w)){if(error&&cap)snprintf(error,cap,"Invalid tutorial terrain identity or snapshot");return false;}
    PeMeshData mesh={0};
    if(!pe_world_mesh(w,&mesh)){if(error&&cap)snprintf(error,cap,"Tutorial terrain mesh allocation failed");return false;}
    PeSceneCollision *next=pe_scene_from_mesh(&mesh,(PeVec3){w->origin_x,w->origin_y,w->origin_z},error,cap);
    pe_mesh_free(&mesh);if(!next)return false;
    pe_scene_free(*snapshot);*snapshot=next;return true;
}

bool pe_training_terrain_build_anchors(PeWorld*w,bool clear){
    if(!pe_training_terrain_valid(w))return false;
    PeVoxel target=clear?(PeVoxel){0,0}:(PeVoxel){255,1};
    bool changed=false;
    for(size_t i=0;i<PE_TRAINING_ANCHOR_COUNT;i++){
        PeVoxel old=w->voxels[index_of(pe_training_anchors[i])];
        if(old.volume!=target.volume||old.material!=target.material)changed=true;
    }
    if(!changed)return true;
    if(w->revision==UINT32_MAX)return false;
    for(size_t i=0;i<PE_TRAINING_ANCHOR_COUNT;i++)w->voxels[index_of(pe_training_anchors[i])]=target;
    w->revision++;return true;
}
