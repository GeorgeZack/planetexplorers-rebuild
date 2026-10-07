#include "pe_sandbox_ui.h"
#include "pe_town_trade.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(expr) do { if(!(expr)){fprintf(stderr,"shop quantity/details:%d: %s\n",__LINE__,#expr);exit(1);} } while(0)
static PeSandboxUIInput input(void){return(PeSandboxUIInput){.focused=true,.quickslot=-1};}
static PeSandboxUIInput click(PeTrainingUIRect r){PeSandboxUIInput i=input();i.left_pressed=true;i.mouse_x=r.x+r.w/2;i.mouse_y=r.y+r.h/2;return i;}
static PeSandboxUIResult route(PeSandboxUI *u,PeGame *g,PeSandboxUIInput i){PeSandboxUIResult r=pe_sandbox_ui_route(u,g,i);CHECK(r.consume_world&&!r.consume_actions&&!r.save&&!r.load&&!r.npc_interaction&&!r.camera_cycle_allowed);return r;}
static void enter(PeSandboxUI *u,PeGame *g){PeSandboxUIInput i=input();i.advance=true;route(u,g,i);}
static void typed(PeSandboxUI *u,PeGame *g,const char *text){PeSandboxUIInput i=input();i.typed=text;i.quickslot=0;i.response=1;i.save=i.load=true;route(u,g,i);}
static void tab(PeSandboxUI *u,PeGame *g,PeSandboxShopMode mode){route(u,g,click(pe_sandbox_ui_shop_tab(u,mode)));CHECK(u->shop_mode==mode);}
static PeSandboxShopRow select_item(PeSandboxUI *u,PeGame *g,int id){PeSandboxShopRow row;int found=-1;for(int k=0;k<pe_sandbox_ui_shop_count(u,g);k++)if(pe_sandbox_ui_shop_item(u,g,k,&row)&&row.item.item_id==id){found=k;break;}CHECK(found>=0);while(found<u->shop_scroll||found>=u->shop_scroll+8){PeSandboxUIInput i=click(pe_sandbox_ui_shop_row(u,0));i.left_pressed=false;i.wheel=found<u->shop_scroll?1:-1;route(u,g,i);}route(u,g,click(pe_sandbox_ui_shop_row(u,found-u->shop_scroll)));CHECK(pe_sandbox_ui_shop_item(u,g,u->shop_selection,&row));return row;}
static void edit(PeSandboxUI *u,PeGame *g){route(u,g,click(pe_sandbox_ui_shop_quantity_rect(u)));CHECK(u->shop_edit);}
static void hover(PeSandboxUI *u,PeTrainingUIRect r){u->mouse_x=r.x+r.w/2;u->mouse_y=r.y+r.h/2;}
static void inside(PeTrainingUIRect r,const PeSandboxUI *u){CHECK(r.x>=0&&r.y>=0&&r.x+r.w<=u->layout.width+.01f&&r.y+r.h<=u->layout.height+.01f);}
int main(void){
    PeGame *g=calloc(1,sizeof *g);CHECK(g&&pe_game_init_mode(g,511,PE_GAME_ADVENTURE));g->inventory=(PeInventory){.next_instance_id=g->inventory.next_instance_id,.categorized=true};g->equipped_instance_id=0;memset(g->quickslots,0,sizeof g->quickslots);
    CHECK(pe_inventory_add(&g->inventory,916,30)&&pe_inventory_add(&g->inventory,26,1)&&pe_inventory_add(&g->inventory,877,1));g->money=100000;
    PeSandboxUI u;pe_sandbox_ui_init(&u,1280,720);CHECK(pe_sandbox_ui_open_town(&u,g,20002,"Vendor",(Texture2D){0},true));PeSandboxUIInput i=input();i.response=1;route(&u,g,i);CHECK(u.shop_open);
    tab(&u,g,PE_SANDBOX_SHOP_SELL);PeSandboxShopRow row=select_item(&u,g,916);edit(&u,g);typed(&u,g,"12");CHECK(u.shop_quantity==12&&pe_inventory_count(&g->inventory,916)==30);enter(&u,g);CHECK(!u.shop_edit&&pe_inventory_count(&g->inventory,916)==30);enter(&u,g);CHECK(pe_inventory_count(&g->inventory,916)==18&&pe_town_trade_at(g,20002,0)->count==12);
    tab(&u,g,PE_SANDBOX_SHOP_BUYBACK);select_item(&u,g,916);edit(&u,g);typed(&u,g,"5");PeSandboxUIResult r=route(&u,g,click(pe_sandbox_ui_shop_button(&u,2)));CHECK(!u.shop_edit&&pe_inventory_count(&g->inventory,916)==18);PeSandboxPointerGate gate={0};CHECK(pe_sandbox_pointer_route(&gate,&r,1));r=route(&u,g,input());CHECK(pe_sandbox_pointer_route(&gate,&r,1));r=route(&u,g,input());CHECK(pe_sandbox_pointer_route(&gate,&r,0));enter(&u,g);CHECK(pe_inventory_count(&g->inventory,916)==23&&pe_town_trade_at(g,20002,0)->count==7);
    edit(&u,g);typed(&u,g,"9999999999999999999999999");CHECK(u.shop_quantity==7);i=input();i.escape=true;route(&u,g,i);CHECK(!u.shop_edit&&u.shop_quantity==1&&u.shop_open);
    edit(&u,g);typed(&u,g,"0abc");CHECK(u.shop_quantity==1);i=input();i.backspace=true;route(&u,g,i);CHECK(!u.shop_edit_text[0]&&u.shop_quantity==1);typed(&u,g,"3");i=input();i.focused=false;route(&u,g,i);CHECK(!u.shop_edit&&u.shop_quantity==1);
    /* Exact row/stock changes while editing and after Enter cannot trade a different snapshot. */
    edit(&u,g);typed(&u,g,"2");CHECK(pe_town_trade_buyback(g,20002,0,1));int owned=pe_inventory_count(&g->inventory,916);enter(&u,g);CHECK(!u.shop_edit&&u.shop_quantity==1&&pe_inventory_count(&g->inventory,916)==owned);
    select_item(&u,g,916);edit(&u,g);typed(&u,g,"2");enter(&u,g);CHECK(pe_town_trade_buyback(g,20002,0,1));owned=pe_inventory_count(&g->inventory,916);enter(&u,g);CHECK(u.shop_quantity==1&&pe_inventory_count(&g->inventory,916)==owned);
    tab(&u,g,PE_SANDBOX_SHOP_SELL);row=select_item(&u,g,26);edit(&u,g);g->inventory.slots[row.bag_slot].durability=100;enter(&u,g);CHECK(!u.shop_edit&&g->inventory.slots[row.bag_slot].item_id==26);select_item(&u,g,26);edit(&u,g);enter(&u,g);g->inventory.slots[row.bag_slot].durability=99;enter(&u,g);CHECK(g->inventory.slots[row.bag_slot].item_id==26);
    /* Buy uses the same field and preserves source money/stock checks. */
    tab(&u,g,PE_SANDBOX_SHOP_BUY);int buy_index=-1;for(int k=0;k<pe_sandbox_ui_shop_count(&u,g);k++){CHECK(pe_sandbox_ui_shop_item(&u,g,k,&row));if(row.item.count>=3){buy_index=k;break;}}CHECK(buy_index>=0);row=select_item(&u,g,row.item.item_id);owned=pe_inventory_count(&g->inventory,row.item.item_id);edit(&u,g);typed(&u,g,"3");enter(&u,g);CHECK(pe_inventory_count(&g->inventory,row.item.item_id)==owned);enter(&u,g);CHECK(pe_inventory_count(&g->inventory,row.item.item_id)==owned+3);
    /* Disappearing final rows must not retarget the same Enter to their predecessor. */
    tab(&u,g,PE_SANDBOX_SHOP_SELL);int last=pe_sandbox_ui_shop_count(&u,g)-1;while(last>=u.shop_scroll+8){i=click(pe_sandbox_ui_shop_row(&u,0));i.left_pressed=false;i.wheel=-1;route(&u,g,i);}
    route(&u,g,click(pe_sandbox_ui_shop_row(&u,last-u.shop_scroll)));CHECK(pe_sandbox_ui_shop_item(&u,g,last,&row));edit(&u,g);enter(&u,g);g->inventory.slots[row.bag_slot]=(PeSlot){0};PeInventory snapshot=g->inventory;uint32_t money=g->money;size_t buyback=pe_town_trade_count(g,20002);enter(&u,g);CHECK(!memcmp(&snapshot,&g->inventory,sizeof snapshot)&&g->money==money&&pe_town_trade_count(g,20002)==buyback&&u.shop_quantity==1);
    tab(&u,g,PE_SANDBOX_SHOP_BUY);
    /* Display values reproduce source scaling, including raw float boundary differences. */
    CHECK(pe_sandbox_ui_durability(0,false)==0&&pe_sandbox_ui_durability(100,false)==1&&pe_sandbox_ui_durability(100.01f,false)==2&&pe_sandbox_ui_durability(2369,false)==24);
    CHECK(pe_sandbox_ui_durability(NAN,false)==-1&&pe_sandbox_ui_durability(-1,true)==-1&&pe_sandbox_ui_durability(INFINITY,false)==-1);
    for(int k=0;k<10000;k++){float raw=k*.37f;CHECK(pe_sandbox_ui_durability(raw,false)==(int)ceilf(raw/100.f)&&pe_sandbox_ui_durability(raw,true)==(int)ceilf(raw*.01f));}
    char text[1024];PeSlot script={877,1,0,0};CHECK(!pe_sandbox_ui_script_known(g,877)&&pe_sandbox_ui_item_details(g,&script,false,text,sizeof text)&&strstr(text,"not learned"));const PeItemDef *d=pe_item_def(877);CHECK(d->learn_count>0);g->known_recipes[g->known_recipe_count++]=d->learn_ids[0];CHECK(pe_sandbox_ui_script_known(g,877)&&pe_sandbox_ui_item_details(g,&script,false,text,sizeof text)&&strstr(text,"already learned"));CHECK(!pe_sandbox_ui_script_known(g,26));CHECK(!pe_sandbox_ui_item_details(g,&script,false,text,2));
    /* Template _durability defaults do not establish a condition component. */
    const int no_condition[]={877,916,924,925};for(unsigned k=0;k<sizeof no_condition/sizeof *no_condition;k++){PeSlot value={no_condition[k],1,0,0};CHECK(pe_sandbox_ui_item_details(g,&value,false,text,sizeof text)&&!strstr(text,"Durability"));}
    /* New purchases show full condition through a non-owning copy; stock metadata stays literal. */
    PeSandboxShopRow fresh={0};for(int k=0;k<pe_sandbox_ui_shop_count(&u,g);k++){CHECK(pe_sandbox_ui_shop_item(&u,g,k,&fresh));const PeItemDef *f=pe_item_def(fresh.item.item_id);if(f->stack_limit==1&&pe_item_has_durability(f->id)&&f->durability_max>0&&fresh.item.count>0)break;}
    CHECK(pe_item_def(fresh.item.item_id)->durability_max>0&&fresh.item.durability==0&&!fresh.item.instance_id);
    fresh=select_item(&u,g,fresh.item.item_id);hover(&u,pe_sandbox_ui_shop_row(&u,u.shop_selection-u.shop_scroll));PeTrainingUIRect fresh_box;CHECK(pe_sandbox_ui_item_tooltip(&u,g,text,sizeof text,&fresh_box));char condition[64];int maximum=pe_sandbox_ui_durability((float)pe_item_def(fresh.item.item_id)->durability_max,false);snprintf(condition,sizeof condition,"Durability %d / %d",maximum,maximum);CHECK(strstr(text,condition));PeSandboxShopRow after;CHECK(pe_sandbox_ui_shop_item(&u,g,u.shop_selection,&after)&&after.item.durability==0&&!after.item.instance_id&&after.item.count==fresh.item.count);
    /* Shop hover uses authoritative condition-priced row and is never itself input. */
    tab(&u,g,PE_SANDBOX_SHOP_SELL);row=select_item(&u,g,26);hover(&u,pe_sandbox_ui_shop_row(&u,u.shop_selection-u.shop_scroll));PeTrainingUIRect box;CHECK(pe_sandbox_ui_item_tooltip(&u,g,text,sizeof text,&box)&&strstr(text,"Durability 1 /")&&strstr(text,"Sell unit price:")&&strstr(text,"Repair: Wood"));inside(box,&u);edit(&u,g);CHECK(!pe_sandbox_ui_item_tooltip(&u,g,text,sizeof text,&box));i=input();i.escape=true;route(&u,g,i);
    const int sizes[][2]={{1280,720},{640,480},{320,240}};for(unsigned k=0;k<3;k++){pe_sandbox_ui_resize(&u,sizes[k][0],sizes[k][1]);u.shop_offset_x=9000;u.shop_offset_y=9000;hover(&u,pe_sandbox_ui_shop_row(&u,u.shop_selection-u.shop_scroll));CHECK(pe_sandbox_ui_item_tooltip(&u,g,text,sizeof text,&box));inside(box,&u);inside(pe_sandbox_ui_shop_quantity_rect(&u),&u);}
    pe_sandbox_ui_cancel_town(&u);u.inventory_open=true;u.inventory_tab=1;u.inventory_page=0;hover(&u,u.layout.inventory_slots[0]);CHECK(pe_sandbox_ui_item_tooltip(&u,g,text,sizeof text,&box)&&strstr(text,"Durability 1 /"));inside(box,&u);
    /* Reserved repair details do not require copying ownership into a UI slot. */
    PeSlot tool=g->inventory.slots[420];u.inventory_open=false;u.repair_open=true;g->repair.item=tool;hover(&u,pe_sandbox_ui_repair_reserved(&u));CHECK(pe_sandbox_ui_item_tooltip(&u,g,text,sizeof text,&box)&&strstr(text,"Repair: Wood"));inside(box,&u);u.repair_confirm=1;CHECK(!pe_sandbox_ui_item_tooltip(&u,g,text,sizeof text,&box));g->repair=(PeCampRepair){0};u.repair_open=false;u.repair_confirm=0;
    /* Qualified live medicines own actions only, including empty and cooldown retries. */
    g->quickslots[0]=(PeQuickslot){924,0};i=input();i.quickslot=0;r=pe_sandbox_ui_route(&u,g,i);CHECK(r.consume_actions&&!r.consume_world);CHECK(pe_inventory_add(&g->inventory,924,2));g->hp=g->max_hp-300;r=pe_sandbox_ui_route(&u,g,i);CHECK(r.consume_actions&&!r.consume_world&&pe_inventory_count(&g->inventory,924)==1);r=pe_sandbox_ui_route(&u,g,i);CHECK(r.consume_actions&&!r.consume_world&&pe_inventory_count(&g->inventory,924)==1);
    pe_game_destroy(g);free(g);puts("shop quantity/details UI tests passed");return 0;
}
