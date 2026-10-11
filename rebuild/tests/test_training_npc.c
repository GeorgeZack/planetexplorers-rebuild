#define _POSIX_C_SOURCE 200809L
#include "pe_test_paths.h"
#include "pe_training_npc.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
static int failures;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL training NPC %d: %s\n",__LINE__,#x);failures++;}}while(0)
int pe_test_training_npc(void){
    failures=0;const int ids[]={9004,9005,9006,9009,9020,9031,9041,9047,9049};
    for(size_t i=0;i<PE_TRAINING_NPC_COUNT;i++){
        const PeTrainingNpc*n=pe_training_npc_at(i);CHECK(n&&n->id==ids[i]&&pe_training_npc_find(ids[i])==n);
        CHECK(n&&n->radius>.369f&&n->radius<.371f&&n->position.x!=0&&n->bundle&&n->model&&n->behavior);
        CHECK(n&&n->tutorial_neutralized==(i<7));
    }
    CHECK(!pe_training_npc_at(9)&&!pe_training_npc_find(9001));
    CHECK(pe_training_npc_find(9041)->yaw==275&&pe_training_npc_find(9041)->position.y==1.8f);
    CHECK(pe_training_npc_find(9020)->center_y-pe_training_npc_find(9020)->height*.5f<-.0374f);
    PeVertex vertices[6]={0};PeVec3 p[]={{-100,1.5f,-100},{100,1.5f,100},{100,1.5f,-100},{-100,1.5f,-100},{-100,1.5f,100},{100,1.5f,100}};
    for(unsigned i=0;i<6;i++)vertices[i].position=p[i];
    PeMeshData mesh={vertices,6};char error[128];
    PeSceneCollision*floor=pe_scene_from_mesh(&mesh,(PeVec3){0},error,sizeof error);CHECK(floor!=NULL);
    if(floor){
        PeVec3 root={9,8,7};const PeTrainingNpc*andy=pe_training_npc_find(9041);
        CHECK(pe_training_npc_ground(andy,floor,&root)&&fabsf(root.y-1.503f)<.0001f&&root.x==32&&root.z==12);
        CHECK(pe_training_npc_ground(pe_training_npc_find(9020),floor,&root)&&fabsf(root.y-1.5405f)<.0002f);
        PeTrainingNpc embedded=*andy;embedded.position.y=1.3f;
        CHECK(pe_training_npc_ground(&embedded,floor,&root)&&fabsf(root.y-1.503f)<.0001f);
        PeVec3 before=root;embedded.position.y=NAN;
        CHECK(!pe_training_npc_ground(&embedded,floor,&root)&&!memcmp(&before,&root,sizeof root));
        embedded=*andy;embedded.position.y=20;
        CHECK(!pe_training_npc_ground(&embedded,floor,&root)&&!memcmp(&before,&root,sizeof root));
        CHECK(!pe_training_npc_ground(NULL,floor,&root));pe_scene_free(floor);
    }
    const char*path=pe_test_path("rebuild/build/reference-assets/room-collision.obj");
    if(access(path,R_OK)==0){
        PeSceneCollision*room=pe_scene_load_obj(path,error,sizeof error);CHECK(room!=NULL);
        if(room){
            for(size_t i=0;i<PE_TRAINING_NPC_COUNT;i++){
                const PeTrainingNpc*n=pe_training_npc_at(i);PeVec3 root;
                bool ok=pe_training_npc_ground(n,room,&root);CHECK(ok);
                if(ok){PeVec3 feet=root;feet.y+=n->center_y-n->height*.5f;
                    CHECK(root.x==n->position.x&&root.z==n->position.z&&!pe_scene_capsule_overlap(room,feet,n->radius,n->height));
                    printf("Training NPC %d settled root %.6f %.6f %.6f yaw %.0f\n",n->id,root.x,root.y,root.z,n->yaw);
                }
            }
            pe_scene_free(room);
        }
    }
    return failures;
}
