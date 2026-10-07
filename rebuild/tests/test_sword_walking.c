/* Recovered female Attack6..9 source curves and actual body/blade/hare assets.
 * Constructed CPU fixtures; this is not an ordinary gameplay or kill proof. */
#include "pe_game.h"
#include "pe_sandbox_melee.h"
#include "pe_sword_root_motion.h"
#include "pe_sword_motion.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef PE_SOURCE_ROOT
#define PE_SOURCE_ROOT ".."
#endif
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"sword walking:%d %s\n",__LINE__,#c);failures++;}}while(0)
static bool near(PeVec3 a,PeVec3 b,float e){return fabsf(a.x-b.x)<e&&fabsf(a.y-b.y)<e&&fabsf(a.z-b.z)<e;}
static PeSandboxMeleeGate gate(PeVec3 velocity){
    return (PeSandboxMeleeGate){.alive=true,.weapon_owned=true,.grounded=true,.third_person=true,
        .action_allowed=true,.move_requested=true,.equipped_item_id=33,.equipped_instance_id=77,.drawn_instance_id=77,.velocity=velocity};
}
static bool flat(PeGame *g,PeSandboxCollision *c,bool wall){
    pe_game_destroy(g);pe_game_init(g,44);g->mode=PE_GAME_ADVENTURE;
    for(int z=0;z<PE_WORLD_Z;z++)for(int y=0;y<PE_WORLD_Y;y++)for(int x=0;x<PE_WORLD_X;x++)
        pe_world_set(&g->world,x,y,z,(PeVoxel){y<10?255:0,y<10?PE_MAT_DIRT:PE_MAT_AIR});
    g->player=(PeVec3){32,10.05f,32};g->velocity=(PeVec3){0};g->block_count=g->tree_count=0;
    if(wall)for(int y=9;y<13;y++)for(int z=25;z<40;z++)g->blocks[g->block_count++]=(PeBlock){34,y,z,PE_ITEM_FENCE};
    memset(&g->inventory,0,sizeof g->inventory);g->inventory.next_instance_id=78;
    g->inventory.slots[0]=(PeSlot){.item_id=33,.count=1,.instance_id=77,.durability=100};g->equipped_instance_id=77;
    memset(&g->craft,0,sizeof g->craft);g->hp=g->max_hp=500;g->stamina=g->stamina_max=100;
    char error[160];return pe_game_collision_update(g,c,error,sizeof error)&&pe_game_collision_settle(g,c);
}
static void admission(PeSandboxMeleeSetup setup){
    const PeVec3 velocities[]={{0,0,2},{0,0,-2},{2,0,0},{-2,0,0},{0,0,4.5f},
        {-2.2190206050872803f,-2.0396177768707275f,3.341542959213257f}};
    for(unsigned i=0;i<sizeof velocities/sizeof *velocities;i++){
        PeSandboxMelee h;CHECK(pe_sandbox_melee_init(&h,&setup));float stamina=100;
        PeSandboxMeleeGate g=gate(velocities[i]);CHECK(pe_sandbox_melee_begin(&h,g,&stamina));
        CHECK(h.action.active&&h.action.variant==PE_COMBAT_STATIONARY&&stamina==95);
        /* Admission is selected once. Authored roots may subsequently exceed
         * sprint threshold; held controls and that speed cannot change it. */
        g.velocity=(PeVec3){14,0,-9};CHECK(pe_sandbox_melee_advance(&h,g,&stamina));
        PeCombatInterval interval;CHECK(pe_sandbox_melee_interval(&h,&interval));
        CHECK(interval.valid&&interval.variant==PE_COMBAT_STATIONARY&&interval.stage==0);
        PeSandboxMeleeFrame f={.gate=g,.player_scale=1,.attacker_eligible=true};PeSandboxMeleeUpdate u;
        CHECK(pe_sandbox_melee_resolve(&h,&f,NULL,0,&u)&&!u.cancelled&&h.action.active&&stamina==95);
    }
    for(unsigned i=0;i<5;i++){
        PeSandboxMelee h;CHECK(pe_sandbox_melee_init(&h,&setup));PeSandboxMelee before=h;
        PeSandboxMeleeGate g=gate((PeVec3){0,0,2});float stamina=100;
        if(i==0)g.velocity.z=nextafterf(7,INFINITY);
        if(i==1)g.grounded=false;
        if(i==2)g.in_water=true;
        if(i==3)g.third_person=false;
        if(i==4)g.velocity.y=NAN;
        CHECK(!pe_sandbox_melee_begin(&h,g,&stamina)&&stamina==100&&!memcmp(&h,&before,sizeof h));
    }
    setup.ordinary_motion_ready=false;
    PeSandboxMelee h;CHECK(pe_sandbox_melee_init(&h,&setup));PeSandboxMelee before=h;float stamina=100;
    CHECK(!pe_sandbox_melee_begin(&h,gate((PeVec3){0,0,2}),&stamina)&&stamina==100&&!memcmp(&h,&before,sizeof h));
}
/* Uses the outgoing interval's source identity, not mutable new action.stage.
 * This tests the public core pipeline; host dispatch has its own regression. */
static bool request(const PeSwordRootMotion *curve,const PeCombatInterval *span,PeVec3 velocity,PeVec3 *out){
    return pe_sword_root_motion_request(curve,span,velocity,90,PE_SANDBOX_MELEE_DT,out);
}
static void scenario(PeGame *g,PeSandboxCollision *c,PeSandboxMeleeSetup setup,const PeSwordRootMotion *curve,bool wall){
    CHECK(flat(g,c,wall));g->velocity=(PeVec3){0,0,-2};PeSandboxMelee melee;CHECK(pe_sandbox_melee_init(&melee,&setup));
    CHECK(pe_sandbox_melee_begin(&melee,gate(g->velocity),&g->stamina));
    const uint32_t expected[]={231,240,247,254};unsigned seen=0,transitions=0,blocked=0,ticks=0;
    PeVec3 start=g->player;PeSandboxMeleeUpdate update={0};
    const PeInput adversarial={.move_x=-1,.move_z=1,.jump=true,.sprint=true};
    while(melee.action.active&&ticks++<600){
        pe_sandbox_melee_queue(&melee,gate(g->velocity));
        uint64_t prior=melee.action.update_index;CHECK(pe_sandbox_melee_advance(&melee,gate(g->velocity),&g->stamina));
        CHECK(melee.action.update_index==prior+1);PeCombatInterval span;CHECK(pe_sandbox_melee_interval(&melee,&span));
        CHECK(span.valid&&span.variant==PE_COMBAT_STATIONARY&&span.stage<4&&span.to>=span.from);
        const PeCombatClip *clip=pe_combat_variant_clip(span.gender,span.view,span.variant,span.stage);
        CHECK(clip&&(uint32_t)clip->path_id==expected[span.stage]);seen|=1u<<span.stage;
        PeVec3 incoming=g->velocity,motion={0};CHECK(request(curve,&span,incoming,&motion));
        PeSandboxMelee pending=melee;float energy=g->stamina;
        CHECK(!pe_sandbox_melee_advance(&melee,gate(g->velocity),&g->stamina)&&!memcmp(&pending,&melee,sizeof melee)&&g->stamina==energy);
        PeVec3 before=g->player;double clock=g->game_seconds;
        CHECK(pe_game_step_collision_motion(g,adversarial,PE_FIXED_DT,c,motion));
        CHECK(fabs(g->game_seconds-clock-12.0*PE_FIXED_DT)<1e-9&&g->stamina==energy);
        CHECK(g->grounded&&!g->jumping&&!g->sprinting&&!pe_sandbox_collision_overlap(c,g->player,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT));
        PeVec3 wanted={before.x+motion.x*PE_FIXED_DT,g->player.y,before.z+motion.z*PE_FIXED_DT};
        if(!near(wanted,g->player,.00003f))blocked++;
        if(!wall)CHECK(near(wanted,g->player,.00003f));
        PeSandboxMeleeFrame f={.gate=gate(g->velocity),.player_position=g->player,.player_yaw=90,.player_scale=1,.attacker_eligible=true};
        prior=melee.action.update_index;double authored=melee.action.clip_time;
        CHECK(pe_sandbox_melee_resolve(&melee,&f,NULL,0,&update));
        CHECK(!melee.tick_pending&&melee.action.update_index==prior&&melee.action.clip_time==authored&&g->stamina==energy);
        if(update.weapon_valid){
            size_t pose=pe_actor_find_clip(setup.drawn_weapon,pe_combat_pose_name(&melee.action));PeHitboxCapsule blade;
            CHECK(pe_hitbox_pose(setup.drawn_capsule,setup.drawn_weapon,0,pose,authored,g->player,90,1,&blade));
            CHECK(near(blade.a,update.weapon.a,.000001f)&&near(blade.b,update.weapon.b,.000001f));
        }
        if(update.advance==PE_COMBAT_NEXT_STAGE){
            CHECK(span.stage+1==melee.action.stage&&melee.action.clip_time==0&&span.from>0&&span.to>span.from);
            /* The retained outgoing delta remains usable after new time0. */
            PeVec3 repeat;CHECK(request(curve,&span,incoming,&repeat)&&near(repeat,motion,.000001f));transitions++;
        }
    }
    CHECK(!melee.action.active&&ticks<600&&seen==15&&transitions==3&&g->stamina==80);
    CHECK(update.interval.stage==3&&update.interval.to==pe_combat_clip(PE_COMBAT_FEMALE,PE_COMBAT_THIRD_PERSON,3)->events[4].time);
    if(wall)CHECK(blocked>0&&g->player.x<33.71f);else CHECK(g->player.x>start.x+2);
    PeVec3 stopped=g->player;g->block_count=0;char error[128];CHECK(pe_game_collision_update(g,c,error,sizeof error));
    CHECK(pe_game_step_collision_motion(g,adversarial,PE_FIXED_DT,c,(PeVec3){0})&&near(stopped,g->player,.00003f));
    CHECK(pe_sandbox_melee_advance(&melee,gate(g->velocity),&g->stamina));PeCombatInterval idle;
    CHECK(pe_sandbox_melee_interval(&melee,&idle)&&!idle.valid);
}
static void contact(PeGame *g,PeSandboxCollision *c,PeSandboxMeleeSetup setup,const PeSwordRootMotion *curve){
    const char *ap=PE_SOURCE_ROOT "/rebuild/build/reference-assets/hare.peac",*hp=PE_SOURCE_ROOT "/rebuild/build/reference-assets/hare.pehb";
    FILE *file=fopen(ap,"rb");if(!file){fprintf(stderr,"sword walking: SKIP recovered hare fixture %s\n",ap);return;}fclose(file);
    char error[160];PeActor *hare=pe_actor_load(ap,error,sizeof error);PeHitboxes *boxes=hare?pe_hitboxes_load(hp,hare,error,sizeof error):NULL;
    CHECK(hare&&boxes);if(!hare||!boxes)goto done;
    size_t pose=pe_actor_find_clip(hare,"idle"),defence=SIZE_MAX;
    for(size_t i=0;i<pe_hitboxes_count(boxes);i++){const PeHitboxDefinition *b=pe_hitboxes_get(boxes,i);if(b->kind==PE_HITBOX_DEFENCE&&b->joint!=PE_HITBOX_DISABLED){defence=i;break;}}
    CHECK(pose!=PE_ACTOR_BIND_POSE&&defence!=SIZE_MAX);if(pose==PE_ACTOR_BIND_POSE||defence==SIZE_MAX)goto done;
    PeHitboxCapsule target;CHECK(pe_hitbox_pose(boxes,hare,defence,pose,.37,(PeVec3){0},27,1,&target));
    PeVec3 target_root={0};unsigned hit_tick=0;
    /* Measure first open POST-sweep blade, then arrange a fixed whole-hare
     * overlap before a fresh second action. Never move a target with the blade. */
    for(unsigned pass=0;pass<2;pass++){
        CHECK(flat(g,c,false));g->velocity=(PeVec3){-2,0,0};PeSandboxMelee h;CHECK(pe_sandbox_melee_init(&h,&setup));
        CHECK(pe_sandbox_melee_begin(&h,gate(g->velocity),&g->stamina));
        PeKnifeContactTarget targets[2];for(unsigned i=0;i<2;i++)targets[i]=(PeKnifeContactTarget){.entity_id=700+i,
            .actor=hare,.hitboxes=boxes,.clip=pose,.seconds=.37,.position=target_root,.yaw=27,.scale=1,.defense=29,.hp=212,.max_hp=212,.eligible=true};
        targets[1].position.z+=20;unsigned hits=0,ticks=0;
        while(h.action.active&&ticks<120){
            CHECK(pe_sandbox_melee_advance(&h,gate(g->velocity),&g->stamina));PeCombatInterval span;CHECK(pe_sandbox_melee_interval(&h,&span));
            PeVec3 motion;CHECK(request(curve,&span,g->velocity,&motion));
            CHECK(pe_game_step_collision_motion(g,(PeInput){.move_x=1,.move_z=-1},PE_FIXED_DT,c,motion));
            PeSandboxMeleeFrame f={.gate=gate(g->velocity),.player_position=g->player,.player_yaw=90,.player_scale=1,.attacker_eligible=true};
            PeSandboxMeleeUpdate u;bool open=h.action.contact_open;uint64_t index=h.action.update_index;
            CHECK(pe_sandbox_melee_resolve(&h,&f,pass?targets:NULL,pass?2:0,&u));CHECK(h.action.update_index==index);
            if(!pass&&open){
                CHECK(u.weapon_valid);target_root=(PeVec3){(u.weapon.a.x+u.weapon.b.x-target.a.x-target.b.x)*.5f,
                    (u.weapon.a.y+u.weapon.b.y-target.a.y-target.b.y)*.5f,(u.weapon.a.z+u.weapon.b.z-target.a.z-target.b.z)*.5f};
                hit_tick=ticks;break;
            }
            if(pass&&u.status[0]==PE_KNIFE_CONTACT_HIT){CHECK(open&&ticks==hit_tick);hits++;}
            if(pass)CHECK(u.status[1]!=PE_KNIFE_CONTACT_HIT&&targets[1].hp==212);
            ticks++;
        }
        if(!pass)CHECK(hit_tick>0&&hit_tick<120);
        else CHECK(!h.action.active&&ticks<120&&hits==1&&targets[0].hp<212&&g->stamina==95);
    }
done:pe_hitboxes_free(boxes);pe_actor_free(hare);
}
int pe_test_sword_walking(void){
    failures=0;const char *motion=getenv("PE_SWORD_ROOT_MOTION");bool explicit_motion=motion&&*motion;
    if(!explicit_motion)motion=PE_SOURCE_ROOT "/rebuild/build/reference-assets/player-sword-motion/sword-root.pesr";
    const char *root=getenv("PE_RUNNING_STAGE");if(!root||!*root)root=PE_SOURCE_ROOT "/rebuild/build/running-player-20261006";
    const char *names[]={"player/player-part-0-material-0.peac","player-knife-drawn/knife-drawn-part-0-material-0.peac","player-knife-drawn/knife-drawn.pehb"};
    char paths[3][2048],error[256];FILE *file=fopen(motion,"rb");
    if(!file){fprintf(stderr,"sword walking: %s source curve absent: %s\n",explicit_motion?"FAIL":"SKIP",motion);return explicit_motion?1:0;}fclose(file);
    for(unsigned i=0;i<3;i++){
        int n=snprintf(paths[i],sizeof paths[i],"%s/%s",root,names[i]);if(n<0||(size_t)n>=sizeof paths[i])return 1;
        file=fopen(paths[i],"rb");if(!file){fprintf(stderr,"sword walking: SKIP recovered actor absent: %s\n",paths[i]);return 0;}fclose(file);
    }
    PeActor *body=pe_actor_load(paths[0],error,sizeof error),*knife=pe_actor_load(paths[1],error,sizeof error);
    PeHitboxes *boxes=knife?pe_hitboxes_load(paths[2],knife,error,sizeof error):NULL;
    PeSwordRootMotion *curve=pe_sword_root_motion_load(motion,error,sizeof error);
    PeGame *game=calloc(1,sizeof *game);PeSandboxCollision *collision=pe_sandbox_collision_create();
    CHECK(body&&knife&&boxes&&curve&&game&&collision);
    if(body&&knife&&boxes&&curve&&game&&collision){
        PeSandboxMeleeSetup setup={.body=body,.drawn_weapon=knife,.drawn_capsule=boxes,.gender=PE_COMBAT_FEMALE,
            .weapon_item_id=33,.player_id=1,.attack=19,.stamina_reduce_percent=1,.running_motion_ready=true,.ordinary_motion_ready=true};
        admission(setup);scenario(game,collision,setup,curve,false);scenario(game,collision,setup,curve,true);contact(game,collision,setup,curve);
    }
    pe_sandbox_collision_free(collision);if(game)pe_game_destroy(game);free(game);
    pe_sword_root_motion_free(curve);pe_hitboxes_free(boxes);pe_actor_free(body);pe_actor_free(knife);
    if(!failures)puts("sword walking: recovered ordinary admission, four outgoing roots, sole movement clock, blade pose and blocked displacement passed");
    return failures;
}
