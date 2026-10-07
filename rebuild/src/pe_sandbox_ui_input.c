#include "pe_sandbox_ui.h"
#include "pe_adventure_medicine.h"
#include "pe_adventure_hunt.h"
#include "pe_deployable.h"
#include "pe_shield.h"
#include "pe_town_trade.h"
#include "pe_town_wallet.h"
#include "pe_build_history.h"
#include "pe_camp_repair.h"
#include "../generated/pe_revival_source.h"
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool hit(PeTrainingUIRect r,PeSandboxUIInput i) {
    return pe_training_ui_contains(r,i.mouse_x,i.mouse_y);
}
static PeTrainingUIRect relative(const PeSandboxUI *u,PeTrainingUIRect p,float x,float y,float w,float h) {
    float s=u->layout.scale;
    return (PeTrainingUIRect){p.x+x*s,p.y+y*s,w*s,h*s};
}
bool pe_sandbox_pointer_release_gate(PeSandboxPointerGate *gate,bool ui_owned,unsigned buttons_down) {
    if(!gate)return true;
    if(ui_owned&&buttons_down){gate->waiting_for_release=true;gate->actions_only=false;}
    bool blocked=gate->waiting_for_release;
    if(!buttons_down){gate->waiting_for_release=false;gate->actions_only=false;}
    return blocked;
}
bool pe_sandbox_pointer_route(PeSandboxPointerGate *gate,PeSandboxUIResult *r,unsigned buttons){
    if(!gate||!r)return true;
    if((r->consume_world||r->consume_actions)&&buttons){
        if(!gate->waiting_for_release)gate->actions_only=!r->consume_world;
        else if(r->consume_world)gate->actions_only=false;
        gate->waiting_for_release=true;
    }
    bool blocked=gate->waiting_for_release;
    if(blocked){
        if(gate->actions_only)r->consume_actions=true;
        else r->consume_world=true;
        r->npc_interaction=false;r->camera_cycle_allowed=false;
    }
    if(!buttons){gate->waiting_for_release=false;gate->actions_only=false;}
    return blocked;
}
void pe_sandbox_ui_init(PeSandboxUI *u,float w,float h) {
    if(!u)return;
    memset(u,0,sizeof *u);u->batches=1;u->selected_slot=-1;u->drag_slot=-1;u->drag_quickslot=-1;u->repair_slot=-1;
    u->build_item=PE_ITEM_WOOD;u->build_shape=PE_B45_CUBE;u->build_brush=4;
    pe_training_ui_windows_layout(w,h,&u->windows,&u->layout);
}
void pe_sandbox_ui_resize(PeSandboxUI *u,float w,float h) {
    if(u)pe_training_ui_windows_layout(w,h,&u->windows,&u->layout);
}
bool pe_sandbox_ui_set_resident_markers(PeSandboxUI *u,const PeSandboxMapNpc *markers,size_t count) {
    if(!u||count>128||(count&&!markers))return false;
    for(size_t i=0;i<count;i++)if(!markers[i].entity_id||!isfinite(markers[i].position.x)||!isfinite(markers[i].position.y)||!isfinite(markers[i].position.z))return false;
    u->resident_markers=markers;u->resident_marker_count=count;return true;
}
bool pe_sandbox_ui_cursor(const PeSandboxUI *u) {
    return u&&(u->revival_open||u->repair_open||u->town_open||u->dialogue.stage!=PE_ADVENTURE_DIALOGUE_IDLE||u->building||u->free_cursor||u->camera_cursor||u->inventory_open||u->character_open||u->missions_open||u->replicator_open||u->paused||u->menu_open||u->dragging);
}
static bool panel(const PeSandboxUI *u) {
    return u->revival_open||u->repair_open||u->town_open||u->dialogue.stage!=PE_ADVENTURE_DIALOGUE_IDLE||u->inventory_open||u->character_open||u->missions_open||u->replicator_open||u->paused||u->menu_open;
}
PeTrainingUIRect pe_sandbox_ui_supplier_menu(const PeSandboxUI *u) {
    return pe_training_ui_map(&u->layout,(PeTrainingUIRect){107,140,426,486},PE_UI_CENTER);
}
PeTrainingUIRect pe_sandbox_ui_supplier_choice(const PeSandboxUI *u,int index) {
    if(u->dialogue.stage==PE_ADVENTURE_DIALOGUE_CHOICE)return pe_training_ui_map(&u->layout,(PeTrainingUIRect){395,489+20*(float)index,645,20},PE_UI_BOTTOM_CENTER);
    float step=u->dialogue.role==3?48.0f:21.0f;
    return relative(u,pe_sandbox_ui_supplier_menu(u),29,85+step*(float)index,370,step);
}
PeTrainingUIRect pe_sandbox_ui_supplier_close(const PeSandboxUI *u) {
    return relative(u,pe_sandbox_ui_supplier_menu(u),391,19,25,25);
}
PeTrainingUIRect pe_sandbox_ui_shop_panel(const PeSandboxUI *u) {
    PeTrainingUIRect p=pe_training_ui_map(&u->layout,(PeTrainingUIRect){325,100,630,520},PE_UI_CENTER);
    p.x+=u->shop_offset_x*u->layout.scale;p.y+=u->shop_offset_y*u->layout.scale;
    p.x=fmaxf(0,fminf(p.x,u->layout.width-p.w));p.y=fmaxf(0,fminf(p.y,u->layout.height-p.h));return p;
}
PeTrainingUIRect pe_sandbox_ui_shop_row(const PeSandboxUI *u,int row) {
    return relative(u,pe_sandbox_ui_shop_panel(u),22,116+34*(float)row,586,32);
}
PeTrainingUIRect pe_sandbox_ui_shop_tab(const PeSandboxUI *u,PeSandboxShopMode mode){
    return relative(u,pe_sandbox_ui_shop_panel(u),22+196*(float)mode,60,190,27);
}
int pe_sandbox_ui_shop_count(const PeSandboxUI *u,const PeGame *g){
    if(!u||!g)return 0;
    if(u->shop_mode==PE_SANDBOX_SHOP_BUY)return (int)pe_town_shop_count(u->town_entity);
    if(u->shop_mode==PE_SANDBOX_SHOP_BUYBACK)return (int)pe_town_trade_count(g,u->town_entity);
    if(u->shop_mode!=PE_SANDBOX_SHOP_SELL)return 0;
    int n=0;for(int i=0;i<pe_inventory_capacity(&g->inventory);i++)if(g->inventory.slots[i].count>0)n++;
    return n;
}
bool pe_sandbox_ui_shop_item(const PeSandboxUI *u,const PeGame *g,int index,PeSandboxShopRow *out){
    if(!u||!g||!out||index<0||index>=pe_sandbox_ui_shop_count(u,g))return false;
    PeSandboxShopRow row={.bag_slot=-1,.eligible=true,.reason=""};
    if(u->shop_mode==PE_SANDBOX_SHOP_BUY){
        const PeTownOffer *o=pe_town_shop_offer(u->town_entity,(size_t)index);if(!o)return false;
        row.item=(PeSlot){.item_id=o->item_id,.count=(int)pe_town_shop_stock(g,u->town_entity,o->store_id)};
        row.unit_price=o->price;row.store_id=o->store_id;
    }else{
        const PeSlot *slot=NULL;
        if(u->shop_mode==PE_SANDBOX_SHOP_SELL){
            for(int i=0;i<pe_inventory_capacity(&g->inventory);i++)if(g->inventory.slots[i].count>0&&index--==0){row.bag_slot=i;slot=&g->inventory.slots[i];break;}
        }else slot=pe_town_trade_at(g,u->town_entity,(size_t)index);
        if(!slot)return false;
        row.item=*slot;uint32_t base=0;
        if(!pe_town_trade_price(slot,&row.unit_price,&base)){row.eligible=false;row.reason="This item's condition is not supported for trading.";}
        if(u->shop_mode==PE_SANDBOX_SHOP_SELL&&slot->instance_id&&
           (slot->instance_id==g->equipped_instance_id||slot->instance_id==g->shield_instance_id)){
            row.eligible=false;row.reason="Unequip this item before selling.";
        }
    }
    if(row.item.count<=0){row.eligible=false;row.reason="Out of stock.";}
    *out=row;return true;
}
bool pe_sandbox_ui_shop_can_trade(const PeSandboxUI *u,const PeGame *g){
    PeSandboxShopRow row;
    if(!pe_sandbox_ui_shop_item(u,g,u?u->shop_selection:-1,&row)||!row.eligible||
       !u->shop_quantity||u->shop_quantity>(unsigned)row.item.count)return false;
    if(u->shop_mode==PE_SANDBOX_SHOP_SELL)return pe_town_trade_can_sell(g,u->town_entity,row.bag_slot,u->shop_quantity);
    if(u->shop_mode==PE_SANDBOX_SHOP_BUYBACK)return pe_town_trade_can_buyback(g,u->town_entity,(size_t)u->shop_selection,u->shop_quantity);
    uint32_t vendor=0;uint64_t total=(uint64_t)row.unit_price*u->shop_quantity;
    if(g->hp<=0||!isfinite(g->hp)||g->mode!=PE_GAME_ADVENTURE||!pe_town_state_valid(g)||
       !pe_town_wallet_balance(g,u->town_entity,&vendor)||total>g->money||total>(uint32_t)INT32_MAX-vendor)return false;
    PeInventory staged=g->inventory;return pe_inventory_add(&staged,row.item.item_id,(int)u->shop_quantity);
}
PeTrainingUIRect pe_sandbox_ui_shop_button(const PeSandboxUI *u,int b) {
    PeTrainingUIRect p=pe_sandbox_ui_shop_panel(u);
    if(b==0)return relative(u,p,24,408,32,30);
    if(b==1)return relative(u,p,122,408,32,30);
    if(b==2)return relative(u,p,190,408,190,30);
    if(b==3)return relative(u,p,445,408,160,30);
    return relative(u,p,591,19,25,25);
}
void pe_sandbox_ui_cancel_town(PeSandboxUI *u){if(u){u->town_open=u->shop_open=u->shop_dragging=false;u->town_entity=u->deploy_entity=0;u->deploy_get_pending=false;u->deploy_get_ticks=0;u->town_name[0]=0;u->town_portrait=(Texture2D){0};}}
static bool item_visible(const PeSandboxUI *u,const PeGame *g,int i) {
    const PeSlot *s=&g->inventory.slots[i];const PeItemDef *d=pe_item_def(s->item_id);
    return s->count>0&&d&&d->tab==u->inventory_tab&&
        !(s->instance_id&&(s->instance_id==g->equipped_instance_id||s->instance_id==g->shield_instance_id));
}
int pe_sandbox_ui_inventory_count(const PeSandboxUI *u,const PeGame *g) {
    if(!u||!g)return 0;
    int n=0;for(int i=0;i<pe_inventory_capacity(&g->inventory);i++)if(item_visible(u,g,i))n++;
    return n;
}
int pe_sandbox_ui_inventory_pages(const PeSandboxUI *u,const PeGame *g) {
    if(!u||!g)return 1;
    int n=g->inventory.categorized?pe_inventory_category_capacity(&g->inventory,u->inventory_tab):pe_sandbox_ui_inventory_count(u,g);
    return n>0?(n+PE_UI_INVENTORY_SLOTS-1)/PE_UI_INVENTORY_SLOTS:1;
}
int pe_sandbox_ui_inventory_cell(const PeSandboxUI *u,const PeGame *g,int visible) {
    if(!u||!g||visible<0||visible>=PE_UI_INVENTORY_SLOTS||u->inventory_page<0||u->inventory_page>=pe_sandbox_ui_inventory_pages(u,g))return -1;
    int wanted=u->inventory_page*PE_UI_INVENTORY_SLOTS+visible;
    if(g->inventory.categorized){
        int start=pe_inventory_category_start(&g->inventory,u->inventory_tab);
        if(start<0||wanted>=pe_inventory_category_capacity(&g->inventory,u->inventory_tab))return -1;
        int index=start+wanted;const PeSlot *slot=&g->inventory.slots[index];
        if(slot->count&&!item_visible(u,g,index))return -1;
        return index;
    }
    for(int i=0;i<pe_inventory_capacity(&g->inventory);i++)if(item_visible(u,g,i)&&wanted--==0)return i;
    return -1;
}
int pe_sandbox_ui_inventory_slot(const PeSandboxUI *u,const PeGame *g,int visible) {
    int index=pe_sandbox_ui_inventory_cell(u,g,visible);
    return index>=0&&g->inventory.slots[index].count>0?index:-1;
}
PeTrainingUIRect pe_sandbox_ui_inventory_action(const PeSandboxUI *u,int action){
    return relative(u,u->layout.inventory,153+70*(float)action,529,64,25);
}
PeTrainingUIRect pe_sandbox_ui_delete_panel(const PeSandboxUI *u){return relative(u,u->layout.inventory,24,185,328,166);}
PeTrainingUIRect pe_sandbox_ui_delete_button(const PeSandboxUI *u,int button){
    return relative(u,pe_sandbox_ui_delete_panel(u),30+140*(float)button,119,128,30);
}
PeTrainingUIRect pe_sandbox_ui_split_panel(const PeSandboxUI *u){
    return relative(u,u->layout.inventory,24,185,328,166);
}
PeTrainingUIRect pe_sandbox_ui_split_button(const PeSandboxUI *u,int button){
    PeTrainingUIRect p=pe_sandbox_ui_split_panel(u);
    if(button<2)return relative(u,p,54+184*(float)button,65,36,30);
    return relative(u,p,30+140*(float)(button-2),119,128,30);
}
static bool same_slot(PeSlot a,PeSlot b){
    return a.item_id==b.item_id&&a.count==b.count&&a.instance_id==b.instance_id&&a.durability==b.durability;
}
static void delete_begin(PeSandboxUI *u,PeGame *g,int index){
    u->dragging=false;
    if(!pe_game_can_discard_slot(g,index)){
        snprintf(g->message,sizeof g->message,"This item cannot be discarded. Unequip equipment first.");return;
    }
    u->delete_slot=index;u->delete_source=g->inventory.slots[index];u->delete_open=true;
}
static void delete_route(PeSandboxUI *u,PeGame *g,PeSandboxUIInput i){
    if(!u->inventory_open||!pe_inventory_slot_valid(&g->inventory,u->delete_slot)||
       !same_slot(u->delete_source,g->inventory.slots[u->delete_slot])||!pe_game_can_discard_slot(g,u->delete_slot)){
        u->delete_open=false;snprintf(g->message,sizeof g->message,"Discard cancelled: the selected item changed.");return;
    }
    if(i.escape||(i.left_pressed&&hit(pe_sandbox_ui_delete_button(u,1),i))){u->delete_open=false;return;}
    if(i.advance||(i.left_pressed&&hit(pe_sandbox_ui_delete_button(u,0),i))){
        if(pe_game_discard_slot(g,u->delete_slot,&u->delete_source)){
            u->delete_open=false;u->selected_slot=-1;
            snprintf(g->message,sizeof g->message,"Item discarded.");
        }else snprintf(g->message,sizeof g->message,"Cannot discard the selected item.");
    }
}
static void split_begin(PeSandboxUI *u,const PeGame *g,int index){
    if(!pe_inventory_slot_valid(&g->inventory,index))return;
    const PeSlot *s=&g->inventory.slots[index];
    if(s->count<2||s->instance_id)return;
    u->split_slot=index;u->split_source=*s;u->split_count=1;u->split_open=true;
    u->split_armed=u->split_edit_started=u->dragging=false;
}
static void split_route(PeSandboxUI *u,PeGame *g,PeSandboxUIInput i){
    if(!u->inventory_open||!pe_inventory_slot_valid(&g->inventory,u->split_slot)||
       !same_slot(u->split_source,g->inventory.slots[u->split_slot])){u->split_open=false;return;}
    if(i.escape||(i.left_pressed&&hit(pe_sandbox_ui_split_button(u,3),i))){u->split_open=false;return;}
    int maximum=u->split_source.count-1;
    if(i.backspace){u->split_count/=10;u->split_edit_started=true;}
    if(i.typed)for(const unsigned char *p=(const unsigned char *)i.typed;*p;p++)if(*p>='0'&&*p<='9'){
        if(!u->split_edit_started){u->split_count=0;u->split_edit_started=true;}
        int64_t entered=(int64_t)u->split_count*10+(*p-'0');u->split_count=entered>maximum?maximum:(int)entered;
        if(u->split_count>maximum)u->split_count=maximum;
    }
    if((i.left_pressed&&hit(pe_sandbox_ui_split_button(u,0),i))||i.wheel<0)u->split_count--;
    if((i.left_pressed&&hit(pe_sandbox_ui_split_button(u,1),i))||i.wheel>0)u->split_count++;
    if(u->split_count<0)u->split_count=0;
    if(u->split_count>maximum)u->split_count=maximum;
    if(i.advance||(i.left_pressed&&hit(pe_sandbox_ui_split_button(u,2),i))){
        int destination=-1;
        if(u->split_count>0&&pe_inventory_split(&g->inventory,u->split_slot,u->split_count,&destination)){
            u->selected_slot=destination;u->split_open=false;
            snprintf(g->message,sizeof g->message,"Stack split: %d items.",u->split_count);
        }else snprintf(g->message,sizeof g->message,"Cannot split: choose a quantity and free a slot in this category.");
    }
}
static bool matches(const char *name,const char *query) {
    if(!*query)return true;
    for(;*name;name++) {
        const char *a=name,*b=query;
        while(*a&&*b&&tolower((unsigned char)*a)==tolower((unsigned char)*b)){a++;b++;}
        if(!*b)return true;
    }
    return false;
}
int pe_sandbox_ui_recipe_count(const PeSandboxUI *u,const PeGame *g) {
    if(!u||!g)return 0;
    int n=0;
    for(size_t i=0;i<pe_recipe_count;i++) {
        const PeItemDef *d=pe_item_def(pe_recipes[i].output_id);
        if(d&&pe_game_recipe_known(g,pe_recipes[i].id)&&matches(d->name,u->search))n++;
    }
    return n;
}
int pe_sandbox_ui_recipe_at(const PeSandboxUI *u,const PeGame *g,int row) {
    if(!u||!g||row<0||row>=6||u->recipe_scroll<0||u->recipe_scroll>(int)pe_recipe_count)return 0;
    int n=u->recipe_scroll+row;
    for(size_t i=0;i<pe_recipe_count;i++) {
        const PeItemDef *d=pe_item_def(pe_recipes[i].output_id);
        if(d&&pe_game_recipe_known(g,pe_recipes[i].id)&&matches(d->name,u->search)&&n--==0)return pe_recipes[i].id;
    }
    return 0;
}
PeTrainingUIRect pe_sandbox_ui_recipe_row(const PeSandboxUI *u,int row) {
    return relative(u,u->layout.replicator_recipes,0,(float)row*58,200,54);
}
PeTrainingUIRect pe_sandbox_ui_quantity(const PeSandboxUI *u,int b) {
    static const float x[]={322,434,485,566};
    return relative(u,u->layout.replicator,x[b>=0&&b<4?b:0],400,b<2?28:56,28);
}
PeTrainingUIRect pe_sandbox_ui_pause_button(const PeSandboxUI *u,int b) {
    return pe_training_ui_map(&u->layout,(PeTrainingUIRect){485,280+48*(float)b,310,38},PE_UI_CENTER);
}
PeTrainingUIRect pe_sandbox_ui_revival_button(const PeSandboxUI *u,int b) {
    if(!u||b<0||b>1)return (PeTrainingUIRect){0};
    return pe_training_ui_map(&u->layout,(PeTrainingUIRect){455,395+48*(float)b,370,38},PE_UI_CENTER);
}
PeTrainingUIRect pe_sandbox_ui_cancel_button(const PeSandboxUI *u) {
    return relative(u,u->layout.replicator,665,392,90,32);
}
PeTrainingUIRect pe_sandbox_ui_search(const PeSandboxUI *u) {
    return relative(u,u->layout.replicator,378,36,400,30);
}
PeTrainingUIRect pe_sandbox_ui_build_toolbar(const PeSandboxUI *u) {
    return pe_training_ui_map(&u->layout,(PeTrainingUIRect){333,599,614,120},PE_UI_BOTTOM_CENTER);
}
PeTrainingUIRect pe_sandbox_ui_build_slot(const PeSandboxUI *u,int index) {
    if(index<0||index>=9)return (PeTrainingUIRect){0};
    return pe_training_ui_map(&u->layout,(PeTrainingUIRect){365+58*(float)index,664,48,48},PE_UI_BOTTOM_CENTER);
}
PeTrainingUIRect pe_sandbox_ui_build_tool(const PeSandboxUI *u,int index) {
    static const float x[]={365,425,515,575,635,695,775,870};
    if(index<0||index>=8)return (PeTrainingUIRect){0};
    return pe_training_ui_map(&u->layout,(PeTrainingUIRect){x[index],602,index==7?70:48,46},PE_UI_BOTTOM_CENTER);
}
static void build_select(const PeSandboxUI *u,PeSandboxUIResult *r,int index) {
    static const int items[]={0,PE_ITEM_DIRT,PE_ITEM_STONE,PE_ITEM_WOOD,0,PE_ITEM_FENCE,PE_ITEM_LADDER};
    if(index==0)r->build_shape=PE_B45_CUBE;
    else if(index==4)r->build_shape=PE_B45_RAMP;
    else if(index>=0&&index<7)r->selected_item=items[index];
    if(r->build_shape&&(u->build_item==PE_ITEM_FENCE||u->build_item==PE_ITEM_LADDER))r->selected_item=PE_ITEM_WOOD;
}
static int maximum_batches(const PeGame *g,const PeRecipeDef *r) {
    if(!r||r->ingredient_count<1)return 1;
    int maximum=9999;
    for(int i=0;i<r->ingredient_count;i++) {
        int cost=r->ingredients[i].count;
        if(cost<=0)return 1;
        int available=pe_inventory_count(&g->inventory,r->ingredients[i].item_id)/cost;
        if(available<maximum)maximum=available;
    }
    return maximum>0?maximum:1;
}
static bool use_inventory_slot(const PeSandboxUI *u,PeGame *g,int slot) {
    if(!pe_inventory_slot_valid(&g->inventory,slot))return false;
    int item=g->inventory.slots[slot].item_id;
    if((item==1||item==PE_SHIELD_ITEM)&&!u->starter_pair_available){
        snprintf(g->message,sizeof g->message,"Sword and shield gameplay is not available in this build yet.");
        return false;
    }
    if(item==PE_SHIELD_ITEM)return pe_shield_equip(g,slot);
    return pe_game_use_slot(g,slot);
}
static void quick_use(const PeSandboxUI *u,PeSandboxUIResult *r,PeGame *g,int i) {
    if(i<0||i>=9)return;
    const PeItemDef *d=pe_item_def(g->quickslots[i].item_id);
    /* Fence/ladder are source tab0 nonstack placeables, not tab2 resources. */
    if(d&&(d->tab==2||d->id==PE_ITEM_FENCE||d->id==PE_ITEM_LADDER)&&
       pe_inventory_count(&g->inventory,d->id)>0)r->selected_item=d->id;
    else if(d&&(d->id==1||d->id==PE_SHIELD_ITEM)){
        PeQuickslot q=g->quickslots[i];
        for(int slot=0;slot<pe_inventory_capacity(&g->inventory);slot++){
            const PeSlot *item=&g->inventory.slots[slot];
            if(item->count&&item->item_id==q.item_id&&item->instance_id==q.instance_id){use_inventory_slot(u,g,slot);break;}
        }
    }else pe_game_quickslot_use(g,i);
    /* A cooldown/empty-stack retry owns the key too, but must not interrupt
       locomotion, camera capture, drawn equipment or enemy clocks. */
    if(d&&(d->id==916||d->id==925))r->consume_actions=true;
    else r->consume_world=true;
}
static void close_panels(PeSandboxUI *u) {
    u->inventory_open=u->character_open=u->missions_open=u->replicator_open=false;
    u->menu_open=u->search_focus=false;u->dragging=false;u->windows.dragging=0;u->split_open=u->split_armed=false;u->delete_open=u->delete_armed=false;
}
bool pe_sandbox_ui_open_welcome(PeSandboxUI *u,const PeGame *g,uint32_t entity,bool eligible) {
    if(!u||!g||u->town_open||u->repair_open||u->paused||u->building||!pe_adventure_dialogue_open(&u->dialogue,g,1,entity,eligible))return false;
    u->dialogue_result=PE_ADVENTURE_DIALOGUE_REJECTED;close_panels(u);return true;
}
bool pe_sandbox_ui_open_supplier(PeSandboxUI *u,const PeGame *g,uint32_t entity,bool eligible) {
    if(!u||!g||u->town_open||u->repair_open||u->paused||u->building||!pe_adventure_dialogue_open(&u->dialogue,g,2,entity,eligible))return false;
    u->dialogue_result=PE_ADVENTURE_DIALOGUE_REJECTED;close_panels(u);return true;
}
bool pe_sandbox_ui_open_annot(PeSandboxUI *u,const PeGame *g,uint32_t entity,Texture2D portrait,bool eligible) {
    if(!u||!g||u->town_open||u->repair_open||u->paused||u->building||!pe_adventure_dialogue_open(&u->dialogue,g,3,entity,eligible))return false;
    u->dialogue_result=PE_ADVENTURE_DIALOGUE_REJECTED;u->annot_portrait=portrait;close_panels(u);return true;
}
bool pe_sandbox_ui_open_town(PeSandboxUI *u,const PeGame *g,uint32_t entity,const char *name,Texture2D portrait,bool eligible) {
    if(!u||!g||!entity||!name||strlen(name)>128||!eligible||u->town_open||u->repair_open||u->paused||u->building||u->dialogue.stage!=PE_ADVENTURE_DIALOGUE_IDLE||g->mode!=PE_GAME_ADVENTURE||!isfinite(g->hp)||g->hp<=0)return false;
    close_panels(u);u->town_open=true;u->shop_open=false;u->town_entity=entity;u->deploy_entity=0;u->deploy_get_pending=false;u->deploy_get_ticks=0;u->town_portrait=portrait;u->shop_selection=u->shop_scroll=0;u->shop_quantity=1;
    snprintf(u->town_name,sizeof u->town_name,"%s",name);return true;
}
bool pe_sandbox_ui_open_deployable(PeSandboxUI *u,const PeGame *g,uint32_t instance){
    if(!u||!g||u->town_open||u->repair_open||u->paused||u->building||u->dialogue.stage!=PE_ADVENTURE_DIALOGUE_IDLE||
       g->mode!=PE_GAME_ADVENTURE||g->hp<=0||!pe_deployable_in_range(g,instance))return false;
    close_panels(u);u->town_open=true;u->shop_open=false;u->town_entity=0;u->deploy_entity=instance;u->deploy_get_pending=false;u->deploy_get_ticks=0;
    u->town_portrait=(Texture2D){0};snprintf(u->town_name,sizeof u->town_name,"Torch");return true;
}
int pe_sandbox_ui_dialogue_choice_index(const PeSandboxUI *u,const PeGame *g,int row) {
    if(!u||!g||row<0||row>=9)return -1;
    for(unsigned source=0;source<2;source++)
        if(pe_adventure_dialogue_choice(&u->dialogue,g,source)&&row--==0)return (int)source;
    return -1;
}
static void town_route(PeSandboxUI *u,PeGame *g,PeSandboxUIInput i) {
    if(i.escape||g->mode!=PE_GAME_ADVENTURE||!isfinite(g->hp)||g->hp<=0){pe_sandbox_ui_cancel_town(u);return;}
    if(u->deploy_entity){
        if(!pe_deployable_in_range(g,u->deploy_entity)){pe_sandbox_ui_cancel_town(u);return;}
        if(i.left_pressed&&hit(pe_sandbox_ui_supplier_close(u),i)){pe_sandbox_ui_cancel_town(u);return;}
        if(u->deploy_get_pending){
            /* UIItemOp hides its command list while the timer runs. A click
               starts the clock; neither holding nor repeated clicks drive it.
               Cancellation/range checks above precede the commit boundary. */
            if(!u->paused&&isfinite(i.frame_dt)&&i.frame_dt>0){
                /* UTimer.Update truncates each float dt to 100,000 ticks/s.
                   Bound before conversion so even huge finite dt is safe. */
                float ticks=i.frame_dt*100000.0f;
                uint32_t remaining=200001u-u->deploy_get_ticks;
                u->deploy_get_ticks+=ticks>=(float)remaining?remaining:(uint32_t)ticks;
            }
            if(u->deploy_get_ticks>200000u){
                pe_deployable_get(g,u->deploy_entity);
                /* The source panel closes even when CanAdd rejects the list.
                   Reopen the still-existing Torch to retry with room. */
                pe_sandbox_ui_cancel_town(u);
            }
            return;
        }
        if(i.advance||i.response==1||(i.left_pressed&&hit(pe_sandbox_ui_supplier_choice(u,0),i))){
            u->deploy_get_pending=true;u->deploy_get_ticks=0;
        }else if(i.response==2||(i.left_pressed&&hit(pe_sandbox_ui_supplier_choice(u,1),i)))pe_deployable_turn(g,u->deploy_entity);
        else if(i.response==3||(i.left_pressed&&hit(pe_sandbox_ui_supplier_choice(u,2),i)))pe_sandbox_ui_cancel_town(u);
        return;
    }
    if(!u->shop_open){
        bool vendor=pe_town_shop_count(u->town_entity)>0;
        if(i.left_pressed&&hit(pe_sandbox_ui_supplier_close(u),i))pe_sandbox_ui_cancel_town(u);
        else if(vendor&&(i.advance||i.response==1||(i.left_pressed&&hit(pe_sandbox_ui_supplier_choice(u,0),i)))){
            if(pe_town_shop_open(g,u->town_entity)){u->shop_open=true;u->shop_mode=PE_SANDBOX_SHOP_BUY;u->shop_selection=u->shop_scroll=0;u->shop_quantity=1;}
        }else if((!vendor&&i.advance)||i.response==(unsigned)(vendor?2:1)||(i.left_pressed&&hit(pe_sandbox_ui_supplier_choice(u,vendor?1:0),i)))pe_sandbox_ui_cancel_town(u);
        return;
    }
    PeTrainingUIRect p=pe_sandbox_ui_shop_panel(u);
    if(u->shop_dragging){
        PeTrainingUIRect base=pe_training_ui_map(&u->layout,(PeTrainingUIRect){325,100,630,520},PE_UI_CENTER);
        float x=fmaxf(0,fminf(i.mouse_x-u->shop_grab_x,u->layout.width-p.w));
        float y=fmaxf(0,fminf(i.mouse_y-u->shop_grab_y,u->layout.height-p.h));
        u->shop_offset_x=(x-base.x)/u->layout.scale;u->shop_offset_y=(y-base.y)/u->layout.scale;
        if(i.left_released)u->shop_dragging=false;
        return;
    }
    if(i.left_pressed&&hit(pe_sandbox_ui_shop_button(u,4),i)){pe_sandbox_ui_cancel_town(u);return;}
    if(i.left_pressed&&hit(relative(u,p,10,10,570,46),i)){u->shop_dragging=true;u->shop_grab_x=i.mouse_x-p.x;u->shop_grab_y=i.mouse_y-p.y;return;}
    if(i.left_pressed&&hit(pe_sandbox_ui_shop_button(u,3),i)){u->shop_open=false;return;}
    if(i.left_pressed)for(int tab=0;tab<3;tab++)if(hit(pe_sandbox_ui_shop_tab(u,(PeSandboxShopMode)tab),i)){
        u->shop_mode=(PeSandboxShopMode)tab;u->shop_selection=u->shop_scroll=0;u->shop_quantity=1;return;
    }
    int count=pe_sandbox_ui_shop_count(u,g),maxscroll=count>8?count-8:0;
    if(i.wheel&&hit(relative(u,p,22,116,586,272),i))u->shop_scroll+=i.wheel>0?-1:1;
    if(u->shop_scroll<0)u->shop_scroll=0;
    if(u->shop_scroll>maxscroll)u->shop_scroll=maxscroll;
    int selected=u->shop_selection;
    if(selected<u->shop_scroll)selected=u->shop_scroll;
    if(selected>=u->shop_scroll+8)selected=u->shop_scroll+7;
    if(selected>=count)selected=count-1;
    if(selected!=u->shop_selection){u->shop_selection=selected;u->shop_quantity=1;}
    if(i.left_pressed)for(int row=0;row<8&&row+u->shop_scroll<count;row++)if(hit(pe_sandbox_ui_shop_row(u,row),i)){u->shop_selection=row+u->shop_scroll;u->shop_quantity=1;}
    PeSandboxShopRow selected_row;
    if(!pe_sandbox_ui_shop_item(u,g,u->shop_selection,&selected_row))return;
    unsigned stock=(unsigned)selected_row.item.count;
    if(i.left_pressed&&hit(pe_sandbox_ui_shop_button(u,0),i)&&u->shop_quantity>1)u->shop_quantity--;
    if(i.left_pressed&&hit(pe_sandbox_ui_shop_button(u,1),i)&&u->shop_quantity<stock)u->shop_quantity++;
    if(!u->shop_quantity)u->shop_quantity=1;
    if(stock&&u->shop_quantity>stock)u->shop_quantity=stock;
    if(i.advance||(i.left_pressed&&hit(pe_sandbox_ui_shop_button(u,2),i))){
        bool committed=false;
        if(!selected_row.eligible)snprintf(g->message,sizeof g->message,"%s",selected_row.reason);
        else if(u->shop_mode==PE_SANDBOX_SHOP_SELL)committed=pe_town_trade_sell(g,u->town_entity,selected_row.bag_slot,u->shop_quantity);
        else if(u->shop_mode==PE_SANDBOX_SHOP_BUYBACK)committed=pe_town_trade_buyback(g,u->town_entity,(size_t)u->shop_selection,u->shop_quantity);
        else committed=pe_town_shop_buy(g,u->town_entity,selected_row.store_id,u->shop_quantity);
        if(committed){
            count=pe_sandbox_ui_shop_count(u,g);maxscroll=count>8?count-8:0;
            if(u->shop_scroll>maxscroll)u->shop_scroll=maxscroll;
            if(u->shop_selection>=count)u->shop_selection=count-1;
            u->shop_quantity=1;
        }
    }
}

PeTrainingUIRect pe_sandbox_ui_repair_panel(const PeSandboxUI *u){
    PeTrainingUIRect p=pe_training_ui_map(&u->layout,(PeTrainingUIRect){325,100,630,520},PE_UI_CENTER);
    p.x+=u->repair_offset_x*u->layout.scale;p.y+=u->repair_offset_y*u->layout.scale;
    p.x=fmaxf(0,fminf(p.x,u->layout.width-p.w));p.y=fmaxf(0,fminf(p.y,u->layout.height-p.h));return p;
}
PeTrainingUIRect pe_sandbox_ui_repair_row(const PeSandboxUI *u,int row){return relative(u,pe_sandbox_ui_repair_panel(u),22,90+46*(float)row,280,42);}
PeTrainingUIRect pe_sandbox_ui_repair_reserved(const PeSandboxUI *u){return relative(u,pe_sandbox_ui_repair_panel(u),430,90,64,64);}
PeTrainingUIRect pe_sandbox_ui_repair_button(const PeSandboxUI *u,int button){
    PeTrainingUIRect p=pe_sandbox_ui_repair_panel(u);
    switch(button){
    case PE_REPAIR_UI_INSERT:return relative(u,p,22,422,280,32);
    case PE_REPAIR_UI_START:return relative(u,p,320,376,136,32);
    case PE_REPAIR_UI_CANCEL:return relative(u,p,468,376,136,32);
    case PE_REPAIR_UI_TAKE:return relative(u,p,320,422,284,32);
    case PE_REPAIR_UI_PREVIOUS:return relative(u,p,22,376,70,32);
    case PE_REPAIR_UI_NEXT:return relative(u,p,232,376,70,32);
    default:return relative(u,p,591,19,25,25);
    }
}
PeTrainingUIRect pe_sandbox_ui_repair_confirm_panel(const PeSandboxUI *u){return relative(u,pe_sandbox_ui_repair_panel(u),105,158,420,190);}
PeTrainingUIRect pe_sandbox_ui_repair_confirm_button(const PeSandboxUI *u,int button){return relative(u,pe_sandbox_ui_repair_confirm_panel(u),24+190*(float)button,138,180,32);}
static bool repair_candidate(const PeGame *g,int slot){
    return pe_game_repair_candidate(g,slot);
}
int pe_sandbox_ui_repair_count(const PeGame *g){
    if(!g||g->repair.running)return 0;
    int count=0;for(int k=0;k<pe_inventory_capacity(&g->inventory);k++)if(repair_candidate(g,k))count++;
    return count;
}
int pe_sandbox_ui_repair_slot(const PeSandboxUI *u,const PeGame *g,int row){
    if(!u||!g||row<0||row>=PE_REPAIR_UI_ROWS||u->repair_page<0||u->repair_page>PE_INVENTORY_MAX_SLOTS/PE_REPAIR_UI_ROWS)return -1;
    int skip=u->repair_page*PE_REPAIR_UI_ROWS+row;
    for(int k=0;k<pe_inventory_capacity(&g->inventory);k++)if(repair_candidate(g,k)&&skip--==0)return k;
    return -1;
}
void pe_sandbox_ui_cancel_repair(PeSandboxUI *u){
    if(!u)return;
    u->repair_open=u->repair_dragging=u->repair_window_dragging=false;u->repair_confirm=0;u->repair_slot=-1;
    u->repair_selected=u->repair_expected=(PeSlot){0};u->repair_notice[0]=0;
}
bool pe_sandbox_ui_open_repair(PeSandboxUI *u,const PeGame *g,bool eligible){
    if(!u||!g||!eligible||panel(u)||u->building||!isfinite(g->hp)||g->hp<=0||g->mode!=PE_GAME_ADVENTURE||
       pe_revival_frozen(&g->revival)||!pe_game_repair_state_valid(g))return false;
    close_panels(u);pe_sandbox_ui_cancel_repair(u);u->repair_open=true;return true;
}
static void repair_notice(PeSandboxUI *u,const char *message){snprintf(u->repair_notice,sizeof u->repair_notice,"%s",message);}
static bool repair_selection(const PeSandboxUI *u,const PeGame *g){
    return pe_inventory_slot_valid(&g->inventory,u->repair_slot)&&same_slot(u->repair_selected,g->inventory.slots[u->repair_slot])&&repair_candidate(g,u->repair_slot);
}
static void repair_insert(PeSandboxUI *u,PeGame *g){
    if(repair_selection(u,g)&&pe_game_repair_insert(g,u->repair_slot,&u->repair_selected)){
        u->repair_slot=-1;u->repair_selected=(PeSlot){0};repair_notice(u,"Item placed in the machine.");
    }else repair_notice(u,"Cannot insert: the item changed or is still equipped.");
    u->repair_dragging=false;
}
static void repair_route(PeSandboxUI *u,PeGame *g,PeSandboxUIInput i){
    if(!isfinite(g->hp)||g->hp<=0||g->mode!=PE_GAME_ADVENTURE||!pe_game_repair_state_valid(g)){
        pe_sandbox_ui_cancel_repair(u);return;
    }
    if(u->repair_confirm){
        if(!same_slot(g->repair.item,u->repair_expected)||(u->repair_confirm==1&&g->repair.running)||(u->repair_confirm==2&&!g->repair.running)){
            u->repair_confirm=0;repair_notice(u,"The repair state changed. Review it before continuing.");return;
        }
        if(i.escape||(i.left_pressed&&hit(pe_sandbox_ui_repair_confirm_button(u,1),i))){u->repair_confirm=0;return;}
        if(i.advance||(i.left_pressed&&hit(pe_sandbox_ui_repair_confirm_button(u,0),i))){
            bool start=u->repair_confirm==1;
            bool ok=start?pe_game_repair_start(g):pe_game_repair_cancel(g);
            u->repair_confirm=0;
            if(ok)repair_notice(u,start?"Repair started. You can close this window.":"Repair cancelled. Materials were not refunded.");
            else repair_notice(u,start?"Cannot start: check the required materials and item.":"The repair could not be cancelled.");
        }
        return;
    }
    if(i.escape||(i.left_pressed&&hit(pe_sandbox_ui_repair_button(u,PE_REPAIR_UI_CLOSE),i))){pe_sandbox_ui_cancel_repair(u);return;}
    PeTrainingUIRect p=pe_sandbox_ui_repair_panel(u);
    if(u->repair_window_dragging){
        PeTrainingUIRect base=pe_training_ui_map(&u->layout,(PeTrainingUIRect){325,100,630,520},PE_UI_CENTER);
        float x=fmaxf(0,fminf(i.mouse_x-u->repair_grab_x,u->layout.width-p.w));
        float y=fmaxf(0,fminf(i.mouse_y-u->repair_grab_y,u->layout.height-p.h));
        u->repair_offset_x=(x-base.x)/u->layout.scale;u->repair_offset_y=(y-base.y)/u->layout.scale;
        if(i.left_released)u->repair_window_dragging=false;
        return;
    }
    if(i.left_pressed&&hit(relative(u,p,10,10,570,46),i)){
        u->repair_window_dragging=true;u->repair_dragging=false;u->repair_grab_x=i.mouse_x-p.x;u->repair_grab_y=i.mouse_y-p.y;return;
    }
    if(u->repair_slot>=0&&!repair_selection(u,g)){
        u->repair_slot=-1;u->repair_selected=(PeSlot){0};u->repair_dragging=false;
        repair_notice(u,"The selected item changed. Select it again.");
        /* Never reinterpret this same Enter/drop as another transaction. */
        return;
    }
    if(u->repair_dragging&&i.left_released){
        if(hit(pe_sandbox_ui_repair_reserved(u),i))repair_insert(u,g);
        else u->repair_dragging=false;
        return;
    }
    int count=pe_sandbox_ui_repair_count(g),pages=count?(count+PE_REPAIR_UI_ROWS-1)/PE_REPAIR_UI_ROWS:1;
    int page=u->repair_page;
    if(i.page_up||(i.left_pressed&&hit(pe_sandbox_ui_repair_button(u,PE_REPAIR_UI_PREVIOUS),i)))page--;
    if(i.page_down||(i.left_pressed&&hit(pe_sandbox_ui_repair_button(u,PE_REPAIR_UI_NEXT),i)))page++;
    if(i.wheel&&hit(relative(u,p,22,90,280,276),i))page+=i.wheel>0?-1:1;
    if(page<0)page=0;
    if(page>=pages)page=pages-1;
    if(page!=u->repair_page){u->repair_page=page;u->repair_slot=-1;u->repair_selected=(PeSlot){0};u->repair_dragging=false;return;}
    if(i.left_pressed)for(int row=0;row<PE_REPAIR_UI_ROWS;row++)if(hit(pe_sandbox_ui_repair_row(u,row),i)){
        int slot=pe_sandbox_ui_repair_slot(u,g,row);
        if(slot>=0){u->repair_slot=slot;u->repair_selected=g->inventory.slots[slot];u->repair_dragging=true;u->repair_notice[0]=0;}
        return;
    }
    bool insert=(i.advance&&u->repair_slot>=0)||(i.left_pressed&&hit(pe_sandbox_ui_repair_button(u,PE_REPAIR_UI_INSERT),i));
    if(insert){repair_insert(u,g);return;}
    bool start=(i.advance&&u->repair_slot<0)||(i.left_pressed&&hit(pe_sandbox_ui_repair_button(u,PE_REPAIR_UI_START),i));
    bool cancel=i.left_pressed&&hit(pe_sandbox_ui_repair_button(u,PE_REPAIR_UI_CANCEL),i);
    if(start||cancel){
        u->repair_dragging=false;
        if((start&&pe_game_repair_can_start(g))||(cancel&&g->repair.running)){
            u->repair_confirm=start?1:2;u->repair_expected=g->repair.item;
        }else if(start)repair_notice(u,"Place a damaged item in the machine and bring the required materials.");
        return;
    }
    if(i.left_pressed&&hit(pe_sandbox_ui_repair_button(u,PE_REPAIR_UI_TAKE),i)){
        if(pe_game_repair_take(g,NULL)){u->repair_slot=-1;u->repair_selected=(PeSlot){0};u->repair_dragging=false;repair_notice(u,"Item returned to your bag.");}
        else repair_notice(u,g->repair.running?"Cancel the running repair before taking the item.":"Cannot take the item. Make room in its bag category.");
    }
}

PeSandboxUIResult pe_sandbox_ui_route(PeSandboxUI *u,PeGame *g,PeSandboxUIInput i) {
    PeSandboxUIResult r={0};
    if(!u||!g||!isfinite(i.mouse_x)||!isfinite(i.mouse_y)||!isfinite(i.wheel)||u->layout.scale<=0){r.consume_world=true;return r;}
    u->revival_open=g->mode==PE_GAME_ADVENTURE&&(g->hp<=0||pe_revival_frozen(&g->revival));
    if(u->revival_open){
        close_panels(u);pe_sandbox_ui_cancel_town(u);pe_sandbox_ui_cancel_repair(u);pe_adventure_dialogue_cancel(&u->dialogue);
        u->windows.dragging=0;u->building=false;
    }
    u->mouse_x=i.mouse_x;u->mouse_y=i.mouse_y;
    if(u->discard_input){u->discard_input=false;u->windows.dragging=0;u->dragging=u->shop_dragging=u->repair_dragging=u->repair_window_dragging=false;return (PeSandboxUIResult){.consume_world=true,.cursor_visible=pe_sandbox_ui_cursor(u),.paused=u->paused};}
    bool before=panel(u),cursor_before=pe_sandbox_ui_cursor(u);
    if(!i.focused){u->repair_dragging=u->repair_window_dragging=false;u->shop_dragging=false;u->windows.dragging=0;u->dragging=false;r.consume_world=true;r.cursor_visible=cursor_before;r.paused=u->paused;return r;}
    bool history_owned=u->building&&(u->build_history_keys_owned||i.build_history_chord);
    u->build_history_keys_owned=history_owned&&i.build_history_keys_down;
    if(history_owned)r.consume_actions=true;
    if(u->revival_open&&!u->paused){
        r.consume_world=true;
        if(i.escape)u->paused=true;
        else{
            bool request=i.response==1||i.advance||(i.left_pressed&&hit(pe_sandbox_ui_revival_button(u,0),i));
            r.load=i.load||i.response==2||(i.left_pressed&&hit(pe_sandbox_ui_revival_button(u,1),i));
            r.save=i.save;
            r.revive=request&&!r.load&&!r.save&&g->hp==0&&pe_inventory_count(&g->inventory,pe_revival_source.item_id)>0;
        }
        goto done;
    }
    if(u->repair_open){r.consume_world=true;repair_route(u,g,i);goto done;}
    if(u->town_open){r.consume_world=true;town_route(u,g,i);goto done;}
    if(u->delete_open){r.consume_world=true;delete_route(u,g,i);goto done;}
    if(u->split_open){r.consume_world=true;split_route(u,g,i);goto done;}
    if(u->delete_armed&&i.right_pressed){u->delete_armed=false;r.consume_world=true;goto done;}
    unsigned visible=(u->inventory_open?1u<<PE_UI_WINDOW_INVENTORY:0)|(u->character_open?1u<<PE_UI_WINDOW_CHARACTER:0)|
        (u->missions_open?1u<<PE_UI_WINDOW_MISSION:0)|(u->replicator_open?1u<<PE_UI_WINDOW_REPLICATOR:0);
    int pointer_window=pe_training_ui_window_at(&u->layout,visible,i.mouse_x,i.mouse_y);
    bool drag_cancel=!i.focused||i.escape||u->paused||u->menu_open||u->dragging||u->dialogue.stage!=PE_ADVENTURE_DIALOGUE_IDLE||
        (u->windows.dragging&&!(visible&(1u<<(u->windows.dragging-1))));
    if(pe_training_ui_window_drag(&u->windows,&u->layout,pointer_window,i.mouse_x,i.mouse_y,
       i.left_pressed,i.left_released,drag_cancel)){r.consume_world=true;goto done;}
    if(u->dialogue.stage!=PE_ADVENTURE_DIALOGUE_IDLE) {
        r.consume_world=true;
        bool offer=pe_adventure_dialogue_current(&u->dialogue,g)!=NULL;
        bool empty_menu=u->dialogue.stage==PE_ADVENTURE_DIALOGUE_MENU&&g->mode==PE_GAME_ADVENTURE&&g->hp>0&&
            (g->welcome_page<PE_ADVENTURE_WELCOME_PAGES||g->adventure_mission==PE_ADVENTURE_ARRIVAL||g->adventure_mission==PE_ADVENTURE_SUPPLIES);
        if(i.escape||(!offer&&!empty_menu))pe_adventure_dialogue_cancel(&u->dialogue);
        else if(u->dialogue.stage==PE_ADVENTURE_DIALOGUE_CHOICE) {
            int selected=i.advance?0:i.response>=1&&i.response<=9?(int)i.response-1:-1;
            if(i.left_pressed)for(int row=0;row<9;row++)
                if(pe_sandbox_ui_dialogue_choice_index(u,g,row)>=0&&hit(pe_sandbox_ui_supplier_choice(u,row),i)){selected=row;break;}
            int source=pe_sandbox_ui_dialogue_choice_index(u,g,selected);
            if(source>=0)u->dialogue_result=pe_adventure_dialogue_choose_medicine(&u->dialogue,g,source==0);
        } else if(u->dialogue.stage==PE_ADVENTURE_DIALOGUE_MENU) {
            if(i.left_pressed&&hit(pe_sandbox_ui_supplier_close(u),i))pe_adventure_dialogue_cancel(&u->dialogue);
            else if(offer&&(i.advance||i.response==1||(i.left_pressed&&hit(pe_sandbox_ui_supplier_choice(u,0),i))))pe_adventure_dialogue_choose_supply(&u->dialogue,g);
            else if((!offer&&i.advance)||i.response==(unsigned)(offer?2:1)||(i.left_pressed&&hit(pe_sandbox_ui_supplier_choice(u,offer?1:0),i)))pe_adventure_dialogue_cancel(&u->dialogue);
        } else if(i.advance||i.response==1||(i.left_pressed&&(hit(u->layout.dialogue_next,i)||hit(u->layout.dialogue,i))))u->dialogue_result=pe_adventure_dialogue_advance(&u->dialogue,g);
        goto done;
    }
    if(i.escape) {
        if(u->dragging)u->dragging=false;
        else if(u->paused)u->paused=false;
        else if(before)close_panels(u);
        else u->paused=true;
    } else if(!u->paused) {
        if(i.inventory){u->inventory_open=!u->inventory_open;u->missions_open=u->replicator_open=false;u->dragging=false;u->split_armed=u->delete_armed=false;}
        if(i.character){u->character_open=!u->character_open;u->missions_open=u->replicator_open=false;}
        if(i.missions){bool open=!u->missions_open;close_panels(u);u->missions_open=open;}
        if(i.replicator){bool open=!u->replicator_open;close_panels(u);u->replicator_open=open;}
        if(i.tab)u->free_cursor=!u->free_cursor;
        if(!u->search_focus){r.save=i.save;r.load=i.load;}
        if(!before&&!panel(u)&&!i.escape&&!i.inventory&&!i.character&&!i.missions&&!i.replicator&&i.quickslot>=0){if(u->building){build_select(u,&r,i.quickslot);r.consume_world=true;}else quick_use(u,&r,g,i.quickslot);}
    }
    bool cursor=pe_sandbox_ui_cursor(u);
    /* A closing/toggling frame is consumed as well: held clicks cannot leak. */
    r.consume_world=r.consume_world||before||panel(u)||i.escape||i.inventory||i.character||i.missions||i.replicator||i.tab;
    if(u->paused) {
        if(i.left_pressed)for(int b=0;b<4;b++)if(hit(pe_sandbox_ui_pause_button(u,b),i)) {
            if(b==0)u->paused=false;
            if(b==1)r.save=true;
            if(b==2)r.load=true;
            if(b==3)r.return_main=true;
        }
        goto done;
    }
    if(cursor&&!u->building&&hit(u->layout.hud,i))r.consume_actions=true;
    if(cursor&&(hit(u->layout.minimap,i)||hit(u->layout.menu_button,i)))r.consume_world=true;
    if(i.left_pressed&&cursor&&hit(u->layout.menu_button,i)){u->menu_open=!u->menu_open;r.consume_world=true;}
    if(u->menu_open&&i.left_pressed)for(int b=0;b<PE_UI_MENU_ITEMS;b++)if(hit(u->layout.menu_items[b],i)) {
        close_panels(u);
        if(b==0)u->inventory_open=true;
        if(b==1)u->character_open=true;
        if(b==2)u->replicator_open=true;
        if(b==3)u->missions_open=true;
        if(b==4)u->paused=true;
        r.consume_world=true;goto done;
    }
    if(u->menu_open)goto done;
    if(u->building&&!panel(u)&&!u->dragging) {
        if(!r.consume_world&&(i.undo_build||i.redo_build)){
            r.undo_build=i.undo_build;r.redo_build=!i.undo_build&&i.redo_build;
            r.consume_actions=true;
        }
        if(hit(pe_sandbox_ui_build_toolbar(u),i))r.consume_world=true;
        if(i.left_pressed&&!before&&!i.escape&&!i.inventory&&!i.character&&!i.missions&&!i.replicator&&!i.tab) {
            for(int k=0;k<9;k++)if(hit(pe_sandbox_ui_build_slot(u,k),i))build_select(u,&r,k);
            for(int k=0;k<8;k++)if(hit(pe_sandbox_ui_build_tool(u,k),i)) {
                if(k==0)r.selected_item=u->build_item==PE_ITEM_DIRT?PE_ITEM_STONE:u->build_item==PE_ITEM_STONE?PE_ITEM_WOOD:PE_ITEM_DIRT;
                if(k==1)build_select(u,&r,u->build_shape==PE_B45_CUBE?4:0);
                if(k==2)r.build_brush=1;
                if(k==3)r.build_brush=4;
                if(k==4)r.rotate_build=true;
                if(k==5&&pe_build_history_can_undo(g))r.undo_build=true;
                if(k==6&&pe_build_history_can_redo(g))r.redo_build=true;
                if(k==7)r.toggle_build=true;
            }
        }
        goto done;
    }
    if(u->missions_open) {
        if(i.left_pressed&&hit(u->layout.mission_close,i))u->missions_open=false;
        goto done;
    }
    if(u->replicator_open) {
        if(i.left_pressed&&hit(u->layout.replicator_close,i)){u->replicator_open=false;u->search_focus=false;goto done;}
        if(i.left_pressed)u->search_focus=hit(pe_sandbox_ui_search(u),i);
        if(u->search_focus) {
            size_t n=strlen(u->search);
            if(i.backspace&&n)u->search[--n]=0;
            if(i.typed)for(const unsigned char *p=(const unsigned char *)i.typed;*p&&n+1<sizeof u->search;p++)if(*p>=32&&*p<127)u->search[n++]=(char)*p;
            u->search[n]=0;
            if(i.backspace||(i.typed&&*i.typed))u->recipe_scroll=0;
        }
        int maxscroll=pe_sandbox_ui_recipe_count(u,g)-6;if(maxscroll<0)maxscroll=0;
        if(hit(u->layout.replicator_recipes,i)&&i.wheel)u->recipe_scroll+=i.wheel>0?-1:1;
        if(u->recipe_scroll<0)u->recipe_scroll=0;
        if(u->recipe_scroll>maxscroll)u->recipe_scroll=maxscroll;
        if(i.left_pressed) {
            for(int row=0;row<6;row++)if(hit(pe_sandbox_ui_recipe_row(u,row),i)) {
                int id=pe_sandbox_ui_recipe_at(u,g,row);if(id){u->recipe_id=id;u->batches=1;}
            }
            const PeRecipeDef *recipe=pe_recipe_def(u->recipe_id);
            for(int b=0;b<4;b++)if(hit(pe_sandbox_ui_quantity(u,b),i)) {
                if(b==0&&u->batches>1)u->batches--;
                if(b==1&&u->batches<9999)u->batches++;
                if(b==2)u->batches=1;
                if(b==3)u->batches=maximum_batches(g,recipe);
            }
            if(hit(u->layout.replicator_craft,i))pe_game_start_craft(g,u->recipe_id,u->batches);
            if(hit(pe_sandbox_ui_cancel_button(u),i))pe_game_cancel_craft(g);
        }
        goto done;
    }
    if(u->character_open&&pointer_window==PE_UI_WINDOW_CHARACTER) {
        if(i.left_pressed&&hit(u->layout.equipment_close,i))u->character_open=false;
        if(i.right_pressed&&hit(u->layout.equipment_weapon,i))pe_game_unequip(g);
        if(i.right_pressed&&hit(u->layout.equipment_slots[9],i))pe_shield_unequip(g);
    }
    if(u->inventory_open){
        int pages=pe_sandbox_ui_inventory_pages(u,g);
        if(i.page_up)u->inventory_page--;
        if(i.page_down)u->inventory_page++;
        if(u->inventory_page<0)u->inventory_page=0;
        if(u->inventory_page>=pages)u->inventory_page=pages-1;
    }
    if(u->inventory_open&&pointer_window==PE_UI_WINDOW_INVENTORY) {
        if(i.left_pressed&&hit(u->layout.inventory_close,i)){u->inventory_open=false;u->dragging=u->split_armed=u->delete_armed=false;goto done;}
        if(i.left_pressed)for(int b=0;b<PE_UI_INVENTORY_TABS;b++)if(hit(u->layout.inventory_tabs[b],i)) {
            u->inventory_tab=b;u->inventory_page=0;u->dragging=u->split_armed=u->delete_armed=false;
        }
        int pages=pe_sandbox_ui_inventory_pages(u,g);
        if(u->inventory_tab>=0&&u->inventory_tab<4&&i.left_pressed&&hit(pe_sandbox_ui_inventory_action(u,0),i)){
            u->dragging=u->delete_armed=false;u->split_armed=!u->split_armed;
            if(u->split_armed&&pe_inventory_slot_valid(&g->inventory,u->selected_slot)&&item_visible(u,g,u->selected_slot))split_begin(u,g,u->selected_slot);
            goto done;
        }
        if(u->inventory_tab>=0&&u->inventory_tab<4&&i.left_pressed&&hit(pe_sandbox_ui_inventory_action(u,1),i)){
            u->dragging=u->split_armed=u->delete_armed=false;
            if(pe_inventory_sort(&g->inventory,u->inventory_tab)){u->selected_slot=-1;snprintf(g->message,sizeof g->message,"Inventory category sorted.");}
            goto done;
        }
        if(u->inventory_tab>=0&&u->inventory_tab<4&&i.left_pressed&&hit(pe_sandbox_ui_inventory_action(u,2),i)){
            u->dragging=u->split_armed=false;u->delete_armed=!u->delete_armed;r.save=r.load=false;goto done;
        }
        if(i.wheel&&hit(u->layout.inventory,i))u->inventory_page+=i.wheel>0?-1:1;
        if(i.left_pressed&&hit(u->layout.inventory_page,i)) {
            float x=(i.mouse_x-u->layout.inventory_page.x)/u->layout.scale;
            if(x<20)u->inventory_page=0;else if(x<45)u->inventory_page--;else if(x>=130)u->inventory_page=pages-1;else if(x>=100)u->inventory_page++;
        }
        if(u->inventory_page<0)u->inventory_page=0;
        if(u->inventory_page>=pages)u->inventory_page=pages-1;
        int visible=pe_training_ui_hit(u->layout.inventory_slots,PE_UI_INVENTORY_SLOTS,i.mouse_x,i.mouse_y);
        int index=pe_sandbox_ui_inventory_slot(u,g,visible);
        if(index>=0&&u->delete_armed&&(i.left_pressed||i.right_pressed)){
            if(i.left_pressed)delete_begin(u,g,index);
            r.save=r.load=false;
            goto done;
        }
        if(index>=0&&u->split_armed&&(i.left_pressed||i.right_pressed)){split_begin(u,g,index);goto done;}
        if(index>=0&&i.right_pressed){use_inventory_slot(u,g,index);u->dragging=false;}
        if(index>=0&&i.left_pressed) {
            const PeSlot *s=&g->inventory.slots[index];u->selected_slot=index;u->dragging=true;u->drag_slot=index;u->drag_quickslot=-1;
            u->drag_source=*s;u->drag_item=s->item_id;u->drag_instance=s->instance_id;u->drag_x=i.mouse_x;u->drag_y=i.mouse_y;
        }
    }
    bool covered=(u->inventory_open&&hit(u->layout.inventory,i))||
        (u->character_open&&hit(u->layout.equipment,i));
    if(u->dragging&&i.left_released) {
        int q=u->building?-1:pe_training_ui_hit(u->layout.quickslots,9,i.mouse_x,i.mouse_y),found=-1;
        if(u->drag_slot>=0){
            if(u->drag_instance){
                for(int k=0;k<pe_inventory_capacity(&g->inventory);k++)if(same_slot(g->inventory.slots[k],u->drag_source)){found=k;break;}
            }else if(pe_inventory_slot_valid(&g->inventory,u->drag_slot)&&same_slot(g->inventory.slots[u->drag_slot],u->drag_source))found=u->drag_slot;
        }else for(int k=0;k<pe_inventory_capacity(&g->inventory);k++) {
            const PeSlot *s=&g->inventory.slots[k];
            if(s->count>0&&s->item_id==u->drag_item&&(!u->drag_instance||s->instance_id==u->drag_instance)){found=k;break;}
        }
        if(found>=0&&u->drag_slot>=0&&u->inventory_open&&pointer_window==PE_UI_WINDOW_INVENTORY){
            int cell=pe_sandbox_ui_inventory_cell(u,g,pe_training_ui_hit(u->layout.inventory_slots,PE_UI_INVENTORY_SLOTS,i.mouse_x,i.mouse_y));
            if(cell>=0&&cell!=found){
                bool moved=i.merge_stacks?pe_inventory_merge(&g->inventory,found,cell):pe_inventory_move(&g->inventory,found,cell);
                if(moved)u->selected_slot=cell;
            }
        }
        if(q>=0&&found>=0&&!covered) {
            PeQuickslot old=g->quickslots[q];
            if(pe_game_quickslot_assign(g,q,found)&&u->drag_quickslot>=0&&q!=u->drag_quickslot) {
                int replacement=-1;
                for(int k=0;k<pe_inventory_capacity(&g->inventory);k++) {
                    const PeSlot *s=&g->inventory.slots[k];
                    if(s->count>0&&s->item_id==old.item_id&&s->instance_id==old.instance_id){replacement=k;break;}
                }
                pe_game_quickslot_assign(g,u->drag_quickslot,replacement);
            }
        }
        u->dragging=false;r.consume_world=true;
    } else if(cursor&&!u->dragging&&!covered&&!u->building) {
        int q=pe_training_ui_hit(u->layout.quickslots,9,i.mouse_x,i.mouse_y);
        /* UIMainMidCtrl: left selects/drags, right uses. Moving a shortcut
           never moves or consumes the underlying owned inventory instance. */
        if(i.left_pressed&&q>=0&&g->quickslots[q].item_id) {
            u->dragging=true;u->drag_quickslot=q;u->drag_slot=-1;
            u->drag_item=g->quickslots[q].item_id;u->drag_instance=g->quickslots[q].instance_id;
            u->drag_x=i.mouse_x;u->drag_y=i.mouse_y;
        }
        if(i.right_pressed&&q>=0)quick_use(u,&r,g,q);
        if(i.left_pressed&&hit(u->layout.build_button,i)){r.toggle_build=true;close_panels(u);}
    }
 done:
    r.npc_interaction=!before&&!panel(u)&&!u->building&&!u->dragging&&
        !i.escape&&!i.inventory&&!i.character&&!i.missions&&!i.replicator&&!i.tab&&
        !i.save&&!i.load&&i.quickslot<0&&!r.toggle_build&&
        !hit(u->layout.hud,i)&&!hit(u->layout.minimap,i)&&!hit(u->layout.menu_button,i);
    if(r.load)u->map_valid=false;
    r.cursor_visible=pe_sandbox_ui_cursor(u);r.paused=u->paused;
    r.consume_world=r.consume_world||panel(u)||u->dragging;
    /* Tab blocks gameplay, but alone must not block the keyboard camera mode
     * command. All real UI activity, including closing frames, still wins. */
    r.camera_cycle_allowed=!r.consume_world&&!r.consume_actions&&!r.save&&!r.load;
    r.consume_world=r.consume_world||(!u->building&&u->free_cursor);
    return r;
}

int pe_sandbox_ui_hunt_mission_id(const PeGame *g){
    if(!g||g->mode!=PE_GAME_ADVENTURE)return 0;
    switch(g->hunt_state){
    case PE_HUNT_TRAVEL:return 9075;
    case PE_HUNT_ENCOUNTER_PENDING:return 9126;
    case PE_HUNT_RETURN_PENDING:case PE_HUNT_RETURN_TALK1:case PE_HUNT_RETURN_TALK2:case PE_HUNT_COMPLETE:return 9127;
    default:return 0;
    }
}
const char *pe_sandbox_ui_mission_title(const PeGame *g) {
    if(!g||g->mode!=PE_GAME_ADVENTURE||g->welcome_page<PE_ADVENTURE_WELCOME_PAGES)return "";
    if(g->hunt_state)return pe_adventure_mission_title(pe_sandbox_ui_hunt_mission_id(g));
    if(g->medicine_state==PE_MEDICINE_ACTIVE)return pe_adventure_mission_title(9072);
    if(g->annot_complete)return "";
    if(g->arrival_complete&&!g->supplies_claimed)return "";
    return pe_adventure_mission_title(g->supplies_claimed?PE_ADVENTURE_SUPPLIES:PE_ADVENTURE_ARRIVAL);
}
bool pe_sandbox_ui_hunt_goal(const PeSandboxUI *u,const PeGame *g,char *out,size_t capacity){
    if(!u||!g||!out||!capacity||g->mode!=PE_GAME_ADVENTURE||g->welcome_page<PE_ADVENTURE_WELCOME_PAGES)return false;
    char value[320];int length;
    if(g->hunt_state==PE_HUNT_TRAVEL)length=snprintf(value,sizeof value,"Travel to the hunting area at P: %.0f, %.0f.",g->hunt_destination.x,g->hunt_destination.z);
    else if(g->hunt_state==PE_HUNT_ENCOUNTER_PENDING)length=snprintf(value,sizeof value,"Kill Lepus hares (%u/4).%s",g->hunt_kills,u->hunt_encounter_available?"":" Creature encounter unavailable.");
    else if(g->hunt_state==PE_HUNT_RETURN_PENDING){PeVec3 target;
        if(!pe_adventure_hunt_return_reference(&target))return false;
        length=snprintf(value,sizeof value,"Lepus hares: 4/4. Return to town at P: %.0f, %.0f.",target.x,target.z);
    }else if(g->hunt_state==PE_HUNT_RETURN_TALK1||g->hunt_state==PE_HUNT_RETURN_TALK2)
        length=snprintf(value,sizeof value,"Rewards received. Finish reporting to Naoko.");
    else if(g->hunt_state==PE_HUNT_COMPLETE)length=snprintf(value,sizeof value,"Bug Hunt complete. Town Defense is not available yet.");
    else return false;
    if(length<0||(size_t)length>=sizeof value||(size_t)length>=capacity)return false;
    memcpy(out,value,(size_t)length+1);return true;
}
