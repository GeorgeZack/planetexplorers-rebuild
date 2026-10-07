/* Source whole-slot deletion and actual confirmation routing; no GPU/window. */
#include "pe_sandbox_ui.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static PeSandboxUIInput input(void){return(PeSandboxUIInput){.focused=true,.quickslot=-1,.mouse_x=100,.mouse_y=100};}
static PeSandboxUIInput click(PeTrainingUIRect r){PeSandboxUIInput i=input();i.left_pressed=true;i.mouse_x=r.x+r.w*.5f;i.mouse_y=r.y+r.h*.5f;return i;}
static void owned(PeSandboxUIResult r){assert(r.consume_world&&!r.save&&!r.load&&!r.npc_interaction&&!r.camera_cycle_allowed);}
static PeGame *fresh(void){PeGame *g=calloc(1,sizeof *g);assert(g);g->mode=PE_GAME_ADVENTURE;g->hp=g->max_hp=200;g->inventory.categorized=true;g->inventory.next_instance_id=10;return g;}
static int find(const PeGame *g,int id){for(int k=0;k<pe_inventory_capacity(&g->inventory);k++)if(g->inventory.slots[k].item_id==id)return k;return -1;}
static void rejected(PeGame *g,int slot,const PeSlot *expected){unsigned char *copy=malloc(sizeof *g);assert(copy);memcpy(copy,g,sizeof *g);assert(!pe_game_discard_slot(g,slot,expected));assert(!memcmp(copy,g,sizeof *g));free(copy);}
static void core(void){
    PeGame *g=fresh();assert(pe_inventory_item_discardable(279));assert(!pe_inventory_item_discardable(1332));assert(!pe_inventory_item_discardable(90000000));assert(!pe_inventory_item_discardable(99999999));assert(!pe_inventory_item_discardable(12345678));
    assert(pe_inventory_add(&g->inventory,279,25));int slot=find(g,279),other=-1;assert(slot==840);
    assert(pe_game_quickslot_assign(g,0,slot));assert(pe_inventory_split(&g->inventory,slot,7,&other));
    PeSlot expected=g->inventory.slots[other];uint32_t next=g->inventory.next_instance_id;
    assert(pe_game_can_discard_slot(g,other)&&pe_game_discard_slot(g,other,&expected));
    assert(!g->inventory.slots[other].count&&g->inventory.slots[slot].count==18&&g->inventory.next_instance_id==next&&g->quickslots[0].item_id==279);
    rejected(g,other,&expected);expected=g->inventory.slots[slot];g->inventory.slots[slot].count--;rejected(g,slot,&expected);g->inventory.slots[slot].count++;
    g->hp=0;rejected(g,slot,&expected);g->hp=NAN;rejected(g,slot,&expected);g->hp=200;
    rejected(g,-1,&expected);rejected(g,1680,&expected);rejected(g,slot,NULL);
    assert(pe_game_discard_slot(g,slot,&expected));assert(g->quickslots[0].item_id==279); /* existing native exhaustion policy */
    assert(pe_inventory_add(&g->inventory,12,2));slot=find(g,12);other=slot+1;
    assert(pe_game_quickslot_assign(g,1,slot)&&pe_game_quickslot_assign(g,2,other));expected=g->inventory.slots[slot];
    g->equipped_instance_id=expected.instance_id;rejected(g,slot,&expected);g->equipped_instance_id=0;
    g->shield_instance_id=expected.instance_id;rejected(g,slot,&expected);g->shield_instance_id=0;
    g->outfit.items[0].instance_id=expected.instance_id;rejected(g,slot,&expected);g->outfit.items[0].instance_id=0;
    g->deployed_count=1;g->deployed[0].instance_id=expected.instance_id;rejected(g,slot,&expected);g->deployed_count=0;
    g->inventory.slots[slot].durability--;rejected(g,slot,&expected);expected=g->inventory.slots[slot];
    PeSlot retained=g->inventory.slots[other];next=g->inventory.next_instance_id;
    assert(pe_game_discard_slot(g,slot,&expected));assert(!g->quickslots[1].item_id&&g->quickslots[2].instance_id==retained.instance_id);
    assert(!memcmp(&retained,&g->inventory.slots[other],sizeof retained)&&g->inventory.next_instance_id==next);
    expected=retained;g->inventory.slots[other].count=2;rejected(g,other,&expected);g->inventory.slots[other].count=1;
    assert(pe_game_discard_slot(g,other,&expected));free(g);
}
static void begin(PeSandboxUI *u,PeGame *g,int cell){
    if(!u->delete_armed){PeSandboxUIInput arm=click(pe_sandbox_ui_inventory_action(u,2));arm.save=arm.load=true;owned(pe_sandbox_ui_route(u,g,arm));}
    assert(u->delete_armed&&!u->delete_open&&!u->split_armed);
    PeSandboxUIInput i=click(u->layout.inventory_slots[cell]);i.save=i.load=true;
    owned(pe_sandbox_ui_route(u,g,i));assert(u->delete_open&&!u->dragging);
}
static void ui(void){
    PeGame *g=fresh();PeSandboxUI u;pe_sandbox_ui_init(&u,1280,720);u.inventory_open=true;u.inventory_tab=2;
    assert(pe_inventory_add(&g->inventory,279,25));int slot=find(g,279);begin(&u,g,0);
    PeSandboxUIInput i=input();i.quickslot=0;i.response=1;i.save=i.load=i.inventory=i.character=i.missions=i.replicator=i.tab=true;i.typed="9";i.wheel=1;
    owned(pe_sandbox_ui_route(&u,g,i));assert(u.delete_open&&u.inventory_open&&!u.character_open&&!u.replicator_open&&!u.missions_open&&g->inventory.slots[slot].count==25);
    i=click(pe_sandbox_ui_delete_button(&u,1));i.save=i.load=true;PeSandboxUIResult r=pe_sandbox_ui_route(&u,g,i);owned(r);assert(!u.delete_open&&u.delete_armed&&g->inventory.slots[slot].count==25);
    PeSandboxPointerGate gate={0};assert(pe_sandbox_pointer_route(&gate,&r,1));r=(PeSandboxUIResult){0};assert(pe_sandbox_pointer_route(&gate,&r,1)&&r.consume_world);r=(PeSandboxUIResult){0};assert(pe_sandbox_pointer_route(&gate,&r,0)&&r.consume_world);r=(PeSandboxUIResult){0};assert(!pe_sandbox_pointer_route(&gate,&r,0));
    begin(&u,g,0);g->inventory.slots[slot].count--;i=input();i.advance=true;owned(pe_sandbox_ui_route(&u,g,i));assert(!u.delete_open&&g->inventory.slots[slot].count==24);
    begin(&u,g,0);i=input();i.focused=false;i.advance=true;owned(pe_sandbox_ui_route(&u,g,i));assert(u.delete_open&&g->inventory.slots[slot].count==24);
    i=input();i.escape=true;owned(pe_sandbox_ui_route(&u,g,i));assert(!u.delete_open&&u.inventory_open);
    begin(&u,g,0);i=click(pe_sandbox_ui_delete_button(&u,0));i.save=i.load=true;owned(pe_sandbox_ui_route(&u,g,i));assert(!u.delete_open&&!g->inventory.slots[slot].count&&u.selected_slot==-1);
    i=input();i.advance=true;owned(pe_sandbox_ui_route(&u,g,i));assert(!g->inventory.slots[slot].count);
    /* Switching operation disarms deletion; source right-click never confirms. */
    assert(pe_inventory_add(&g->inventory,279,12));owned(pe_sandbox_ui_route(&u,g,click(pe_sandbox_ui_inventory_action(&u,0))));assert(!u.delete_armed&&u.split_armed);
    u.split_armed=false;u.selected_slot=-1;begin(&u,g,0);i=input();i.right_pressed=true;owned(pe_sandbox_ui_route(&u,g,i));assert(u.delete_open);i=input();i.escape=true;pe_sandbox_ui_route(&u,g,i);
    i=input();i.right_pressed=true;owned(pe_sandbox_ui_route(&u,g,i));assert(!u.delete_armed&&!u.delete_open&&g->inventory.slots[slot].count==12);
    for(int width=640;width<=1280;width+=640){pe_sandbox_ui_resize(&u,width,width*9.f/16);for(int action=0;action<3;action++){PeTrainingUIRect b=pe_sandbox_ui_inventory_action(&u,action);assert(b.x>=u.layout.inventory.x&&b.x+b.w<=u.layout.inventory.x+u.layout.inventory.w);}}
    i=input();i.escape=true;owned(pe_sandbox_ui_route(&u,g,i));assert(!u.delete_open&&!u.delete_armed&&!u.inventory_open);free(g);
}
int main(void){core();ui();puts("Inventory discard: protected/owned guards, exact whole-stack commit and modal pointer ownership passed");return 0;}
