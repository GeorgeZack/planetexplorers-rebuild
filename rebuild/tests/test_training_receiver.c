#include "pe_training_receiver.h"
#include "pe_block.h"
#include "pe_scene_collision.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int failures;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL receiver %d: %s\n",__LINE__,#x);failures++;}}while(0)
typedef struct {PeB45Cell cells[50];unsigned count,queries;int mode;} Scene;
static bool query(void*ctx,const PeTrainingReceiverRay*r,PeTrainingReceiverHit*out){
    Scene*s=ctx;s->queries++;
    if(s->mode==1){*out=(PeTrainingReceiverHit){0};return true;}
    if(s->mode==2&&s->queries==50){*out=(PeTrainingReceiverHit){true,0,NAN};return true;}
    if(s->mode==3&&s->queries==25)return false;
    if(!pe_training_receiver_boxes(r,out))return false;
    for(unsigned i=0;i<s->count;i++){
        PeB45Hit h;
        if(pe_b45_raycast(&s->cells[i],r->origin,r->direction,r->range,&h)&&(!out->hit||h.distance<out->distance))*out=(PeTrainingReceiverHit){true,0,h.distance};
    }
    return true;
}
int pe_test_training_receiver(void){
    failures=0;Scene s={0};PeTrainingReceiverResult result;
    CHECK(pe_training_receivers_evaluate(query,&s,&result)&&s.queries==50&&result.score==-5000&&!result.complete&&result.receiver_mask==((UINT64_C(1)<<50)-1));
    for(unsigned i=0;i<50;i++){
        PeTrainingReceiverRay r;PeTrainingReceiverHit h;CHECK(pe_training_receiver_ray(i,&r)&&r.range==20);
        CHECK(pe_training_receiver_boxes(&r,&h)&&h.receiver==(i<25?1u:2u)&&fabsf(h.distance-18.9f)<.00001f);
        s.cells[i]=i<25?(PeB45Cell){20+(int)(i/5),4+(int)(i%5),24,4,0}:
            (PeB45Cell){34+(int)((i-25)/5),8,20+(int)((i-25)%5),4,0};
    }
    s.count=49;s.queries=0;CHECK(pe_training_receivers_evaluate(query,&s,&result)&&!result.complete&&result.score==-51&&result.receiver_mask==(UINT64_C(1)<<49));
    s.count=50;s.queries=0;CHECK(pe_training_receivers_evaluate(query,&s,&result)&&result.complete&&result.score==50&&!result.receiver_mask);
    /* Blocks behind the receiver must not count as successful shielding. */
    s.count=1;s.cells[0].z=16;CHECK(pe_training_receivers_evaluate(query,&s,&result)&&!result.complete&&(result.receiver_mask&1));
    s.mode=1;CHECK(pe_training_receivers_evaluate(query,&s,&result)&&result.complete&&result.score==50); /* source miss semantics */
    PeTrainingReceiverResult before=result;
    s.mode=2;s.queries=0;CHECK(!pe_training_receivers_evaluate(query,&s,&result)&&!memcmp(&before,&result,sizeof result));
    s.mode=3;s.queries=0;CHECK(!pe_training_receivers_evaluate(query,&s,&result)&&!memcmp(&before,&result,sizeof result));
    CHECK(!pe_training_receivers_evaluate(NULL,&s,&result));
    PeTrainingReceiverRay ray;CHECK(pe_training_receiver_ray(0,&ray));PeTrainingReceiverRay old=ray;
    CHECK(!pe_training_receiver_ray(50,&ray)&&!memcmp(&old,&ray,sizeof ray));
    PeTrainingReceiverHit h={true,2,3},saved=h;ray.direction.x=NAN;
    CHECK(!pe_training_receiver_boxes(&ray,&h)&&!memcmp(&saved,&h,sizeof h));
    PeMeshData mesh={0};CHECK(pe_training_receiver_mesh(&mesh)&&mesh.count==72);
    CHECK(!pe_training_receiver_mesh(&mesh));char error[128];
    PeSceneCollision*scene=pe_scene_from_mesh(&mesh,(PeVec3){0},error,sizeof error);CHECK(scene!=NULL);
    if(scene){
        for(unsigned i=0;i<50;i++){CHECK(pe_training_receiver_ray(i,&ray));PeSceneHit hit=pe_scene_raycast(scene,ray.origin,ray.direction,ray.range);CHECK(hit.hit&&fabsf(hit.distance-18.9f)<.00001f);}
        CHECK(pe_scene_capsule_overlap(scene,(PeVec3){11.5f,2.5f,9.5f},.3f,1.8f));
        CHECK(!pe_scene_capsule_overlap(scene,(PeVec3){11.5f,2.5f,10.0f},.3f,1.8f));
        pe_scene_free(scene);
    }
    pe_mesh_free(&mesh);
    return failures;
}
