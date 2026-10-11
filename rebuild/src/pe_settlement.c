#include "pe_paths.h"
#include "pe_settlement.h"
#include "../generated/pe_adventure_settlement.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static char root[4096];
static PeSceneCollision *geometry,*route_geometry;
static void err(char *out,size_t cap,const char *message){if(out&&cap)snprintf(out,cap,"%s",message);}
static bool default_root(char *error,size_t cap){
    if(root[0]||pe_paths_default_assets(root,sizeof root))return true;
    err(error,cap,"Cannot locate settlement assets beside the executable");return false;
}
bool pe_settlement_configure(const char *path,char *error,size_t cap){
    if(!path||!*path||strlen(path)>=sizeof root){err(error,cap,"Invalid settlement asset directory");return false;}
    if((geometry||route_geometry)&&strcmp(root,path)){err(error,cap,"Settlement assets cannot change while snapshots borrow their geometry");return false;}
    snprintf(root,sizeof root,"%s",path);return true;
}
bool pe_settlement_is_route(const PeWorld *w){
    return w&&w->source==1&&pe_world_width(w)==416&&pe_world_size_y(w)==64&&pe_world_depth(w)==416&&w->origin_x==-640&&w->origin_y==0&&w->origin_z==64;
}
bool pe_settlement_matches(const PeWorld *w){
    return pe_settlement_is_route(w)||(w&&w->source==1&&pe_world_width(w)==64&&pe_world_size_y(w)==32&&pe_world_depth(w)==128&&w->origin_x==-627&&w->origin_y==16&&w->origin_z==323);
}
const PeSceneCollision *pe_settlement_collision_for_world(const PeWorld *w,char *error,size_t cap){
    if(!default_root(error,cap))return NULL;
    if(!pe_settlement_is_route(w))return pe_settlement_collision(error,cap);
    if(route_geometry)return route_geometry;
    char path[4096];int n=snprintf(path,sizeof path,"%s/adventure-route/route-collision.obj",root);
    if(n<0||(size_t)n>=sizeof path){err(error,cap,"Route collision path is too long");return NULL;}
    PeSceneCollision *candidate=pe_scene_load_obj(path,error,cap);if(!candidate)return NULL;
    if(!pe_scene_stats(candidate).triangles){pe_scene_free(candidate);err(error,cap,"Route collision geometry is empty");return NULL;}
    route_geometry=candidate;return route_geometry;
}
static bool asset_path(char *path,size_t cap,const char *name,char *error,size_t ec){
    if(!default_root(error,ec))return false;
    int n=snprintf(path,cap,"%s/adventure-settlement/%s",root,name);
    if(n<0||(size_t)n>=cap){err(error,ec,"Settlement asset path is too long");return false;}return true;
}
const PeSceneCollision *pe_settlement_collision(char *error,size_t cap){
    if(geometry)return geometry;
    char path[4096];if(!asset_path(path,sizeof path,"settlement-collision.obj",error,cap))return NULL;
    PeSceneCollision *candidate=pe_scene_load_obj(path,error,cap);if(!candidate)return NULL;
    PeSceneStats stats=pe_scene_stats(candidate);
    if(!stats.triangles){pe_scene_free(candidate);err(error,cap,"Settlement collision geometry is empty");return NULL;}
    geometry=candidate;return geometry;
}
static uint32_t u32(const unsigned char *b){return(uint32_t)b[0]|(uint32_t)b[1]<<8|(uint32_t)b[2]<<16|(uint32_t)b[3]<<24;}
bool pe_settlement_load_world(const char *path,PeWorld *out,char *error,size_t cap){
    if(!path||!out){err(error,cap,"Invalid settlement terrain input");return false;}
    FILE *f=fopen(path,"rb");if(!f){err(error,cap,"Recovered settlement terrain is missing; run reference settlement capture");return false;}
    unsigned char header[32];PeWorld *w=calloc(1,sizeof *w);bool ok=false;
    if(!w){err(error,cap,"Settlement terrain allocation failed");goto done;}
    if(fread(header,1,sizeof header,f)!=sizeof header||memcmp(header,"PEVX",4)||u32(header+4)!=1){err(error,cap,"Invalid settlement terrain header");goto done;}
    w->source=1;w->origin_x=(int32_t)u32(header+8);w->origin_y=(int32_t)u32(header+12);w->origin_z=(int32_t)u32(header+16);
    w->width=u32(header+20);w->height=u32(header+24);w->depth=u32(header+28);w->revision=1;
    if(!pe_world_resize(w,(int)w->width,(int)w->height,(int)w->depth)||!pe_settlement_matches(w)){err(error,cap,"Invalid settlement terrain dimensions or allocation");goto done;}
    PeVoxel *data=pe_world_data(w);
    for(size_t i=0;i<pe_world_voxel_count(w);i++){
        unsigned char b[2];if(fread(b,1,2,f)!=2){err(error,cap,"Truncated settlement terrain");goto done;}
        data[i]=(PeVoxel){b[0],b[1]};
    }
    if(fgetc(f)!=EOF||ferror(f)){err(error,cap,"Unexpected settlement terrain payload");goto done;}
    pe_world_move(out,w);ok=true;
done:if(w)pe_world_destroy(w);free(w);fclose(f);return ok;
}
PeVec3 pe_settlement_npc_anchor_for_world(const PeWorld *w,int role){
    const double *p=pe_adventure_settlement_npcs[role==1?0:1].position;
    return(PeVec3){(float)(p[0]-w->origin_x),(float)(p[1]-w->origin_y),(float)(p[2]-w->origin_z)};
}
PeVec3 pe_settlement_npc_anchor(int role){
    const double *p=pe_adventure_settlement_npcs[role==1?0:1].position;
    return(PeVec3){(float)(p[0]+627),(float)(p[1]-16),(float)(p[2]-323)};
}
float pe_settlement_npc_yaw(int role){
    const double *q=pe_adventure_settlement_npcs[role==1?0:1].rotation;
    return(float)(atan2(2*(q[3]*q[1]+q[0]*q[2]),1-2*(q[1]*q[1]+q[2]*q[2]))*57.29577951308232);
}
bool pe_settlement_start(PeGame *game,char *error,size_t cap){
    if(!game||game->mode!=PE_GAME_ADVENTURE){err(error,cap,"Settlement requires Adventure mode");return false;}
    char path[4096];if(!asset_path(path,sizeof path,"terrain.pevx",error,cap))return false;
    char route[4096];int rn=snprintf(route,sizeof route,"%s/adventure-route/terrain.pevx",root);
    if(rn>0&&(size_t)rn<sizeof route){FILE *available=fopen(route,"rb");if(available){fclose(available);snprintf(path,sizeof path,"%s",route);}}
    PeWorld *w=calloc(1,sizeof *w);if(!w){err(error,cap,"Settlement allocation failed");return false;}
    bool ok=pe_settlement_load_world(path,w,error,cap);
    if(ok){
        const double *p=pe_adventure_settlement_initial_spawn;
        PeVec3 spawn={(float)(p[0]-w->origin_x),(float)(p[1]-w->origin_y),(float)(p[2]-w->origin_z)};
        w->seed=game->world.seed;
        ok=pe_game_set_world(game,w,spawn);
        if(!ok)err(error,cap,"Recovered settlement has no safe initial spawn or collision assets");
        else {game->yaw=3.14159265358979323846f;snprintf(game->message,sizeof game->message,"Welcome to the settlement.");}
    }
    pe_world_destroy(w);free(w);return ok;
}
