#include "pe_town_interaction.h"
#include "../generated/pe_town_offers.h"
#include "../generated/pe_adventure_settlement.h"
#include <math.h>
#include <stdio.h>

static bool alive(const PeGame *g){return g&&g->mode==PE_GAME_ADVENTURE&&isfinite(g->hp)&&g->hp>0;}
size_t pe_town_shop_count(uint32_t entity){
    size_t n=0;for(size_t i=0;i<PE_TOWN_STOCK_CAPACITY;i++)if(pe_town_offers[i].entity_id==entity)n++;
    return n;
}
const PeTownOffer *pe_town_shop_offer(uint32_t entity,size_t index){
    for(size_t i=0;i<PE_TOWN_STOCK_CAPACITY;i++)if(pe_town_offers[i].entity_id==entity){if(!index)return &pe_town_offers[i];index--;}
    return NULL;
}
static int offer_index(int store){for(int i=0;i<PE_TOWN_STOCK_CAPACITY;i++)if(pe_town_offers[i].store_id==store)return i;return -1;}
bool pe_town_state_valid(const PeGame *g){
    if(!g||!isfinite(g->game_seconds)||g->game_seconds<0)return false;
    if(g->annot_complete&&(g->mode!=PE_GAME_ADVENTURE||!g->supplies_claimed||!g->arrival_complete||g->adventure_mission!=PE_ADVENTURE_SUPPLIES))return false;
    for(int i=0;i<PE_TOWN_STOCK_CAPACITY;i++){
        const PeTownStock *s=&g->town_stock[i];
        if(!isfinite(s->create_time)||s->create_time<0||s->create_time>g->game_seconds||s->remaining>pe_town_offers[i].limit)return false;
        if(!s->initialized&&(s->remaining||s->create_time!=0))return false;
        if(s->initialized&&g->mode!=PE_GAME_ADVENTURE)return false;
    }
    return true;
}
bool pe_town_shop_open(PeGame *g,uint32_t entity){
    if(!alive(g)||!pe_town_state_valid(g)||!pe_town_shop_count(entity))return false;
    for(int i=0;i<PE_TOWN_STOCK_CAPACITY;i++){
        const PeTownOffer *o=&pe_town_offers[i];if(o->entity_id!=entity)continue;
        PeTownStock *s=&g->town_stock[i];
        if(!s->initialized||g->game_seconds-s->create_time>o->refresh_seconds){s->initialized=true;s->remaining=o->limit;s->create_time=g->game_seconds;}
        else if(s->remaining)s->create_time=g->game_seconds;
    }
    return true;
}
unsigned pe_town_shop_stock(const PeGame *g,int store){
    int i=offer_index(store);return g&&i>=0&&g->town_stock[i].initialized?g->town_stock[i].remaining:0;
}
bool pe_town_shop_buy(PeGame *g,uint32_t entity,int store,unsigned count){
    int i=offer_index(store);
    if(!alive(g)||!pe_town_state_valid(g)||i<0||!count||pe_town_offers[i].entity_id!=entity)return false;
    const PeTownOffer *o=&pe_town_offers[i];PeTownStock *s=&g->town_stock[i];
    if(!s->initialized||count>s->remaining){snprintf(g->message,sizeof g->message,"Not enough stock.");return false;}
    uint64_t price=(uint64_t)o->price*count;
    if(price>g->money){snprintf(g->message,sizeof g->message,"Not enough currency.");return false;}
    PeInventory staged=g->inventory;
    if(!pe_inventory_add(&staged,o->item_id,(int)count)){snprintf(g->message,sizeof g->message,"Inventory full.");return false;}
    g->inventory=staged;g->money-=(uint32_t)price;s->remaining-=count;
    const PeItemDef *item=pe_item_def(o->item_id);
    snprintf(g->message,sizeof g->message,"Bought %u %s for %u.",count,item?item->name:"items",(unsigned)price);
    return true;
}
bool pe_adventure_annot_step(PeGame *g){
    if(!alive(g)||!g->world.source||!g->supplies_claimed||!g->arrival_complete||g->annot_complete||
       g->adventure_mission!=PE_ADVENTURE_SUPPLIES||!isfinite(g->player.x)||!isfinite(g->player.y)||!isfinite(g->player.z))return false;
    double x=(double)g->player.x+g->world.origin_x,y=(double)g->player.y+g->world.origin_y,z=(double)g->player.z+g->world.origin_z;
    if(y<0||hypot(x-pe_adventure_settlement_town4_destination[0],z-pe_adventure_settlement_town4_destination[2])>=PE_ADVENTURE_SETTLEMENT_DESTINATION_RADIUS)return false;
    g->annot_complete=true;snprintf(g->message,sizeof g->message,"Reached Annot. Ask Naoko about the delayed supplies.");return true;
}
