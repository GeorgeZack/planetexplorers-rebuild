/* Headless integration of the actual staged female408 pose/qualified root group.
 * Ordinary core APIs only; this is a constructed collision fixture, not a live
 * gameplay/kill proof. Missing recovered assets are explicitly skipped. */
#include "pe_game.h"
#include "pe_running_attack_motion.h"
#include "pe_sandbox_melee.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef PE_SOURCE_ROOT
#define PE_SOURCE_ROOT ".."
#endif
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"running pipeline:%d %s\n",__LINE__,#c);failures++;}}while(0)
static bool near(PeVec3 a,PeVec3 b,float epsilon){return fabsf(a.x-b.x)<epsilon&&fabsf(a.y-b.y)<epsilon&&fabsf(a.z-b.z)<epsilon;}
static PeSandboxMeleeGate gate(const PeGame *g){
    return (PeSandboxMeleeGate){.alive=g->hp>0,.weapon_owned=true,.grounded=g->grounded,.third_person=true,
        .action_allowed=true,.move_requested=true,.equipped_item_id=33,.equipped_instance_id=77,.drawn_instance_id=77,.velocity=g->velocity};
}
static bool flat(PeGame *g,PeSandboxCollision *c,unsigned obstacle){
    pe_game_destroy(g);pe_game_init(g,44);g->mode=PE_GAME_ADVENTURE;
    for(int z=0;z<PE_WORLD_Z;z++)for(int y=0;y<PE_WORLD_Y;y++)for(int x=0;x<PE_WORLD_X;x++)
        pe_world_set(&g->world,x,y,z,(PeVoxel){y<10?255:0,y<10?PE_MAT_DIRT:PE_MAT_AIR});
    g->player=(PeVec3){32,10.05f,32};g->velocity=(PeVec3){0};g->block_count=g->tree_count=0;
    if(obstacle==1)for(int y=9;y<13;y++)for(int z=30;z<35;z++)g->blocks[g->block_count++]=(PeBlock){34,y,z,PE_ITEM_FENCE};
    if(obstacle==2){g->trees[0]=(PeTree){.position={35.1f,9.5f,32.2f},.remaining=1,.health=100};g->tree_count=1;}
    memset(&g->inventory,0,sizeof g->inventory);g->inventory.next_instance_id=78;
    g->inventory.slots[0]=(PeSlot){.item_id=33,.count=1,.instance_id=77,.durability=100};g->equipped_instance_id=77;
    memset(&g->craft,0,sizeof g->craft);g->hp=g->max_hp=500;g->stamina=g->stamina_max=100;g->medpack_cooldown=5;
    char error[160];
    if(!pe_game_collision_update(g,c,error,sizeof error)||!pe_game_collision_settle(g,c)){fprintf(stderr,"running pipeline collision: %s\n",error);return false;}
    g->velocity=(PeVec3){5,0,0};return true;
}
static void scenario(PeGame *g,PeSandboxCollision *c,PeSandboxMeleeSetup setup,const PeRootMotion *curve,unsigned obstacle,bool combos){
    CHECK(flat(g,c,obstacle));PeSandboxMelee melee;CHECK(pe_sandbox_melee_init(&melee,&setup));
    CHECK(pe_sandbox_melee_begin(&melee,gate(g),&g->stamina));
    PeVec3 start=g->player,combo_root={0};bool have_combo_root=false;unsigned stages=1,blocked=0,ticks=0;
    PeSandboxMeleeUpdate update={0};double running_end=0;
    const PeInput held={.move_x=-1,.move_z=1,.jump=true,.sprint=true};
    while(melee.action.active&&ticks<600){
        if(combos)pe_sandbox_melee_queue(&melee,gate(g));
        uint64_t previous_update=melee.action.update_index;
        CHECK(pe_sandbox_melee_advance(&melee,gate(g),&g->stamina));
        CHECK(melee.action.update_index==previous_update+1&&melee.tick_pending);
        PeCombatInterval span;CHECK(pe_sandbox_melee_interval(&melee,&span));CHECK(span.valid);
        PeVec3 request={0};
        if(span.stage==0){
            CHECK(pe_running_attack_motion_request(curve,&span,g->velocity,90,PE_SANDBOX_MELEE_DT,&request));
            running_end=span.to;
        }
        double clock=g->game_seconds;float cooldown=g->medpack_cooldown,energy=g->stamina;
        PeVec3 before=g->player;
        CHECK(pe_game_step_collision_motion(g,held,PE_FIXED_DT,c,request));
        CHECK(fabs(g->game_seconds-clock-12.0*PE_FIXED_DT)<1e-9);
        CHECK(fabsf(g->medpack_cooldown-(cooldown-(float)PE_SANDBOX_MELEE_DT))<1e-6f);
        CHECK(g->stamina==energy&&g->grounded&&!g->jumping&&!g->sprinting);
        CHECK(!pe_sandbox_collision_overlap(c,g->player,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT));
        float dx=g->player.x-before.x,dz=g->player.z-before.z;
        if(fabsf(dx-request.x*PE_FIXED_DT)>.002f||fabsf(dz-request.z*PE_FIXED_DT)>.002f)blocked++;
        if(!obstacle){CHECK(fabsf(dx-request.x*PE_FIXED_DT)<.00002f);CHECK(fabsf(dz-request.z*PE_FIXED_DT)<.00002f);}
        if(have_combo_root)CHECK(near(g->player,combo_root,.00002f));
        PeSandboxMeleeFrame frame={.gate=gate(g),.player_position=g->player,.player_yaw=90,.player_scale=1,.attacker_eligible=true};
        uint64_t advanced=melee.action.update_index;double authored=melee.action.clip_time;
        CHECK(pe_sandbox_melee_resolve(&melee,&frame,NULL,0,&update));
        CHECK(!melee.tick_pending&&melee.action.update_index==advanced&&melee.action.clip_time==authored&&g->stamina==energy);
        if(update.weapon_valid){
            size_t clip=pe_actor_find_clip(setup.drawn_weapon,pe_combat_pose_name(&melee.action));PeHitboxCapsule expected;
            CHECK(pe_hitbox_pose(setup.drawn_capsule,setup.drawn_weapon,0,clip,authored,g->player,90,1,&expected));
            CHECK(near(expected.a,update.weapon.a,.000001f)&&near(expected.b,update.weapon.b,.000001f)&&expected.radius==update.weapon.radius);
        }
        if(update.advance==PE_COMBAT_NEXT_STAGE){stages++;if(span.stage==0){combo_root=g->player;have_combo_root=true;}}
        ticks++;
    }
    CHECK(!melee.action.active&&!melee.tick_pending&&ticks<600);
    CHECK(stages==(combos?4u:1u)&&g->stamina==(combos?80:95));
    CHECK(running_end==(combos?.7091204524040222:1.1680725812911987));
    if(!obstacle)CHECK(g->player.x>start.x+1);
    if(obstacle){
        CHECK(blocked>0);
        if(obstacle==1)CHECK(g->player.x<33.71f);
        else{PeTreeCapsule tree=pe_tree_collision_source(g->trees[0].position);CHECK(!pe_tree_collision_overlap(&tree,g->player,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT));}
        /* Removing the blocker after the action cannot release stored root
         * displacement or replay the ended outgoing interval. */
        PeVec3 stopped=g->player;g->block_count=g->tree_count=0;char error[160];CHECK(pe_game_collision_update(g,c,error,sizeof error));
        CHECK(pe_game_step_collision_motion(g,held,PE_FIXED_DT,c,(PeVec3){0}));CHECK(near(stopped,g->player,.00002f));
    }
    /* Inactive preparation still has to resolve once, but supplies no root. */
    CHECK(pe_sandbox_melee_advance(&melee,gate(g),&g->stamina));PeCombatInterval idle;
    CHECK(pe_sandbox_melee_interval(&melee,&idle)&&!idle.valid);
    PeSandboxMeleeFrame frame={.gate=gate(g),.player_position=g->player,.player_yaw=90,.player_scale=1};
    CHECK(pe_sandbox_melee_resolve(&melee,&frame,NULL,0,&update)&&!melee.tick_pending&&update.advance==PE_COMBAT_IDLE);
}
static void outgoing_contacts(PeGame *g,PeSandboxCollision *c,PeSandboxMeleeSetup setup,const PeRootMotion *curve){
    const char *actor_path=PE_SOURCE_ROOT "/rebuild/build/reference-assets/hare.peac";
    const char *boxes_path=PE_SOURCE_ROOT "/rebuild/build/reference-assets/hare.pehb";
    const char *paths[]={actor_path,boxes_path};
    for(unsigned i=0;i<2;i++){FILE *f=fopen(paths[i],"rb");if(!f){fprintf(stderr,"running pipeline: SKIP recovered hare contact fixture missing: %s\n",paths[i]);return;}fclose(f);}
    char error[256];PeActor *hare=pe_actor_load(actor_path,error,sizeof error);
    PeHitboxes *boxes=hare?pe_hitboxes_load(boxes_path,hare,error,sizeof error):NULL;CHECK(hare&&boxes);
    if(!hare||!boxes)goto done;
    size_t clip=pe_actor_find_clip(hare,"idle"),defence=SIZE_MAX;
    for(size_t i=0;i<pe_hitboxes_count(boxes);i++){
        const PeHitboxDefinition *d=pe_hitboxes_get(boxes,i);
        if(d->kind==PE_HITBOX_DEFENCE&&d->joint!=PE_HITBOX_DISABLED){defence=i;break;}
    }
    CHECK(clip!=PE_ACTOR_BIND_POSE&&defence!=SIZE_MAX);
    if(clip==PE_ACTOR_BIND_POSE||defence==SIZE_MAX)goto done;
    PeHitboxCapsule target_pose;CHECK(pe_hitbox_pose(boxes,hare,defence,clip,.37,(PeVec3){0},27,1,&target_pose));
    PeVec3 target_root={0};unsigned sample_tick=0;
    /* First pass measures the real blade at the first open tick, AFTER its
     * qualified root displacement and collision sweep. Second pass fixes the
     * whole posed hare at that predetermined root before the action starts.
     * This deliberately arranged overlap is a test fixture, not a supported
     * world spawn or evidence that the player earned a mission kill. */
    for(unsigned pass=0;pass<2;pass++){
        CHECK(flat(g,c,0));PeSandboxMelee melee;CHECK(pe_sandbox_melee_init(&melee,&setup));
        CHECK(pe_sandbox_melee_begin(&melee,gate(g),&g->stamina));
        PeKnifeContactTarget targets[3];
        for(unsigned i=0;i<3;i++)targets[i]=(PeKnifeContactTarget){.entity_id=700+i,.actor=hare,.hitboxes=boxes,
            .clip=clip,.seconds=.37,.position=target_root,.yaw=27,.scale=1,.defense=29,.hp=i==1?.01f:212,.max_hp=212,.eligible=true};
        targets[2].position.z+=20;
        unsigned hits[3]={0},deaths[3]={0},ticks=0;float damage=0;
        while(melee.action.active&&ticks<120){
            CHECK(pe_sandbox_melee_advance(&melee,gate(g),&g->stamina));PeCombatInterval span;
            CHECK(pe_sandbox_melee_interval(&melee,&span));PeVec3 request;
            CHECK(pe_running_attack_motion_request(curve,&span,g->velocity,90,PE_SANDBOX_MELEE_DT,&request));
            CHECK(pe_game_step_collision_motion(g,(PeInput){0},PE_FIXED_DT,c,request));
            PeSandboxMeleeFrame frame={.gate=gate(g),.player_position=g->player,.player_yaw=90,.player_scale=1,.attacker_eligible=true};
            PeSandboxMeleeUpdate update={0};bool open=melee.action.contact_open;uint64_t clock=melee.action.update_index;
            if(!pass){
                CHECK(pe_sandbox_melee_resolve(&melee,&frame,NULL,0,&update));
                if(open){
                    CHECK(update.weapon_valid);
                    target_root=(PeVec3){(update.weapon.a.x+update.weapon.b.x-target_pose.a.x-target_pose.b.x)*.5f,
                        (update.weapon.a.y+update.weapon.b.y-target_pose.a.y-target_pose.b.y)*.5f,
                        (update.weapon.a.z+update.weapon.b.z-target_pose.a.z-target_pose.b.z)*.5f};
                    sample_tick=ticks;break;
                }
            }else{
                float previous[3]={targets[0].hp,targets[1].hp,targets[2].hp};
                CHECK(pe_sandbox_melee_resolve(&melee,&frame,targets,3,&update));
                CHECK(!melee.tick_pending&&melee.action.update_index==clock);
                for(unsigned i=0;i<3;i++){
                    if(update.status[i]==PE_KNIFE_CONTACT_HIT){
                        CHECK(open&&ticks==sample_tick&&i<2);hits[i]++;deaths[i]+=update.contact[i].died;
                        float expected;CHECK(pe_combat_damage(setup.attack,29,false,0,false,update.contact[i].contact_scale,&expected));
                        CHECK(fabsf(update.contact[i].damage-expected)<.00001f);
                        CHECK(targets[i].hp==fmaxf(0,previous[i]-expected));if(!i)damage=expected;
                    }else CHECK(targets[i].hp==previous[i]);
                }
                if(open)CHECK(update.status[2]==PE_KNIFE_CONTACT_MISS);
            }
            ticks++;
        }
        if(!pass)CHECK(sample_tick>0&&sample_tick<120);
        else{
            CHECK(!melee.action.active&&ticks<120&&g->stamina==95);
            CHECK(hits[0]==1&&hits[1]==1&&hits[2]==0&&deaths[0]==0&&deaths[1]==1&&deaths[2]==0);
            CHECK(damage>0&&targets[0].hp==212-damage&&targets[1].hp==0&&targets[2].hp==212);
            printf("running pipeline: recovered408 post-sweep blade hit posed hare once (damage %.6f), one fixture death, far target untouched\n",damage);
        }
    }
 done:pe_hitboxes_free(boxes);pe_actor_free(hare);
}
int pe_test_running_attack_pipeline(void){
    failures=0;const char *root=getenv("PE_RUNNING_STAGE");if(!root)root=PE_SOURCE_ROOT "/rebuild/build/running-player-20261006";
    const char *motion=getenv("PE_RUNNING_MOTION");if(!motion)motion=PE_SOURCE_ROOT "/rebuild/build/running-motion-20261006/running-knife.perm";
    const char *names[]={"player/player-part-0-material-0.peac","player-knife-drawn/knife-drawn-part-0-material-0.peac","player-knife-drawn/knife-drawn.pehb"};
    char paths[3][2048],error[256];
    for(unsigned i=0;i<3;i++){
        int n=snprintf(paths[i],sizeof paths[i],"%s/%s",root,names[i]);if(n<0||(size_t)n>=sizeof paths[i])return 1;
        FILE *f=fopen(paths[i],"rb");if(!f){fprintf(stderr,"running pipeline: SKIP recovered pose fixture missing: %s\n",paths[i]);return 0;}fclose(f);
    }
    FILE *file=fopen(motion,"rb");if(!file){fprintf(stderr,"running pipeline: SKIP qualified motion fixture missing: %s\n",motion);return 0;}fclose(file);
    PeActor *body=pe_actor_load(paths[0],error,sizeof error),*knife=pe_actor_load(paths[1],error,sizeof error);
    PeHitboxes *boxes=knife?pe_hitboxes_load(paths[2],knife,error,sizeof error):NULL;
    PeRootMotion *curve=pe_root_motion_load(motion,error,sizeof error);PeGame *g=calloc(1,sizeof *g);PeSandboxCollision *c=pe_sandbox_collision_create();
    CHECK(body&&knife&&boxes&&curve&&g&&c);
    if(body&&knife&&boxes&&curve&&g&&c){
        PeSandboxMeleeSetup setup={.weapon_item_id=33,.history_enabled=true,.history_local_center={0,.875f,0},.body=body,.drawn_weapon=knife,.drawn_capsule=boxes,.gender=PE_COMBAT_FEMALE,.player_id=1,
            .attack=19,.stamina_reduce_percent=1,.running_motion_ready=true};
        scenario(g,c,setup,curve,0,false);scenario(g,c,setup,curve,0,true);
        scenario(g,c,setup,curve,1,false);scenario(g,c,setup,curve,2,false);
        outgoing_contacts(g,c,setup,curve);
    }
    pe_sandbox_collision_free(c);if(g)pe_game_destroy(g);free(g);pe_root_motion_free(curve);pe_hitboxes_free(boxes);pe_actor_free(body);pe_actor_free(knife);
    if(!failures)puts("running pipeline: recovered408 advance/motion/sweep/resolve, single clocks, combos, wall/tree and no deferred displacement passed");
    return failures;
}
