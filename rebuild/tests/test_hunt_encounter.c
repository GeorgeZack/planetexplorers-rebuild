#define _POSIX_C_SOURCE 200809L
#include "pe_test_paths.h"
#include "pe_hunt_encounter.h"
#include "pe_settlement.h"
#include "../generated/pe_adventure_hunt_encounter_source.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"hunt encounter:%d: %s\n",__LINE__,#c);failures++;}}while(0)
static void u32(unsigned char*p,uint32_t v){for(int i=0;i<4;i++)p[i]=(unsigned char)(v>>(i*8));}
static void f32(unsigned char*p,float v){uint32_t u;memcpy(&u,&v,4);u32(p,u);}
static void matrix(unsigned char*p,float x){float m[]={1,0,0,x,0,1,0,0,0,0,1,0};for(unsigned i=0;i<12;i++)f32(p+4*i,m[i]);}
static bool write_bytes(const char*path,const void*p,size_t n){FILE*f=fopen(path,"wb");if(!f)return false;bool ok=fwrite(p,1,n,f)==n;return fclose(f)==0&&ok;}
static PeActor *actor_fixture(const char*path){
    unsigned char b[2556]={0};memcpy(b,"PEAC",4);u32(b+4,2);u32(b+8,3);u32(b+12,3);u32(b+16,1);u32(b+20,8);
    for(unsigned i=0;i<3;i++){f32(b+24+i*56,(float)i);f32(b+40+i*56,1);f32(b+56+i*56,1);}
    u32(b+196,1);u32(b+200,2);matrix(b+204,0);
    const char*names[]={"Attack6","Attack7","Attack8","Attack9","idle","walk","run","RunSwordAttack"};
    for(unsigned c=0;c<8;c++){unsigned char*p=b+252+c*288;snprintf((char*)p,32,"%s",names[c]);f32(p+32,2);u32(p+36,5);f32(p+40,2);u32(p+44,c>=4&&c<7);
        for(unsigned frame=0;frame<5;frame++)matrix(p+48+frame*48,(float)frame*.1f);}
    CHECK(write_bytes(path,b,sizeof b));char error[128];return pe_actor_load(path,error,sizeof error);
}
static PeHitboxes *boxes_fixture(const char*path,const PeActor*actor,bool attack){
    unsigned char b[136]={0};memcpy(b,"PEHB",4);u32(b+4,1);u32(b+8,1);u32(b+12,1);uint64_t hash=pe_actor_skeleton_hash(actor);u32(b+16,(uint32_t)hash);u32(b+20,(uint32_t)(hash>>32));
    unsigned char*p=b+32;memcpy(p,attack?"SingleHandSword":"Head",attack?16:5);u32(p+32,attack?1:0);u32(p+36,attack?1:0);f32(p+52,attack?1:2);f32(p+72,1);f32(p+92,attack?.3f:.5f);
    CHECK(write_bytes(path,b,sizeof b));char error[128];return pe_hitboxes_load(path,actor,error,sizeof error);
}
static PeSandboxMeleeGate gate(void){return(PeSandboxMeleeGate){.alive=true,.weapon_owned=true,.grounded=true,.third_person=true,.action_allowed=true,.equipped_item_id=33,.equipped_instance_id=1,.drawn_instance_id=1};}
static void check_deaths(const PeHuntEncounter*initial){
    PeHuntEncounter h=*initial,before;unsigned kills=0;
    PeHuntEncounterDeath external={999,14,212,0};
    before=h;CHECK(pe_hunt_encounter_death(&h,&kills,external)==PE_HUNT_DEATH_IGNORED);CHECK(kills==0&&!memcmp(&h,&before,sizeof h));
    external.prototype=13;external.hp_after=1;CHECK(pe_hunt_encounter_death(&h,&kills,external)==PE_HUNT_DEATH_INVALID);
    external.hp_after=0;external.hp_before=0;CHECK(pe_hunt_encounter_death(&h,&kills,external)==PE_HUNT_DEATH_INVALID);
    external.hp_before=NAN;CHECK(pe_hunt_encounter_death(&h,&kills,external)==PE_HUNT_DEATH_INVALID);
    external.hp_before=212;CHECK(pe_hunt_encounter_death(&h,&kills,external)==PE_HUNT_DEATH_CREDITED);CHECK(kills==1);
    /* External death is counted without proximity or a last-hit/player field;
     * its entity is never inserted into or removed from this cohort. */
    CHECK(!memcmp(h.actors,initial->actors,sizeof h.actors));before=h;
    CHECK(pe_hunt_encounter_death(&h,&kills,external)==PE_HUNT_DEATH_IGNORED);CHECK(kills==1&&!memcmp(&h,&before,sizeof h));
    PeHuntEncounterDeath owned={h.actors[0].entity_id,13,h.actors[0].hp-1,0};
    CHECK(pe_hunt_encounter_death(&h,&kills,owned)==PE_HUNT_DEATH_INVALID);CHECK(!memcmp(&h,&before,sizeof h));
    owned.hp_before=h.actors[0].hp;CHECK(pe_hunt_encounter_death(&h,&kills,owned)==PE_HUNT_DEATH_CREDITED);CHECK(kills==2&&h.actors[0].hp==0);
    CHECK(pe_hunt_encounter_death(&h,&kills,owned)==PE_HUNT_DEATH_IGNORED);CHECK(kills==2);
    for(unsigned i=0;i<6;i++)CHECK(h.actors[i].present);
    external.entity_id=1000;CHECK(pe_hunt_encounter_death(&h,&kills,external)==PE_HUNT_DEATH_CREDITED);CHECK(kills==3);
    external.entity_id=1001;CHECK(pe_hunt_encounter_death(&h,&kills,external)==PE_HUNT_DEATH_COMPLETE);CHECK(kills==4&&!h.active);
    for(unsigned i=0;i<6;i++)CHECK(!h.actors[i].present);
    external.entity_id=1002;before=h;CHECK(pe_hunt_encounter_death(&h,&kills,external)==PE_HUNT_DEATH_IGNORED);CHECK(kills==4&&!memcmp(&h,&before,sizeof h));
    kills=3;CHECK(pe_hunt_encounter_death(&h,&kills,external)==PE_HUNT_DEATH_INVALID);CHECK(kills==3&&!memcmp(&h,&before,sizeof h));
}
static void check_melee(const PeHuntEncounter *initial,const PeActor*actor,const PeHitboxes*weapon){
    PeHuntEncounter h=*initial;PeSandboxMelee m;
    PeSandboxMeleeSetup setup={.weapon_item_id=33,.body=actor,.drawn_weapon=actor,.drawn_capsule=weapon,.gender=PE_COMBAT_FEMALE,.player_id=1,.attack=19,.stamina_reduce_percent=1};
    CHECK(pe_sandbox_melee_init(&m,&setup));float stamina=100;unsigned kills=0;bool done=false;
    bool eligible[6]={true,true,true,true,true,true};
    PeSandboxMeleeFrame frame={.gate=gate(),.player_position=h.actors[0].motion.position,.player_scale=1,.attacker_eligible=true};
    /* Synthetic low-HP contact fixture, explicitly not source spawn evidence. */
    for(unsigned i=0;i<6;i++){h.actors[i].motion.position=frame.player_position;h.actors[i].hp=1;}
    CHECK(pe_sandbox_melee_begin(&m,gate(),&stamina));
    PeSandboxMeleeUpdate update;
    double first=pe_combat_clip(PE_COMBAT_FEMALE,PE_COMBAT_THIRD_PERSON,0)->events[0].time;
    while(m.action.clip_time+pe_combat_animation_speed(stamina)*PE_SANDBOX_MELEE_DT<first)
        CHECK(pe_hunt_encounter_melee(&h,&m,&frame,eligible,&stamina,&kills,&update,&done));
    PeHuntEncounter before=h;PeSandboxMelee old_m=m;float old_stamina=stamina;
    memset(&update,0x5a,sizeof update);PeSandboxMeleeUpdate old_update=update;done=true;
    h.actors[5].seconds=NAN;PeHuntEncounter invalid=h;
    CHECK(!pe_hunt_encounter_melee(&h,&m,&frame,eligible,&stamina,&kills,&update,&done));
    CHECK(!memcmp(&h,&invalid,sizeof h)&&!memcmp(&m,&old_m,sizeof m)&&stamina==old_stamina&&kills==0&&done&&!memcmp(&update,&old_update,sizeof update));
    h=before;CHECK(pe_hunt_encounter_melee(&h,&m,&frame,eligible,&stamina,&kills,&update,&done));
    CHECK(done&&kills==4&&!h.active&&m.action.update_index==old_m.action.update_index+1);
    for(unsigned i=0;i<6;i++)CHECK(!h.actors[i].present&&h.actors[i].hp==0);
    CHECK(pe_hunt_encounter_melee(&h,&m,&frame,eligible,&stamina,&kills,&update,&done));CHECK(!done&&kills==4&&update.target_count==0);
    /* A denied real resolver contact never becomes a quest death. */
    h=before;m=old_m;stamina=old_stamina;kills=0;frame.attacker_eligible=false;
    CHECK(pe_hunt_encounter_melee(&h,&m,&frame,eligible,&stamina,&kills,&update,&done));CHECK(!done&&kills==0);
    for(unsigned i=0;i<6;i++)CHECK(h.actors[i].hp==1);
}
static void check_pending_melee(const PeHuntEncounter *initial,const PeActor*actor,const PeHitboxes*weapon){
    for(unsigned running=0;running<2;running++){
        PeHuntEncounter h=*initial;PeSandboxMelee m={0};
        PeSandboxMeleeSetup setup={.weapon_item_id=33,.body=actor,.drawn_weapon=actor,.drawn_capsule=weapon,.gender=PE_COMBAT_FEMALE,
            .player_id=1,.attack=19,.stamina_reduce_percent=1,.running_motion_ready=running!=0};
        CHECK(pe_sandbox_melee_init(&m,&setup));float stamina=100;unsigned kills=0;bool done=false;
        bool eligible[6]={true,true,true,true,true,true};PeSandboxMeleeGate g=gate();
        if(running){g.velocity.z=5;g.move_requested=true;}
        PeSandboxMeleeFrame f={.gate=g,.player_position=h.actors[0].motion.position,.player_scale=1,.attacker_eligible=true};
        for(unsigned i=0;i<6;i++){h.actors[i].motion.position=f.player_position;h.actors[i].hp=1;}
        CHECK(pe_sandbox_melee_begin(&m,g,&stamina));
        PeSandboxMeleeUpdate result={0};unsigned ticks=0;
        while(!done&&ticks++<90){
            CHECK(pe_sandbox_melee_advance(&m,g,&stamina));PeSandboxMelee pending=m;
            double clock=m.action.clip_time;uint64_t updates=m.action.update_index;float energy=stamina;
            PeCombatInterval interval;CHECK(pe_sandbox_melee_interval(&m,&interval));
            CHECK(interval.valid&&interval.variant==(running?PE_COMBAT_RUNNING:PE_COMBAT_STATIONARY));
            if(m.action.contact_open){
                PeHuntEncounter saved=h;h.actors[5].seconds=NAN;PeHuntEncounter invalid=h;
                PeSandboxMeleeUpdate old=result;bool old_done=done;unsigned old_kills=kills;
                CHECK(!pe_hunt_encounter_melee(&h,&m,&f,eligible,&stamina,&kills,&result,&done));
                CHECK(!memcmp(&h,&invalid,sizeof h)&&!memcmp(&m,&pending,sizeof m)&&!memcmp(&result,&old,sizeof result));
                CHECK(stamina==energy&&kills==old_kills&&done==old_done);h=saved;
                float invalid_energy=NAN;
                CHECK(!pe_hunt_encounter_melee(&h,&m,&f,eligible,&invalid_energy,&kills,&result,&done));
                CHECK(isnan(invalid_energy)&&!memcmp(&h,&saved,sizeof h)&&!memcmp(&m,&pending,sizeof m));
            }
            CHECK(pe_hunt_encounter_melee(&h,&m,&f,eligible,&stamina,&kills,&result,&done));
            CHECK(!m.tick_pending&&m.action.clip_time==clock&&m.action.update_index==updates&&stamina==energy);
            CHECK(!memcmp(&result.interval,&interval,sizeof interval));
        }
        CHECK(done&&kills==4&&stamina==95&&result.weapon_valid);
        CHECK(!h.active&&result.interval.variant==(running?PE_COMBAT_RUNNING:PE_COMBAT_STATIONARY));
    }
}
static void actual_route_child(void){
    const char *world_path=pe_test_path("rebuild/build/reference-assets/adventure-route/terrain.pevx");
    const char *actor_path=pe_test_path("rebuild/build/reference-assets/hare.peac");
    const char *defence_path=pe_test_path("rebuild/build/reference-assets/hare.pehb");
    const char *files[]={world_path,actor_path,defence_path};
    for(unsigned i=0;i<3;i++){FILE*f=fopen(files[i],"rb");if(!f){fprintf(stderr,"hunt encounter: optional actual route/actor unavailable, not tested\n");return;}fclose(f);}
    PeWorld *world=calloc(1,sizeof *world);PeSandboxCollision *collision=pe_sandbox_collision_create();char error[256];
    PeActor *actor=pe_actor_load(actor_path,error,sizeof error);PeHitboxes *defence=pe_hitboxes_load(defence_path,actor,error,sizeof error);
    CHECK(world&&collision&&actor&&defence);if(!world||!collision||!actor||!defence)goto done;
    CHECK(pe_settlement_load_world(world_path,world,error,sizeof error));
    const PeSceneCollision *buildings=pe_settlement_collision_for_world(world,error,sizeof error);CHECK(buildings);
    CHECK(pe_sandbox_collision_update_with_static(collision,world,NULL,0,NULL,0,NULL,0,buildings,error,sizeof error));
    PeHuntEncounter h={0};bool ok=pe_hunt_encounter_prepare(&h,world,collision,actor,defence,0,error,sizeof error);
    if(!ok)fprintf(stderr,"hunt encounter actual placement: %s\n",error);
    CHECK(ok);
    if(ok)for(unsigned i=0;i<6;i++){
        const PeHuntEncounterSourceActor *source=&pe_hunt_encounter_source_actors[i];
        CHECK(h.actors[i].motion.position.x==source->position[0]-world->origin_x&&h.actors[i].motion.position.z==source->position[2]-world->origin_z);
        CHECK(h.actors[i].hp==source->hp&&!pe_sandbox_collision_overlap(collision,h.actors[i].motion.position,h.actors[i].motion.radius,h.actors[i].motion.height));
        fprintf(stderr,"hunt actor %u sourceID%llu supported deltaY %.6f\n",i,(unsigned long long)h.actors[i].entity_id,h.actors[i].motion.position.y-(source->position[1]-world->origin_y));
    }
 done:pe_actor_free(actor);pe_hitboxes_free(defence);pe_sandbox_collision_free(collision);pe_world_destroy(world);free(world);
}
static void actual_route(void){
    /* Actual static collision is immutable process state. Keep its cache out
     * of the parent so later synthetic-root tests remain independent. */
    pid_t pid=fork();CHECK(pid>=0);if(pid<0)return;
    if(pid==0){int before=failures;actual_route_child();_exit(failures!=before);}
    int status=0;pid_t waited;
    do{waited=waitpid(pid,&status,0);}while(waited<0&&errno==EINTR);
    CHECK(waited==pid&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
}
int pe_test_hunt_encounter(void){
    failures=0;char path[]="/tmp/pe-hunt-encounter-XXXXXX";int fd=mkstemp(path);CHECK(fd>=0);if(fd<0)return failures;close(fd);
    PeActor*actor=actor_fixture(path);PeHitboxes *defence=actor?boxes_fixture(path,actor,false):NULL,*weapon=actor?boxes_fixture(path,actor,true):NULL;
    PeWorld *world=calloc(1,sizeof *world);PeSandboxCollision*collision=pe_sandbox_collision_create();char error[256];
    CHECK(actor&&defence&&weapon&&world&&collision);if(!actor||!defence||!weapon||!world||!collision)goto done;
    CHECK(pe_world_resize(world,416,64,416));world->source=1;world->origin_x=-640;world->origin_y=0;world->origin_z=64;
    for(int z=148;z<=174;z++)for(int x=280;x<=301;x++)for(int y=0;y<=18;y++)CHECK(pe_world_set(world,x,y,z,(PeVoxel){255,1}));
    CHECK(pe_sandbox_collision_update(collision,world,NULL,0,NULL,0,error,sizeof error));
    PeHuntEncounter h={0};bool ready=pe_hunt_encounter_prepare(&h,world,collision,actor,defence,0,error,sizeof error);if(!ready)fprintf(stderr,"encounter fixture: %s\n",error);CHECK(ready);if(!ready)goto done;
    for(unsigned i=0;i<6;i++){CHECK(h.actors[i].present&&h.actors[i].hp==pe_hunt_encounter_source_actors[i].hp&&h.actors[i].motion.grounded);}
    PeHuntEncounter before=h;CHECK(!pe_hunt_encounter_prepare(&h,world,collision,actor,weapon,0,error,sizeof error));CHECK(!memcmp(&h,&before,sizeof h));
    CHECK(!pe_hunt_encounter_prepare(&h,world,collision,actor,defence,4,error,sizeof error));CHECK(!memcmp(&h,&before,sizeof h));
    world->origin_x++;CHECK(!pe_hunt_encounter_prepare(&h,world,collision,actor,defence,0,error,sizeof error));CHECK(!memcmp(&h,&before,sizeof h));world->origin_x--;
    /* A block on the final captured actor must not partially install the first5. */
    PeVec3 last=before.actors[5].motion.position;PeSandboxBox box={{last.x-.1f,last.y,last.z-.1f},{last.x+.1f,last.y+3,last.z+.1f}};
    CHECK(pe_sandbox_collision_update(collision,world,NULL,0,&box,1,error,sizeof error));
    CHECK(!pe_hunt_encounter_prepare(&h,world,collision,actor,defence,0,error,sizeof error));CHECK(!memcmp(&h,&before,sizeof h));
    CHECK(pe_sandbox_collision_update(collision,world,NULL,0,NULL,0,error,sizeof error));
    PeHuntEncounterMotion motion[6]={0};for(unsigned i=0;i<6;i++)motion[i]=(PeHuntEncounterMotion){.movement_locked=true,.locked_clip=0,.locked_seconds=.25};
    CHECK(pe_hunt_encounter_step(&h,world,collision,motion));for(unsigned i=0;i<6;i++)CHECK(h.actors[i].clip==0&&h.actors[i].seconds==.25&&!memcmp(&h.actors[i].motion.position,&before.actors[i].motion.position,sizeof(PeVec3)));
    PeHuntEncounter locked=h;motion[5].locked_seconds=NAN;CHECK(!pe_hunt_encounter_step(&h,world,collision,motion));CHECK(!memcmp(&h,&locked,sizeof h));
    memset(motion,0,sizeof motion);CHECK(pe_hunt_encounter_step(&h,world,collision,motion));for(unsigned i=0;i<6;i++)CHECK(h.actors[i].clip==h.idle_clip);
    check_deaths(&before);check_melee(&before,actor,weapon);check_pending_melee(&before,actor,weapon);
    /* Reload retains external progress but creates fresh captured HP/poses and
     * a new transient event history; it is not a persisted damaged cohort. */
    CHECK(pe_hunt_encounter_prepare(&h,world,collision,actor,defence,2,error,sizeof error));CHECK(h.starting_kills==2&&h.credited_count==0);
    unsigned kills=2;PeHuntEncounterDeath d={h.actors[0].entity_id,13,h.actors[0].hp,0};CHECK(pe_hunt_encounter_death(&h,&kills,d)==PE_HUNT_DEATH_CREDITED&&kills==3);
    CHECK(pe_hunt_encounter_prepare(&h,world,collision,actor,defence,kills,error,sizeof error));CHECK(h.actors[0].hp==pe_hunt_encounter_source_actors[0].hp);
    CHECK(pe_hunt_encounter_death(&h,&kills,d)==PE_HUNT_DEATH_COMPLETE&&kills==4);
    pe_hunt_encounter_clear(&h);PeHuntEncounter zero={0};CHECK(!memcmp(&h,&zero,sizeof h));
 done:pe_hitboxes_free(defence);pe_hitboxes_free(weapon);pe_actor_free(actor);pe_sandbox_collision_free(collision);pe_world_destroy(world);free(world);unlink(path);
    actual_route();return failures;
}
