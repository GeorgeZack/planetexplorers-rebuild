#include "pe_town_trade.h"
#include "pe_town_interaction.h"
#include "pe_town_wallet.h"
#include "pe_deployable.h"
#include "pe_shield.h"
#include "../generated/pe_town_trade_prices.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int vendor_index(uint32_t entity){
    for(unsigned i=0;i<PE_TOWN_WALLET_CAPACITY;i++){
        const PeTownWalletSource *source=pe_town_wallet_source_at(i);
        if(source&&source->entity_id==entity)return (int)i;
    }
    return -1;
}
static bool empty_slot(const PeSlot *s){return !s->item_id&&!s->count&&!s->instance_id&&s->durability==0;}
static bool slot_valid(const PeSlot *s){
    if(!s)return false;
    const PeItemDef *d=pe_item_def(s->item_id);
    if(!d||s->count<1||s->count>d->stack_limit||!isfinite(s->durability))return false;
    if(d->stack_limit==1)return s->instance_id&&s->durability>=0&&s->durability<=d->durability_max;
    return !s->instance_id&&s->durability==0;
}
bool pe_town_trade_price(const PeSlot *s,uint32_t *unit,uint32_t *base){
    if(!slot_valid(s))return false;
    const PeTownTradePrice *source=NULL;
    for(size_t i=0;i<PE_TOWN_TRADE_PRICE_COUNT;i++)
        if(pe_town_trade_prices[i].item_id==s->item_id){source=&pe_town_trade_prices[i];break;}
    if(!source||source->base_currency>INT32_MAX)return false;
    float factor=1;
    if(source->factor==PE_TOWN_PRICE_DURABILITY){
        const PeItemDef *def=pe_item_def(s->item_id);
        if(def->durability_max<=0)return false;
        factor=s->durability/(float)def->durability_max;
    }else if(source->factor!=PE_TOWN_PRICE_FULL)return false;
    /* These are deliberately float operations, matching C# GetPrice. Check
     * the rounded float before integer conversion to avoid undefined overflow. */
    float price=(float)source->base_currency*(0.8f*factor+0.2f);
    if(!isfinite(price)||price<0||(double)price>INT32_MAX)return false;
    if(unit)*unit=(uint32_t)price;
    if(base)*base=source->base_currency;
    return true;
}
bool pe_town_trade_state_valid(const PeGame *g){
    if(!g||(g->mode!=PE_GAME_LEGACY&&g->mode!=PE_GAME_ADVENTURE))return false;
    uint32_t owned[PE_TOWN_WALLET_CAPACITY*PE_TOWN_BUYBACK_CAPACITY];size_t owners=0;
    for(unsigned v=0;v<PE_TOWN_WALLET_CAPACITY;v++){
        const PeTownBuyback *b=&g->town_buyback[v];
        if(b->count>PE_TOWN_BUYBACK_CAPACITY||(b->count&&g->mode!=PE_GAME_ADVENTURE))return false;
        for(unsigned i=0;i<PE_TOWN_BUYBACK_CAPACITY;i++){
            const PeSlot *s=&b->slots[i];
            if(i>=b->count){if(!empty_slot(s))return false;continue;}
            if(!pe_town_trade_price(s,NULL,NULL)||!g->inventory.next_instance_id)return false;
            uint32_t id=s->instance_id;if(!id)continue;
            if(id>=g->inventory.next_instance_id||id==g->equipped_instance_id||id==g->shield_instance_id)return false;
            for(size_t j=0;j<owners;j++)if(owned[j]==id)return false;
            owned[owners++]=id;
            for(unsigned j=0;j<PE_INVENTORY_SLOTS;j++)if(g->inventory.slots[j].instance_id==id)return false;
            for(unsigned j=0;j<PE_QUICKSLOTS;j++)if(g->quickslots[j].instance_id==id)return false;
            for(unsigned j=0;j<PE_OUTFIT_SLOTS;j++)if(g->outfit.items[j].instance_id==id)return false;
            for(unsigned j=0;j<PE_MAX_DEPLOYABLES;j++)if(g->deployed[j].instance_id==id)return false;
        }
    }
    return true;
}
size_t pe_town_trade_count(const PeGame *g,uint32_t entity){
    int v=vendor_index(entity);
    return v>=0&&pe_town_trade_state_valid(g)?g->town_buyback[v].count:0;
}
const PeSlot *pe_town_trade_at(const PeGame *g,uint32_t entity,size_t row){
    int v=vendor_index(entity);
    if(v<0||!pe_town_trade_state_valid(g)||row>=g->town_buyback[v].count)return NULL;
    return &g->town_buyback[v].slots[row];
}
static bool inventory_valid(const PeGame *g){
    if(!g->inventory.next_instance_id)return false;
    for(unsigned i=0;i<PE_INVENTORY_SLOTS;i++){
        const PeSlot *s=&g->inventory.slots[i];
        if(!s->count){if(!empty_slot(s))return false;continue;}
        if(!slot_valid(s))return false;
        if(s->instance_id){
            if(s->instance_id>=g->inventory.next_instance_id)return false;
            for(unsigned j=0;j<i;j++)if(g->inventory.slots[j].instance_id==s->instance_id)return false;
        }
    }
    for(unsigned i=0;i<PE_QUICKSLOTS;i++){
        PeQuickslot q=g->quickslots[i];
        if(!q.item_id){if(q.instance_id)return false;continue;}
        const PeItemDef *d=pe_item_def(q.item_id);if(!d)return false;
        if(d->stack_limit>1){if(q.instance_id)return false;continue;}
        bool found=false;
        for(unsigned j=0;j<PE_INVENTORY_SLOTS;j++){
            const PeSlot *s=&g->inventory.slots[j];
            if(s->item_id==q.item_id&&s->count==1&&s->instance_id==q.instance_id)found=true;
        }
        if(!found)return false;
    }
    if(g->equipped_instance_id){
        const PeSlot *s=pe_game_equipped_tool(g);const PeItemDef *d=s?pe_item_def(s->item_id):NULL;
        if(!d||d->durability_max<=0||(d->tool_kind==PE_TOOL_NONE&&
            !((d->equip_position&16)&&(d->item_type==9||d->item_type==10||d->item_type==14))))return false;
    }
    return true;
}
static const char *admit(const PeGame *g,uint32_t entity,int *vendor){
    if(!g||g->mode!=PE_GAME_ADVENTURE||!isfinite(g->hp)||g->hp<=0)return "Trading is unavailable.";
    int v=vendor_index(entity);if(v<0)return "This resident is not a vendor.";
    if(g->money>INT32_MAX||!pe_town_state_valid(g)||!inventory_valid(g)||
       !pe_outfit_state_valid(g)||!pe_shield_state_valid(g)||!pe_deployable_state_valid(g)||!pe_town_trade_state_valid(g))
        return "Item ownership or currency is invalid.";
    *vendor=v;return NULL;
}
typedef struct {int vendor;uint32_t cost,credit;PeInventory inventory;} TradePlan;
static const char *sale_plan(const PeGame *g,uint32_t entity,int slot,unsigned count,TradePlan *p){
    const char *error=admit(g,entity,&p->vendor);if(error)return error;
    if(slot<0||slot>=PE_INVENTORY_SLOTS||!count)return "Select an item and quantity.";
    const PeSlot *s=&g->inventory.slots[slot];
    if(s->count<=0||count>(unsigned)s->count)return "Not enough items in this stack.";
    if(s->instance_id&&(s->instance_id==g->equipped_instance_id||s->instance_id==g->shield_instance_id))
        return "Unequip this item before selling.";
    uint32_t unit;if(!pe_town_trade_price(s,&unit,NULL))return "This item's condition is not supported for trading.";
    if(g->town_buyback[p->vendor].count>=PE_TOWN_BUYBACK_CAPACITY)return "This vendor's buyback list is full.";
    uint64_t total=(uint64_t)unit*count;
    if(total>g->town_wallet[p->vendor].balance)return "Vendor cannot afford this sale.";
    if(total>(uint32_t)INT32_MAX-g->money)return "Currency limit reached.";
    p->cost=(uint32_t)total;return NULL;
}
static const char *repurchase_plan(const PeGame *g,uint32_t entity,size_t row,unsigned count,TradePlan *p){
    const char *error=admit(g,entity,&p->vendor);if(error)return error;
    const PeTownBuyback *b=&g->town_buyback[p->vendor];
    if(row>=b->count||!count)return "Select an item and quantity.";
    const PeSlot *s=&b->slots[row];
    if(count>(unsigned)s->count)return "Not enough items to buy back.";
    uint32_t unit,base;if(!pe_town_trade_price(s,&unit,&base))return "This item's condition is not supported for trading.";
    uint64_t total=(uint64_t)unit*count,credit=(uint64_t)base*count;
    if(total>g->money)return "Not enough currency.";
    if(credit>(uint32_t)INT32_MAX-g->town_wallet[p->vendor].balance)return "Vendor currency limit reached.";
    p->inventory=g->inventory;
    if(s->instance_id){
        int free_slot=-1;
        for(int i=0;i<PE_INVENTORY_SLOTS;i++)if(!p->inventory.slots[i].count){free_slot=i;break;}
        if(free_slot<0)return "Inventory full.";
        p->inventory.slots[free_slot]=*s;
    }else if(!pe_inventory_add(&p->inventory,s->item_id,(int)count))return "Inventory full.";
    p->cost=(uint32_t)total;p->credit=(uint32_t)credit;return NULL;
}
bool pe_town_trade_can_sell(const PeGame *g,uint32_t entity,int slot,unsigned count){
    TradePlan p;return !sale_plan(g,entity,slot,count,&p);
}
bool pe_town_trade_can_buyback(const PeGame *g,uint32_t entity,size_t row,unsigned count){
    TradePlan p;return !repurchase_plan(g,entity,row,count,&p);
}
bool pe_town_trade_sell(PeGame *g,uint32_t entity,int slot,unsigned count){
    TradePlan p;const char *error=sale_plan(g,entity,slot,count,&p);
    if(error){if(g)snprintf(g->message,sizeof g->message,"%s",error);return false;}
    PeSlot sold=g->inventory.slots[slot];sold.count=(int)count;
    PeTownBuyback *b=&g->town_buyback[p.vendor];b->slots[b->count++]=sold;
    g->inventory.slots[slot].count-=(int)count;
    if(!g->inventory.slots[slot].count)g->inventory.slots[slot]=(PeSlot){0};
    if(sold.instance_id)for(unsigned i=0;i<PE_QUICKSLOTS;i++)
        if(g->quickslots[i].instance_id==sold.instance_id)g->quickslots[i]=(PeQuickslot){0};
    g->money+=p.cost;g->town_wallet[p.vendor].balance-=p.cost;
    snprintf(g->message,sizeof g->message,"Sold %u %s for %u.",count,pe_item_def(sold.item_id)->name,p.cost);
    return true;
}
bool pe_town_trade_buyback(PeGame *g,uint32_t entity,size_t row,unsigned count){
    TradePlan p;const char *error=repurchase_plan(g,entity,row,count,&p);
    if(error){if(g)snprintf(g->message,sizeof g->message,"%s",error);return false;}
    PeTownBuyback *b=&g->town_buyback[p.vendor];int item=b->slots[row].item_id;
    g->inventory=p.inventory;g->money-=p.cost;g->town_wallet[p.vendor].balance+=p.credit;
    b->slots[row].count-=(int)count;
    if(!b->slots[row].count){
        memmove(&b->slots[row],&b->slots[row+1],(b->count-row-1)*sizeof b->slots[0]);
        b->slots[--b->count]=(PeSlot){0};
    }
    snprintf(g->message,sizeof g->message,"Bought back %u %s for %u.",count,pe_item_def(item)->name,p.cost);
    return true;
}
