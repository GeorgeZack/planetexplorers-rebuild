/* Real repair UI routes and transactions, with no graphics or desktop input. */
#include "pe_sandbox_ui.h"
#include "pe_camp_repair.h"
#include "pe_settlement.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static PeSandboxUIInput input(void){return (PeSandboxUIInput){.focused=true,.quickslot=-1};}
static PeSandboxUIInput click(PeTrainingUIRect r){PeSandboxUIInput i=input();i.left_pressed=true;i.mouse_x=r.x+r.w/2;i.mouse_y=r.y+r.h/2;return i;}
static PeSandboxUIInput release(PeTrainingUIRect r){PeSandboxUIInput i=click(r);i.left_pressed=false;i.left_released=true;return i;}
static PeSandboxUIResult route(PeSandboxUI *u,PeGame *g,PeSandboxUIInput i){PeSandboxUIResult r=pe_sandbox_ui_route(u,g,i);assert(r.consume_world&&!r.npc_interaction&&!r.toggle_build&&!r.camera_cycle_allowed&&!r.save&&!r.load);return r;}
static void button(PeSandboxUI *u,PeGame *g,int b){route(u,g,click(pe_sandbox_ui_repair_button(u,b)));}
static void enter(PeSandboxUI *u,PeGame *g){PeSandboxUIInput i=input();i.advance=true;route(u,g,i);}
static void escape(PeSandboxUI *u,PeGame *g){PeSandboxUIInput i=input();i.escape=true;route(u,g,i);}
static bool same(PeSlot a,PeSlot b){return a.item_id==b.item_id&&a.count==b.count&&a.instance_id==b.instance_id&&a.durability==b.durability;}
static int find(const PeGame *g,int id){for(int k=0;k<pe_inventory_capacity(&g->inventory);k++)if(g->inventory.slots[k].item_id==id)return k;return -1;}
static void inside(PeTrainingUIRect r,float w,float h){assert(r.x>=0&&r.y>=0&&r.w>0&&r.h>0&&r.x+r.w<=w+.01f&&r.y+r.h<=h+.01f);}
int main(void){
    PeGame *g=calloc(1,sizeof *g);assert(g&&pe_game_init_mode(g,811,PE_GAME_ADVENTURE));char error[256];
    bool started=pe_settlement_start(g,error,sizeof error);if(!started)fprintf(stderr,"repair UI needs route fixture: %s\n",error);assert(started);
    g->inventory=(PeInventory){.next_instance_id=g->inventory.next_instance_id,.categorized=true};g->equipped_instance_id=g->shield_instance_id=0;memset(g->quickslots,0,sizeof g->quickslots);
    assert(pe_inventory_add(&g->inventory,26,9)&&pe_inventory_add(&g->inventory,33,1)&&pe_inventory_add(&g->inventory,279,100)&&pe_inventory_add(&g->inventory,916,2));
    for(int k=420;k<429;k++)g->inventory.slots[k].durability=pe_item_def(26)->durability_max*.5f;
    assert(pe_inventory_move(&g->inventory,428,839));g->equipped_instance_id=g->inventory.slots[421].instance_id;
    PeSandboxUI u;pe_sandbox_ui_init(&u,1280,720);assert(!pe_sandbox_ui_open_repair(&u,g,false));
    u.inventory_open=true;assert(!pe_sandbox_ui_open_repair(&u,g,true));u.inventory_open=false;u.building=true;assert(!pe_sandbox_ui_open_repair(&u,g,true));u.building=false;
    float hp=g->hp;g->hp=0;assert(!pe_sandbox_ui_open_repair(&u,g,true));g->hp=hp;assert(pe_sandbox_ui_open_repair(&u,g,true));assert(pe_sandbox_ui_cursor(&u));
    assert(pe_sandbox_ui_repair_count(g)==8&&pe_sandbox_ui_repair_slot(&u,g,0)==420);
    PeSandboxUIInput i=input();i.save=i.load=i.inventory=i.character=i.missions=i.replicator=i.tab=true;i.response=1;i.quickslot=0;route(&u,g,i);assert(u.repair_open&&!u.inventory_open&&pe_inventory_count(&g->inventory,916)==2);
    i=input();i.page_down=true;route(&u,g,i);assert(u.repair_page==1&&pe_sandbox_ui_repair_slot(&u,g,1)==839);
    i=input();i.page_up=true;route(&u,g,i);assert(u.repair_page==0);
    /* A stale exact wear snapshot cannot insert or turn Enter into Start. */
    route(&u,g,click(pe_sandbox_ui_repair_row(&u,0)));assert(u.repair_slot==420);g->inventory.slots[420].durability--;
    enter(&u,g);assert(u.repair_slot==-1&&!g->repair.item.count&&!u.repair_confirm);
    route(&u,g,release(pe_sandbox_ui_repair_reserved(&u)));assert(!g->repair.item.count);
    route(&u,g,click(pe_sandbox_ui_repair_row(&u,0)));route(&u,g,release(pe_sandbox_ui_repair_row(&u,0)));assert(!u.repair_dragging&&u.repair_slot==420);
    /* Focus loss cancels drag; release does not transfer a stale pointer. */
    route(&u,g,click(pe_sandbox_ui_repair_row(&u,0)));i=input();i.focused=false;route(&u,g,i);assert(!u.repair_dragging);route(&u,g,release(pe_sandbox_ui_repair_reserved(&u)));assert(!g->repair.item.count);
    assert(pe_game_quickslot_assign(g,8,420));PeSlot selected=g->inventory.slots[420];
    route(&u,g,click(pe_sandbox_ui_repair_row(&u,0)));route(&u,g,release(pe_sandbox_ui_repair_reserved(&u)));assert(same(g->repair.item,selected)&&!g->inventory.slots[420].count&&!g->quickslots[8].item_id);
    PeCampRepairCost cost;assert(pe_camp_repair_quote(&g->repair.item,&cost)&&cost.count==1&&cost.materials[0].item_id==279);int debit=cost.materials[0].count,wood=pe_inventory_count(&g->inventory,279);
    button(&u,g,PE_REPAIR_UI_START);assert(u.repair_confirm==1&&!g->repair.running&&pe_inventory_count(&g->inventory,279)==wood);escape(&u,g);assert(!u.repair_confirm&&u.repair_open);
    button(&u,g,PE_REPAIR_UI_START);int woodslot=find(g,279);assert(woodslot>=0);PeSlot materials=g->inventory.slots[woodslot];g->inventory.slots[woodslot]=(PeSlot){0};enter(&u,g);assert(!g->repair.running&&!u.repair_confirm&&same(g->repair.item,selected));g->inventory.slots[woodslot]=materials;
    button(&u,g,PE_REPAIR_UI_START);enter(&u,g);assert(g->repair.running&&g->repair.remaining_ticks==6000000&&pe_inventory_count(&g->inventory,279)==wood-debit);
    button(&u,g,PE_REPAIR_UI_TAKE);assert(g->repair.running&&same(g->repair.item,selected)&&pe_sandbox_ui_repair_count(g)==0);
    button(&u,g,PE_REPAIR_UI_CANCEL);assert(u.repair_confirm==2);escape(&u,g);assert(g->repair.running);button(&u,g,PE_REPAIR_UI_CANCEL);enter(&u,g);assert(!g->repair.running&&same(g->repair.item,selected)&&pe_inventory_count(&g->inventory,279)==wood-debit);
    button(&u,g,PE_REPAIR_UI_START);enter(&u,g);assert(g->repair.running&&pe_inventory_count(&g->inventory,279)==wood-2*debit);
    /* Closing and held/released pointer frames retain modal ownership. */
    PeSandboxPointerGate gate={0};PeSandboxUIResult r=route(&u,g,click(pe_sandbox_ui_repair_button(&u,PE_REPAIR_UI_CLOSE)));assert(!u.repair_open&&g->repair.running&&pe_sandbox_pointer_route(&gate,&r,1));
    r=pe_sandbox_ui_route(&u,g,input());assert(pe_sandbox_pointer_route(&gate,&r,1)&&r.consume_world);r=pe_sandbox_ui_route(&u,g,input());assert(pe_sandbox_pointer_route(&gate,&r,0)&&r.consume_world);r=pe_sandbox_ui_route(&u,g,input());assert(!pe_sandbox_pointer_route(&gate,&r,0));
    bool complete=false;assert(pe_game_repair_frame(g,10,true,&complete)&&!complete&&g->repair.remaining_ticks==5000000);assert(pe_sandbox_ui_open_repair(&u,g,true));
    button(&u,g,PE_REPAIR_UI_CANCEL);assert(u.repair_confirm==2);assert(pe_game_repair_frame(g,50,true,&complete)&&complete);enter(&u,g);assert(!u.repair_confirm&&!g->repair.running&&g->repair.item.durability==pe_item_def(26)->durability_max);
    /* Full category retains the exact reserved owner; a same-category swap is possible. */
    int occupied=0;for(int k=420;k<840;k++)occupied+=g->inventory.slots[k].count!=0;assert(pe_inventory_add(&g->inventory,34,420-occupied));PeSlot repaired=g->repair.item;
    button(&u,g,PE_REPAIR_UI_TAKE);assert(same(g->repair.item,repaired));
    u.repair_page=0;int replacement=pe_sandbox_ui_repair_slot(&u,g,0);assert(replacement>=420);PeSlot other=g->inventory.slots[replacement];route(&u,g,click(pe_sandbox_ui_repair_row(&u,0)));button(&u,g,PE_REPAIR_UI_INSERT);assert(same(g->repair.item,other)&&same(g->inventory.slots[replacement],repaired));
    int shield=find(g,34);assert(shield>=0);g->inventory.slots[shield]=(PeSlot){0};button(&u,g,PE_REPAIR_UI_TAKE);assert(!g->repair.item.count&&same(g->inventory.slots[shield],other));
    /* Moved and resized hit targets remain inside the viewport, including confirmation. */
    PeTrainingUIRect panel=pe_sandbox_ui_repair_panel(&u);i=click((PeTrainingUIRect){panel.x+20,panel.y+20,100,20});route(&u,g,i);assert(u.repair_window_dragging);i=input();i.mouse_x=9000;i.mouse_y=-9000;i.advance=true;route(&u,g,i);assert(!g->repair.item.count);i.left_released=true;route(&u,g,i);assert(!u.repair_window_dragging);
    const int sizes[][2]={{1280,720},{640,480},{320,240}};for(unsigned k=0;k<3;k++){pe_sandbox_ui_resize(&u,sizes[k][0],sizes[k][1]);inside(pe_sandbox_ui_repair_panel(&u),sizes[k][0],sizes[k][1]);for(int b=0;b<=PE_REPAIR_UI_CLOSE;b++)inside(pe_sandbox_ui_repair_button(&u,b),sizes[k][0],sizes[k][1]);for(int row=0;row<6;row++)inside(pe_sandbox_ui_repair_row(&u,row),sizes[k][0],sizes[k][1]);inside(pe_sandbox_ui_repair_confirm_panel(&u),sizes[k][0],sizes[k][1]);inside(pe_sandbox_ui_repair_confirm_button(&u,1),sizes[k][0],sizes[k][1]);}
    i=input();i.escape=i.save=i.load=true;route(&u,g,i);assert(!u.repair_open);assert(pe_sandbox_ui_open_repair(&u,g,true));pe_sandbox_ui_cancel_repair(&u);u.discard_input=true;route(&u,g,i);assert(!u.repair_open&&!u.paused);
    pe_game_destroy(g);free(g);puts("sandbox repair UI tests passed");return 0;
}
