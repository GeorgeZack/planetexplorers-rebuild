/* Real headless shop UI/core transactions; no graphics or input injection. */
#include "pe_sandbox_ui.h"
#include "pe_town_trade.h"
#include "pe_town_wallet.h"
#include "pe_shield.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static PeSandboxUIInput input(void){return (PeSandboxUIInput){.focused=true,.quickslot=-1};}
static PeSandboxUIInput click(PeTrainingUIRect r){
    PeSandboxUIInput i=input();i.left_pressed=true;i.mouse_x=r.x+r.w*.5f;i.mouse_y=r.y+r.h*.5f;return i;
}
static PeSandboxUIResult route(PeSandboxUI *u,PeGame *g,PeSandboxUIInput i){
    PeSandboxUIResult r=pe_sandbox_ui_route(u,g,i);assert(r.consume_world&&!r.npc_interaction&&!r.toggle_build);return r;
}
static int slot(const PeGame *g,int id){for(int k=0;k<PE_INVENTORY_SLOTS;k++)if(g->inventory.slots[k].item_id==id&&g->inventory.slots[k].count)return k;return -1;}
static void open(PeSandboxUI *u,PeGame *g,uint32_t entity){
    assert(pe_sandbox_ui_open_town(u,g,entity,"Vendor",(Texture2D){0},true));
    PeSandboxUIInput i=input();i.response=1;i.quickslot=0;route(u,g,i);
    assert(u->shop_open&&u->shop_mode==PE_SANDBOX_SHOP_BUY);
}
static void tab(PeSandboxUI *u,PeGame *g,PeSandboxShopMode mode){
    PeSandboxUIInput i=click(pe_sandbox_ui_shop_tab(u,mode));i.quickslot=0;i.response=1;
    route(u,g,i);assert(u->shop_mode==mode&&u->shop_scroll==0&&u->shop_selection==0&&u->shop_quantity==1);
}
static PeSandboxShopRow select_item(PeSandboxUI *u,PeGame *g,int item,uint32_t instance){
    PeSandboxShopRow row={0};int found=-1;
    for(int i=0;i<pe_sandbox_ui_shop_count(u,g);i++){
        assert(pe_sandbox_ui_shop_item(u,g,i,&row));
        if(row.item.item_id==item&&(!instance||row.item.instance_id==instance)){found=i;break;}
    }
    assert(found>=0);
    while(found<u->shop_scroll||found>=u->shop_scroll+8){
        PeSandboxUIInput i=click(pe_sandbox_ui_shop_row(u,0));i.left_pressed=false;i.wheel=found<u->shop_scroll?1:-1;route(u,g,i);
    }
    route(u,g,click(pe_sandbox_ui_shop_row(u,found-u->shop_scroll)));
    assert(u->shop_selection==found);assert(pe_sandbox_ui_shop_item(u,g,found,&row));return row;
}
static void commit(PeSandboxUI *u,PeGame *g){route(u,g,click(pe_sandbox_ui_shop_button(u,2)));}
int main(void){
    PeGame *g=calloc(1,sizeof *g);assert(g&&pe_game_init_mode(g,811,PE_GAME_ADVENTURE));
    g->inventory=(PeInventory){.next_instance_id=g->inventory.next_instance_id};
    g->equipped_instance_id=g->shield_instance_id=0;memset(g->quickslots,0,sizeof g->quickslots);
    assert(pe_inventory_add(&g->inventory,33,1)&&pe_inventory_add(&g->inventory,34,1)&&
        pe_inventory_add(&g->inventory,916,10)&&pe_inventory_add(&g->inventory,308,9)&&pe_inventory_add(&g->inventory,12,10));
    int knife=slot(g,33),shield=slot(g,34),juice=slot(g,916);
    assert(knife>=0&&shield>=0&&juice>=0);g->inventory.slots[knife].durability=pe_item_def(33)->durability_max*.5f;
    uint32_t knife_id=g->inventory.slots[knife].instance_id;float wear=g->inventory.slots[knife].durability;
    assert(pe_game_equip_slot(g,knife)&&pe_shield_equip(g,shield)&&pe_game_quickslot_assign(g,0,juice));
    PeSandboxUI ui;pe_sandbox_ui_init(&ui,1280,720);open(&ui,g,20002);tab(&ui,g,PE_SANDBOX_SHOP_SELL);
    PeSandboxShopRow row=select_item(&ui,g,33,knife_id);assert(!row.eligible&&strstr(row.reason,"Unequip"));
    assert(!pe_sandbox_ui_shop_can_trade(&ui,g));PeInventory bag=g->inventory;uint32_t money=g->money;
    commit(&ui,g);assert(!memcmp(&bag,&g->inventory,sizeof bag)&&money==g->money&&strstr(g->message,"Unequip"));
    row=select_item(&ui,g,34,0);assert(!row.eligible&&!pe_sandbox_ui_shop_can_trade(&ui,g));
    commit(&ui,g);assert(!memcmp(&bag,&g->inventory,sizeof bag)&&money==g->money);
    g->equipped_instance_id=0;row=select_item(&ui,g,33,knife_id);
    assert(row.eligible&&row.unit_price==8&&row.item.durability==wear&&pe_sandbox_ui_shop_can_trade(&ui,g));
    uint32_t vendor=0;assert(pe_town_wallet_balance(g,20002,&vendor));
    commit(&ui,g);assert(pe_town_trade_count(g,20002)==1&&slot(g,33)<0&&g->money==money+8);
    uint32_t current=0;assert(pe_town_wallet_balance(g,20002,&current)&&current==vendor-8);
    tab(&ui,g,PE_SANDBOX_SHOP_BUYBACK);row=select_item(&ui,g,33,knife_id);
    assert(row.unit_price==8&&row.item.durability==wear&&row.item.instance_id==knife_id);
    assert(pe_sandbox_ui_shop_can_trade(&ui,g));commit(&ui,g);
    assert(!pe_town_trade_count(g,20002)&&ui.shop_selection==-1&&ui.shop_quantity==1&&g->money==money);
    assert(pe_town_wallet_balance(g,20002,&current)&&current==vendor-8+14);
    assert(slot(g,33)>=0&&g->inventory.slots[slot(g,33)].instance_id==knife_id&&g->inventory.slots[slot(g,33)].durability==wear);
    /* Modal digits never use the identically numbered quickslot, including
     * empty buyback and shop close frames. */
    PeSandboxUIInput i=input();i.response=1;i.quickslot=0;i.inventory=i.save=i.load=true;
    PeSandboxUIResult result=route(&ui,g,i);assert(!result.save&&!result.load&&!ui.inventory_open&&pe_inventory_count(&g->inventory,916)==10);
    tab(&ui,g,PE_SANDBOX_SHOP_SELL);row=select_item(&ui,g,916,0);assert(row.eligible&&row.item.count==10);
    route(&ui,g,click(pe_sandbox_ui_shop_button(&ui,1)));route(&ui,g,click(pe_sandbox_ui_shop_button(&ui,1)));
    assert(ui.shop_quantity==3);money=g->money;commit(&ui,g);
    assert(pe_inventory_count(&g->inventory,916)==7&&g->money==money+row.unit_price*3&&ui.shop_quantity==1);
    tab(&ui,g,PE_SANDBOX_SHOP_BUYBACK);row=select_item(&ui,g,916,0);assert(row.item.count==3);
    route(&ui,g,click(pe_sandbox_ui_shop_button(&ui,1)));assert(ui.shop_quantity==2);commit(&ui,g);
    assert(pe_inventory_count(&g->inventory,916)==9&&pe_town_trade_at(g,20002,0)->count==1);
    /* Close/reopen preserves the actual vendor owner; another vendor cannot
     * display or buy the first vendor's transient rows. */
    i=click(pe_sandbox_ui_shop_button(&ui,4));i.quickslot=0;i.response=1;result=route(&ui,g,i);
    assert(!ui.town_open&&pe_inventory_count(&g->inventory,916)==9);
    PeSandboxPointerGate gate={0};assert(pe_sandbox_pointer_route(&gate,&result,1));
    result=(PeSandboxUIResult){0};assert(pe_sandbox_pointer_route(&gate,&result,1)&&result.consume_world);
    result=(PeSandboxUIResult){0};assert(pe_sandbox_pointer_route(&gate,&result,0)&&result.consume_world);
    result=(PeSandboxUIResult){0};assert(!pe_sandbox_pointer_route(&gate,&result,0));
    open(&ui,g,20011);tab(&ui,g,PE_SANDBOX_SHOP_BUYBACK);assert(!pe_sandbox_ui_shop_count(&ui,g));
    pe_sandbox_ui_cancel_town(&ui);open(&ui,g,20002);tab(&ui,g,PE_SANDBOX_SHOP_BUYBACK);
    assert(pe_sandbox_ui_shop_count(&ui,g)==1);row=select_item(&ui,g,916,0);assert(row.item.count==1);
    /* A real source zero-price item remains tradable with an empty purse. */
    g->money=0;assert(row.unit_price==0&&pe_sandbox_ui_shop_can_trade(&ui,g));commit(&ui,g);
    assert(pe_inventory_count(&g->inventory,916)==10&&!pe_town_trade_count(g,20002)&&g->money==0);
    tab(&ui,g,PE_SANDBOX_SHOP_SELL);select_item(&ui,g,33,knife_id);commit(&ui,g);
    tab(&ui,g,PE_SANDBOX_SHOP_BUYBACK);row=select_item(&ui,g,33,knife_id);assert(row.unit_price>0);
    g->money=0;assert(!pe_sandbox_ui_shop_can_trade(&ui,g));bag=g->inventory;commit(&ui,g);
    assert(!memcmp(&bag,&g->inventory,sizeof bag)&&pe_town_trade_count(g,20002)==1&&g->message[0]);
    /* Scroll source-independent Sell rows through the actual route, then
     * clamp after selling the last visible entry. */
    g->money=1000;tab(&ui,g,PE_SANDBOX_SHOP_SELL);int count=pe_sandbox_ui_shop_count(&ui,g);assert(count>8);
    i=click(pe_sandbox_ui_shop_row(&ui,0));i.left_pressed=false;i.wheel=-1;
    for(int k=0;k<count+5;k++)route(&ui,g,i);
    assert(ui.shop_scroll==count-8);route(&ui,g,click(pe_sandbox_ui_shop_row(&ui,7)));
    assert(ui.shop_selection==count-1&&pe_sandbox_ui_shop_can_trade(&ui,g));commit(&ui,g);
    assert(pe_sandbox_ui_shop_count(&ui,g)==count-1&&ui.shop_scroll==count-9&&ui.shop_selection==count-2);
    /* Insufficient vendor funds preserves a selected stack and every purse. */
    row=select_item(&ui,g,308,0);g->town_wallet[0].balance=0;bag=g->inventory;money=g->money;
    assert(!pe_sandbox_ui_shop_can_trade(&ui,g));commit(&ui,g);
    assert(!memcmp(&bag,&g->inventory,sizeof bag)&&g->money==money&&strstr(g->message,"afford"));
    assert(pe_inventory_add(&g->inventory,365,1));row=select_item(&ui,g,365,0);
    assert(!row.eligible&&strstr(row.reason,"not supported"));bag=g->inventory;commit(&ui,g);assert(!memcmp(&bag,&g->inventory,sizeof bag));
    /* Dragging and resize retain the same modal geometry, never trading from
     * a title press or from an advance key while the panel is dragged. */
    PeTrainingUIRect panel=pe_sandbox_ui_shop_panel(&ui);i=input();i.left_pressed=true;i.mouse_x=panel.x+100;i.mouse_y=panel.y+25;
    route(&ui,g,i);assert(ui.shop_dragging);i=input();i.mouse_x=-100;i.mouse_y=-100;i.advance=true;i.left_released=true;
    route(&ui,g,i);assert(!ui.shop_dragging&&!memcmp(&bag,&g->inventory,sizeof bag));
    pe_sandbox_ui_resize(&ui,800,600);panel=pe_sandbox_ui_shop_panel(&ui);
    assert(panel.x>=0&&panel.y>=0&&panel.x+panel.w<=800&&panel.y+panel.h<=600);
    PeSandboxShopRow unchanged={.unit_price=99},before=unchanged;
    assert(!pe_sandbox_ui_shop_item(&ui,g,-1,&unchanged)&&!memcmp(&unchanged,&before,sizeof before));
    pe_game_destroy(g);free(g);puts("Sandbox shop trade UI: PASS (no graphics or asset skips)");return 0;
}
