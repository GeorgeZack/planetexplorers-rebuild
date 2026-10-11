#define _POSIX_C_SOURCE 200809L
/* Actual host adapters + UI + save pipeline, with staged possessions/position.
 * Requires installed route geometry; no window, GPU, desktop, or travel claim. */
#include "pe_test_paths.h"
#include "../src/pe_sandbox_view.c"
#include <unistd.h>
static unsigned failures;
#define REQUIRE(c) do{if(!(c)){fprintf(stderr,"repair host:%d: %s (%s)\n",__LINE__,#c,error);failures++;goto done;}}while(0)
static PeSandboxUIInput neutral(void){return(PeSandboxUIInput){.focused=true,.quickslot=-1};}
static PeSandboxUIResult click_rect(PeSandboxUI *u,PeGame *g,PeTrainingUIRect r){
    PeSandboxUIInput i=neutral();i.left_pressed=true;i.mouse_x=r.x+r.w*.5f;i.mouse_y=r.y+r.h*.5f;
    return pe_sandbox_ui_route(u,g,i);
}
static int item_slot(const PeGame *g,int id){for(int i=0;i<pe_inventory_capacity(&g->inventory);i++)if(g->inventory.slots[i].item_id==id)return i;return -1;}
int main(void){
    char error[256]={0},path[4096];snprintf(path,sizeof path,"%s",pe_test_path("rebuild/build/repair-host-XXXXXX"));
    int fd=-1;PeGame *g=calloc(1,sizeof *g),*loaded=calloc(1,sizeof *loaded);
    PeSandboxCollision *c=pe_sandbox_collision_create();PeSandboxUI u={0};PeSandboxPointerGate pointer={0};
    REQUIRE(g&&loaded&&c);REQUIRE(pe_settlement_configure(pe_test_path("rebuild/build/reference-assets"),error,sizeof error));
    REQUIRE(pe_game_init_mode(g,973,PE_GAME_ADVENTURE)&&pe_settlement_start(g,error,sizeof error));
    REQUIRE(pe_settlement_is_route(&g->world)&&pe_game_collision_update(g,c,error,sizeof error));
    PeVec3 anchor;REQUIRE(pe_camp_repair_present(g,c,true,&anchor));
    /* Search a supported near-machine contact pose, rather than save an
     * arbitrary floating/penetrating player. Geometry admission stays real. */
    bool approach=false;PeSceneHit pick={0};
    for(int z=-5;z<=5&&!approach;z++)for(int x=-5;x<=5&&!approach;x++){
        if(x*x+z*z<4||x*x+z*z>36)continue;
        g->player=(PeVec3){anchor.x+x,anchor.y+2,anchor.z+z};g->velocity=(PeVec3){0};
        if(!pe_game_collision_settle(g,c)||!pe_camp_repair_in_range(g,c,true))continue;
        PeVec3 eye=g->player;eye.y+=1.5f;
        pick=pe_camp_repair_pick(g,c,true,eye,(PeVec3){anchor.x-eye.x,anchor.y+.5f-eye.y,anchor.z-eye.z});
        approach=pick.hit;
    }
    REQUIRE(approach);PeVec3 near=g->player;
    REQUIRE(pe_inventory_add(&g->inventory,1,1)&&pe_inventory_add(&g->inventory,279,4));
    int slot=item_slot(g,1);REQUIRE(slot>=0);g->inventory.slots[slot].durability=2369*.5f;
    PeSlot worn=g->inventory.slots[slot];REQUIRE(pe_game_quickslot_assign(g,8,slot));
    pe_sandbox_ui_init(&u,1280,720);PeSandboxUIResult action=pe_sandbox_ui_route(&u,g,neutral());
    REQUIRE(action.npc_interaction&&!action.consume_world);
    REQUIRE(!sandbox_repair_interact(&u,g,c,false,true,true,false,pick,INFINITY,&action,&pointer,1));
    REQUIRE(!sandbox_repair_interact(&u,g,c,true,false,true,false,pick,INFINITY,&action,&pointer,1));
    REQUIRE(!sandbox_repair_interact(&u,g,c,true,true,false,false,pick,INFINITY,&action,&pointer,1));
    REQUIRE(!sandbox_repair_interact(&u,g,c,true,true,true,true,pick,INFINITY,&action,&pointer,1));
    REQUIRE(!sandbox_repair_interact(&u,g,c,true,true,true,false,pick,pick.distance-.01f,&action,&pointer,1));
    REQUIRE(sandbox_repair_interact(&u,g,c,true,true,true,false,pick,INFINITY,&action,&pointer,1));
    REQUIRE(u.repair_open&&action.consume_world&&!action.npc_interaction);
    action=(PeSandboxUIResult){0};REQUIRE(pe_sandbox_pointer_route(&pointer,&action,1)&&action.consume_world);
    action=(PeSandboxUIResult){0};REQUIRE(pe_sandbox_pointer_route(&pointer,&action,0)&&action.consume_world);
    int row=-1;for(int i=0;i<PE_REPAIR_UI_ROWS;i++)if(pe_sandbox_ui_repair_slot(&u,g,i)==slot)row=i;
    REQUIRE(row>=0);
    action=click_rect(&u,g,pe_sandbox_ui_repair_row(&u,row));REQUIRE(action.consume_world&&u.repair_slot==slot);
    click_rect(&u,g,pe_sandbox_ui_repair_button(&u,PE_REPAIR_UI_INSERT));
    REQUIRE(g->repair.item.instance_id==worn.instance_id&&!g->inventory.slots[slot].count&&!g->quickslots[8].item_id);
    int wood=pe_inventory_count(&g->inventory,279);
    click_rect(&u,g,pe_sandbox_ui_repair_button(&u,PE_REPAIR_UI_START));
    REQUIRE(u.repair_confirm==1&&!g->repair.running&&pe_inventory_count(&g->inventory,279)==wood);
    click_rect(&u,g,pe_sandbox_ui_repair_confirm_button(&u,0));
    REQUIRE(g->repair.running&&g->repair.remaining_ticks==6000000&&pe_inventory_count(&g->inventory,279)==wood-2);
    REQUIRE(sandbox_repair_frame(g,.25f,true,false)&&g->repair.remaining_ticks==5975000);
    PeSandboxUIInput escape=neutral();escape.escape=true;escape.save=escape.load=true;
    action=pe_sandbox_ui_route(&u,g,escape);REQUIRE(!u.repair_open&&action.consume_world&&!action.save&&!action.load);
    action=pe_sandbox_ui_route(&u,g,neutral());REQUIRE(!action.consume_world&&sandbox_hunt_runs(&u,action));
    REQUIRE(sandbox_repair_frame(g,1,false,false)&&sandbox_repair_frame(g,1,true,true)&&g->repair.remaining_ticks==5975000);
    /* Background work neither borrows fixed ticks nor stops player movement. */
    PeVec3 before=g->player;
    for(int i=0;i<8;i++)pe_game_step_collision(g,(PeInput){.move_x=-1},PE_FIXED_DT,c);
    REQUIRE(g->repair.remaining_ticks==5975000);
    REQUIRE(fabsf(g->player.x-before.x)+fabsf(g->player.z-before.z)>.001f);
    REQUIRE(sandbox_repair_frame(g,.125f,true,false)&&g->repair.remaining_ticks==5962500);
    fd=mkstemp(path);REQUIRE(fd>=0);close(fd);fd=-2;
    REQUIRE(pe_game_save(g,path,error,sizeof error)&&pe_game_load(loaded,path,error,sizeof error));
    REQUIRE(loaded->repair.item.instance_id==worn.instance_id&&loaded->repair.item.durability==worn.durability&&loaded->repair.remaining_ticks==5962500);
    REQUIRE(pe_inventory_count(&loaded->inventory,279)==wood-2&&!loaded->quickslots[8].item_id);
    pe_game_move(g,loaded);pe_sandbox_ui_cancel_repair(&u);
    REQUIRE(pe_game_collision_update(g,c,error,sizeof error));g->player=near;
    REQUIRE(pe_sandbox_ui_open_repair(&u,g,true));
    g->player.x+=20;REQUIRE(sandbox_repair_close_invalid(&u,g,c,true)&&!u.repair_open&&g->repair.running);
    u.discard_input=true;action=pe_sandbox_ui_route(&u,g,escape);REQUIRE(action.consume_world&&!action.save&&!action.load);
    REQUIRE(pe_sandbox_pointer_route(&pointer,&action,1));action=(PeSandboxUIResult){0};
    REQUIRE(pe_sandbox_pointer_route(&pointer,&action,1)&&action.consume_world);action=(PeSandboxUIResult){0};
    REQUIRE(pe_sandbox_pointer_route(&pointer,&action,0)&&action.consume_world);
    g->player=near;REQUIRE(pe_sandbox_ui_open_repair(&u,g,true));float hp=g->hp;g->hp=0;
    REQUIRE(sandbox_repair_close_invalid(&u,g,c,true)&&!u.repair_open);
    REQUIRE(sandbox_repair_frame(g,59.625f,true,false)&&!g->repair.running&&g->repair.item.durability==2369);
    char message[sizeof g->message];memcpy(message,g->message,sizeof message);
    REQUIRE(sandbox_repair_frame(g,1,true,false)&&!memcmp(message,g->message,sizeof message));
    g->hp=hp;REQUIRE(pe_game_save(g,path,error,sizeof error)&&pe_game_load(loaded,path,error,sizeof error));
    pe_game_move(g,loaded);u.revival_open=false;REQUIRE(pe_sandbox_ui_open_repair(&u,g,true));
    click_rect(&u,g,pe_sandbox_ui_repair_button(&u,PE_REPAIR_UI_TAKE));
    slot=item_slot(g,1);REQUIRE(slot>=0&&!g->repair.item.count&&g->inventory.slots[slot].instance_id==worn.instance_id&&g->inventory.slots[slot].durability==2369);
    REQUIRE(pe_inventory_count(&g->inventory,279)==wood-2&&pe_game_repair_state_valid(g));
 done:
    if(fd>=0)close(fd);
    if(fd!=-1)unlink(path);
    pe_sandbox_collision_free(c);pe_game_destroy(g);pe_game_destroy(loaded);free(g);free(loaded);
    printf("Sandbox repair host: %s (%u failures; required installed route, no graphics)\n",failures?"FAIL":"PASS",failures);
    return failures?1:0;
}
