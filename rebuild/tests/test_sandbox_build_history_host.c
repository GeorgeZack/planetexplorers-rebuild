/* Actual sandbox input + dispatch + collision; no window/GPU APIs execute. */
#include "../src/pe_sandbox_view.c"
#include <assert.h>
static PeSandboxUIInput history_input(void){return (PeSandboxUIInput){.focused=true,.quickslot=-1,.mouse_x=100,.mouse_y=300};}
static PeSandboxUIInput history_click(PeTrainingUIRect r){
    PeSandboxUIInput i=history_input();i.left_pressed=true;i.mouse_x=r.x+r.w*.5f;i.mouse_y=r.y+r.h*.5f;return i;
}
static PeSandboxUIResult history_route(PeSandboxUI *ui,PeGame *game,bool undo,bool redo){
    PeSandboxUIInput i=history_input();i.undo_build=undo;i.redo_build=redo;
    i.build_history_chord=i.build_history_keys_down=undo||redo;
    return pe_sandbox_ui_route(ui,game,i);
}
static void history_apply(PeGame *g,PeSandboxUI *u,PeSandboxUIResult r,bool enabled,PeSandboxCollision *collision){
    char error[160]={0};assert(sandbox_build_history_apply(g,u,r,enabled,collision,error,sizeof error));
}
static bool history_ray(const PeSandboxCollision *c){
    return pe_sandbox_collision_ray_geometry(c,(PeVec3){32,10.25f,32.25f},(PeVec3){1,0,0},5).hit;
}
static bool history_sweep(const PeSandboxCollision *c){
    return pe_sandbox_collision_sweep(c,(PeVec3){32,10.05f,32.25f},PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT,(PeVec3){4,0,0}).hit;
}
int main(void){
    PeGame *g=calloc(1,sizeof *g);assert(g);pe_game_init(g,334);
    for(int z=0;z<PE_WORLD_Z;z++)for(int y=0;y<PE_WORLD_Y;y++)for(int x=0;x<PE_WORLD_X;x++)
        pe_world_set(&g->world,x,y,z,(PeVoxel){y<10?255:0,y<10?PE_MAT_DIRT:PE_MAT_AIR});
    g->player=(PeVec3){32,10.05f,32};g->velocity=(PeVec3){0};g->tree_count=g->block_count=g->cell_count=0;
    g->inventory=(PeInventory){.next_instance_id=g->inventory.next_instance_id,.categorized=g->mode==PE_GAME_ADVENTURE};
    g->equipped_instance_id=g->shield_instance_id=0;memset(g->quickslots,0,sizeof g->quickslots);
    assert(pe_inventory_add(&g->inventory,PE_ITEM_WOOD,5));
    PeB45Cell cells[4]={{68,20,64,4,11},{69,20,64,4,11},{68,20,65,4,11},{69,20,65,4,11}};
    assert(pe_game_build_cells(g,cells,4));assert(pe_build_history_can_undo(g));
    PeSandboxCollision *collision=pe_sandbox_collision_create();assert(collision);char error[160]={0};
    assert(pe_game_collision_update(g,collision,error,sizeof error));assert(history_ray(collision)&&history_sweep(collision));
    PeSandboxUI ui;pe_sandbox_ui_init(&ui,1280,720);ui.building=true;
    double clock=g->game_seconds;float cooldown=g->tool_cooldown;unsigned built=g->built;
    PeSandboxUIResult r=history_route(&ui,g,true,true);
    assert(r.undo_build&&!r.redo_build&&r.consume_actions&&!r.consume_world&&!r.toggle_build);
    history_apply(g,&ui,r,true,collision);
    assert(g->cell_count==0&&pe_inventory_count(&g->inventory,PE_ITEM_WOOD)==5);
    /* No fixed update: dispatch itself refreshes both camera ray and motion. */
    assert(!history_ray(collision)&&!history_sweep(collision));
    assert(g->game_seconds==clock&&g->tool_cooldown==cooldown&&g->built==built);
    PeSandboxUIInput i=history_input();i.build_history_keys_down=true; /* Ctrl released first. */
    r=pe_sandbox_ui_route(&ui,g,i);assert(r.consume_actions&&!r.undo_build&&!r.redo_build);
    r=pe_sandbox_ui_route(&ui,g,history_input());assert(r.consume_actions); /* key release */
    r=pe_sandbox_ui_route(&ui,g,history_input());assert(!r.consume_actions);
    r=history_route(&ui,g,false,true);assert(r.redo_build&&!r.undo_build&&r.consume_actions);
    history_apply(g,&ui,r,true,collision);assert(g->cell_count==4&&history_ray(collision)&&history_sweep(collision));
    assert(pe_inventory_count(&g->inventory,PE_ITEM_WOOD)==4);
    /* No key repeats replay; held and released frames retain action ownership. */
    i=history_input();i.build_history_chord=i.build_history_keys_down=true;
    for(int k=0;k<4;k++){r=pe_sandbox_ui_route(&ui,g,i);assert(r.consume_actions&&!r.undo_build&&!r.redo_build);history_apply(g,&ui,r,true,collision);}
    assert(g->cell_count==4);pe_sandbox_ui_route(&ui,g,history_input());
    /* Existing modals, their closing frame and host cancellation win. */
    ui.inventory_open=true;r=history_route(&ui,g,true,false);
    assert(r.consume_world&&!r.undo_build);history_apply(g,&ui,r,true,collision);assert(g->cell_count==4);
    i=history_input();i.inventory=true;i.undo_build=true;r=pe_sandbox_ui_route(&ui,g,i);
    assert(r.consume_world&&!r.undo_build&&!ui.inventory_open);history_apply(g,&ui,r,true,collision);assert(g->cell_count==4);
    r=history_route(&ui,g,true,false);assert(r.undo_build);history_apply(g,&ui,r,false,collision);assert(g->cell_count==4);
    ui.building=false;history_apply(g,&ui,r,true,collision);assert(g->cell_count==4);
    r=history_route(&ui,g,true,false);assert(!r.undo_build&&!r.redo_build);ui.building=true;
    i=history_input();i.focused=false;i.undo_build=true;r=pe_sandbox_ui_route(&ui,g,i);assert(r.consume_world&&!r.undo_build);
    ui.paused=true;r=history_route(&ui,g,true,false);assert(r.consume_world&&!r.undo_build);ui.paused=false;
    /* Fresh toolbar press may execute once; the same pointer is owned through
     * hold/release, including a concurrent keyboard request during the hold. */
    PeSandboxPointerGate gate={0};i=history_click(pe_sandbox_ui_build_tool(&ui,5));
    r=pe_sandbox_ui_route(&ui,g,i);assert(r.undo_build&&r.consume_world);
    bool was_owned=gate.waiting_for_release;assert(pe_sandbox_pointer_route(&gate,&r,1));
    history_apply(g,&ui,r,!was_owned,collision);assert(g->cell_count==0&&!history_ray(collision));
    r=history_route(&ui,g,false,true);was_owned=gate.waiting_for_release;
    assert(pe_sandbox_pointer_route(&gate,&r,1));history_apply(g,&ui,r,!was_owned,collision);assert(g->cell_count==0);
    r=(PeSandboxUIResult){0};assert(pe_sandbox_pointer_route(&gate,&r,0)&&r.consume_world);
    r=(PeSandboxUIResult){0};assert(!pe_sandbox_pointer_route(&gate,&r,0));
    i=history_click(pe_sandbox_ui_build_tool(&ui,6));r=pe_sandbox_ui_route(&ui,g,i);
    assert(r.redo_build&&r.consume_world);history_apply(g,&ui,r,true,collision);assert(g->cell_count==4&&history_ray(collision));
    pe_build_history_clear(g);i=history_click(pe_sandbox_ui_build_tool(&ui,5));r=pe_sandbox_ui_route(&ui,g,i);
    assert(r.consume_world&&!r.undo_build&&!r.redo_build);assert(g->cell_count==4);
    assert(g->game_seconds==clock&&g->tool_cooldown==cooldown);
    pe_sandbox_collision_free(collision);pe_game_destroy(g);free(g);
    puts("Sandbox construction history host: PASS (no graphics or asset skips)");return 0;
}
