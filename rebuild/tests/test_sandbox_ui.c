/* Headless real-game UI routing regressions; no raylib/window calls. */
#include "adventure_test_helpers.h"
#include "pe_sandbox_ui.h"
#include "pe_adventure_medicine.h"
#include "pe_adventure_hunt.h"
#include "pe_deployable.h"
#include "pe_shield.h"
#include "pe_sandbox_camera.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static PeSandboxUIInput input(void) {
    return (PeSandboxUIInput){.focused=true,.mouse_x=640,.mouse_y=350,.quickslot=-1};
}
static PeSandboxUIInput click(PeTrainingUIRect r,bool right) {
    PeSandboxUIInput i=input();i.mouse_x=r.x+r.w*.5f;i.mouse_y=r.y+r.h*.5f;
    i.left_pressed=!right;i.right_pressed=right;return i;
}
static int find(const PeGame *g,int id) {
    for(int i=0;i<PE_INVENTORY_SLOTS;i++)if(g->inventory.slots[i].count&&g->inventory.slots[i].item_id==id)return i;
    return -1;
}
static int visible(PeSandboxUI *u,const PeGame *g,int id) {
    for(int k=0;k<PE_UI_INVENTORY_SLOTS;k++) {
        int index=pe_sandbox_ui_inventory_slot(u,g,k);
        if(index>=0&&g->inventory.slots[index].item_id==id)return k;
    }
    return -1;
}
static void deployed_menu(void){
    PeGame *g=calloc(1,sizeof *g);assert(g&&pe_game_init_mode(g,73,PE_GAME_ADVENTURE));
    PeSandboxUI u;pe_sandbox_ui_init(&u,1280,720);
    /* Seed a valid deployed-scene fixture; placement geometry/identity has a
       separate core test. These checks exercise actual modal transactions. */
    assert(pe_inventory_add(&g->inventory,10,1));int slot=find(g,10);assert(slot>=0);
    uint32_t id=g->inventory.slots[slot].instance_id;
    PeVec3 near=g->player;
    g->deployed[0]=(PeDeployable){.item_id=10,.instance_id=id,.durability=1200,
        .position={near.x+1,near.y,near.z}};g->deployed_count=1;
    assert(pe_inventory_remove(&g->inventory,10,1)&&pe_deployable_state_valid(g));
    assert(!pe_sandbox_ui_open_deployable(&u,g,id+1000));
    assert(pe_sandbox_ui_open_deployable(&u,g,id)&&u.deploy_entity==id&&!u.town_entity);
    PeInventory before=g->inventory;uint32_t money=g->money;
    PeSandboxUIInput i=click(pe_sandbox_ui_supplier_choice(&u,1),false);
    i.save=i.load=i.inventory=true;i.quickslot=0;
    PeSandboxUIResult r=pe_sandbox_ui_route(&u,g,i);
    assert(r.consume_world&&!r.save&&!r.load&&!r.npc_interaction&&!u.inventory_open&&u.town_open);
    assert(g->deployed[0].quarter_turn==1&&!memcmp(&before,&g->inventory,sizeof before)&&g->money==money);
    while(pe_inventory_add(&g->inventory,308,1)){}before=g->inventory;
    PeDeployable torch=g->deployed[0];
    i=click(pe_sandbox_ui_supplier_choice(&u,0),false);i.save=i.load=true;i.frame_dt=10;
    r=pe_sandbox_ui_route(&u,g,i);
    assert(r.consume_world&&!r.save&&!r.load&&u.town_open&&u.deploy_get_pending&&u.deploy_get_ticks==0);
    assert(!memcmp(&before,&g->inventory,sizeof before)&&!memcmp(&torch,&g->deployed[0],sizeof torch));
    /* Invalid, unfocused and paused display time cannot complete an operation. */
    i=input();i.frame_dt=NAN;pe_sandbox_ui_route(&u,g,i);
    i.frame_dt=-1;pe_sandbox_ui_route(&u,g,i);
    i.frame_dt=10;i.focused=false;pe_sandbox_ui_route(&u,g,i);
    i.focused=true;u.paused=true;pe_sandbox_ui_route(&u,g,i);u.paused=false;
    assert(u.deploy_get_ticks==0&&g->deployed_count==1);
    /* Source threshold is strict, and Turn/repeated Get are unavailable while
       the pickup panel owns the operation. It does not require a held button. */
    i=click(pe_sandbox_ui_supplier_choice(&u,1),false);i.frame_dt=1;
    pe_sandbox_ui_route(&u,g,i);assert(g->deployed[0].quarter_turn==torch.quarter_turn);
    i=click(pe_sandbox_ui_supplier_choice(&u,0),false);i.frame_dt=1;
    pe_sandbox_ui_route(&u,g,i);
    assert(u.town_open&&u.deploy_get_ticks==200000&&g->deployed_count==1);
    i=input();i.frame_dt=.001f;i.save=i.load=true;
    r=pe_sandbox_ui_route(&u,g,i);
    assert(r.consume_world&&!r.save&&!r.load&&!u.town_open&&!u.deploy_get_pending);
    assert(!memcmp(&before,&g->inventory,sizeof before)&&!memcmp(&torch,&g->deployed[0],sizeof torch));
    assert(strstr(g->message,"room")||strstr(g->message,"space"));
    /* A capacity failure closes the source panel; the original object remains
       available for a fresh, complete pickup attempt after making room. */
    assert(pe_sandbox_ui_open_deployable(&u,g,id));
    i=input();i.advance=true;pe_sandbox_ui_route(&u,g,i);
    i=input();i.frame_dt=1;pe_sandbox_ui_route(&u,g,i);
    /* Range loss wins even on a would-be completion frame. */
    g->player.x+=20;i.frame_dt=2;r=pe_sandbox_ui_route(&u,g,i);
    assert(r.consume_world&&!u.town_open&&!u.deploy_entity&&!u.deploy_get_pending);
    assert(g->deployed_count==1&&!memcmp(&torch,&g->deployed[0],sizeof torch));
    assert(!pe_sandbox_ui_open_deployable(&u,g,id));g->player=near;
    assert(pe_sandbox_ui_open_deployable(&u,g,id));
    i=input();i.advance=true;pe_sandbox_ui_route(&u,g,i);
    i=input();i.frame_dt=3;i.escape=true;r=pe_sandbox_ui_route(&u,g,i);
    assert(r.consume_world&&!u.town_open&&g->deployed_count==1);
    /* Death and host cancellation (including load) discard the transient
       operation; no stale elapsed time carries into another interaction. */
    assert(pe_sandbox_ui_open_deployable(&u,g,id));
    i=input();i.advance=true;pe_sandbox_ui_route(&u,g,i);
    float hp=g->hp;g->hp=0;i=input();i.frame_dt=3;r=pe_sandbox_ui_route(&u,g,i);g->hp=hp;
    assert(r.consume_world&&!u.town_open&&g->deployed_count==1);
    assert(pe_sandbox_ui_open_deployable(&u,g,id));
    i=input();i.advance=true;pe_sandbox_ui_route(&u,g,i);
    pe_sandbox_ui_cancel_town(&u);u.discard_input=true;
    i=input();i.frame_dt=3;i.save=i.load=true;r=pe_sandbox_ui_route(&u,g,i);
    assert(r.consume_world&&!r.save&&!r.load&&!u.deploy_get_pending&&g->deployed_count==1);
    /* Reopening a normal resident must clear any stale deployed selection. */
    u.deploy_entity=id;assert(pe_sandbox_ui_open_town(&u,g,20002,"Cherish Mercer",(Texture2D){0},true));
    assert(!u.deploy_entity&&u.town_entity==20002);pe_sandbox_ui_cancel_town(&u);
    assert(pe_inventory_remove(&g->inventory,308,1));
    int charcoal=pe_inventory_count(&g->inventory,1001);
    assert(pe_sandbox_ui_open_deployable(&u,g,id));
    i=click(pe_sandbox_ui_supplier_choice(&u,0),false);i.save=i.load=i.inventory=true;i.quickslot=0;
    r=pe_sandbox_ui_route(&u,g,i);
    assert(r.consume_world&&!r.save&&!r.load&&!r.npc_interaction&&u.town_open&&!u.inventory_open);
    assert(g->deployed_count==1&&u.deploy_get_ticks==0);
    PeSandboxPointerGate gate={0};assert(pe_sandbox_pointer_release_gate(&gate,r.consume_world,1));
    i=input();i.frame_dt=1.0f/60.0f;
    for(int frame=0;frame<120;frame++)r=pe_sandbox_ui_route(&u,g,i);
    assert(r.consume_world&&u.town_open&&g->deployed_count==1&&u.deploy_get_ticks==199920);
    /* Source frame quantization keeps this just below two seconds. */
    i.frame_dt=.0008f;r=pe_sandbox_ui_route(&u,g,i);
    assert(r.consume_world&&u.town_open&&g->deployed_count==1&&u.deploy_get_ticks==200000);
    assert(pe_sandbox_pointer_release_gate(&gate,r.consume_world,1));
    i.frame_dt=.01f;i.left_pressed=true;i.save=i.load=i.inventory=true;i.quickslot=0;
    r=pe_sandbox_ui_route(&u,g,i);
    assert(r.consume_world&&!r.save&&!r.load&&!r.npc_interaction&&!u.town_open&&!u.deploy_entity&&!u.inventory_open);
    assert(!g->deployed_count&&!pe_inventory_count(&g->inventory,10)&&pe_inventory_count(&g->inventory,1001)==charcoal+1);
    assert(pe_sandbox_pointer_release_gate(&gate,r.consume_world,1));
    for(int held=0;held<9;held++)assert(pe_sandbox_pointer_release_gate(&gate,false,1));
    assert(pe_sandbox_pointer_release_gate(&gate,false,0)&&!pe_sandbox_pointer_release_gate(&gate,false,1));
    assert(!pe_sandbox_ui_open_deployable(&u,g,id)&&pe_deployable_state_valid(g));
    pe_game_destroy(g);free(g);
}
static void starter_pair_readiness(void){
    PeGame *g=calloc(1,sizeof *g);assert(g&&pe_game_init_mode(g,73,PE_GAME_ADVENTURE));
    assert(pe_test_ack_welcome(g)&&pe_game_arrival_complete(g)&&pe_game_accept_supplies(g));
    int sword=find(g,1),shield=find(g,34);assert(sword>=0&&shield>=0);
    PeSlot sword_before=g->inventory.slots[sword],shield_before=g->inventory.slots[shield];
    PeSandboxUI u;pe_sandbox_ui_init(&u,1280,720);assert(!u.starter_pair_available);
    u.inventory_open=true;u.inventory_tab=pe_item_def(1)->tab;
    int v=visible(&u,g,1);assert(v>=0);
    PeSandboxUIResult r=pe_sandbox_ui_route(&u,g,click(u.layout.inventory_slots[v],true));
    assert(r.consume_world&&!g->equipped_instance_id&&!g->shield_instance_id);
    u.inventory_tab=pe_item_def(34)->tab;v=visible(&u,g,34);assert(v>=0);
    r=pe_sandbox_ui_route(&u,g,click(u.layout.inventory_slots[v],true));
    assert(r.consume_world&&!g->equipped_instance_id&&!g->shield_instance_id);
    assert(pe_game_quickslot_assign(g,0,sword)&&pe_game_quickslot_assign(g,1,shield));
    u.inventory_open=false;PeSandboxUIInput i=input();i.quickslot=0;
    r=pe_sandbox_ui_route(&u,g,i);assert(r.consume_world&&!g->equipped_instance_id);
    i.quickslot=1;r=pe_sandbox_ui_route(&u,g,i);assert(r.consume_world&&!g->shield_instance_id);
    /* Only the graphics/action host may advertise the complete pair. UI tests
       inject that transient capability without claiming an installed group. */
    u.starter_pair_available=true;i.quickslot=0;
    r=pe_sandbox_ui_route(&u,g,i);assert(r.consume_world&&g->equipped_instance_id==sword_before.instance_id);
    i.quickslot=1;r=pe_sandbox_ui_route(&u,g,i);
    assert(r.consume_world&&g->shield_instance_id==shield_before.instance_id&&g->equipped_instance_id==sword_before.instance_id);
    u.inventory_tab=pe_item_def(34)->tab;assert(visible(&u,g,34)<0);
    u.character_open=true;
    r=pe_sandbox_ui_route(&u,g,click(u.layout.equipment_slots[9],true));
    assert(r.consume_world&&!g->shield_instance_id&&g->equipped_instance_id==sword_before.instance_id);
    assert(!memcmp(&sword_before,&g->inventory.slots[sword],sizeof sword_before));
    assert(!memcmp(&shield_before,&g->inventory.slots[shield],sizeof shield_before));
    /* Reinitialize a host/load never persists capability readiness. */
    pe_sandbox_ui_init(&u,1280,720);assert(!u.starter_pair_available);
    pe_game_destroy(g);free(g);
}
static void hunt_presentation(void){
    PeGame *g=calloc(1,sizeof(*g));assert(g);g->mode=PE_GAME_ADVENTURE;g->welcome_page=PE_ADVENTURE_WELCOME_PAGES;
    PeSandboxUI u;pe_sandbox_ui_init(&u,1280,720);char goal[320];
    assert(!u.hunt_encounter_available&&!pe_sandbox_ui_hunt_goal(&u,g,goal,sizeof(goal)));
    assert(!*pe_sandbox_ui_mission_title(NULL));g->hunt_state=PE_HUNT_TRAVEL;g->hunt_destination=(PeVec3){-349,19.5f,233};
    assert(!strcmp(pe_sandbox_ui_mission_title(g),pe_adventure_mission_title(9075)));
    assert(pe_sandbox_ui_hunt_goal(&u,g,goal,sizeof(goal))&&strstr(goal,"-349, 233")&&!strstr(goal,"unavailable"));
    u.hunt_encounter_available=true;assert(pe_sandbox_ui_hunt_goal(&u,g,goal,sizeof(goal))&&!strstr(goal,"unavailable"));
    g->hunt_state=PE_HUNT_ENCOUNTER_PENDING;
    for(unsigned count=0;count<4;count++){
        g->hunt_kills=count;char expected[64];snprintf(expected,sizeof(expected),"Kill Lepus hares (%u/4).",count);
        assert(!strcmp(pe_sandbox_ui_mission_title(g),pe_adventure_mission_title(9126)));
        assert(pe_sandbox_ui_hunt_goal(&u,g,goal,sizeof(goal))&&!strcmp(goal,expected));
    }
    u.hunt_encounter_available=false;assert(pe_sandbox_ui_hunt_goal(&u,g,goal,sizeof(goal))&&strstr(goal,"3/4")&&strstr(goal,"unavailable"));
    g->hunt_state=PE_HUNT_RETURN_PENDING;g->hunt_kills=4;
    assert(!strcmp(pe_sandbox_ui_mission_title(g),pe_adventure_mission_title(9127)));
    for(int available=0;available<2;available++){
        u.hunt_encounter_available=available;assert(pe_sandbox_ui_hunt_goal(&u,g,goal,sizeof(goal)));
        assert(strstr(goal,"4/4")&&strstr(goal,"Return to town")&&strstr(goal,"-266, 136")&&!strstr(goal,"not available"));
    }
    for(unsigned state=PE_HUNT_RETURN_PENDING;state<=PE_HUNT_COMPLETE;state++){
        g->hunt_state=state;
        assert(pe_sandbox_ui_hunt_mission_id(g)==9127);
        assert(!strcmp(pe_sandbox_ui_mission_title(g),pe_adventure_mission_title(9127)));
        assert(pe_adventure_mission_description(pe_sandbox_ui_hunt_mission_id(g))!=NULL);
    }
    g->hunt_state=PE_HUNT_RETURN_TALK1;
    assert(pe_sandbox_ui_hunt_goal(&u,g,goal,sizeof goal)&&strstr(goal,"Rewards received"));
    g->hunt_state=PE_HUNT_RETURN_TALK2;
    assert(pe_sandbox_ui_hunt_goal(&u,g,goal,sizeof goal)&&strstr(goal,"Naoko"));
    g->hunt_state=PE_HUNT_COMPLETE;
    assert(pe_sandbox_ui_hunt_goal(&u,g,goal,sizeof goal)&&strstr(goal,"Town Defense is not available"));
    char tiny[2]={'x','y'};assert(!pe_sandbox_ui_hunt_goal(&u,g,tiny,sizeof(tiny))&&tiny[0]=='x'&&tiny[1]=='y');
    g->mode=PE_GAME_LEGACY;assert(!*pe_sandbox_ui_mission_title(g)&&!pe_sandbox_ui_hunt_goal(&u,g,goal,sizeof(goal)));
    free(g);
}
static void medpack_use(void){
    PeGame *g=calloc(1,sizeof *g);assert(g&&pe_game_init_mode(g,91,PE_GAME_ADVENTURE));
    g->inventory=(PeInventory){.next_instance_id=g->inventory.next_instance_id};g->equipped_instance_id=0;memset(g->quickslots,0,sizeof g->quickslots);
    assert(pe_inventory_add(&g->inventory,925,3));g->hp=10;
    assert(pe_game_quickslot_assign(g,0,find(g,925)));
    PeSandboxUI u;pe_sandbox_ui_init(&u,1280,720);u.inventory_open=true;u.inventory_tab=pe_item_def(925)->tab;
    int v=visible(&u,g,925);assert(v>=0);
    PeSandboxUIResult r=pe_sandbox_ui_route(&u,g,click(u.layout.inventory_slots[v],true));
    assert(r.consume_world&&g->hp==360&&g->medpack_cooldown==10&&pe_inventory_count(&g->inventory,925)==2);
    pe_sandbox_ui_route(&u,g,click(u.layout.inventory_slots[v],true));assert(pe_inventory_count(&g->inventory,925)==2);
    u.inventory_open=false;PeSandboxUIInput i=input();i.quickslot=0;
    r=pe_sandbox_ui_route(&u,g,i);assert(!r.consume_world&&r.consume_actions&&pe_inventory_count(&g->inventory,925)==2);
    g->medpack_cooldown=0; /* UI-only clock precondition; exact countdown tested in core. */
    r=pe_sandbox_ui_route(&u,g,i);assert(!r.consume_world&&r.consume_actions&&g->hp==500&&pe_inventory_count(&g->inventory,925)==1);
    g->medpack_cooldown=0;g->hp=2;u.paused=true;
    r=pe_sandbox_ui_route(&u,g,i);assert(r.consume_world&&g->hp==2&&pe_inventory_count(&g->inventory,925)==1);
    u.paused=false;u.inventory_open=true;
    r=pe_sandbox_ui_route(&u,g,i);assert(r.consume_world&&g->hp==2&&pe_inventory_count(&g->inventory,925)==1);
    u.inventory_open=false;u.camera_cursor=true;
    r=pe_sandbox_ui_route(&u,g,click(u.layout.quickslots[0],true));
    assert(!r.consume_world&&r.consume_actions&&g->hp==352&&pe_inventory_count(&g->inventory,925)==0&&g->quickslots[0].item_id==925);
    assert(!pe_game_quickslot_use(g,0));pe_game_destroy(g);free(g);
}
static void consumable_ownership(void){
    PeGame *g=calloc(1,sizeof *g);assert(g&&pe_game_init_mode(g,91,PE_GAME_ADVENTURE));
    g->inventory=(PeInventory){.next_instance_id=g->inventory.next_instance_id};g->equipped_instance_id=0;memset(g->quickslots,0,sizeof g->quickslots);
    assert(pe_inventory_add(&g->inventory,925,3)&&pe_inventory_add(&g->inventory,916,3));
    assert(pe_game_quickslot_assign(g,0,find(g,925))&&pe_game_quickslot_assign(g,1,find(g,916)));
    for(int item=0;item<2;item++)for(int mode=0;mode<3;mode++){
        PeSandboxUI u;pe_sandbox_ui_init(&u,1280,720);u.camera_cursor=mode==0;
        PeSandboxUIInput i=input();i.quickslot=item;
        PeSandboxUIResult r=pe_sandbox_ui_route(&u,g,i);
        assert(!r.consume_world&&r.consume_actions&&!r.camera_cycle_allowed&&!r.npc_interaction);
        /* The same key with a held attack cannot turn the release gate into
           a full-modal lock. Cooldown retries have the identical policy. */
        PeSandboxPointerGate gate={0};assert(pe_sandbox_pointer_route(&gate,&r,1));
        assert(gate.actions_only&&!r.consume_world&&r.consume_actions);
        PeSandboxCameraControls c;pe_sandbox_camera_controls_init(&c);c.mode=mode;
        if(mode==0){c.right_tracking=c.right_dragged=true;c.right_seconds=.25f;}
        PeSandboxCameraControlInput ci={.dt=.016f,.focused=true,.input_allowed=!r.consume_world,
            .captured=true,.right_down=mode==0,.mouse_dx=8};
        PeSandboxCameraControlResult co;
        assert(pe_sandbox_camera_controls_step(&c,ci,&co)&&co.world_input&&co.capture&&co.look);
        i=input();r=pe_sandbox_ui_route(&u,g,i);
        assert(pe_sandbox_pointer_route(&gate,&r,1)&&!r.consume_world&&r.consume_actions);
        r=pe_sandbox_ui_route(&u,g,i);
        assert(pe_sandbox_pointer_route(&gate,&r,0)&&!r.consume_world&&r.consume_actions);
        r=pe_sandbox_ui_route(&u,g,i);
        assert(!pe_sandbox_pointer_route(&gate,&r,0)&&!r.consume_world&&!r.consume_actions);
    }
    assert(pe_inventory_count(&g->inventory,925)==2&&pe_inventory_count(&g->inventory,916)==2);
    PeSandboxUI u;pe_sandbox_ui_init(&u,1280,720);u.camera_cursor=true;
    PeSandboxUIInput i=click(u.layout.quickslots[0],true);PeSandboxUIResult r=pe_sandbox_ui_route(&u,g,i);
    PeSandboxPointerGate gate={0};assert(pe_sandbox_pointer_route(&gate,&r,2));
    assert(!r.consume_world&&r.consume_actions&&pe_inventory_count(&g->inventory,925)==2);
    /* Moving the held pointer off the HUD never admits an attack/stow; a
       true modal then escalates ownership through its closing/release frame. */
    i=input();r=pe_sandbox_ui_route(&u,g,i);assert(pe_sandbox_pointer_route(&gate,&r,2)&&r.consume_actions&&!r.consume_world);
    i.inventory=true;r=pe_sandbox_ui_route(&u,g,i);assert(pe_sandbox_pointer_route(&gate,&r,2)&&r.consume_world&&!gate.actions_only);
    i=input();i.escape=true;r=pe_sandbox_ui_route(&u,g,i);assert(pe_sandbox_pointer_route(&gate,&r,2)&&r.consume_world&&!u.inventory_open);
    i=input();r=pe_sandbox_ui_route(&u,g,i);assert(pe_sandbox_pointer_route(&gate,&r,0)&&r.consume_world);
    r=pe_sandbox_ui_route(&u,g,i);assert(!pe_sandbox_pointer_route(&gate,&r,0)&&!r.consume_world&&!r.consume_actions);
    assert(pe_inventory_remove(&g->inventory,925,2));i.quickslot=0;r=pe_sandbox_ui_route(&u,g,i);
    assert(!r.consume_world&&r.consume_actions&&!pe_inventory_count(&g->inventory,925));
    pe_game_destroy(g);free(g);
}
static PeSandboxUIResult numbered(PeSandboxUI *u,PeGame *g,unsigned response){
    PeSandboxUIInput i=input();i.response=response;i.quickslot=response?(int)response-1:-1;
    i.save=i.load=i.inventory=true; /* Same-frame shortcuts must stay owned. */
    PeSandboxUIResult r=pe_sandbox_ui_route(u,g,i);
    assert(r.consume_world&&!r.save&&!r.load&&!r.toggle_build&&!r.build_shape&&!r.selected_item&&!u->inventory_open);
    return r;
}
static void numbered_responses(void){
    PeGame *g=calloc(1,sizeof *g);assert(g&&pe_game_init_mode(g,83,PE_GAME_ADVENTURE));
    PeSandboxUI u;pe_sandbox_ui_init(&u,1280,720);
    /* No offer means the sole visible close option is response1, not2. */
    assert(pe_sandbox_ui_open_supplier(&u,g,20016,true));
    numbered(&u,g,2);assert(u.dialogue.stage==PE_ADVENTURE_DIALOGUE_MENU);
    numbered(&u,g,1);assert(u.dialogue.stage==PE_ADVENTURE_DIALOGUE_IDLE);
    assert(pe_sandbox_ui_open_welcome(&u,g,20008,true));
    PeInventory before=g->inventory;
    numbered(&u,g,9);assert(!g->welcome_page);
    for(unsigned page=0;page<PE_ADVENTURE_WELCOME_PAGES;page++){
        numbered(&u,g,1);assert(g->welcome_page==page+1);
        assert(!memcmp(&before,&g->inventory,sizeof before));
    }
    assert(u.dialogue.stage==PE_ADVENTURE_DIALOGUE_IDLE&&pe_game_arrival_complete(g));
    assert(pe_sandbox_ui_open_supplier(&u,g,20016,true));
    numbered(&u,g,2);assert(u.dialogue.stage==PE_ADVENTURE_DIALOGUE_IDLE&&!g->supplies_claimed);
    assert(pe_sandbox_ui_open_supplier(&u,g,20016,true));numbered(&u,g,1);
    for(unsigned page=0;page<5;page++){
        assert(!g->supplies_claimed);numbered(&u,g,1);
    }
    assert(g->supplies_claimed&&u.dialogue.stage==PE_ADVENTURE_DIALOGUE_IDLE);
    assert(pe_sandbox_ui_open_town(&u,g,20002,"Cherish Mercer",(Texture2D){0},true));
    numbered(&u,g,1);assert(u.shop_open);pe_sandbox_ui_cancel_town(&u);u.discard_input=false;
    assert(pe_sandbox_ui_open_town(&u,g,20002,"Cherish Mercer",(Texture2D){0},true));
    numbered(&u,g,2);assert(!u.town_open);
    assert(pe_sandbox_ui_open_town(&u,g,20009,"Dikeledi Sehgal",(Texture2D){0},true));
    numbered(&u,g,2);assert(u.town_open);numbered(&u,g,1);assert(!u.town_open);
    /* Both source choices retain their visible order and atomic transactions. */
    g->annot_complete=true;
    assert(pe_sandbox_ui_open_annot(&u,g,20018,(Texture2D){0},true));
    numbered(&u,g,1);numbered(&u,g,1);assert(u.dialogue.stage==PE_ADVENTURE_DIALOGUE_CHOICE);
    assert(pe_sandbox_ui_dialogue_choice_index(&u,g,0)==0&&pe_sandbox_ui_dialogue_choice_index(&u,g,1)==1);
    assert(pe_sandbox_ui_dialogue_choice_index(&u,g,2)==-1&&pe_sandbox_ui_dialogue_choice_index(&u,g,-1)==-1);
    numbered(&u,g,9);assert(u.dialogue.stage==PE_ADVENTURE_DIALOGUE_CHOICE);
    numbered(&u,g,2);assert(g->medicine_state==PE_MEDICINE_NONE&&u.dialogue.stage==PE_ADVENTURE_DIALOGUE_IDLE);
    assert(pe_game_use_slot(g,find(g,552))&&pe_game_use_slot(g,find(g,512)));
    assert(pe_sandbox_ui_open_annot(&u,g,20018,(Texture2D){0},true));
    numbered(&u,g,1);numbered(&u,g,1);numbered(&u,g,1);
    assert(g->medicine_state==PE_MEDICINE_ACTIVE&&pe_inventory_count(&g->inventory,877)==1);
    assert(pe_sandbox_ui_open_annot(&u,g,20018,(Texture2D){0},true));
    before=g->inventory;
    numbered(&u,g,1);assert(u.dialogue.stage==PE_ADVENTURE_DIALOGUE_MENU&&!memcmp(&before,&g->inventory,sizeof before));
    numbered(&u,g,2);assert(u.dialogue.stage==PE_ADVENTURE_DIALOGUE_IDLE);
    assert(pe_inventory_add(&g->inventory,917,5));
    assert(pe_sandbox_ui_open_annot(&u,g,20018,(Texture2D){0},true));numbered(&u,g,1);
    assert(pe_adventure_dialogue_current(&u.dialogue,g)->talk_id==4100&&pe_inventory_count(&g->inventory,917)==5);
    numbered(&u,g,2);assert(pe_inventory_count(&g->inventory,917)==5&&g->medicine_state==PE_MEDICINE_ACTIVE);
    PeSandboxUIResult closed=numbered(&u,g,1);
    assert(g->medicine_state==PE_MEDICINE_COMPLETE&&pe_inventory_count(&g->inventory,917)==0);
    PeSandboxPointerGate gate={0};
    assert(pe_sandbox_pointer_release_gate(&gate,closed.consume_world,1));
    assert(pe_sandbox_pointer_release_gate(&gate,false,1));
    assert(pe_sandbox_pointer_release_gate(&gate,false,0));
    assert(!pe_sandbox_pointer_release_gate(&gate,false,0));
    pe_game_destroy(g);free(g);
}
int main(void) {
    medpack_use();
    /* A 150ms dialogue click outlives its closing frame. No frame in that
       hold, including release, may recapture or supply a held tool action. */
    PeSandboxPointerGate gate={0};
    assert(!pe_sandbox_pointer_release_gate(&gate,false,1)); /* ordinary world hold */
    assert(pe_sandbox_pointer_release_gate(&gate,true,1)); /* GUI closes here */
    for(int frame=0;frame<9;frame++)assert(pe_sandbox_pointer_release_gate(&gate,false,1));
    assert(pe_sandbox_pointer_release_gate(&gate,false,3)); /* second button joins */
    assert(pe_sandbox_pointer_release_gate(&gate,false,2)); /* first button released */
    assert(pe_sandbox_pointer_release_gate(&gate,false,0)); /* release is consumed */
    assert(!pe_sandbox_pointer_release_gate(&gate,false,0)); /* capture permitted now */
    assert(!pe_sandbox_pointer_release_gate(&gate,false,1)); /* fresh world click works */
    assert(!pe_sandbox_pointer_release_gate(&gate,true,0)); /* keyboard-only close */

    PeGame *g=calloc(1,sizeof *g);assert(g);
    assert(pe_game_init_mode(g,73,PE_GAME_ADVENTURE));
    PeSandboxUI u;pe_sandbox_ui_init(&u,1280,720);
    assert(u.selected_slot==-1&&u.batches==1);
    PeSandboxUIInput i=input();i.inventory=true;
    PeSandboxUIResult r=pe_sandbox_ui_route(&u,g,i);
    assert(r.consume_world&&r.cursor_visible&&u.inventory_open&&!r.paused);
    /* Source starter scanner updates are not recipes. Use through UI only. */
    int scanner=-1;
    for(int k=0;k<PE_INVENTORY_SLOTS;k++) {
        const PeItemDef *d=pe_item_def(g->inventory.slots[k].item_id);
        if(d&&d->learn_kind==PE_LEARN_SCAN&&g->inventory.slots[k].count){scanner=k;break;}
    }
    assert(scanner>=0);int scanner_id=g->inventory.slots[scanner].item_id;
    u.inventory_tab=pe_item_def(scanner_id)->tab;
    int v=visible(&u,g,scanner_id);assert(v>=0);
    r=pe_sandbox_ui_route(&u,g,click(u.layout.inventory_slots[v],true));
    assert(r.consume_world&&g->scanner_mask&&!g->known_recipe_count);
    assert(pe_inventory_count(&g->inventory,scanner_id)==0);
    /* UI never grants supply items merely by opening quests or inventory. */
    assert(!g->arrival_complete&&!g->supplies_claimed);
    assert(pe_test_ack_welcome(g));
    assert(pe_game_arrival_complete(g)&&pe_game_accept_supplies(g));
    u.inventory_tab=pe_item_def(406)->tab;v=visible(&u,g,406);assert(v>=0);
    r=pe_sandbox_ui_route(&u,g,click(u.layout.inventory_slots[v],true));
    assert(r.consume_world&&pe_game_recipe_known(g,1)&&pe_inventory_count(&g->inventory,406)==0);
    assert(pe_sandbox_ui_recipe_count(&u,g)==1);
    /* Equip from filtered bag without consuming/reordering the instance. */
    u.inventory_tab=pe_item_def(12)->tab;v=visible(&u,g,12);assert(v>=0);
    int shovel=find(g,12);uint32_t identity=g->inventory.slots[shovel].instance_id;
    int owned=pe_inventory_count(&g->inventory,12);
    r=pe_sandbox_ui_route(&u,g,click(u.layout.inventory_slots[v],true));
    assert(r.consume_world&&g->equipped_instance_id==identity&&pe_inventory_count(&g->inventory,12)==owned);
    assert(visible(&u,g,12)==-1); /* one supplied instance is equipped */
    pe_game_unequip(g);
    /* Stable drag survives an inventory slot move before release. */
    v=visible(&u,g,12);assert(v>=0);
    pe_sandbox_ui_route(&u,g,click(u.layout.inventory_slots[v],false));assert(u.dragging);
    int other=(shovel+1)%PE_INVENTORY_SLOTS;
    PeSlot swap=g->inventory.slots[shovel];g->inventory.slots[shovel]=g->inventory.slots[other];g->inventory.slots[other]=swap;
    i=click(u.layout.quickslots[0],false);i.left_pressed=false;i.left_released=true;
    r=pe_sandbox_ui_route(&u,g,i);
    assert(r.consume_world&&!u.dragging&&g->quickslots[0].instance_id==identity);
    assert(pe_inventory_count(&g->inventory,12)==owned);
    /* Closing panel consumes its frame. A subsequent hotkey selects that instance. */
    i=input();i.escape=true;r=pe_sandbox_ui_route(&u,g,i);assert(r.consume_world&&!r.cursor_visible);
    i=input();i.quickslot=0;r=pe_sandbox_ui_route(&u,g,i);assert(g->equipped_instance_id==identity);
    assert(r.consume_world);
    /* Original quickbar mouse semantics: right uses; left drags/swaps shortcuts. */
    int early_juice=find(g,916);assert(early_juice>=0);
    assert(pe_game_quickslot_assign(g,1,early_juice));
    i=input();i.tab=true;pe_sandbox_ui_route(&u,g,i);
    int before_juice=pe_inventory_count(&g->inventory,916);
    r=pe_sandbox_ui_route(&u,g,click(u.layout.quickslots[1],true));
    assert(r.consume_world&&pe_inventory_count(&g->inventory,916)==before_juice-1);
    pe_sandbox_ui_route(&u,g,click(u.layout.quickslots[0],false));assert(u.dragging);
    i=click(u.layout.quickslots[1],false);i.left_pressed=false;i.left_released=true;
    pe_sandbox_ui_route(&u,g,i);
    assert(g->quickslots[1].instance_id==identity&&g->quickslots[0].item_id==916);
    assert(pe_inventory_count(&g->inventory,12)==owned);
    i=input();i.tab=true;pe_sandbox_ui_route(&u,g,i);
    /* Pause blocks quickslot actions, offers save/load/menu as requests only. */
    i=input();i.escape=true;r=pe_sandbox_ui_route(&u,g,i);assert(r.paused&&r.consume_world);
    r=pe_sandbox_ui_route(&u,g,click(pe_sandbox_ui_pause_button(&u,1),false));assert(r.save&&r.paused&&!r.load);
    assert(pe_sandbox_pointer_release_gate(&gate,r.consume_world,1)&&r.save); /* UI command survives held-press ownership. */
    r=pe_sandbox_ui_route(&u,g,click(pe_sandbox_ui_pause_button(&u,2),false));assert(r.load&&r.paused);
    assert(pe_sandbox_pointer_release_gate(&gate,r.consume_world,1)&&r.load);
    assert(pe_sandbox_pointer_release_gate(&gate,false,0));
    r=pe_sandbox_ui_route(&u,g,click(pe_sandbox_ui_pause_button(&u,3),false));assert(r.return_main&&r.consume_world);
    r=pe_sandbox_ui_route(&u,g,click(pe_sandbox_ui_pause_button(&u,0),false));assert(!r.paused&&r.consume_world);
    /* Search only learned recipes; bounded quantity/craft routes real transactions. */
    i=input();i.replicator=true;r=pe_sandbox_ui_route(&u,g,i);assert(u.replicator_open&&r.consume_world);
    pe_sandbox_ui_route(&u,g,click(pe_sandbox_ui_search(&u),false));assert(u.search_focus);
    i=input();i.typed="SHOVEL";pe_sandbox_ui_route(&u,g,i);assert(pe_sandbox_ui_recipe_count(&u,g)==1);
    pe_sandbox_ui_route(&u,g,click(pe_sandbox_ui_recipe_row(&u,0),false));assert(u.recipe_id==1&&!u.search_focus);
    pe_sandbox_ui_route(&u,g,click(pe_sandbox_ui_quantity(&u,1),false));assert(u.batches==2);
    pe_sandbox_ui_route(&u,g,click(pe_sandbox_ui_quantity(&u,2),false));assert(u.batches==1);
    r=pe_sandbox_ui_route(&u,g,click(u.layout.replicator_craft,false));assert(r.consume_world&&g->craft.recipe_id==1);
    pe_sandbox_ui_route(&u,g,click(pe_sandbox_ui_cancel_button(&u),false));assert(!g->craft.recipe_id);
    /* A closed search cannot retain keyboard capture when opening another panel. */
    i=input();i.escape=true;pe_sandbox_ui_route(&u,g,i);assert(!u.search_focus&&!u.replicator_open);
    /* Inventory overlays right quickslots: clicks must not fall through chrome. */
    pe_game_unequip(g);int juice=find(g,916);assert(juice>=0);
    assert(pe_game_quickslot_assign(g,5,juice));int juices=pe_inventory_count(&g->inventory,916);
    i=input();i.inventory=true;pe_sandbox_ui_route(&u,g,i);
    r=pe_sandbox_ui_route(&u,g,click(u.layout.quickslots[5],false));
    assert(r.consume_world&&pe_inventory_count(&g->inventory,916)==juices);
    /* Drag cancellation outside UI conserves all items and does not touch world. */
    u.inventory_tab=pe_item_def(916)->tab;v=visible(&u,g,916);assert(v>=0);
    pe_sandbox_ui_route(&u,g,click(u.layout.inventory_slots[v],false));assert(u.dragging);
    i=input();i.mouse_x=30;i.mouse_y=30;i.left_released=true;r=pe_sandbox_ui_route(&u,g,i);
    assert(r.consume_world&&!u.dragging&&pe_inventory_count(&g->inventory,916)==juices);
    /* No input actions from an unfocused or nonfinite pointer event. */
    i=input();i.focused=false;i.escape=true;r=pe_sandbox_ui_route(&u,g,i);assert(r.consume_world&&u.inventory_open);
    i=input();i.mouse_x=NAN;i.inventory=true;r=pe_sandbox_ui_route(&u,g,i);assert(r.consume_world&&u.inventory_open);
    pe_sandbox_ui_resize(&u,1920,1080);assert(fabsf(u.layout.scale-1.5f)<.001f);
    i=input();i.escape=true;pe_sandbox_ui_route(&u,g,i);i=input();r=pe_sandbox_ui_route(&u,g,i);assert(!r.consume_world);
    /* Source tab0 furniture still selects the host placement item. Selection
       must neither consume its instance nor turn unrelated tab0 items into
       build materials. Exercise both hotkey and original right-click routes. */
    pe_game_init(g,91);pe_sandbox_ui_init(&u,1280,720);
    assert(pe_item_def(PE_ITEM_FENCE)->tab==0&&pe_item_def(PE_ITEM_LADDER)->tab==0);
    assert(pe_inventory_add(&g->inventory,PE_ITEM_FENCE,1));
    assert(pe_inventory_add(&g->inventory,PE_ITEM_LADDER,1));
    assert(pe_inventory_add(&g->inventory,PE_ITEM_WOOD,4));
    assert(pe_inventory_add(&g->inventory,916,2));
    const int placeables[]={PE_ITEM_FENCE,PE_ITEM_LADDER,PE_ITEM_WOOD};
    for(int k=0;k<3;k++) {
        int owned_before=pe_inventory_count(&g->inventory,placeables[k]);
        assert(pe_game_quickslot_assign(g,k,find(g,placeables[k])));
        i=input();i.quickslot=k;r=pe_sandbox_ui_route(&u,g,i);
        assert(r.consume_world&&r.selected_item==placeables[k]&&!g->equipped_instance_id);
        assert(pe_inventory_count(&g->inventory,placeables[k])==owned_before);
    }
    u.free_cursor=true;
    r=pe_sandbox_ui_route(&u,g,click(u.layout.quickslots[1],true));
    assert(r.selected_item==PE_ITEM_LADDER&&r.consume_world&&pe_inventory_count(&g->inventory,PE_ITEM_LADDER)==1);
    assert(pe_game_quickslot_assign(g,3,find(g,916)));
    r=pe_sandbox_ui_route(&u,g,click(u.layout.quickslots[3],true));
    assert(!r.selected_item&&pe_inventory_count(&g->inventory,916)==1);
    assert(pe_inventory_remove(&g->inventory,PE_ITEM_FENCE,1));
    i=input();i.quickslot=0;r=pe_sandbox_ui_route(&u,g,i);assert(!r.selected_item);
    /* Construction has a bounded independent palette, never ordinary item
       shortcuts. A inherited normal-mode cursor flag must not block canvas. */
    pe_sandbox_ui_init(&u,1280,720);u.building=true;u.free_cursor=true;
    PeInventory inventory_before=g->inventory;
    PeQuickslot shortcuts_before[PE_QUICKSLOTS];memcpy(shortcuts_before,g->quickslots,sizeof shortcuts_before);
    uint32_t equipped_before=g->equipped_instance_id;
    i=input();i.left_pressed=true;r=pe_sandbox_ui_route(&u,g,i);
    assert(r.cursor_visible&&!r.consume_world&&!r.selected_item&&!r.toggle_build);
    const int build_items[]={0,PE_ITEM_DIRT,PE_ITEM_STONE,PE_ITEM_WOOD,0,PE_ITEM_FENCE,PE_ITEM_LADDER};
    for(int key=0;key<9;key++) {
        i=input();i.quickslot=key;r=pe_sandbox_ui_route(&u,g,i);
        assert(r.consume_world&&r.selected_item==(key<7?build_items[key]:0));
        assert(r.build_shape==(key==0?PE_B45_CUBE:key==4?PE_B45_RAMP:0));
        r=pe_sandbox_ui_route(&u,g,click(pe_sandbox_ui_build_slot(&u,key),false));
        assert(r.consume_world&&r.selected_item==(key<7?build_items[key]:0));
        assert(r.build_shape==(key==0?PE_B45_CUBE:key==4?PE_B45_RAMP:0));
    }
    assert(!memcmp(&inventory_before,&g->inventory,sizeof inventory_before));
    assert(!memcmp(shortcuts_before,g->quickslots,sizeof shortcuts_before));
    assert(equipped_before==g->equipped_instance_id);
    /* Selecting a voxel shape after furniture must restore a voxel material,
       otherwise the host would continue placing fence/ladder despite the icon. */
    for(int furniture=0;furniture<2;furniture++) {
        u.build_item=furniture?PE_ITEM_LADDER:PE_ITEM_FENCE;
        i=input();i.quickslot=furniture?4:0;r=pe_sandbox_ui_route(&u,g,i);
        assert(r.selected_item==PE_ITEM_WOOD&&r.build_shape==(furniture?PE_B45_RAMP:PE_B45_CUBE));
        r=pe_sandbox_ui_route(&u,g,click(pe_sandbox_ui_build_tool(&u,1),false));
        assert(r.selected_item==PE_ITEM_WOOD&&r.build_shape==PE_B45_RAMP);
    }
    u.build_item=PE_ITEM_STONE;i=input();i.quickslot=0;r=pe_sandbox_ui_route(&u,g,i);
    assert(r.build_shape==PE_B45_CUBE&&!r.selected_item); /* preserve valid material */
    u.build_item=PE_ITEM_WOOD;
    /* Chrome gaps, disabled buttons and right clicks own the pointer too. */
    i=click(pe_sandbox_ui_build_toolbar(&u),false);i.mouse_x=340;i.mouse_y=654;
    r=pe_sandbox_ui_route(&u,g,i);assert(r.consume_world&&!r.selected_item&&!r.build_shape);
    for(int tool=0;tool<8;tool++) {
        r=pe_sandbox_ui_route(&u,g,click(pe_sandbox_ui_build_tool(&u,tool),false));
        assert(r.consume_world);
        assert(r.selected_item==(tool==0?PE_ITEM_DIRT:0));
        assert(r.build_shape==(tool==1?PE_B45_RAMP:0));
        assert(r.build_brush==(tool==2?1:tool==3?4:0));
        assert(r.rotate_build==(tool==4)&&r.toggle_build==(tool==7));
        assert(!r.save&&!r.load); /* unavailable source creation commands */
    }
    r=pe_sandbox_ui_route(&u,g,click(pe_sandbox_ui_build_slot(&u,3),true));
    assert(r.consume_world&&!r.selected_item&&!r.build_shape);
    /* Panels/pause own both number keys and the frame that dismisses them. */
    for(int modal=0;modal<6;modal++) {
        bool *state=modal==0?&u.inventory_open:modal==1?&u.character_open:modal==2?&u.missions_open:
                    modal==3?&u.replicator_open:modal==4?&u.menu_open:&u.paused;
        *state=true;i=input();i.quickslot=3;
        r=pe_sandbox_ui_route(&u,g,i);assert(r.consume_world&&!r.selected_item&&!r.build_shape);
        i=click(pe_sandbox_ui_build_slot(&u,3),false);i.escape=true;i.quickslot=3;
        r=pe_sandbox_ui_route(&u,g,i);assert(r.consume_world&&!r.selected_item&&!r.build_shape&&!*state);
    }
    u.inventory_open=true;i=click(pe_sandbox_ui_build_slot(&u,3),false);i.inventory=true;i.quickslot=3;
    r=pe_sandbox_ui_route(&u,g,i);assert(r.consume_world&&!r.selected_item&&!u.inventory_open);
    /* Inventory drag cannot assign a hidden normal quickbar in build mode. */
    u.dragging=true;u.drag_item=916;u.drag_instance=0;u.drag_quickslot=-1;
    i=click(u.layout.quickslots[2],false);i.left_pressed=false;i.left_released=true;
    r=pe_sandbox_ui_route(&u,g,i);assert(r.consume_world&&!u.dragging);
    assert(!memcmp(shortcuts_before,g->quickslots,sizeof shortcuts_before));
    for(int size=0;size<2;size++) {
        pe_sandbox_ui_resize(&u,size?1920:1000,size?1080:700);
        r=pe_sandbox_ui_route(&u,g,click(pe_sandbox_ui_build_slot(&u,4),false));
        assert(r.consume_world&&r.build_shape==PE_B45_RAMP);
        i=input();i.mouse_x=u.layout.width/2;i.mouse_y=u.layout.height/2;
        r=pe_sandbox_ui_route(&u,g,i);assert(!r.consume_world);
    }
    /* The supplier offer and source pages are modal and grant only at final ACK.
       Closing/advancing frames cannot use shortcuts, save/load, or place. */
    assert(pe_game_init_mode(g,73,PE_GAME_ADVENTURE));
    pe_sandbox_ui_init(&u,1280,720);
    assert(pe_sandbox_ui_open_supplier(&u,g,20016,true));
    assert(!pe_adventure_dialogue_current(&u.dialogue,g));
    r=pe_sandbox_ui_route(&u,g,click(pe_sandbox_ui_supplier_choice(&u,0),false));
    assert(r.consume_world&&!r.npc_interaction&&u.dialogue.stage==PE_ADVENTURE_DIALOGUE_IDLE&&!g->arrival_complete);
    assert(pe_test_ack_welcome(g));
    assert(pe_game_arrival_complete(g));
    assert(!pe_sandbox_ui_open_supplier(&u,g,20016,false));
    assert(pe_sandbox_ui_open_supplier(&u,g,20016,true));
    i=input();i.inventory=i.character=i.save=i.load=true;i.quickslot=0;
    r=pe_sandbox_ui_route(&u,g,i);
    assert(r.consume_world&&r.cursor_visible&&!r.save&&!r.load&&!u.inventory_open&&!u.character_open&&!g->supplies_claimed);
    r=pe_sandbox_ui_route(&u,g,click(pe_sandbox_ui_supplier_choice(&u,0),false));
    assert(r.consume_world&&u.dialogue.stage==PE_ADVENTURE_DIALOGUE_TALK);
    assert(pe_adventure_dialogue_current(&u.dialogue,g)->talk_id==4090);
    i=input();i.advance=true;
    r=pe_sandbox_ui_route(&u,g,i);assert(r.consume_world&&!g->supplies_claimed);
    i=input();i.escape=true;i.quickslot=0;
    r=pe_sandbox_ui_route(&u,g,i);
    assert(r.consume_world&&!r.paused&&u.dialogue.stage==PE_ADVENTURE_DIALOGUE_IDLE&&!g->supplies_claimed);
    assert(pe_sandbox_ui_open_supplier(&u,g,20016,true));
    r=pe_sandbox_ui_route(&u,g,click(pe_sandbox_ui_supplier_choice(&u,0),false));
    for(int page=0;page<5;page++) {
        assert(pe_adventure_dialogue_current(&u.dialogue,g)->talk_id==4090+page);
        assert(!g->supplies_claimed);
        i=click(u.layout.dialogue_next,false);i.quickslot=0;i.save=true;
        r=pe_sandbox_ui_route(&u,g,i);assert(r.consume_world&&!r.save);
    }
    assert(g->supplies_claimed&&pe_inventory_count(&g->inventory,19)==1&&!g->known_recipe_count);
    assert(u.dialogue.stage==PE_ADVENTURE_DIALOGUE_IDLE);
    assert(pe_sandbox_ui_open_supplier(&u,g,20016,true));
    assert(!pe_adventure_dialogue_current(&u.dialogue,g));
    r=pe_sandbox_ui_route(&u,g,click(pe_sandbox_ui_supplier_choice(&u,0),false));
    assert(r.consume_world&&u.dialogue.stage==PE_ADVENTURE_DIALOGUE_IDLE&&pe_inventory_count(&g->inventory,19)==1);
    pe_sandbox_ui_init(&u,1280,720);u.free_cursor=true;
    i=input();i.mouse_x=640;i.mouse_y=360;i.right_pressed=true;
    r=pe_sandbox_ui_route(&u,g,i);assert(r.consume_world&&r.npc_interaction);
    i.mouse_x=u.layout.hud.x+2;i.mouse_y=u.layout.hud.y+2;
    r=pe_sandbox_ui_route(&u,g,i);assert(r.consume_world&&!r.npc_interaction);
    i=input();i.mouse_x=640;i.mouse_y=360;i.inventory=true;
    r=pe_sandbox_ui_route(&u,g,i);assert(!r.npc_interaction&&u.inventory_open);
    r=pe_sandbox_ui_route(&u,g,i);assert(!r.npc_interaction&&!u.inventory_open);
    i.inventory=false;i.focused=false;
    r=pe_sandbox_ui_route(&u,g,i);assert(!r.npc_interaction);
    /* Welcome ACKs persist while Escape only closes the current page. Modal
       world/save/quickslot suppression applies to every source welcome page. */
    assert(pe_game_init_mode(g,73,PE_GAME_ADVENTURE));pe_sandbox_ui_init(&u,1280,720);
    PeInventory welcome_inventory=g->inventory;
    assert(!pe_sandbox_ui_open_welcome(&u,g,20008,false));
    assert(pe_sandbox_ui_open_welcome(&u,g,20008,true));
    for(unsigned page=0;page<PE_ADVENTURE_WELCOME_PAGES;page++){
        assert(pe_adventure_dialogue_current(&u.dialogue,g)->talk_id==(page?4080+(int)page:1949));
        i=input();i.escape=true;i.save=true;i.quickslot=0;
        r=pe_sandbox_ui_route(&u,g,i);
        assert(r.consume_world&&!r.save&&!r.paused&&u.dialogue.stage==PE_ADVENTURE_DIALOGUE_IDLE);
        assert(g->welcome_page==page&&!g->adventure_mission);
        assert(pe_sandbox_ui_open_welcome(&u,g,20008,true));
        i=input();i.inventory=i.character=i.save=i.load=true;i.quickslot=0;
        r=pe_sandbox_ui_route(&u,g,i);
        assert(r.consume_world&&!r.save&&!r.load&&!u.inventory_open&&!u.character_open);
        i=click(u.layout.dialogue_next,false);i.save=true;i.quickslot=0;
        r=pe_sandbox_ui_route(&u,g,i);
        assert(r.consume_world&&!r.save&&g->welcome_page==page+1);
        assert(!g->arrival_complete&&!g->supplies_claimed&&!memcmp(&g->inventory,&welcome_inventory,sizeof welcome_inventory));
    }
    assert(g->adventure_mission==PE_ADVENTURE_ARRIVAL&&u.dialogue.stage==PE_ADVENTURE_DIALOGUE_IDLE);
    /* Title dragging stays presentation-only; close/item hit targets move
       together, release is consumed, and covered character controls stay inert. */
    pe_sandbox_ui_init(&u,1280,720);u.inventory_open=true;u.character_open=true;
    PeInventory before_drag=g->inventory;
    i=input();i.mouse_x=u.layout.inventory.x+80;i.mouse_y=u.layout.inventory.y+24;i.left_pressed=true;
    r=pe_sandbox_ui_route(&u,g,i);assert(r.consume_world&&u.windows.dragging==PE_UI_WINDOW_INVENTORY+1&&!u.dragging);
    i.left_pressed=false;i.mouse_x=130;i.mouse_y=124;i.quickslot=0;i.save=true;i.character=true;
    r=pe_sandbox_ui_route(&u,g,i);assert(r.consume_world&&!r.save&&u.character_open&&!u.dragging);
    assert(u.layout.inventory.x==50&&u.layout.inventory.y==100);
    i.quickslot=-1;i.save=i.character=false;i.left_released=true;
    r=pe_sandbox_ui_route(&u,g,i);assert(r.consume_world&&!u.windows.dragging);
    assert(!memcmp(&g->inventory,&before_drag,sizeof before_drag));
    pe_sandbox_ui_resize(&u,1000,700);
    assert(u.layout.inventory.x>=0&&u.layout.inventory.x+u.layout.inventory.w<=1000);
    r=pe_sandbox_ui_route(&u,g,click(u.layout.inventory_close,false));assert(r.consume_world&&!u.inventory_open);
    u.inventory_open=true;i=input();i.mouse_x=u.layout.inventory.x+50;i.mouse_y=u.layout.inventory.y+20;i.left_pressed=true;
    pe_sandbox_ui_route(&u,g,i);assert(u.windows.dragging);
    i.focused=false;r=pe_sandbox_ui_route(&u,g,i);assert(r.consume_world&&!u.windows.dragging);
    /* A host-cancelled encounter must consume keys before any transaction. */
    pe_sandbox_ui_init(&u,1280,720);u.discard_input=true;i=input();i.inventory=i.save=i.load=i.tab=true;i.quickslot=0;
    PeInventory discarded_inventory=g->inventory;
    r=pe_sandbox_ui_route(&u,g,i);assert(r.consume_world&&!r.save&&!r.load&&!r.npc_interaction&&!u.inventory_open&&!u.free_cursor&&!u.discard_input);
    assert(!memcmp(&discarded_inventory,&g->inventory,sizeof discarded_inventory));
    PeSandboxMapNpc markers[]={{20002,{43,12,32}},{20011,{41,12,88}}};
    assert(pe_sandbox_ui_set_resident_markers(&u,markers,2));assert(u.resident_markers==markers&&u.resident_marker_count==2);
    assert(!pe_sandbox_ui_set_resident_markers(&u,NULL,1));assert(u.resident_marker_count==2);
    markers[0].position.x=NAN;assert(!pe_sandbox_ui_set_resident_markers(&u,markers,2));markers[0].position.x=43;
    assert(pe_sandbox_ui_set_resident_markers(&u,NULL,0));assert(!u.resident_markers&&!u.resident_marker_count);
    /* Archived vendors share modal input ownership, including close/drag frames. */
    pe_sandbox_ui_init(&u,1280,720);g->money=500;
    assert(!pe_sandbox_ui_open_town(&u,g,20002,"Cherish Mercer",(Texture2D){0},false));
    assert(pe_sandbox_ui_open_town(&u,g,20002,"Cherish Mercer",(Texture2D){0},true));
    i=input();i.inventory=i.save=i.load=true;i.quickslot=0;
    r=pe_sandbox_ui_route(&u,g,i);assert(r.consume_world&&r.cursor_visible&&!r.save&&!r.load&&!u.inventory_open&&!r.npc_interaction);
    r=pe_sandbox_ui_route(&u,g,click(pe_sandbox_ui_supplier_choice(&u,0),false));assert(u.shop_open&&pe_town_shop_stock(g,20002,1184)==3);
    r=pe_sandbox_ui_route(&u,g,click(pe_sandbox_ui_shop_row(&u,2),false));assert(u.shop_selection==2);
    int shovels=pe_inventory_count(&g->inventory,12);
    r=pe_sandbox_ui_route(&u,g,click(pe_sandbox_ui_shop_button(&u,2),false));
    assert(r.consume_world&&g->money==342&&pe_inventory_count(&g->inventory,12)==shovels+1&&pe_town_shop_stock(g,20002,1184)==2);
    r=pe_sandbox_ui_route(&u,g,click(pe_sandbox_ui_shop_button(&u,1),false));assert(u.shop_quantity==2);
    r=pe_sandbox_ui_route(&u,g,click(pe_sandbox_ui_shop_button(&u,2),false));assert(g->money==26&&pe_town_shop_stock(g,20002,1184)==0);
    PeTrainingUIRect shop=pe_sandbox_ui_shop_panel(&u);i=input();i.left_pressed=true;i.mouse_x=shop.x+100;i.mouse_y=shop.y+25;
    r=pe_sandbox_ui_route(&u,g,i);assert(u.shop_dragging&&r.consume_world);
    i.left_pressed=false;i.mouse_x=-100;i.mouse_y=-100;i.left_released=true;r=pe_sandbox_ui_route(&u,g,i);
    shop=pe_sandbox_ui_shop_panel(&u);assert(!u.shop_dragging&&shop.x==0&&shop.y==0&&r.consume_world);
    pe_sandbox_ui_resize(&u,1000,700);shop=pe_sandbox_ui_shop_panel(&u);assert(shop.x>=0&&shop.x+shop.w<=1000&&shop.y>=0&&shop.y+shop.h<=700);
    i=input();i.escape=i.save=true;r=pe_sandbox_ui_route(&u,g,i);assert(r.consume_world&&!r.save&&!u.town_open&&!r.paused);
    assert(pe_sandbox_ui_open_town(&u,g,20009,"Dikeledi Sehgal",(Texture2D){0},true));
    i=input();i.advance=true;r=pe_sandbox_ui_route(&u,g,i);assert(!u.town_open&&r.consume_world&&!r.npc_interaction);
    assert(!pe_sandbox_ui_open_annot(&u,g,20018,(Texture2D){0},true));assert(pe_game_init_mode(g,73,PE_GAME_ADVENTURE));assert(pe_test_ack_welcome(g));assert(pe_game_arrival_complete(g));assert(pe_game_accept_supplies(g));g->money=26;g->annot_complete=true;
    assert(pe_sandbox_ui_open_annot(&u,g,20018,(Texture2D){0},true));
    i=input();i.advance=true;r=pe_sandbox_ui_route(&u,g,i);assert(pe_adventure_dialogue_current(&u.dialogue,g)->talk_id==4096);
    r=pe_sandbox_ui_route(&u,g,i);assert(u.dialogue.stage==PE_ADVENTURE_DIALOGUE_CHOICE&&r.consume_world&&g->money==26);
    pe_sandbox_ui_resize(&u,1280,720);
    PeTrainingUIRect accept_choice=pe_sandbox_ui_supplier_choice(&u,0),refuse_choice=pe_sandbox_ui_supplier_choice(&u,1);
    assert(accept_choice.x==395&&accept_choice.y==489&&refuse_choice.y==509&&accept_choice.h==20);
    assert(pe_training_ui_contains(u.layout.dialogue,accept_choice.x,accept_choice.y));
    i=click(pe_sandbox_ui_supplier_choice(&u,1),false);i.save=i.load=true;i.quickslot=0;
    r=pe_sandbox_ui_route(&u,g,i);assert(r.consume_world&&!r.save&&!r.load&&u.dialogue.stage==PE_ADVENTURE_DIALOGUE_IDLE&&g->medicine_state==PE_MEDICINE_NONE);
    assert(pe_sandbox_ui_open_annot(&u,g,20018,(Texture2D){0},true));
    i=input();i.advance=true;pe_sandbox_ui_route(&u,g,i);pe_sandbox_ui_route(&u,g,i);
    assert(u.dialogue.stage==PE_ADVENTURE_DIALOGUE_CHOICE);
    assert(pe_game_use_slot(g,find(g,552)));assert(pe_game_use_slot(g,find(g,512)));
    int scripts=pe_inventory_count(&g->inventory,877);
    i=click(pe_sandbox_ui_supplier_choice(&u,0),false);i.inventory=i.save=i.load=true;i.quickslot=0;
    r=pe_sandbox_ui_route(&u,g,i);assert(r.consume_world&&!r.save&&!r.load&&!u.inventory_open&&u.dialogue.stage==PE_ADVENTURE_DIALOGUE_IDLE);
    assert(g->medicine_state==PE_MEDICINE_ACTIVE&&pe_inventory_count(&g->inventory,877)==scripts+1);
    assert(pe_sandbox_ui_open_annot(&u,g,20018,(Texture2D){0},true));
    i=input();i.advance=true;r=pe_sandbox_ui_route(&u,g,i);assert(r.consume_world&&u.dialogue.stage==PE_ADVENTURE_DIALOGUE_MENU);
    assert(pe_inventory_add(&g->inventory,917,5));
    r=pe_sandbox_ui_route(&u,g,i);assert(pe_adventure_dialogue_current(&u.dialogue,g)->talk_id==4100&&g->medicine_state==PE_MEDICINE_ACTIVE);
    i.escape=true;r=pe_sandbox_ui_route(&u,g,i);assert(r.consume_world&&u.dialogue.stage==PE_ADVENTURE_DIALOGUE_IDLE&&pe_inventory_count(&g->inventory,917)==5);
    assert(pe_sandbox_ui_open_annot(&u,g,20018,(Texture2D){0},true));i=input();i.advance=true;pe_sandbox_ui_route(&u,g,i);
    r=pe_sandbox_ui_route(&u,g,i);assert(r.consume_world&&u.dialogue.stage==PE_ADVENTURE_DIALOGUE_IDLE&&g->medicine_state==PE_MEDICINE_COMPLETE);
    assert(pe_inventory_count(&g->inventory,917)==0);
    /* Source9074 may be read/refused, but unavailable destination acceptance
       stays modal and displays the returned reason without spending anything. */
    assert(pe_sandbox_ui_open_annot(&u,g,20018,(Texture2D){0},true));
    assert(u.dialogue_result==PE_ADVENTURE_DIALOGUE_REJECTED&&pe_adventure_dialogue_current(&u.dialogue,g)->talk_id==4101);
    i=input();i.advance=true;pe_sandbox_ui_route(&u,g,i);pe_sandbox_ui_route(&u,g,i);
    assert(u.dialogue.stage==PE_ADVENTURE_DIALOGUE_CHOICE&&pe_adventure_dialogue_choice(&u.dialogue,g,0)->talk_id==4103);
    PeInventory before_hunt=g->inventory;uint32_t before_money=g->money;
    i=click(pe_sandbox_ui_supplier_choice(&u,0),false);i.save=i.load=i.inventory=true;i.quickslot=0;
    for(int retry=0;retry<2;retry++){
        r=pe_sandbox_ui_route(&u,g,i);
        assert(r.consume_world&&!r.save&&!r.load&&!u.inventory_open&&!r.npc_interaction);
        assert(u.dialogue_result==PE_ADVENTURE_DIALOGUE_UNAVAILABLE&&u.dialogue.stage==PE_ADVENTURE_DIALOGUE_CHOICE&&g->message[0]);
        assert(g->hunt_state==PE_HUNT_NONE&&g->medicine_state==PE_MEDICINE_COMPLETE&&g->money==before_money&&!memcmp(&before_hunt,&g->inventory,sizeof before_hunt));
    }
    r=pe_sandbox_ui_route(&u,g,click(pe_sandbox_ui_supplier_choice(&u,1),false));
    assert(r.consume_world&&u.dialogue.stage==PE_ADVENTURE_DIALOGUE_IDLE&&g->hunt_state==PE_HUNT_NONE);
    assert(pe_sandbox_ui_open_annot(&u,g,20018,(Texture2D){0},true));
    assert(u.dialogue_result==PE_ADVENTURE_DIALOGUE_REJECTED);i=input();i.escape=true;pe_sandbox_ui_route(&u,g,i);
    /* The large Annot catalogs remain bounded to eight visible rows and consume
       wheel/keys. Reopening another vendor resets hidden selection/quantity. */
    uint32_t large_vendor=0;for(uint32_t entity=20020;entity<20100;entity++)if(pe_town_shop_count(entity)>8){large_vendor=entity;break;}
    assert(large_vendor&&pe_sandbox_ui_open_town(&u,g,large_vendor,"Annot vendor",(Texture2D){0},true));
    i=input();i.advance=true;pe_sandbox_ui_route(&u,g,i);assert(u.shop_open);
    i=input();PeTrainingUIRect row0=pe_sandbox_ui_shop_row(&u,0);i.mouse_x=row0.x+10;i.mouse_y=row0.y+10;i.wheel=-1;i.save=true;
    for(int n=0;n<120;n++){r=pe_sandbox_ui_route(&u,g,i);assert(r.consume_world&&!r.save);}
    assert(u.shop_scroll==(int)pe_town_shop_count(large_vendor)-8&&u.shop_selection>=u.shop_scroll);
    r=pe_sandbox_ui_route(&u,g,click(pe_sandbox_ui_shop_row(&u,7),false));assert(u.shop_selection==(int)pe_town_shop_count(large_vendor)-1&&r.consume_world);
    pe_sandbox_ui_cancel_town(&u);assert(pe_sandbox_ui_open_town(&u,g,20002,"Cherish Mercer",(Texture2D){0},true));assert(!u.shop_scroll&&!u.shop_selection&&u.shop_quantity==1);
    /* A free-camera pointer is playable world input, while real HUD/modal
       presses still own every held frame and release. */
    pe_sandbox_ui_init(&u,1280,720);u.camera_cursor=true;
    i=input();r=pe_sandbox_ui_route(&u,g,i);
    assert(r.cursor_visible&&!r.consume_world&&r.npc_interaction);
    i.left_pressed=true;r=pe_sandbox_ui_route(&u,g,i);
    assert(!r.consume_world);
    i=click(u.layout.minimap,false);r=pe_sandbox_ui_route(&u,g,i);
    assert(r.consume_world);
    PeSandboxPointerGate camera_gate={0};
    assert(pe_sandbox_pointer_release_gate(&camera_gate,r.consume_world,1));
    assert(pe_sandbox_pointer_release_gate(&camera_gate,false,1));
    assert(pe_sandbox_pointer_release_gate(&camera_gate,false,0));
    assert(!pe_sandbox_pointer_release_gate(&camera_gate,false,0));
    /* F5 admission differs from Tab-only world blocking, but must never
       bypass an actual modal or its opening/closing/held-release frame. */
    pe_sandbox_ui_init(&u,1280,720);u.free_cursor=true;
    i=input();r=pe_sandbox_ui_route(&u,g,i);
    assert(r.consume_world&&r.camera_cycle_allowed&&u.free_cursor);
    i.tab=true;r=pe_sandbox_ui_route(&u,g,i);
    assert(r.consume_world&&!r.camera_cycle_allowed&&!u.free_cursor);
    i=input();u.free_cursor=true;i.inventory=true;r=pe_sandbox_ui_route(&u,g,i);
    assert(r.consume_world&&!r.camera_cycle_allowed&&u.inventory_open);
    i=input();r=pe_sandbox_ui_route(&u,g,i);assert(!r.camera_cycle_allowed);
    i.escape=true;r=pe_sandbox_ui_route(&u,g,i);
    assert(r.consume_world&&!r.camera_cycle_allowed&&!u.inventory_open);
    i=input();r=pe_sandbox_ui_route(&u,g,i);assert(r.camera_cycle_allowed);
    i=click(u.layout.minimap,false);r=pe_sandbox_ui_route(&u,g,i);
    assert(r.consume_world&&!r.camera_cycle_allowed);
    PeSandboxPointerGate tab_gate={0};
    assert(pe_sandbox_pointer_release_gate(&tab_gate,r.consume_world,1));
    i=input();r=pe_sandbox_ui_route(&u,g,i);assert(r.camera_cycle_allowed);
    assert(pe_sandbox_pointer_release_gate(&tab_gate,r.consume_world,1));
    assert(pe_sandbox_pointer_release_gate(&tab_gate,r.consume_world,0));
    assert(!pe_sandbox_pointer_release_gate(&tab_gate,r.consume_world,0));
    i.focused=false;r=pe_sandbox_ui_route(&u,g,i);assert(!r.camera_cycle_allowed);
    i=input();u.discard_input=true;r=pe_sandbox_ui_route(&u,g,i);assert(!r.camera_cycle_allowed);
    i.save=true;r=pe_sandbox_ui_route(&u,g,i);assert(r.save&&!r.camera_cycle_allowed);
    i=input();i.load=true;r=pe_sandbox_ui_route(&u,g,i);assert(r.load&&!r.camera_cycle_allowed);
    i=input();i.quickslot=0;r=pe_sandbox_ui_route(&u,g,i);assert(!r.camera_cycle_allowed);
        pe_game_destroy(g);free(g);deployed_menu();starter_pair_readiness();hunt_presentation();numbered_responses();consumable_ownership();puts("sandbox UI: inventory, crafting, quickslots, dialogue, deployed interactions and modal ownership passed");return 0;
}
