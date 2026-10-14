#define _POSIX_C_SOURCE 200809L
#include "pe_training_tree_hit.h"
#include "pe_training.h"
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <stdio.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"tree ray:%d: %s\n",__LINE__,#x);failures++;}}while(0)
static int solid_movement_tests(void);
int pe_test_training_tree_hit(void){
    int failures=0;float distance=99;PeVec3 c=pe_training_tree_center();
    CHECK(pe_training_tree_hit((PeVec3){c.x-3,c.y,c.z},(PeVec3){1,0,0},&distance));CHECK(fabsf(distance-2)<1e-6f);
    CHECK(!pe_training_tree_hit((PeVec3){c.x-3.001f,c.y,c.z},(PeVec3){1,0,0},NULL));
    CHECK(pe_training_tree_hit((PeVec3){c.x-2,c.y,c.z},(PeVec3){100,0,0},&distance));CHECK(fabsf(distance-1)<1e-6f);
    CHECK(!pe_training_tree_hit((PeVec3){c.x-2,c.y,c.z},(PeVec3){-1,0,0},NULL));
    CHECK(pe_training_tree_hit((PeVec3){c.x,c.y+5.5f,c.z},(PeVec3){0,-1,0},&distance));CHECK(fabsf(distance-2)<1e-6f);
    CHECK(pe_training_tree_hit((PeVec3){c.x,c.y-4,c.z},(PeVec3){0,1,0},&distance));CHECK(fabsf(distance-.5f)<1e-6f);
    /* At actual room player feet height, the lower rounded cap is reachable. */
    CHECK(pe_training_tree_hit((PeVec3){c.x-2.5f,1.647f,c.z},(PeVec3){1,0,0},&distance));
    CHECK(distance>1.5f&&distance<1.7f);
    CHECK(!pe_training_tree_hit((PeVec3){c.x-2.5f,.5f,c.z},(PeVec3){1,0,0},NULL));
    CHECK(pe_training_tree_hit((PeVec3){c.x-2,c.y,c.z+1},(PeVec3){1,0,0},&distance));CHECK(fabsf(distance-2)<1e-6f);
    CHECK(!pe_training_tree_hit((PeVec3){c.x-2,c.y,c.z+1.001f},(PeVec3){1,0,0},NULL));
    CHECK(!pe_training_tree_hit(c,(PeVec3){1,0,0},NULL));
    CHECK(!pe_training_tree_hit((PeVec3){c.x,c.y-3.4f,c.z},(PeVec3){0,-1,0},NULL));
    distance=123;
    CHECK(!pe_training_tree_hit(c,(PeVec3){0,0,0},&distance));CHECK(distance==123);
    CHECK(!pe_training_tree_hit((PeVec3){NAN,0,0},(PeVec3){1,0,0},&distance));CHECK(distance==123);
    CHECK(!pe_training_tree_hit(c,(PeVec3){INFINITY,0,0},NULL));
    /* The grounder's ray: the same capsule, nearest entry, any direction, its own range. */
    PeTreeCapsule capsule=pe_training_tree_capsule();
    CHECK(capsule.center.x==c.x&&capsule.center.y==c.y&&capsule.center.z==c.z&&capsule.radius==1&&capsule.height==7);
    PeSceneHit ray=pe_training_tree_ray((PeVec3){c.x,c.y+5,c.z},(PeVec3){0,-1,0},3);
    CHECK(ray.hit&&fabsf(ray.distance-1.5f)<1e-5f&&ray.normal.y>.999f);
    CHECK(!pe_training_tree_ray((PeVec3){c.x,c.y+5,c.z},(PeVec3){0,-1,0},1.4f).hit);
    ray=pe_training_tree_ray((PeVec3){c.x-3,c.y,c.z},(PeVec3){1,0,0},5);CHECK(ray.hit&&fabsf(ray.distance-2)<1e-5f&&ray.normal.x<-.999f);
    CHECK(!pe_training_tree_ray((PeVec3){c.x-1.01f,2,c.z},(PeVec3){0,-1,0},1).hit);
    CHECK(!pe_training_tree_ray(c,(PeVec3){0,-1,0},10).hit);
    PeSceneHit hit=pe_training_tree_sweep((PeVec3){c.x-3,1.503f,c.z},(PeVec3){6,0,0});
    CHECK(hit.hit&&fabsf(hit.fraction-1.7f/6)<1e-6f&&hit.normal.x<-.999f&&hit.triangle==UINT32_MAX);
    CHECK(!pe_training_tree_sweep((PeVec3){c.x-3,1.503f,c.z},(PeVec3){-6,0,0}).hit);
    CHECK(!pe_training_tree_sweep((PeVec3){c.x-3,c.y+4,c.z},(PeVec3){6,0,0}).hit);
    hit=pe_training_tree_sweep((PeVec3){c.x,c.y+5,c.z},(PeVec3){0,-4,0});
    CHECK(hit.hit&&fabsf(hit.fraction-.375f)<1e-6f&&hit.normal.y>.999f);
    CHECK(pe_training_tree_overlap((PeVec3){c.x,1.503f,c.z}));
    CHECK(!pe_training_tree_overlap((PeVec3){c.x-1.301f,1.503f,c.z}));
    CHECK(pe_training_tree_overlap((PeVec3){c.x-1.299f,1.503f,c.z}));
    CHECK(!pe_training_tree_sweep((PeVec3){c.x-1.299f,1.503f,c.z},(PeVec3){-1,0,0}).hit);
    CHECK(pe_training_tree_sweep((PeVec3){c.x-1.299f,1.503f,c.z},(PeVec3){1,0,0}).hit);
    CHECK(!pe_training_tree_sweep(c,(PeVec3){0,0,0}).hit);
    CHECK(pe_training_tree_overlap((PeVec3){NAN,0,0}));
    CHECK(!pe_training_tree_sweep((PeVec3){NAN,0,0},(PeVec3){1,0,0}).hit);
    return failures+solid_movement_tests();
}

static int solid_movement_tests(void){
    int failures=0;char path[]="/tmp/pe-tree-floor-XXXXXX",error[160];
    int fd=mkstemp(path);CHECK(fd>=0);if(fd<0)return failures;
    FILE *file=fdopen(fd,"w");CHECK(file!=NULL);if(!file){close(fd);unlink(path);return failures;}
    fputs("v 0 1.5 0\nv 60 1.5 0\nv 60 1.5 30\nv 0 1.5 30\nf 1 2 3\nf 1 3 4\n",file);
    CHECK(!fclose(file));PeSceneCollision *scene=pe_scene_load_obj(path,error,sizeof error);unlink(path);CHECK(scene!=NULL);if(!scene)return failures;
    PeTraining t;CHECK(pe_training_init(&t,scene));
    t.tutorial=(PeTutorial){748,3,15,0};t.dialogue_index=2;
    CHECK(pe_training_gathering_enter(&t.gathering,719));
    for(unsigned i=0;i<5;i++){
        CHECK(pe_training_gathering_begin_herb(&t.gathering,i,pe_training_gathering_herb_position(i)));
        CHECK(pe_training_gathering_step(&t.gathering,1.1,(PeGatheringInput){0}));
    }
    CHECK(pe_training_gathering_enter(&t.gathering,748));
    PeVec3 c=pe_training_tree_center();t.position=(PeVec3){c.x-3,1.503f,c.z};
    for(int i=0;i<120;i++){
        pe_training_step(&t,scene,(PeTrainingInput){.move_x=1});
        CHECK(!pe_training_tree_overlap(t.position));
    }
    CHECK(t.position.x<c.x-1.29f&&t.position.x>c.x-1.32f);CHECK(t.grounded);
    /* The step records the floor contact for the grounder's ground angle; in the air there is none. */
    CHECK(t.ground_contact&&t.ground_normal.y>.999f);
    float z=t.position.z;
    for(int i=0;i<30;i++){
        pe_training_step(&t,scene,(PeTrainingInput){.move_x=1,.move_z=1});
        CHECK(!pe_training_tree_overlap(t.position));
    }
    CHECK(t.position.z>z+1.3f);
    t.position=(PeVec3){c.x-2,1.503f,c.z};t.velocity=(PeVec3){0};t.grounded=true;
    pe_training_step(&t,scene,(PeTrainingInput){.move_x=1,.jump=true});
    for(int i=0;i<80;i++){
        pe_training_step(&t,scene,(PeTrainingInput){.move_x=1});
        CHECK(!pe_training_tree_overlap(t.position));
    }
    CHECK(t.position.x<c.x-1.29f); /* A short jump cannot tunnel through tall trunk. */
    t.position=(PeVec3){c.x-2,c.y+5,c.z};t.velocity=(PeVec3){0};t.grounded=false;
    pe_training_step(&t,scene,(PeTrainingInput){.move_x=1});CHECK(!t.ground_contact);
    for(int i=0;i<24;i++)pe_training_step(&t,scene,(PeTrainingInput){.move_x=1});
    CHECK(t.position.x>c.x);CHECK(!pe_training_tree_overlap(t.position));
    /* No global obstacle remains after depletion, even during held mission748. */
    t.gathering.tree_hp=0;t.position=(PeVec3){c.x-2,1.503f,c.z};t.velocity=(PeVec3){0};
    for(int i=0;i<60;i++)pe_training_step(&t,scene,(PeTrainingInput){.move_x=1});
    CHECK(t.position.x>c.x+2.9f);
    pe_scene_free(scene);return failures;
}
