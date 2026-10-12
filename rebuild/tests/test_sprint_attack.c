/* Actual staged sprint/ordinary assets through the shared clock, motor and
 * collision/contact owners. Constructed flat fixtures, not earned gameplay. */
#include "pe_test_paths.h"
#include "pe_game.h"
#include "pe_sandbox_melee.h"
#include "pe_sprint_root_motion.h"
#include "pe_sword_root_motion.h"
#include "pe_sword_motor.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static PeSandboxMeleeGate gate(PeGame *g){return (PeSandboxMeleeGate){.alive=true,.weapon_owned=true,.grounded=true,.third_person=true,.action_allowed=true,
    .move_requested=true,.equipped_item_id=33,.equipped_instance_id=77,.drawn_instance_id=77,.velocity=g->velocity};}
static void scenario(PeSandboxMeleeSetup setup,const PeSprintRootMotion *sprint,const PeSwordRootMotion *ordinary,const PeActor *defence,const PeHitboxes *boxes,bool wall){
    PeGame *g=calloc(1,sizeof *g);assert(g);pe_game_init(g,44);g->mode=PE_GAME_ADVENTURE;
    for(int z=0;z<PE_WORLD_Z;z++)for(int y=0;y<PE_WORLD_Y;y++)for(int x=0;x<PE_WORLD_X;x++)
        pe_world_set(&g->world,x,y,z,(PeVoxel){y<10?255:0,y<10?PE_MAT_DIRT:PE_MAT_AIR});
    g->player=(PeVec3){32,10.05f,32};g->block_count=g->tree_count=0;
    if(wall)for(int y=9;y<13;y++)for(int z=25;z<40;z++)g->blocks[g->block_count++]=(PeBlock){34,y,z,PE_ITEM_FENCE};
    memset(&g->inventory,0,sizeof g->inventory);g->inventory.next_instance_id=78;
    g->inventory.slots[0]=(PeSlot){.item_id=33,.instance_id=77,.count=1,.durability=100};g->equipped_instance_id=77;
    memset(&g->craft,0,sizeof g->craft);g->hp=g->max_hp=500;g->stamina=g->stamina_max=100;
    char error[160];PeSandboxCollision *c=pe_sandbox_collision_create();assert(c&&pe_game_collision_update(g,c,error,sizeof error)&&pe_game_collision_settle(g,c));
    g->velocity=(PeVec3){8,0,0};PeSandboxMelee h;assert(pe_sandbox_melee_init(&h,&setup));
    assert(pe_sandbox_melee_begin(&h,gate(g),&g->stamina)&&h.action.variant==PE_COMBAT_SPRINT&&g->stamina==95);
    PeSwordMotor motor;pe_sword_motor_reset(&motor);unsigned ticks=0,seen=0,transitions=0,contacts=0,blocked=0;double movement_debit=0;
    while(h.action.active&&ticks<400){
        bool before=ticks>0;assert(pe_sandbox_melee_advance(&h,gate(g),&g->stamina));
        PeCombatInterval span;assert(pe_sandbox_melee_interval(&h,&span)&&span.valid);seen|=1u<<span.stage;
        double delta[3];assert(span.stage==0?pe_sprint_root_motion_world_delta(sprint,&span,90,delta):pe_sword_root_motion_world_delta(ordinary,&span,90,delta));
        PeSwordMotorStep move;assert(pe_sword_motor_consume(&motor,before,g->velocity,g->grounded,PE_FIXED_DT,&move));
        float x=g->player.x,energy=g->stamina;
        assert(move.owns_motion?pe_game_step_collision_motion(g,(PeInput){.move_x=-1,.stamina_action=true},PE_FIXED_DT,c,move.velocity):pe_game_step_collision(g,(PeInput){.move_x=1,.sprint=true,.stamina_action=true},PE_FIXED_DT,c));
        movement_debit+=energy-g->stamina;
        if(move.owns_motion&&fabs(g->player.x-x)<fabs(move.velocity.x)*PE_FIXED_DT*.5)blocked++;
        PeSandboxMeleeFrame frame={.gate=gate(g),.player_position=g->player,.player_yaw=90,.player_scale=1,.attacker_eligible=true};
        PeSandboxMeleeUpdate update;uint64_t index=h.action.update_index;
        assert(pe_sandbox_melee_resolve(&h,&frame,NULL,0,&update)&&h.action.update_index==index&&!update.cancelled);
        if(update.weapon_valid){
            const char *name=pe_combat_pose_name(&h.action);size_t clip=pe_actor_find_clip(setup.drawn_weapon,name);PeHitboxCapsule expected;
            assert(pe_hitbox_pose(setup.drawn_capsule,setup.drawn_weapon,0,clip,h.action.clip_time,g->player,90,1,&expected));
            assert(!memcmp(&expected,&update.weapon,sizeof expected));contacts++;
        }
        const char *name=pe_combat_pose_name(&h.action);size_t clip=pe_actor_find_clip(defence,name);
        for(size_t b=0;b<pe_hitboxes_count(boxes);b++){PeHitboxCapsule capsule;assert(pe_hitbox_pose(boxes,defence,b,clip,h.action.clip_time,g->player,90,1,&capsule));}
        assert(pe_sword_motor_update(&motor,before,delta,g->velocity,PE_FIXED_DT));
        if(h.action.combo_window&&!h.action.combo_queued&&h.action.stage<3)assert(pe_sandbox_melee_queue(&h,gate(g)));
        if(update.advance==PE_COMBAT_NEXT_STAGE){assert(span.stage+1==h.action.stage&&h.action.clip_time==0);transitions++;}
        ticks++;
    }
    assert(!h.action.active&&seen==15&&transitions==3&&contacts>0&&fabs(g->stamina-(80-movement_debit))<1e-4&&movement_debit>0&&movement_debit<.1);
    PeSwordMotorStep trailing;assert(pe_sword_motor_consume(&motor,false,g->velocity,g->grounded,PE_FIXED_DT,&trailing)&&trailing.owns_motion);
    assert(pe_game_step_collision_motion(g,(PeInput){0},PE_FIXED_DT,c,trailing.velocity));
    assert(pe_sword_motor_update(&motor,false,NULL,g->velocity,PE_FIXED_DT)&&!motor.has_last_move);
    if(wall)assert(blocked>0&&g->player.x<33.71f);else assert(g->player.x>34);
    printf("Sprint constructed %s: %u ticks,4 stages,3 handoffs,%u blade frames,x=%.4f\n",wall?"wall":"open",ticks,contacts,g->player.x);
    pe_sandbox_collision_free(c);pe_game_destroy(g);free(g);
}
int main(void){
    char path[4096],error[160];const char *root=pe_test_root();
    pe_test_format(path,sizeof path,"%s/rebuild/build/sprint-player-06/player/player-part-0-material-0.peac",root);PeActor *body=pe_actor_load(path,error,sizeof error);
    pe_test_format(path,sizeof path,"%s/rebuild/build/sprint-player-06/player-knife-drawn/knife-drawn-part-0-material-0.peac",root);PeActor *knife=pe_actor_load(path,error,sizeof error);
    pe_test_format(path,sizeof path,"%s/rebuild/build/sprint-player-06/player-knife-drawn/knife-drawn.pehb",root);PeHitboxes *blade=pe_hitboxes_load(path,knife,error,sizeof error);
    pe_test_format(path,sizeof path,"%s/rebuild/build/sprint-player-06/player-defence/player-defence.peac",root);PeActor *defence=pe_actor_load(path,error,sizeof error);
    pe_test_format(path,sizeof path,"%s/rebuild/build/sprint-player-06/player-defence/player-defence.pehb",root);PeHitboxes *boxes=pe_hitboxes_load(path,defence,error,sizeof error);
    pe_test_format(path,sizeof path,"%s/rebuild/build/sprint-source-motion-01/sprint-root.pesr",root);PeSprintRootMotion *sprint=pe_sprint_root_motion_load(path,error,sizeof error);
    pe_test_format(path,sizeof path,"%s/rebuild/build/reference-assets/player-sword-motion/sword-root.pesr",root);PeSwordRootMotion *ordinary=pe_sword_root_motion_load(path,error,sizeof error);
    if(!body||!knife||!blade||!defence||!boxes||!sprint||!ordinary){fprintf(stderr,"Sprint fixture: recovered staged assets required (%s)\n",error);return 77;}
    PeSandboxMeleeSetup setup={.body=body,.drawn_weapon=knife,.drawn_capsule=blade,.gender=PE_COMBAT_FEMALE,.weapon_item_id=33,.player_id=900,
        .attack=20,.stamina_reduce_percent=1,.sprint_motion_ready=true,.ordinary_motion_ready=true};
    scenario(setup,sprint,ordinary,defence,boxes,false);scenario(setup,sprint,ordinary,defence,boxes,true);
    pe_actor_free(body);pe_actor_free(knife);pe_hitboxes_free(blade);pe_actor_free(defence);pe_hitboxes_free(boxes);pe_sprint_root_motion_free(sprint);pe_sword_root_motion_free(ordinary);
    puts("Sprint shared motor/collision/contact fixture: PASS");return 0;
}
