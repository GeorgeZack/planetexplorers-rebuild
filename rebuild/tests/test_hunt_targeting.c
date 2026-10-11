#include "pe_test_paths.h"
#include "pe_hunt_targeting.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int failures;
#define CHECK(c) do {if(!(c)){fprintf(stderr,"hunt targeting:%d %s\n",__LINE__,#c);failures++;}}while(0)
static PeHuntEncounter cohort(const PeActor *actor,const PeHitboxes *boxes){
    PeHuntEncounter h={.ready=true,.active=true,.actor=actor,.defence=boxes};
    for(unsigned i=0;i<PE_HUNT_ENCOUNTER_COUNT;i++)h.actors[i]=(PeHuntEncounterActor){
        .present=true,.entity_id=100+i,.prototype=13,.default_player_id=4,.hp=100,
        .clip=pe_actor_find_clip(actor,"idle"),.motion={.ready=true,.position={100,0,100},.scale=1,.max_hp=100}};
    return h;
}
int pe_test_hunt_targeting(void){
    failures=0;PeSwordTargetResult out={0};PeHuntEncounter empty={0};
    CHECK(pe_hunt_target_acquire(&empty,(PeVec3){0},(PeVec3){0,0,1},&out)&&!out.selected);
    PeSwordTargetResult before=out;
    CHECK(!pe_hunt_target_acquire(&empty,(PeVec3){NAN,0,0},(PeVec3){0,0,1},&out));
    CHECK(!memcmp(&before,&out,sizeof out));
    CHECK(!pe_hunt_target_acquire(&empty,(PeVec3){0},(PeVec3){0,1,0},&out));
    const char *base=getenv("PE_HUNT_TARGET_ASSETS");if(!base)base=pe_test_path("rebuild/build/reference-assets");
    char path[1024],error[256];snprintf(path,sizeof path,"%s/hare.peac",base);
    PeActor *actor=pe_actor_load(path,error,sizeof error);
    if(!actor){fprintf(stderr,"hunt targeting required assets: %s\n",error);return failures+1;}
    snprintf(path,sizeof path,"%s/hare.pehb",base);PeHitboxes *boxes=pe_hitboxes_load(path,actor,error,sizeof error);
    if(!boxes){fprintf(stderr,"hunt targeting required boxes: %s\n",error);pe_actor_free(actor);return failures+1;}
    PeHuntEncounter h=cohort(actor,boxes);PeVec3 origin={0},forward={0,0,1};
    CHECK(pe_hunt_target_acquire(&h,origin,forward,&out)&&!out.selected);
    h.actors[0].motion.position=(PeVec3){0,0,1.5f};
    CHECK(pe_hunt_target_acquire(&h,origin,forward,&out)&&out.selected&&out.aim_active&&out.view_id==100&&out.candidate_index==0);
    CHECK(out.part_index==1); /* Source Body1 centre priority, not closest head. */
    CHECK(out.direction.z>0);
    PeHuntEncounter original=h;
    /* Stable encounter order, not nearest-distance sorting. */
    h.actors[5].motion.position=(PeVec3){0,0,2};
    CHECK(pe_hunt_target_acquire(&h,origin,forward,&out)&&out.view_id==105&&out.candidate_index==5);
    PeHuntEncounterActor swap=h.actors[0];h.actors[0]=h.actors[5];h.actors[5]=swap;
    CHECK(pe_hunt_target_acquire(&h,origin,forward,&out)&&out.view_id==100&&out.candidate_index==5);
    h=original;h.actors[0].hp=0;CHECK(pe_hunt_target_acquire(&h,origin,forward,&out)&&!out.selected);
    h=original;h.actors[0].present=false;CHECK(pe_hunt_target_acquire(&h,origin,forward,&out)&&!out.selected);
    h=original;h.active=false;CHECK(pe_hunt_target_acquire(&h,origin,forward,&out)&&!out.selected);
    /* Horizontal source cone excludes directly behind, but has >90deg reach. */
    h=original;h.actors[0].motion.position=(PeVec3){0,0,-2};
    CHECK(pe_hunt_target_acquire(&h,origin,forward,&out)&&!out.selected);
    h.actors[0].motion.position=(PeVec3){2,0,0};
    CHECK(pe_hunt_target_acquire(&h,origin,forward,&out)&&out.selected);
    h=original;h.actors[0].motion.position=(PeVec3){0,10,1};
    CHECK(pe_hunt_target_acquire(&h,origin,forward,&out)&&!out.selected);
    h=original;h.actors[0].motion.position=(PeVec3){0,0,100};
    CHECK(pe_hunt_target_acquire(&h,origin,forward,&out)&&!out.selected);
    /* Exact visible pose names/clocks, including every sampled attack pose,
     * and arbitrary root yaw/scale use real loaded defence geometry. */
    for(size_t clip=0;clip<pe_actor_stats(actor).clips;clip++){
        const PeActorClip *c=pe_actor_clip(actor,clip);
        for(unsigned frame=0;frame<c->frames;frame++){
            h=original;h.actors[0].clip=clip;h.actors[0].seconds=fmin(c->duration,(double)frame/c->fps);
            h.actors[0].motion.scale=.8f+(frame%6)*.1f;h.actors[0].motion.yaw_degrees=(frame%8)*45;
            CHECK(pe_hunt_target_acquire(&h,origin,forward,&out));
            CHECK(!out.selected||(out.view_id==100&&out.candidate_index==0&&out.part_index<15));
            CHECK(!memcmp(&h.actor,&original.actor,sizeof h.actor));
        }
    }
    /* A late invalid actor never publishes an earlier valid selection. */
    for(int bad=0;bad<12;bad++){
        h=original;PeHuntEncounterActor *a=&h.actors[5];
        if(bad==0)a->entity_id=100;
        if(bad==1)a->default_player_id=1;
        if(bad==2)a->motion.scale=0;
        if(bad==3)a->motion.scale=NAN;
        if(bad==4)a->motion.position.x=NAN;
        if(bad==5)a->motion.yaw_degrees=INFINITY;
        if(bad==6)a->clip=SIZE_MAX;
        if(bad==7)a->seconds=-1;
        if(bad==8)a->hp=NAN;
        if(bad==9)a->prototype=14;
        if(bad==10)h.defence=NULL;
        if(bad==11)h.actor=NULL;
        before=out;PeHuntEncounter saved=h;
        CHECK(!pe_hunt_target_acquire(&h,origin,forward,&out));CHECK(!memcmp(&out,&before,sizeof out));CHECK(!memcmp(&h,&saved,sizeof h));
    }
    pe_hitboxes_free(boxes);pe_actor_free(actor);
    if(!failures)puts("hunt targeting: real source broadphase/defence, ordered acquisition and rollback pass");
    return failures;
}
#ifdef PE_HUNT_TARGETING_STANDALONE
int main(void){return pe_test_hunt_targeting()?1:0;}
#endif
