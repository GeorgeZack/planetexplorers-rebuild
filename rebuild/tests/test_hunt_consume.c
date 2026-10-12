/* Real UI/pointer/camera -> extracted host movement -> collision/hunt test.
 * Explicit isolated near-hare geometry and preexisting player wound; no desktop,
 * reduced hare HP, synthetic contact, earned-survival or full host rendering claim. */
#include "pe_test_paths.h"
#include "pe_hunt_combat.h"
#include "pe_hunt_loot_combat.h"
#include "pe_sandbox_ui.h"
#include "pe_sandbox_camera.h"
#include "pe_settlement.h"
#include "pe_sandbox_actor.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned actor_cancels;
bool pe_sandbox_actor_set_action(PeSandboxActor *actor,const PeSandboxActionPose *pose){
    assert(!actor&&!pose);actor_cancels++;return true;
}
static bool held_w;
static bool key_down(int key){return held_w&&key==KEY_W;}
static bool key_pressed(int key){(void)key;return false;}
#define IsKeyDown key_down
#define IsKeyPressed key_pressed
#include "hunt_consume_host.inc"
#undef IsKeyDown
#undef IsKeyPressed
static int slot(PeGame*g,int id){for(int i=0;i<pe_inventory_capacity(&g->inventory);i++)if(g->inventory.slots[i].count&&g->inventory.slots[i].item_id==id)return i;return -1;}
static PeHuntCombatPlayer pose(const PeGame*g,const PeActor*a){return(PeHuntCombatPlayer){.position=g->player,.velocity=g->velocity,
    .radius=.5f,.height=1.8f,.max_hp=g->max_hp,.alive=g->hp>0,.grounded=g->grounded,.clip=pe_actor_find_clip(a,"NormalIdle")};}
static PeSandboxMeleeFrame frame(const PeGame*g,bool drawn){return(PeSandboxMeleeFrame){.gate={.alive=g->hp>0,
    .weapon_owned=true,.grounded=g->grounded,.third_person=true,.action_allowed=true,.equipped_item_id=33,
    .equipped_instance_id=g->equipped_instance_id,.drawn_instance_id=drawn?g->equipped_instance_id:0,.velocity=g->velocity},
    .player_position=g->player,.player_scale=1,.attacker_eligible=true};}
static bool contact(PeHuntCombat*h,PeGame*g,PeSandboxCollision*c,PeSandboxMelee*b,bool drawn){
    PeSandboxMeleeFrame f=frame(g,drawn);PeSandboxMeleeUpdate update;bool completed=false;
    bool ok=pe_hunt_loot_combat_step(g,h,c,pose(g,h->player_actor),b,&f,&update,&completed);
    assert(!completed&&g->hunt_kills==0&&g->hunt_loot.count==0);return ok;
}
static PeSandboxCameraControlResult controls(PeSandboxCameraControls*c,PeSandboxUIResult r){
    PeSandboxCameraControlResult out;
    assert(pe_sandbox_camera_controls_step(c,(PeSandboxCameraControlInput){.dt=PE_FIXED_DT,.focused=true,
        .input_allowed=!r.consume_world&&!r.paused,.pointer_over_ui=r.consume_world},&out));return out;
}
static PeSandboxUIInput ui_input(void){return(PeSandboxUIInput){.focused=true,.mouse_x=640,.mouse_y=350,.frame_dt=PE_FIXED_DT,.quickslot=-1};}
static int scenario(int medicine,float healing){
    actor_cancels=0;
    const char *root=pe_test_path("rebuild/build/reference-assets");
    const char *names[]={"adventure-route/terrain.pevx","hare.peac","hare.pehb","player-defence/player-defence.peac",
        "player-defence/player-defence.pehb","player/player-part-0-material-0.peac",
        "player-knife-drawn/knife-drawn-part-0-material-0.peac","player-knife-drawn/knife-drawn.pehb"};
    char paths[8][2048],error[256];
    for(unsigned i=0;i<8;i++){snprintf(paths[i],sizeof paths[i],"%s/%s",root,names[i]);FILE*f=fopen(paths[i],"rb");
        if(!f){fprintf(stderr,"hunt consume: SKIP recovered asset unavailable: %s\n",paths[i]);return 77;}fclose(f);}
    PeGame*g=calloc(1,sizeof *g);PeSandboxCollision*c=pe_sandbox_collision_create();assert(g&&c);
    PeActor*hare=pe_actor_load(paths[1],error,sizeof error),*player=pe_actor_load(paths[3],error,sizeof error),
        *body=pe_actor_load(paths[5],error,sizeof error),*knife=pe_actor_load(paths[6],error,sizeof error);assert(hare&&player&&body&&knife);
    PeHitboxes*hb=pe_hitboxes_load(paths[2],hare,error,sizeof error),*pb=pe_hitboxes_load(paths[4],player,error,sizeof error),
        *kb=pe_hitboxes_load(paths[7],knife,error,sizeof error);assert(hb&&pb&&kb);
    assert(pe_settlement_configure(root,error,sizeof error)&&pe_game_init_mode(g,918,PE_GAME_ADVENTURE));
    assert(pe_settlement_start(g,error,sizeof error));
    const PeSceneCollision*buildings=pe_settlement_collision_for_world(&g->world,error,sizeof error);assert(buildings);
    assert(pe_sandbox_collision_update_with_static(c,&g->world,NULL,0,NULL,0,NULL,0,buildings,error,sizeof error));
    PeHuntCombat hunt={0};assert(pe_hunt_combat_prepare(&hunt,&g->world,c,hare,hb,player,pb,0,error,sizeof error));
    for(unsigned i=1;i<6;i++)hunt.encounter.actors[i].present=false; /* Isolate a real source-HP actor. */
    hunt.encounter.actors[0].motion.yaw_degrees=0;
    g->player=hunt.encounter.actors[0].motion.position;g->player.z+=.6f;g->velocity=(PeVec3){0};g->grounded=true;g->yaw=0;
    assert(pe_game_collision_settle(g,c));
    g->hp=120; /* Preexisting wound, not an enemy damage alteration. */
    assert(pe_inventory_add(&g->inventory,33,1)&&pe_inventory_add(&g->inventory,medicine,2));
    assert(pe_game_equip_slot(g,slot(g,33))&&pe_game_quickslot_assign(g,0,slot(g,medicine)));
    int other=medicine==924?925:924;
    assert(pe_inventory_add(&g->inventory,other,2)&&pe_game_quickslot_assign(g,2,slot(g,other)));
    uint32_t instance=g->equipped_instance_id;PeSlot original_knife=*pe_game_equipped_tool(g);
    PeSandboxMelee blade={0};PeSandboxMeleeSetup setup={.weapon_item_id=33,.body=body,.drawn_weapon=knife,.drawn_capsule=kb,
        .gender=PE_COMBAT_FEMALE,.player_id=1,.attack=19,.stamina_reduce_percent=1};assert(pe_sandbox_melee_init(&blade,&setup));
    PeSandboxUI ui;pe_sandbox_ui_init(&ui,1280,720);PeSandboxPointerGate pointer={0};
    PeSandboxCameraControls camera;pe_sandbox_camera_controls_init(&camera);
    bool drawn=true,new_action=false,pending=false;
    for(unsigned i=0;i<30;i++)assert(pe_hunt_combat_sense(&hunt,c,pose(g,player)));
    assert(hunt.hostile[0]);
    /* Find an actual next-tick forearm overlap without mutating committed state.
     * Geometry and HP are not changed to force the hit; the real AI clocks run. */
    bool found=false;unsigned ticks=0;
    for(;ticks<240;ticks++){
        PeHuntCombat probe=hunt;PeSandboxMelee probe_blade=blade;float hp=g->hp,stamina=g->stamina;unsigned kills=g->hunt_kills;
        PeSandboxMeleeFrame f=frame(g,drawn);PeSandboxMeleeUpdate update;bool completed=false;
        assert(pe_hunt_combat_step(&probe,&g->world,c,pose(g,player),&probe_blade,&f,&hp,&stamina,&kills,&update,&completed));
        if(probe.player_hits>hunt.player_hits){found=true;break;}
        assert(contact(&hunt,g,c,&blade,drawn));
    }
    assert(found&&g->hp==120);
    PeSandboxUIInput in=ui_input();in.quickslot=0;in.left_pressed=true;
    PeSandboxUIResult action=pe_sandbox_ui_route(&ui,g,in);assert(action.consume_actions&&!action.consume_world);
    assert(g->hp==120+healing&&g->medpack_cooldown==10&&pe_inventory_count(&g->inventory,medicine)==1);
    pe_sandbox_pointer_route(&pointer,&action,1);held_w=true;pending=true;
    PeInput move=host_motion(g,action,controls(&camera,action),&blade,&drawn,&new_action,&pending);
    assert(move.move_z==-1&&move.move_x==0&&!new_action&&!pending&&drawn&&host_hunt_enabled(hunt,action));
    PeVec3 before=g->player;double clock=hunt.clock;uint64_t hits=hunt.player_hits;
    assert(pe_hunt_combat_sense(&hunt,c,pose(g,player)));
    assert(pe_game_step_collision(g,move,PE_FIXED_DT,c));
    assert(g->player.z<before.z&&g->medpack_cooldown<10);
    assert(contact(&hunt,g,c,&blade,drawn));
    assert(hunt.clock>clock&&hunt.player_hits>hits&&g->hp<120+healing&&g->hp>120);
    assert(g->equipped_instance_id==instance&&!memcmp(pe_game_equipped_tool(g),&original_knife,sizeof original_knife));
    fprintf(stderr,"hunt consume item%d: heal frame moved %.6fm; real incoming contacts %llu; HP %.6f; clock %.6f\n",
        medicine,before.z-g->player.z,(unsigned long long)(hunt.player_hits-hits),g->hp,hunt.clock);
    /* Held use-click, failed cooldown key, and release retain only action
     * ownership: movement, AI clock and equipment survive each frame. */
    for(unsigned i=0;i<3;i++){
        in=ui_input();if(i==1)in.quickslot=2;action=pe_sandbox_ui_route(&ui,g,in);
        pe_sandbox_pointer_route(&pointer,&action,i<2?1:0);
        float hp=g->hp;assert(action.consume_actions&&!action.consume_world&&pe_inventory_count(&g->inventory,medicine)==1&&pe_inventory_count(&g->inventory,other)==2);
        move=host_motion(g,action,controls(&camera,action),&blade,&drawn,&new_action,&pending);
        assert(move.move_z==-1&&!new_action&&drawn&&g->hp==hp&&host_hunt_enabled(hunt,action));
        clock=hunt.clock;assert(pe_hunt_combat_sense(&hunt,c,pose(g,player)));
        assert(pe_game_step_collision(g,move,PE_FIXED_DT,c)&&contact(&hunt,g,c,&blade,drawn));assert(hunt.clock>clock);
    }
    /* A second independent consumable during an already-running stationary
     * swing keeps that authoritative blade clock, instead of stowing/cancelling.
     * Stop normally first; stationary knife admission is not bypassed. */
    held_w=false;assert(pe_game_step_collision(g,(PeInput){0},PE_FIXED_DT,c));
    PeSandboxMeleeFrame start=frame(g,drawn);assert(pe_sandbox_melee_begin(&blade,start.gate,&g->stamina));
    assert(pe_inventory_add(&g->inventory,916,1)&&pe_game_quickslot_assign(g,1,slot(g,916)));
    int juice_before=pe_inventory_count(&g->inventory,916);
    in=ui_input();in.quickslot=1;action=pe_sandbox_ui_route(&ui,g,in);pe_sandbox_pointer_route(&pointer,&action,0);
    assert(action.consume_actions&&!action.consume_world&&pe_inventory_count(&g->inventory,916)==juice_before-1);
    double blade_clock=blade.action.clip_time;uint64_t blade_updates=blade.action.update_index;
    move=host_motion(g,action,controls(&camera,action),&blade,&drawn,&new_action,&pending);
    assert(drawn&&!new_action&&blade.action.active&&host_hunt_enabled(hunt,action));
    assert(pe_game_step_collision(g,move,PE_FIXED_DT,c)&&contact(&hunt,g,c,&blade,drawn));
    assert(blade.action.active&&blade.action.clip_time>blade_clock&&blade.action.update_index==blade_updates+1);
    fprintf(stderr,"hunt consume: active knife clock advanced once to %.6f during juice use\n",blade.action.clip_time);
    /* A genuinely modal inventory is the negative control: same held W must
     * not move, and production host gates omit sensing/contact until released. */
    in=ui_input();in.inventory=true;action=pe_sandbox_ui_route(&ui,g,in);pe_sandbox_pointer_route(&pointer,&action,1);
    assert(action.consume_world);PeHuntCombat untouched=hunt;
    move=host_motion(g,action,controls(&camera,action),&blade,&drawn,&new_action,&pending);
    assert(move.move_x==0&&move.move_z==0&&!new_action&&!drawn&&!host_hunt_enabled(hunt,action));
    assert(!memcmp(&hunt,&untouched,sizeof hunt));
    pe_hitboxes_free(hb);pe_hitboxes_free(pb);pe_hitboxes_free(kb);pe_actor_free(hare);pe_actor_free(player);pe_actor_free(body);pe_actor_free(knife);
    pe_sandbox_collision_free(c);pe_game_destroy(g);free(g);puts("hunt consume: real UI, extracted host movement, recovered incoming contact and modal control passed");return 0;
}

int main(void){
    int result=scenario(925,350);if(result)return result;
    return scenario(924,200);
}
