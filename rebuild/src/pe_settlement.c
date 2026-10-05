#include "pe_settlement.h"
#include "../generated/pe_adventure_settlement.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef PE_SOURCE_ROOT
#define PE_SOURCE_ROOT "."
#endif
static char root[4096]=PE_SOURCE_ROOT "/rebuild/build/reference-assets";
static PeSceneCollision *geometry;
static void err(char *out,size_t cap,const char *message){if(out&&cap)snprintf(out,cap,"%s",message);}
bool pe_settlement_configure(const char *path,char *error,size_t cap){
    if(!path||!*path||strlen(path)>=sizeof root){err(error,cap,"Invalid settlement asset directory");return false;}
    if(geometry&&strcmp(root,path)){err(error,cap,"Settlement assets cannot change while snapshots borrow their geometry");return false;}
    snprintf(root,sizeof root,"%s",path);return true;
}
bool pe_settlement_matches(const PeWorld *w){
    return w&&w->source==1&&pe_world_depth(w)==128&&w->origin_x==-627&&w->origin_y==16&&w->origin_z==323;
}
static bool asset_path(char *path,size_t cap,const char *name,char *error,size_t ec){
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
    if(fread(header,1,sizeof header,f)!=sizeof header||memcmp(header,"PEVX",4)||u32(header+4)!=1||
       (int32_t)u32(header+8)!=-627||(int32_t)u32(header+12)!=16||(int32_t)u32(header+16)!=323||
       u32(header+20)!=64||u32(header+24)!=32||u32(header+28)!=128){err(error,cap,"Invalid settlement terrain header");goto done;}
    w->depth=128;w->source=1;w->origin_x=-627;w->origin_y=16;w->origin_z=323;w->revision=1;
    /* PEVX stores exactly two bytes/sample, independent of C struct padding. */
    for(size_t i=0;i<pe_world_voxel_count(w);i++){
        unsigned char b[2];if(fread(b,1,2,f)!=2){err(error,cap,"Truncated settlement terrain");goto done;}
        w->voxels[i]=(PeVoxel){b[0],b[1]};
    }
    if(fgetc(f)!=EOF||ferror(f)){err(error,cap,"Unexpected settlement terrain payload");goto done;}
    *out=*w;ok=true;
done:free(w);fclose(f);return ok;
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
    PeWorld *w=malloc(sizeof *w);if(!w){err(error,cap,"Settlement allocation failed");return false;}
    bool ok=pe_settlement_load_world(path,w,error,cap);
    if(ok){
        const double *p=pe_adventure_settlement_initial_spawn;
        PeVec3 spawn={(float)(p[0]+627),(float)(p[1]-16),(float)(p[2]-323)};
        w->seed=game->world.seed;
        ok=pe_game_set_world(game,w,spawn);
        if(!ok)err(error,cap,"Recovered settlement has no safe initial spawn or collision assets");
        else {game->yaw=3.14159265358979323846f;snprintf(game->message,sizeof game->message,"Welcome to the settlement.");}
    }
    free(w);return ok;
}
