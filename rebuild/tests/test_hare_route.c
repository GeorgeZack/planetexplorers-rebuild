#include "pe_hare.h"
#include "pe_sandbox_collision.h"
#include "pe_settlement.h"
#include "../generated/pe_adventure_hunt_encounter_source.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifndef PE_SOURCE_ROOT
#define PE_SOURCE_ROOT "."
#endif
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"hare route:%d: %s\n",__LINE__,#c);failures++;}}while(0)
static PeVec3 source_local(const PeWorld *world,const PeHuntEncounterSourceActor *a){
    return(PeVec3){a->position[0]-world->origin_x,a->position[1]-world->origin_y,a->position[2]-world->origin_z};
}
/* Fixture-only transaction demonstration: not an encounter runtime, saved pack
 * or permission to enable passive enemies. Explicit observed anchors only. */
static bool stage_pack(PeHare *out,const PeHareQueries *q,const PeWorld *world){
    PeHare pending[PE_HUNT_ENCOUNTER_ACTOR_COUNT]={0};
    for(unsigned i=0;i<PE_HUNT_ENCOUNTER_ACTOR_COUNT;i++){
        const PeHuntEncounterSourceActor *a=&pe_hunt_encounter_source_actors[i];
        PeHareConfig c=pe_hare_adventure_config(source_local(world,a),a->yaw_degrees);
        if(!pe_hare_init_query(&pending[i],q,&c,a->source_entity_id,a->scale,false))return false;
    }
    memcpy(out,pending,sizeof(pending));return true;
}
int pe_test_hare_route(void){
    failures=0;
    const char *terrain=PE_SOURCE_ROOT "/rebuild/build/reference-assets/adventure-route/terrain.pevx";
    const char *buildings=PE_SOURCE_ROOT "/rebuild/build/reference-assets/adventure-route/route-collision.obj";
    if(!PE_HUNT_ENCOUNTER_REFERENCE_AVAILABLE||access(terrain,R_OK)||access(buildings,R_OK)){
        fprintf(stderr,"hare route: SKIP real six-anchor preparation; reference/route assets unavailable\n");return 0;
    }
    PeWorld *world=calloc(1,sizeof(*world));PeSandboxCollision *collision=pe_sandbox_collision_create();char error[256];
    PeSceneCollision *scene=pe_scene_load_obj(buildings,error,sizeof(error));CHECK(world&&collision&&scene);
    if(!world||!collision||!scene)goto done;
    if(!pe_settlement_load_world(terrain,world,error,sizeof(error))){fprintf(stderr,"hare route: %s\n",error);CHECK(false);goto done;}
    CHECK(pe_settlement_is_route(world));
    if(!pe_sandbox_collision_update_with_static(collision,world,NULL,0,NULL,0,NULL,0,scene,error,sizeof(error))){CHECK(false);goto done;}
    PeHareQueries q=pe_hare_queries_sandbox(collision);PeHare pack[PE_HUNT_ENCOUNTER_ACTOR_COUNT]={0};
    /* Each actor reports independently, so failure identifies the real anchor. */
    bool ready=true;
    for(unsigned i=0;i<PE_HUNT_ENCOUNTER_ACTOR_COUNT;i++){
        const PeHuntEncounterSourceActor *a=&pe_hunt_encounter_source_actors[i];PeVec3 p=source_local(world,a);
        PeHareConfig config=pe_hare_adventure_config(p,a->yaw_degrees);
        bool ok=pe_hare_init_query(&pack[i],&q,&config,a->source_entity_id,a->scale,false);
        if(!ok)fprintf(stderr,"hare route: source agent %u anchor (%.6f,%.6f,%.6f) failed\n",a->source_agent_id,p.x,p.y,p.z);
        CHECK(ok);ready&=ok;if(!ok)continue;
        CHECK(pack[i].scale==a->scale&&pack[i].yaw_degrees==a->yaw_degrees);
        CHECK(fabsf(pack[i].max_hp-a->max_hp)<.0001f&&fabsf(pack[i].attack-a->attack)<.0001f&&fabsf(pack[i].defense-a->defense)<.0001f);
        CHECK(pack[i].spawn.x==p.x&&pack[i].spawn.y==p.y&&pack[i].spawn.z==p.z);
        CHECK(fabsf(pack[i].position.x-p.x)<=.0031f&&fabsf(pack[i].position.z-p.z)<=.0031f); /* Normal-directed contact skin only. */
        CHECK(pack[i].position.y<p.y&&pack[i].position.y>=p.y-5&&pack[i].grounded);
        CHECK(!pe_sandbox_collision_overlap(collision,pack[i].position,pack[i].radius,pack[i].height));
    }
    if(!ready)goto done;
    CHECK(stage_pack(pack,&q,world));PeHare saved[PE_HUNT_ENCOUNTER_ACTOR_COUNT];memcpy(saved,pack,sizeof(saved));
    /* A blocker intersects only the last source anchor: first five still pass. */
    PeVec3 last=source_local(world,&pe_hunt_encounter_source_actors[PE_HUNT_ENCOUNTER_ACTOR_COUNT-1]);
    PeSandboxBox blocker={{last.x-.01f,last.y+.5f,last.z-.01f},{last.x+.01f,last.y+.6f,last.z+.01f}};
    CHECK(pe_sandbox_collision_update_with_static(collision,world,NULL,0,&blocker,1,NULL,0,scene,error,sizeof(error)));
    for(unsigned i=0;i<PE_HUNT_ENCOUNTER_ACTOR_COUNT-1;i++){
        const PeHuntEncounterSourceActor *a=&pe_hunt_encounter_source_actors[i];PeHareConfig c=pe_hare_adventure_config(source_local(world,a),a->yaw_degrees);PeHare h;
        CHECK(pe_hare_init_query(&h,&q,&c,a->source_entity_id,a->scale,false));
    }
    CHECK(!stage_pack(pack,&q,world));CHECK(!memcmp(pack,saved,sizeof(pack)));
 done:
    pe_sandbox_collision_free(collision);pe_scene_free(scene);if(world)pe_world_destroy(world);free(world);return failures;
}
