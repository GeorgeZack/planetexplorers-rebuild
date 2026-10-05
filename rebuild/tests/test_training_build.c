#include "pe_training_build.h"
#include "pe_training_terrain.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"training_build:%d: %s\n",__LINE__,#x);failures++;}}while(0)
int pe_test_training_build(void){
    int failures=0;PeTrainingBuild b,before;pe_training_build_init(&b);
    uint32_t dirt=9006;PeVec3 player={10,0,10};
    PeB45Cell cells[4]={{-2,0,0,4,0},{-1,0,0,4,0},{0,0,0,4,0},{1,0,0,4,0}};
    CHECK(pe_training_build_add(&b,&dirt,cells,4,player,.3f,1.8f));CHECK(b.count==4&&dirt==9005&&b.revision==2);
    PeMeshData mesh={0};CHECK(pe_training_build_mesh(&b,&mesh));CHECK(mesh.count==144);free(mesh.vertices);
    PeSceneCollision *scene=NULL;CHECK(pe_training_build_collision(&b,&scene,NULL,0));
    PeSceneHit hit=pe_scene_raycast(scene,(PeVec3){-.75f,2,.25f},(PeVec3){0,-1,0},3);
    CHECK(hit.hit&&fabsf(hit.position.y-.5f)<1e-6f);
    size_t selected=999;PeB45Hit block=pe_training_build_raycast(&b,(PeVec3){-.75f,2,.25f},(PeVec3){0,-8,0},3,&selected);
    CHECK(block.hit&&selected==0&&fabsf(block.distance-hit.distance)<1e-6f);
    before=b;uint32_t old=dirt;size_t duplicate[2]={0,0};
    CHECK(!pe_training_build_remove(&b,&dirt,duplicate,2));CHECK(memcmp(&b,&before,sizeof b)==0&&dirt==old);
    CHECK(!pe_training_build_add(&b,&dirt,cells,4,player,.3f,1.8f));CHECK(memcmp(&b,&before,sizeof b)==0&&dirt==old);
    size_t all[4]={0,1,2,3};dirt=PE_TRAINING_BUILD_ITEM_LIMIT;
    CHECK(!pe_training_build_remove(&b,&dirt,all,4));CHECK(memcmp(&b,&before,sizeof b)==0&&dirt==PE_TRAINING_BUILD_ITEM_LIMIT);
    dirt=old;CHECK(pe_training_build_remove(&b,&dirt,all,4));CHECK(b.count==0&&dirt==9006);
    /* Snapshots remain independent until explicitly replaced. */
    CHECK(pe_scene_raycast(scene,(PeVec3){-.75f,2,.25f},(PeVec3){0,-1,0},3).hit);
    CHECK(pe_training_build_collision(&b,&scene,NULL,0));CHECK(!pe_scene_raycast(scene,(PeVec3){-.75f,2,.25f},(PeVec3){0,-1,0},3).hit);
    pe_scene_free(scene);scene=NULL;
    for(size_t i=0;i<4;i++)CHECK(pe_training_build_add(&b,&dirt,&cells[i],1,player,.3f,1.8f));
    CHECK(dirt==9002);
    size_t one=0;for(unsigned i=0;i<4;i++)CHECK(pe_training_build_remove(&b,&dirt,&one,1));CHECK(dirt==9002);
    /* True capsule geometry: misses cube's corner despite overlapping AABB. */
    PeB45Cell cube={0,0,0,4,0};
    CHECK(pe_training_build_add(&b,&dirt,&cube,1,(PeVec3){.72f,0,.72f},.3f,1.8f));
    CHECK(pe_training_build_clear(&b));before=b;old=dirt;
    CHECK(!pe_training_build_add(&b,&dirt,&cube,1,(PeVec3){.70f,0,.70f},.3f,1.8f));CHECK(memcmp(&b,&before,sizeof b)==0&&old==dirt);
    /* Solid containment even when tiny capsule never intersects a triangle. */
    CHECK(!pe_training_build_add(&b,&dirt,&cube,1,(PeVec3){.25f,.1f,.25f},.05f,.2f));
    /* Ramp's empty high-X half remains traversable, unlike a box proxy. */
    PeB45Cell ramp={0,0,0,8,0};
    CHECK(pe_training_build_add(&b,&dirt,&ramp,1,(PeVec3){.4f,.3f,.25f},.05f,1.8f));
    CHECK(pe_training_build_collision(&b,&scene,NULL,0));
    PeSceneHit slope=pe_scene_capsule_sweep(scene,(PeVec3){.15f,2,.25f},.05f,1.8f,(PeVec3){0,-2,0});
    CHECK(slope.hit&&slope.normal.x>.6f&&slope.normal.y>.6f);pe_scene_free(scene);
    CHECK(pe_training_build_clear(&b));uint32_t rev=b.revision;CHECK(pe_training_build_clear(&b)&&b.revision==rev);
    before=b;old=dirt;cube.x=256;CHECK(!pe_training_build_add(&b,&dirt,&cube,1,player,.3f,1.8f));cube.x=0;cube.material=1;
    CHECK(!pe_training_build_add(&b,&dirt,&cube,1,player,.3f,1.8f));cube.material=0;
    CHECK(!pe_training_build_add(&b,&dirt,&cube,1,(PeVec3){NAN,0,0},.3f,1.8f));CHECK(memcmp(&b,&before,sizeof b)==0&&dirt==old);
    dirt=0;CHECK(!pe_training_build_add(&b,&dirt,&cube,1,player,.3f,1.8f));
    dirt=1;b.revision=UINT32_MAX;before=b;CHECK(!pe_training_build_add(&b,&dirt,&cube,1,player,.3f,1.8f));CHECK(memcmp(&b,&before,sizeof b)==0&&dirt==1);
    PeWorld *world=malloc(sizeof*world);CHECK(world!=NULL);
    if(world){
        pe_training_terrain_init(world);CHECK(pe_training_terrain_clear(world));CHECK(pe_training_terrain_build_anchors(world,false));
        pe_training_build_init(&b);dirt=10;
        PeTrainingBuildTarget target=pe_training_build_target(world,&b,(PeVec3){18,15,11},(PeVec3){0,-1,0},false);
        CHECK(target.hit&&target.terrain&&target.cell.x==36&&target.cell.y==3&&target.cell.z==22&&target.distance>3);
        CHECK(fabsf(target.position.y-1.5f)<1e-6f);
        CHECK(pe_training_build_add(&b,&dirt,&target.cell,1,player,.3f,1.8f));
        target=pe_training_build_target(world,&b,(PeVec3){18.1f,15,11.1f},(PeVec3){0,-1,0},false);
        CHECK(target.hit&&!target.terrain&&target.cell.y==4);
        target=pe_training_build_target(world,&b,(PeVec3){18.1f,15,11.1f},(PeVec3){0,-1,0},true);
        CHECK(target.hit&&target.index==0&&target.cell.y==3);
        target=pe_training_build_target(world,&b,(PeVec3){18.1f,1.75f,11.1f},(PeVec3){0,1,0},true);
        CHECK(!target.hit); /* Source suppresses origin-inside datasource. */
        target=pe_training_build_target(world,&b,(PeVec3){2,2,2},(PeVec3){0,1,0},false);CHECK(!target.hit); /* No room/floor invention. */
        CHECK(!pe_training_build_target(world,&b,(PeVec3){NAN,0,0},(PeVec3){0,1,0},false).hit);
        CHECK(!pe_training_build_target(world,&b,(PeVec3){0,0,0},(PeVec3){0,0,0},false).hit);
        free(world);
    }
    return failures;
}
