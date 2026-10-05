#include "pe_adventure_npc.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(e) do{if(!(e)){fprintf(stderr,"adventure npc:%d: %s\n",__LINE__,#e);failures++;}}while(0)
static void plane(PeWorld *w,int y){memset(w,0,sizeof *w);w->revision=1;for(int z=0;z<PE_WORLD_Z;z++)for(int x=0;x<PE_WORLD_X;x++)for(int j=0;j<=y;j++)w->voxels[((z*PE_WORLD_Y)+j)*PE_WORLD_X+x]=(PeVoxel){255,1};}
static int picking(void){
    int failures=0;PeAdventureNpc n={.position={32,8,27},.spawn_position={32,8,27},.placed=true};
    PeTreeCapsule body=pe_adventure_npc_capsule(&n);CHECK(body.radius==.3700000047683716f&&body.height==1.75f&&body.center.y==8.875f);
    float distance=-1;PeVec3 player={32,8,23},origin={32,8.875f,22};
    CHECK(pe_adventure_npc_pick(&n,player,origin,(PeVec3){0,0,5},INFINITY,&distance));CHECK(fabsf(distance-4.63f)<1e-5f);
    /* Physical wins even though the larger Defence trigger is nearer. */
    CHECK(distance>4.5f);
    CHECK(!pe_adventure_npc_pick(&n,player,origin,(PeVec3){0,0,-1},INFINITY,&distance));
    /* Source bound distance, not distance to the NPC root or the outer trigger. */
    CHECK(pe_adventure_npc_pick(&n,(PeVec3){32,8,22.131f},origin,(PeVec3){0,0,1},INFINITY,&distance));
    CHECK(!pe_adventure_npc_pick(&n,(PeVec3){32,8,22.129f},origin,(PeVec3){0,0,1},INFINITY,&distance));
    /* Enabled source Defence capsule extends above physical top. */
    CHECK(pe_adventure_npc_pick(&n,player,(PeVec3){32,9.9f,22},(PeVec3){0,0,1},INFINITY,&distance));
    /* Generated OutCollider extends below both source root-level capsules. */
    CHECK(pe_adventure_npc_pick(&n,player,(PeVec3){32,7.95f,22},(PeVec3){0,0,1},INFINITY,&distance));
    CHECK(!pe_adventure_npc_pick(&n,player,(PeVec3){32,7.89f,22},(PeVec3){0,0,1},INFINITY,&distance));
    /* Camera can be far behind a nearby player; source obstacle adds 10m. */
    CHECK(pe_adventure_npc_pick(&n,player,(PeVec3){32,8.875f,7},(PeVec3){0,0,1},9.64f,&distance));
    CHECK(fabsf(distance-19.63f)<1e-5f);
    CHECK(!pe_adventure_npc_pick(&n,player,(PeVec3){32,8.875f,7},(PeVec3){0,0,1},9.62f,&distance));
    CHECK(pe_adventure_npc_pick(&n,player,(PeVec3){32,8.875f,-73.3f},(PeVec3){0,0,1},INFINITY,&distance));
    CHECK(!pe_adventure_npc_pick(&n,player,(PeVec3){32,8.875f,-74},(PeVec3){0,0,1},INFINITY,&distance));
    CHECK(!pe_adventure_npc_pick(&n,player,(PeVec3){32,8.875f,27},(PeVec3){0,0,1},INFINITY,&distance));
    distance=123;
    CHECK(!pe_adventure_npc_pick(&n,player,origin,(PeVec3){0},INFINITY,&distance));
    CHECK(!pe_adventure_npc_pick(&n,player,origin,(PeVec3){NAN,0,1},INFINITY,&distance));
    CHECK(!pe_adventure_npc_pick(&n,player,origin,(PeVec3){0,0,1},NAN,&distance));
    CHECK(!pe_adventure_npc_pick(&n,(PeVec3){INFINITY,0,0},origin,(PeVec3){0,0,1},INFINITY,&distance));
    CHECK(distance==123);n.placed=false;CHECK(!pe_adventure_npc_pick(&n,player,origin,(PeVec3){0,0,1},INFINITY,&distance));
    return failures;
}
int pe_test_adventure_npc(void){
    int failures=picking();char error[128];PeWorld *w=calloc(1,sizeof *w);PeSandboxCollision *c=pe_sandbox_collision_create();
    CHECK(w&&c);if(!w||!c){free(w);pe_sandbox_collision_free(c);return failures;}
    PeAdventureNpc n={0},untouched=n;
    CHECK(!pe_adventure_npc_refresh(&n,w,c,error,sizeof error));CHECK(!memcmp(&n,&untouched,sizeof n));
    plane(w,8);CHECK(pe_sandbox_collision_update(c,w,NULL,0,NULL,0,error,sizeof error));
    CHECK(pe_adventure_npc_refresh(&n,w,c,error,sizeof error));CHECK(n.placed&&n.position.x==32&&n.position.z==27);
    CHECK(fabsf(n.position.y-(8+127.0f/255+.003f))<1e-5f);CHECK(!memcmp(&n.position,&n.spawn_position,sizeof(PeVec3)));
    PeAdventureNpc same={0};CHECK(pe_adventure_npc_refresh(&same,w,c,error,sizeof error));CHECK(!memcmp(&n,&same,sizeof n));
    untouched=n;CHECK(pe_adventure_npc_refresh(&n,w,c,error,sizeof error));CHECK(!memcmp(&n,&untouched,sizeof n));
    /* Same world revision with live tree insertion must relocate, but preserve
     * the source role-2 arrival anchor. No tree is added for the NPC itself. */
    PeTreeCapsule tree={{32,n.position.y+3,27},1,7};
    CHECK(pe_sandbox_collision_update_with_trees(c,w,NULL,0,NULL,0,&tree,1,error,sizeof error));
    CHECK(pe_adventure_npc_refresh(&n,w,c,error,sizeof error));
    CHECK(hypotf(n.position.x-32,n.position.z-27)>=1.3699f);
    CHECK(!memcmp(&n.spawn_position,&untouched.spawn_position,sizeof(PeVec3)));
    CHECK(!pe_sandbox_collision_overlap(c,n.position,.3700000047683716f,1.75f));
    PeVec3 old=n.position;
    CHECK(pe_sandbox_collision_update(c,w,NULL,0,NULL,0,error,sizeof error));
    CHECK(pe_adventure_npc_refresh(&n,w,c,error,sizeof error));CHECK(n.position.x==old.x&&n.position.z==old.z);
    plane(w,7);CHECK(pe_sandbox_collision_update(c,w,NULL,0,NULL,0,error,sizeof error));
    CHECK(pe_adventure_npc_refresh(&n,w,c,error,sizeof error));CHECK(n.position.x==old.x&&n.position.z==old.z&&fabsf(n.position.y-old.y+1)<1e-5f);
    CHECK(!memcmp(&n.spawn_position,&untouched.spawn_position,sizeof(PeVec3)));
    /* No support in an empty world: synthetic crop bottom is not terrain. */
    untouched=n;memset(w,0,sizeof *w);CHECK(pe_sandbox_collision_update(c,w,NULL,0,NULL,0,error,sizeof error));
    CHECK(!pe_adventure_npc_refresh(&n,w,c,error,sizeof error));CHECK(!memcmp(&n,&untouched,sizeof n));
    CHECK(error[0]);
    /* Exact geometry query sees construction/furniture; excludes trees. */
    PeB45Cell cell={64,10,54,4,0};PeSandboxBox box={{31,7,26},{33,8,28}};
    CHECK(pe_sandbox_collision_update_with_trees(c,w,&cell,1,&box,1,&tree,1,error,sizeof error));
    PeSceneHit hit=pe_sandbox_collision_ray_geometry(c,(PeVec3){32.25f,20,27.25f},(PeVec3){0,-3,0},100);
    CHECK(hit.hit&&fabsf(hit.distance-12)<1e-5f);
    CHECK(!pe_sandbox_collision_ray_geometry(c,(PeVec3){32.25f,20,27.25f},(PeVec3){0,-1,0},11.99f).hit);
    CHECK(!pe_sandbox_collision_ray_geometry(c,(PeVec3){32.25f,20,27.25f},(PeVec3){0},100).hit);
    CHECK(!pe_sandbox_collision_ray_geometry(c,(PeVec3){NAN,20,27.25f},(PeVec3){0,-1,0},100).hit);
    CHECK(pe_sandbox_collision_update(c,w,&cell,1,NULL,0,error,sizeof error));
    hit=pe_sandbox_collision_ray_geometry(c,(PeVec3){32.25f,20,27.25f},(PeVec3){0,-1,0},100);
    CHECK(hit.hit&&fabsf(hit.distance-14.5f)<1e-5f);
    CHECK(pe_sandbox_collision_update(c,w,NULL,0,NULL,0,error,sizeof error));
    CHECK(!pe_sandbox_collision_ray_geometry(c,(PeVec3){32,20,27},(PeVec3){0,-1,0},100).hit);
    free(w);pe_sandbox_collision_free(c);return failures;
}
