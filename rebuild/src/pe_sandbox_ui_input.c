#include "pe_sandbox_ui.h"
#include "pe_adventure_medicine.h"
#include "pe_adventure_hunt.h"
#include "pe_deployable.h"
#include "pe_shield.h"
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
    memset(u,0,sizeof *u);u->batches=1;u->selected_slot=-1;u->drag_slot=-1;u->drag_quickslot=-1;
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
    return u&&(u->town_open||u->dialogue.stage!=PE_ADVENTURE_DIALOGUE_IDLE||u->building||u->free_cursor||u->camera_cursor||u->inventory_open||u->character_open||u->missions_open||u->replicator_open||u->paused||u->menu_open||u->dragging);
}
static bool panel(const PeSandboxUI *u) {
    return u->town_open||u->dialogue.stage!=PE_ADVENTURE_DIALOGUE_IDLE||u->inventory_open||u->character_open||u->missions_open||u->replicator_open||u->paused||u->menu_open;
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
    return relative(u,pe_sandbox_ui_shop_panel(u),22,90+38*(float)row,586,36);
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
    int n=0;for(int i=0;i<PE_INVENTORY_SLOTS;i++)if(item_visible(u,g,i))n++;
    return n;
}
int pe_sandbox_ui_inventory_slot(const PeSandboxUI *u,const PeGame *g,int visible) {
    if(!u||!g||visible<0||visible>=PE_UI_INVENTORY_SLOTS||u->inventory_page<0||u->inventory_page>PE_INVENTORY_SLOTS)return -1;
    int wanted=u->inventory_page*PE_UI_INVENTORY_SLOTS+visible;
    for(int i=0;i<PE_INVENTORY_SLOTS;i++)if(item_visible(u,g,i)&&wanted--==0)return i;
    return -1;
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
    if(slot<0||slot>=PE_INVENTORY_SLOTS)return false;
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
        for(int slot=0;slot<PE_INVENTORY_SLOTS;slot++){
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
    u->menu_open=u->search_focus=false;u->dragging=false;
}
bool pe_sandbox_ui_open_welcome(PeSandboxUI *u,const PeGame *g,uint32_t entity,bool eligible) {
    if(!u||!g||u->town_open||u->paused||u->building||!pe_adventure_dialogue_open(&u->dialogue,g,1,entity,eligible))return false;
    u->dialogue_result=PE_ADVENTURE_DIALOGUE_REJECTED;close_panels(u);return true;
}
bool pe_sandbox_ui_open_supplier(PeSandboxUI *u,const PeGame *g,uint32_t entity,bool eligible) {
    if(!u||!g||u->town_open||u->paused||u->building||!pe_adventure_dialogue_open(&u->dialogue,g,2,entity,eligible))return false;
    u->dialogue_result=PE_ADVENTURE_DIALOGUE_REJECTED;close_panels(u);return true;
}
bool pe_sandbox_ui_open_annot(PeSandboxUI *u,const PeGame *g,uint32_t entity,Texture2D portrait,bool eligible) {
    if(!u||!g||u->town_open||u->paused||u->building||!pe_adventure_dialogue_open(&u->dialogue,g,3,entity,eligible))return false;
    u->dialogue_result=PE_ADVENTURE_DIALOGUE_REJECTED;u->annot_portrait=portrait;close_panels(u);return true;
}
bool pe_sandbox_ui_open_town(PeSandboxUI *u,const PeGame *g,uint32_t entity,const char *name,Texture2D portrait,bool eligible) {
    if(!u||!g||!entity||!name||strlen(name)>128||!eligible||u->town_open||u->paused||u->building||u->dialogue.stage!=PE_ADVENTURE_DIALOGUE_IDLE||g->mode!=PE_GAME_ADVENTURE||!isfinite(g->hp)||g->hp<=0)return false;
    close_panels(u);u->town_open=true;u->shop_open=false;u->town_entity=entity;u->deploy_entity=0;u->deploy_get_pending=false;u->deploy_get_ticks=0;u->town_portrait=portrait;u->shop_selection=u->shop_scroll=0;u->shop_quantity=1;
    snprintf(u->town_name,sizeof u->town_name,"%s",name);return true;
}
bool pe_sandbox_ui_open_deployable(PeSandboxUI *u,const PeGame *g,uint32_t instance){
    if(!u||!g||u->town_open||u->paused||u->building||u->dialogue.stage!=PE_ADVENTURE_DIALOGUE_IDLE||
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
            if(pe_town_shop_open(g,u->town_entity)){u->shop_open=true;u->shop_selection=u->shop_scroll=0;u->shop_quantity=1;}
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
    int count=(int)pe_town_shop_count(u->town_entity),maxscroll=count>8?count-8:0;
    if(i.wheel&&hit(relative(u,p,22,90,586,304),i))u->shop_scroll+=i.wheel>0?-1:1;
    if(u->shop_scroll<0)u->shop_scroll=0;
    if(u->shop_scroll>maxscroll)u->shop_scroll=maxscroll;
    int selected=u->shop_selection;
    if(selected<u->shop_scroll)selected=u->shop_scroll;
    if(selected>=u->shop_scroll+8)selected=u->shop_scroll+7;
    if(selected>=count)selected=count-1;
    if(selected!=u->shop_selection){u->shop_selection=selected;u->shop_quantity=1;}
    if(i.left_pressed)for(int row=0;row<8&&row+u->shop_scroll<count;row++)if(hit(pe_sandbox_ui_shop_row(u,row),i)){u->shop_selection=row+u->shop_scroll;u->shop_quantity=1;}
    const PeTownOffer *offer=u->shop_selection<0?NULL:pe_town_shop_offer(u->town_entity,(size_t)u->shop_selection);
    if(!offer)return;
    unsigned stock=pe_town_shop_stock(g,u->town_entity,offer->store_id);
    if(i.left_pressed&&hit(pe_sandbox_ui_shop_button(u,0),i)&&u->shop_quantity>1)u->shop_quantity--;
    if(i.left_pressed&&hit(pe_sandbox_ui_shop_button(u,1),i)&&u->shop_quantity<stock)u->shop_quantity++;
    if(!u->shop_quantity)u->shop_quantity=1;
    if(i.advance||(i.left_pressed&&hit(pe_sandbox_ui_shop_button(u,2),i)))pe_town_shop_buy(g,u->town_entity,offer->store_id,u->shop_quantity);
}
PeSandboxUIResult pe_sandbox_ui_route(PeSandboxUI *u,PeGame *g,PeSandboxUIInput i) {
    PeSandboxUIResult r={0};
    if(!u||!g||!isfinite(i.mouse_x)||!isfinite(i.mouse_y)||!isfinite(i.wheel)||u->layout.scale<=0){r.consume_world=true;return r;}
    u->mouse_x=i.mouse_x;u->mouse_y=i.mouse_y;
    if(u->discard_input){u->discard_input=false;u->windows.dragging=0;u->dragging=u->shop_dragging=false;return (PeSandboxUIResult){.consume_world=true,.cursor_visible=pe_sandbox_ui_cursor(u),.paused=u->paused};}
    bool before=panel(u),cursor_before=pe_sandbox_ui_cursor(u);
    if(!i.focused){u->shop_dragging=false;u->windows.dragging=0;u->dragging=false;r.consume_world=true;r.cursor_visible=cursor_before;r.paused=u->paused;return r;}
    if(u->town_open){r.consume_world=true;town_route(u,g,i);goto done;}
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
        if(i.inventory){u->inventory_open=!u->inventory_open;u->missions_open=u->replicator_open=false;u->dragging=false;}
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
        if(hit(pe_sandbox_ui_build_toolbar(u),i))r.consume_world=true;
        if(i.left_pressed&&!before&&!i.escape&&!i.inventory&&!i.character&&!i.missions&&!i.replicator&&!i.tab) {
            for(int k=0;k<9;k++)if(hit(pe_sandbox_ui_build_slot(u,k),i))build_select(u,&r,k);
            for(int k=0;k<8;k++)if(hit(pe_sandbox_ui_build_tool(u,k),i)) {
                if(k==0)r.selected_item=u->build_item==PE_ITEM_DIRT?PE_ITEM_STONE:u->build_item==PE_ITEM_STONE?PE_ITEM_WOOD:PE_ITEM_DIRT;
                if(k==1)build_select(u,&r,u->build_shape==PE_B45_CUBE?4:0);
                if(k==2)r.build_brush=1;
                if(k==3)r.build_brush=4;
                if(k==4)r.rotate_build=true;
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
    if(u->inventory_open&&pointer_window==PE_UI_WINDOW_INVENTORY) {
        if(i.left_pressed&&hit(u->layout.inventory_close,i)){u->inventory_open=false;u->dragging=false;goto done;}
        if(i.left_pressed)for(int b=0;b<PE_UI_INVENTORY_TABS;b++)if(hit(u->layout.inventory_tabs[b],i)) {
            u->inventory_tab=b;u->inventory_page=0;u->dragging=false;
        }
        int pages=(pe_sandbox_ui_inventory_count(u,g)+PE_UI_INVENTORY_SLOTS-1)/PE_UI_INVENTORY_SLOTS;
        if(pages<1)pages=1;
        if(i.wheel&&hit(u->layout.inventory,i))u->inventory_page+=i.wheel>0?-1:1;
        if(i.left_pressed&&hit(u->layout.inventory_page,i)) {
            float x=(i.mouse_x-u->layout.inventory_page.x)/u->layout.scale;
            if(x<20)u->inventory_page=0;else if(x<45)u->inventory_page--;else if(x>=130)u->inventory_page=pages-1;else if(x>=100)u->inventory_page++;
        }
        if(u->inventory_page<0)u->inventory_page=0;
        if(u->inventory_page>=pages)u->inventory_page=pages-1;
        int visible=pe_training_ui_hit(u->layout.inventory_slots,PE_UI_INVENTORY_SLOTS,i.mouse_x,i.mouse_y);
        int index=pe_sandbox_ui_inventory_slot(u,g,visible);
        if(index>=0&&i.right_pressed){use_inventory_slot(u,g,index);u->dragging=false;}
        if(index>=0&&i.left_pressed) {
            const PeSlot *s=&g->inventory.slots[index];u->selected_slot=index;u->dragging=true;u->drag_slot=index;u->drag_quickslot=-1;
            u->drag_item=s->item_id;u->drag_instance=s->instance_id;u->drag_x=i.mouse_x;u->drag_y=i.mouse_y;
        }
    }
    bool covered=(u->inventory_open&&hit(u->layout.inventory,i))||
        (u->character_open&&hit(u->layout.equipment,i));
    if(u->dragging&&i.left_released) {
        int q=u->building?-1:pe_training_ui_hit(u->layout.quickslots,9,i.mouse_x,i.mouse_y),found=-1;
        for(int k=0;k<PE_INVENTORY_SLOTS;k++) {
            const PeSlot *s=&g->inventory.slots[k];
            if(s->count>0&&s->item_id==u->drag_item&&(!u->drag_instance||s->instance_id==u->drag_instance)){found=k;break;}
        }
        if(q>=0&&found>=0&&!covered) {
            PeQuickslot old=g->quickslots[q];
            if(pe_game_quickslot_assign(g,q,found)&&u->drag_quickslot>=0&&q!=u->drag_quickslot) {
                int replacement=-1;
                for(int k=0;k<PE_INVENTORY_SLOTS;k++) {
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
