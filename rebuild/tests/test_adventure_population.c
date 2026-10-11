#define _POSIX_C_SOURCE 200809L
#include "pe_test_paths.h"
#include "pe_adventure_npc.h"
#include "pe_settlement.h"
#include "../generated/pe_adventure_population.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"population:%d: %s\n",__LINE__,#c);failures++;}}while(0)
static void sex_queries(void){
    /* Wide-world roots must stay pickable beyond the legacy64m crop. Male
     * Defence radius .6 admits this ray; female .5 and physical/outer miss. */
    PeAdventureNpc npc={.position={350,25,350},.spawn_position={350,25,350},.yaw=17,.placed=true};
    PeVec3 player={350,25,348},origin={350.55f,26,345},direction={0,0,1};float distance=-1;
    CHECK(!pe_adventure_npc_pick_sex(&npc,false,player,origin,direction,INFINITY,&distance));CHECK(distance==-1);
    CHECK(pe_adventure_npc_pick_sex(&npc,true,player,origin,direction,INFINITY,&distance));CHECK(distance>4&&distance<5);
    distance=-1;CHECK(!pe_adventure_npc_pick_sex(&npc,true,(PeVec3){350,25,355},origin,direction,INFINITY,&distance));CHECK(distance==-1);
    CHECK(!pe_adventure_npc_pick_sex(&npc,true,player,origin,(PeVec3){0},INFINITY,&distance));CHECK(distance==-1);
    CHECK(!pe_adventure_npc_pick_sex(&npc,true,player,origin,direction,NAN,&distance));
    CHECK(!pe_adventure_npc_pick_sex(&npc,true,player,origin,direction,INFINITY,NULL));
}
static void survey(const char *directory,const char *collision_name){
    char terrain[4096],geometry[4096],error[256]={0};
    pe_test_format(terrain,sizeof terrain,"%s/rebuild/build/reference-assets/%s/terrain.pevx",pe_test_root(),directory);
    pe_test_format(geometry,sizeof geometry,"%s/rebuild/build/reference-assets/%s/%s",pe_test_root(),directory,collision_name);
    if(access(terrain,R_OK)||access(geometry,R_OK)){fprintf(stderr,"SKIP population %s placement: recovered terrain/collision unavailable\n",directory);return;}
    PeWorld *world=calloc(1,sizeof *world);PeSandboxCollision *cache=pe_sandbox_collision_create();
    if(!world||!cache){CHECK(false);free(world);pe_sandbox_collision_free(cache);return;}
    CHECK(pe_settlement_configure(pe_test_path("rebuild/build/reference-assets"),error,sizeof error));
    if(!pe_settlement_load_world(terrain,world,error,sizeof error)){fprintf(stderr,"population load: %s\n",error);CHECK(false);goto done;}
    const PeSceneCollision *scene=pe_settlement_collision_for_world(world,error,sizeof error);
    if(!scene||!pe_sandbox_collision_update_with_static(cache,world,NULL,0,NULL,0,NULL,0,scene,error,sizeof error)){fprintf(stderr,"population collision: %s\n",error);CHECK(false);goto done;}
    unsigned placed=0,omitted=0;
    for(int i=0;i<PE_ADVENTURE_POPULATION_COUNT;i++){
        const PeAdventurePopulationSource *source=&pe_adventure_population_sources[i];
        PeVec3 anchor={source->position[0]-world->origin_x,source->position[1]-world->origin_y,source->position[2]-world->origin_z};
        if(anchor.x<0||anchor.x>=pe_world_width(world)||anchor.z<0||anchor.z>=pe_world_depth(world)){
            fprintf(stderr,"population %s entity%d %s: outside crop, not tested\n",directory,source->entity_id,source->name);omitted++;continue;
        }
        PeAdventureNpc npc={0};
        bool ok=pe_adventure_npc_refresh_source(&npc,world,cache,anchor,source->yaw_degrees,error,sizeof error);
        fprintf(stderr,"population %s entity%d %s: %s archivedY=%.6f settledY=%.6f delta=%.6f%s%s\n",directory,source->entity_id,source->name,ok?"supported":"FAILED",source->position[1],ok?npc.position.y+world->origin_y:NAN,ok?npc.position.y-anchor.y:NAN,ok?"":" ",ok?"":error);
        CHECK(ok);if(!ok)continue;placed++;
        CHECK(npc.placed&&npc.position.x==anchor.x&&npc.position.z==anchor.z&&npc.yaw==source->yaw_degrees);
        CHECK(!memcmp(&npc.spawn_position,&anchor,sizeof anchor));
        PeTreeCapsule capsule=pe_adventure_npc_capsule(&npc);
        PeVec3 feet={capsule.center.x,capsule.center.y-capsule.height*.5f,capsule.center.z};
        CHECK(!pe_sandbox_collision_overlap(cache,feet,capsule.radius,capsule.height));
        if(source->entity_id==20030){
            /* The archived pose touches a terrain edge with normalY .598;
             * preserve all XYZ while generic player support remains strict. */
            CHECK(!memcmp(&npc.position,&anchor,sizeof anchor));
            CHECK(!pe_sandbox_collision_support(cache,feet,capsule.radius,capsule.height,.1f,.65f).hit);
            CHECK(pe_sandbox_collision_support(cache,feet,capsule.radius,capsule.height,.06f,.1f).hit);
            PeSceneHit floor=pe_sandbox_collision_ray_geometry(cache,(PeVec3){feet.x,feet.y+.01f,feet.z},(PeVec3){0,-1,0},.16f);
            CHECK(floor.hit&&floor.normal.y>=.65f);
        }else CHECK(pe_sandbox_collision_support(cache,feet,capsule.radius,capsule.height,.1f,.65f).hit);
        PeVec3 origin={npc.position.x,npc.position.y+1,npc.position.z-3},player={npc.position.x,npc.position.y,npc.position.z-2};
        float distance=-1;CHECK(pe_adventure_npc_pick_sex(&npc,source->male!=0,player,origin,(PeVec3){0,0,1},INFINITY,&distance));
        CHECK(distance>2&&distance<3);
    }
    fprintf(stderr,"population %s: %u/%d supported, %u outside crop (not covered)\n",directory,placed,PE_ADVENTURE_POPULATION_COUNT,omitted);
    CHECK(placed+omitted==PE_ADVENTURE_POPULATION_COUNT);
done:pe_sandbox_collision_free(cache);pe_world_destroy(world);free(world);
}
int pe_test_adventure_population(void){
    sex_queries();
    /* Process-lifetime immutable geometry must not leak into other fixtures. */
    fflush(NULL);pid_t pid=fork();
    if(pid==0){failures=0;survey("adventure-settlement","settlement-collision.obj");survey("adventure-route","route-collision.obj");fflush(NULL);_exit(failures?1:0);}
    CHECK(pid>=0);if(pid>0){int status=0;CHECK(waitpid(pid,&status,0)==pid&&WIFEXITED(status)&&WEXITSTATUS(status)==0);}
    return failures;
}
