/* Actual sandbox UI routing; no window, assets or desktop input. */
#include "pe_sandbox_ui.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static PeSandboxUIInput input(void){return (PeSandboxUIInput){.focused=true,.quickslot=-1,.mouse_x=100,.mouse_y=100};}
static PeSandboxUIInput click(PeTrainingUIRect r){PeSandboxUIInput i=input();i.left_pressed=true;i.mouse_x=r.x+r.w/2;i.mouse_y=r.y+r.h/2;return i;}
static PeSandboxUIResult release(PeSandboxUI *u,PeGame *g,PeTrainingUIRect r,bool merge){
    PeSandboxUIInput i=click(r);i.left_pressed=false;i.left_released=true;i.merge_stacks=merge;return pe_sandbox_ui_route(u,g,i);
}
static PeGame *fresh(PeSandboxUI *u){
    PeGame *g=calloc(1,sizeof *g);assert(g&&pe_game_init_mode(g,93,PE_GAME_ADVENTURE));
    memset(&g->inventory,0,sizeof g->inventory);g->inventory.categorized=true;g->inventory.next_instance_id=100;
    g->equipped_instance_id=g->shield_instance_id=0;memset(g->quickslots,0,sizeof g->quickslots);
    pe_sandbox_ui_init(u,1280,720);u->inventory_open=true;u->inventory_tab=2;return g;
}
static void owned(PeSandboxUIResult r){assert(r.consume_world&&!r.save&&!r.load&&!r.npc_interaction&&!r.camera_cycle_allowed);}
int main(void){
    PeSandboxUI u;PeGame *g=fresh(&u);int base=pe_inventory_category_start(&g->inventory,2);
    assert(base==840&&pe_sandbox_ui_inventory_pages(&u,g)==10);
    assert(pe_sandbox_ui_inventory_cell(&u,g,0)==base&&pe_sandbox_ui_inventory_slot(&u,g,0)==-1);
    PeSandboxUIInput i=input();i.page_down=true;
    owned(pe_sandbox_ui_route(&u,g,i));assert(u.inventory_page==1&&pe_sandbox_ui_inventory_cell(&u,g,0)==base+42);
    for(int k=0;k<20;k++)pe_sandbox_ui_route(&u,g,i);
    assert(u.inventory_page==9&&pe_sandbox_ui_inventory_cell(&u,g,41)==base+419);
    i.page_down=false;i.page_up=true;pe_sandbox_ui_route(&u,g,i);assert(u.inventory_page==8);
    u.inventory_page=0;assert(pe_inventory_add(&g->inventory,PE_ITEM_WOOD,20));
    assert(pe_sandbox_ui_inventory_slot(&u,g,0)==base);
    /* Exact stack drag to an empty cell preserves holes; no compaction. */
    owned(pe_sandbox_ui_route(&u,g,click(u.layout.inventory_slots[0])));assert(u.dragging);
    owned(release(&u,g,u.layout.inventory_slots[4],false));
    assert(!u.dragging&&!g->inventory.slots[base].count&&g->inventory.slots[base+4].count==20);
    assert(pe_sandbox_ui_inventory_slot(&u,g,0)==-1&&pe_sandbox_ui_inventory_slot(&u,g,4)==base+4);
    /* Source Split selection dialog is a nested modal. Digits never use quickslots. */
    owned(pe_sandbox_ui_route(&u,g,click(pe_sandbox_ui_inventory_action(&u,0))));assert(u.split_open);
    i=input();i.typed="7";i.quickslot=6;i.response=7;i.save=i.load=i.inventory=i.character=true;
    owned(pe_sandbox_ui_route(&u,g,i));assert(u.split_open&&u.inventory_open&&!u.character_open&&u.split_count==7);
    i=input();i.advance=true;i.quickslot=0;i.save=i.load=true;
    owned(pe_sandbox_ui_route(&u,g,i));assert(!u.split_open&&g->inventory.slots[base].count==7&&g->inventory.slots[base+4].count==13);
    /* Ordinary same-prototype drop swaps, explicit native Shift+drop merges. */
    pe_sandbox_ui_route(&u,g,click(u.layout.inventory_slots[0]));owned(release(&u,g,u.layout.inventory_slots[4],false));
    assert(g->inventory.slots[base].count==13&&g->inventory.slots[base+4].count==7);
    pe_sandbox_ui_route(&u,g,click(u.layout.inventory_slots[0]));owned(release(&u,g,u.layout.inventory_slots[4],true));
    assert(!g->inventory.slots[base].count&&g->inventory.slots[base+4].count==20);
    /* Release after a source mutation cannot silently pick another equal stack. */
    pe_sandbox_ui_route(&u,g,click(u.layout.inventory_slots[4]));g->inventory.slots[base+4].count=19;
    owned(release(&u,g,u.layout.inventory_slots[5],false));assert(!g->inventory.slots[base+5].count&&g->inventory.slots[base+4].count==19);
    /* Quickbar remains prototype-based for stackables after high-index dragging. */
    pe_sandbox_ui_route(&u,g,click(u.layout.inventory_slots[4]));owned(release(&u,g,u.layout.quickslots[2],false));
    assert(g->quickslots[2].item_id==PE_ITEM_WOOD&&!g->quickslots[2].instance_id&&g->inventory.slots[base+4].count==19);
    pe_sandbox_ui_route(&u,g,click(pe_sandbox_ui_inventory_action(&u,0)));assert(u.split_open);
    i=input();i.advance=true;pe_sandbox_ui_route(&u,g,i);assert(g->inventory.slots[base].count==1);
    owned(pe_sandbox_ui_route(&u,g,click(pe_sandbox_ui_inventory_action(&u,1))));
    assert(g->inventory.slots[base].count==19&&!g->inventory.slots[base+4].count&&g->quickslots[2].item_id==PE_ITEM_WOOD);
    /* Cancel owns closing frame and held/release through the host pointer gate. */
    u.selected_slot=base;pe_sandbox_ui_route(&u,g,click(pe_sandbox_ui_inventory_action(&u,0)));assert(u.split_open);
    i=click(pe_sandbox_ui_split_button(&u,3));i.save=i.load=true;
    PeSandboxUIResult r=pe_sandbox_ui_route(&u,g,i);owned(r);assert(!u.split_open&&u.inventory_open);
    PeSandboxPointerGate gate={0};assert(pe_sandbox_pointer_route(&gate,&r,1));
    r=(PeSandboxUIResult){0};assert(pe_sandbox_pointer_route(&gate,&r,1)&&r.consume_world);
    r=(PeSandboxUIResult){0};assert(pe_sandbox_pointer_route(&gate,&r,0)&&r.consume_world);
    r=(PeSandboxUIResult){0};assert(!pe_sandbox_pointer_route(&gate,&r,0));
    /* A split with no category capacity leaves inventory and modal retryable. */
    for(int k=0;k<420;k++)g->inventory.slots[base+k]=(PeSlot){.item_id=PE_ITEM_WOOD,.count=2};
    PeInventory *before=malloc(sizeof *before);assert(before);*before=g->inventory;
    u.selected_slot=base;pe_sandbox_ui_route(&u,g,click(pe_sandbox_ui_inventory_action(&u,0)));assert(u.split_open);
    i=input();i.advance=true;owned(pe_sandbox_ui_route(&u,g,i));assert(u.split_open&&!memcmp(before,&g->inventory,sizeof *before));
    i=input();i.escape=true;owned(pe_sandbox_ui_route(&u,g,i));assert(!u.split_open);
    /* Stale dialog data is cancelled before a split can touch another stack. */
    pe_sandbox_ui_route(&u,g,click(pe_sandbox_ui_inventory_action(&u,0)));assert(u.split_open);
    g->inventory.slots[base].count=3;i=input();i.advance=true;
    owned(pe_sandbox_ui_route(&u,g,i));assert(!u.split_open&&g->inventory.slots[base].count==3);
    /* Mission tab has no invented capacity, and equipped cells cannot be overwritten. */
    u.inventory_tab=4;assert(pe_sandbox_ui_inventory_pages(&u,g)==1&&pe_sandbox_ui_inventory_cell(&u,g,0)==-1);
    owned(pe_sandbox_ui_route(&u,g,click(pe_sandbox_ui_inventory_action(&u,0))));assert(!u.split_open&&!u.split_armed);
    u.inventory_tab=1;assert(pe_inventory_add(&g->inventory,1527,1));int equipment=pe_inventory_category_start(&g->inventory,1);
    uint32_t instance=g->inventory.slots[equipment].instance_id;assert(instance&&pe_game_equip_slot(g,equipment));
    assert(pe_sandbox_ui_inventory_cell(&u,g,0)==-1&&pe_sandbox_ui_inventory_slot(&u,g,0)==-1);
    pe_game_unequip(g);assert(pe_sandbox_ui_inventory_slot(&u,g,0)==equipment);
    /* Moving/resizing the window retains action/dialog hit geometry. */
    pe_sandbox_ui_resize(&u,800,600);PeTrainingUIRect split=pe_sandbox_ui_split_panel(&u),button=pe_sandbox_ui_split_button(&u,2);
    assert(button.x>=split.x&&button.x+button.w<=split.x+split.w&&button.y+button.h<=split.y+split.h);
    /* Recovered footer and new dialog geometry stay inside a movable narrow
     * inventory. Exercise the actual hit route, including both page arrows. */
    static const float sizes[][2]={{640,480},{800,600},{1024,600}};
    u.inventory_tab=2;u.selected_slot=base;
    for(unsigned size=0;size<sizeof sizes/sizeof *sizes;size++){
        pe_sandbox_ui_resize(&u,sizes[size][0],sizes[size][1]);
        PeTrainingUIRect panel=u.layout.inventory,page=u.layout.inventory_page;
        for(int action=0;action<2;action++){
            PeTrainingUIRect control=pe_sandbox_ui_inventory_action(&u,action);
            assert(control.x>=panel.x&&control.y>=panel.y&&control.x+control.w<=panel.x+panel.w&&control.y+control.h<=panel.y+panel.h);
            assert(!pe_training_ui_contains(page,control.x+control.w/2,control.y+control.h/2));
        }
        u.inventory_page=0;i=click(page);i.mouse_x=page.x+138*u.layout.scale;
        owned(pe_sandbox_ui_route(&u,g,i));assert(u.inventory_page==9);
        i.mouse_x=page.x+8*u.layout.scale;owned(pe_sandbox_ui_route(&u,g,i));assert(u.inventory_page==0);
        owned(pe_sandbox_ui_route(&u,g,click(pe_sandbox_ui_inventory_action(&u,0))));assert(u.split_open);
        PeTrainingUIRect splitbox=pe_sandbox_ui_split_panel(&u);
        assert(splitbox.x>=panel.x&&splitbox.y>=panel.y&&splitbox.x+splitbox.w<=panel.x+panel.w&&splitbox.y+splitbox.h<=panel.y+panel.h);
        for(int action=0;action<4;action++){
            PeTrainingUIRect control=pe_sandbox_ui_split_button(&u,action);
            assert(control.x>=splitbox.x&&control.y>=splitbox.y&&control.x+control.w<=splitbox.x+splitbox.w&&control.y+control.h<=splitbox.y+splitbox.h);
        }
        owned(pe_sandbox_ui_route(&u,g,click(pe_sandbox_ui_split_button(&u,3))));assert(!u.split_open);
    }
    free(before);pe_game_destroy(g);free(g);puts("inventory UI: category pages, exact-slot drag/swap/merge, split/sort and modal ownership passed");return 0;
}
