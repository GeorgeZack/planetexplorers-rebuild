#include "pe_training_terrain.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"terrain:%d: %s\n",__LINE__,#x);failures++;}}while(0)
static PeVoxel world_sample(const PeWorld*w,int x,int y,int z){return pe_world_get(w,x-w->origin_x,y-w->origin_y,z-w->origin_z);}
int pe_test_training_terrain(void){
    int failures=0;PeWorld *w=malloc(sizeof *w),*before=malloc(sizeof *before);CHECK(w&&before);if(!w||!before){free(w);free(before);return failures;}
    memset(w,255,sizeof *w);pe_training_terrain_init(w);CHECK(pe_training_terrain_valid(w));CHECK(w->source==1&&w->seed==751&&w->revision==1);
    size_t counts[32]={0},total=0;
    for(int z=0;z<PE_WORLD_Z;z++)for(int y=0;y<PE_WORLD_Y;y++)for(int x=0;x<PE_WORLD_X;x++){
        PeVoxel v=pe_world_get(w,x,y,z);
        if(v.volume){total++;counts[y]++;CHECK(v.material==1);}else CHECK(v.material==0);
    }
    CHECK(total==726&&counts[4]==360&&counts[5]==288&&counts[6]==59&&counts[7]==19);
    CHECK(world_sample(w,-2,0,2).volume==255&&world_sample(w,15,0,21).volume==255);
    CHECK(world_sample(w,14,1,20).volume==210&&world_sample(w,6,3,11).volume==190);
    CHECK(world_sample(w,3,2,9).volume==130&&world_sample(w,7,3,14).volume==110);
    CHECK(world_sample(w,0,2,0).volume==0&&world_sample(w,-3,0,2).volume==0);
    PeVec3 local=pe_training_terrain_to_local(w,(PeVec3){-2,0,2}),room=pe_training_terrain_to_world(w,local);
    CHECK(local.x==6&&local.y==4&&local.z==10&&room.x==-2&&room.y==0&&room.z==2);
    CHECK(isnan(pe_training_terrain_to_local(w,(PeVec3){NAN,0,0}).x));
    PeHit ray=pe_training_terrain_raycast(w,(PeVec3){6,8,11},(PeVec3){0,-7,0},10);
    CHECK(ray.hit&&ray.position.y>3.30f&&ray.position.y<3.35f&&ray.normal.y>.9f);
    CHECK(ray.x>=0&&ray.x<PE_WORLD_X&&ray.y>=0&&ray.y<PE_WORLD_Y);
    PeHit neg=pe_training_terrain_raycast(w,(PeVec3){-1,8,10},(PeVec3){0,-1,0},10);
    CHECK(neg.hit&&neg.position.x==-1&&neg.position.y>1&&neg.position.y<2);
    PeHit dig=pe_training_terrain_dig_target(w,(PeVec3){6,5,11},(PeVec3){0,-1,0},(PeVec3){6,2,11},3);
    CHECK(dig.hit&&dig.x==14&&dig.y==7&&dig.z==19);CHECK(dig.position.x==6&&dig.position.y==3.5f&&dig.position.z==11);
    char error[180];PeSceneCollision *scene=NULL;CHECK(pe_training_terrain_collision(w,&scene,error,sizeof error));
    if(scene){
        PeSceneHit hit=pe_scene_raycast(scene,(PeVec3){6,8,11},(PeVec3){0,-1,0},10);
        CHECK(hit.hit&&fabsf(hit.position.y-ray.position.y)<1e-5f);
        PeSceneHit fall=pe_scene_capsule_sweep(scene,(PeVec3){6,5,11},.3f,1.8f,(PeVec3){0,-4,0});
        CHECK(fall.hit&&fall.normal.y>.9f&&fall.fraction>.3f&&fall.fraction<.6f);
        CHECK(pe_scene_capsule_overlap(scene,(PeVec3){6,3.1f,11},.3f,1.8f));
        CHECK(!pe_scene_capsule_overlap(scene,(PeVec3){6,3.5f,11},.3f,1.8f));
    }
    *before=*w;CHECK(pe_training_terrain_apply(w)&&!memcmp(w,before,sizeof *w));
    CHECK(pe_world_set(w,3,14,3,(PeVoxel){255,8})); /* Unrelated sample survives source clear. */
    uint32_t rev=w->revision;CHECK(pe_training_terrain_clear(w)&&w->revision==rev+1);
    CHECK(world_sample(w,-2,0,2).volume==0&&pe_world_get(w,3,14,3).volume==255);
    rev=w->revision;CHECK(pe_training_terrain_clear(w)&&w->revision==rev);
    CHECK(!pe_training_terrain_raycast(w,(PeVec3){6,8,11},(PeVec3){0,-1,0},10).hit);
    CHECK(pe_world_set(w,3,14,3,(PeVoxel){0,0}));CHECK(pe_training_terrain_collision(w,&scene,error,sizeof error));
    CHECK(pe_scene_stats(scene).triangles==0&&!pe_scene_capsule_overlap(scene,(PeVec3){6,3,11},.3f,1.8f));
    CHECK(!pe_scene_capsule_sweep(scene,(PeVec3){6,5,11},.3f,1.8f,(PeVec3){0,-4,0}).hit);
    CHECK(pe_training_terrain_apply(w)&&world_sample(w,6,3,11).volume==190);
    w->revision=UINT32_MAX;*before=*w;CHECK(!pe_training_terrain_clear(w)&&!memcmp(w,before,sizeof *w));
    w->revision=9;w->origin_x++;*before=*w;PeSceneCollision *old=scene;
    CHECK(!pe_training_terrain_collision(w,&scene,error,sizeof error)&&scene==old);
    CHECK(!pe_training_terrain_clear(w)&&!memcmp(w,before,sizeof *w));
    CHECK(!pe_training_terrain_apply(w)&&!memcmp(w,before,sizeof *w));
    CHECK(isnan(pe_training_terrain_to_world(w,(PeVec3){0}).x));
    pe_training_terrain_init(w);CHECK(pe_training_terrain_clear(w));
    w->voxels[0]=(PeVoxel){99,7};
    CHECK(pe_training_terrain_build_anchors(w,false));size_t anchor_count=0;
    for(int z=0;z<PE_WORLD_Z;z++)for(int y=0;y<PE_WORLD_Y;y++)for(int x=0;x<PE_WORLD_X;x++)if(pe_world_get(w,x,y,z).volume==255)anchor_count++;
    CHECK(anchor_count==32&&world_sample(w,17,1,10).volume==255&&world_sample(w,20,1,13).material==1);
    CHECK(world_sample(w,10,2,9).volume==255&&world_sample(w,13,5,9).volume==255);
    CHECK(world_sample(w,14,5,9).volume==0&&world_sample(w,17,2,10).volume==0);
    uint32_t anchor_revision=w->revision;CHECK(pe_training_terrain_build_anchors(w,false)&&w->revision==anchor_revision);
    w->revision=UINT32_MAX;*before=*w;CHECK(!pe_training_terrain_build_anchors(w,true)&&memcmp(w,before,sizeof*w)==0);
    w->revision=anchor_revision;CHECK(pe_training_terrain_build_anchors(w,true)&&w->voxels[0].volume==99&&w->voxels[0].material==7);
    CHECK(world_sample(w,17,1,10).volume==0&&world_sample(w,13,5,9).volume==0);
    pe_scene_free(scene);free(w);free(before);return failures;
}
