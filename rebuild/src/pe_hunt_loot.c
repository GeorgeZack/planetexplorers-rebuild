#include "pe_hunt_loot.h"
#include "pe_settlement.h"
#include "../generated/pe_hunt_loot_source.h"
#include <math.h>
#include <string.h>

static bool zero_float(float value){return value==0&&!signbit(value);}
static bool empty_drop(PeHuntLootDrop d){
    return !d.instance_id&&!d.item_id&&!d.count&&zero_float(d.position.x)&&
        zero_float(d.position.y)&&zero_float(d.position.z)&&d.created_seconds==0&&!signbit(d.created_seconds);
}
static bool inside(const PeGame *g,PeVec3 p){
    return isfinite(p.x)&&isfinite(p.y)&&isfinite(p.z)&&p.x>=0&&p.y>=0&&p.z>=0&&
        p.x<pe_world_width(&g->world)&&p.y<pe_world_size_y(&g->world)&&p.z<pe_world_depth(&g->world);
}
static bool supported(int id,int count){
    if(count<=0)return false;
    const PeItemDef *def=pe_item_def(id);
    if(!def||def->stack_limit<=1||pe_item_has_durability(id))return false;
    for(unsigned i=0;i<PE_HUNT_LOOT_ITEM_COUNT;i++)if(pe_hunt_loot_items[i].item_id==id)
        return def->stack_limit==(int)pe_hunt_loot_items[i].max_stack&&count<=def->stack_limit;
    return false;
}
/* References count as aliases even when their surrounding record is malformed.
 * Full owner validation belongs to its existing subsystem/save preflight. */
static bool unbound(const PeGame *g,uint32_t id){
    if(!id||g->deployed_count>PE_MAX_DEPLOYABLES||id==g->equipped_instance_id||id==g->shield_instance_id||id==g->repair.item.instance_id)return false;
    for(unsigned i=0;i<PE_INVENTORY_MAX_SLOTS;i++)if(g->inventory.slots[i].instance_id==id)return false;
    for(unsigned i=0;i<PE_OUTFIT_SLOTS;i++)if(g->outfit.items[i].instance_id==id)return false;
    for(unsigned i=0;i<PE_QUICKSLOTS;i++)if(g->quickslots[i].instance_id==id)return false;
    for(unsigned i=0;i<PE_MAX_DEPLOYABLES;i++)if(g->deployed[i].instance_id==id)return false;
    for(unsigned i=0;i<PE_TOWN_WALLET_CAPACITY;i++){
        if(g->town_buyback[i].count>PE_TOWN_BUYBACK_CAPACITY)return false;
        for(unsigned j=0;j<PE_TOWN_BUYBACK_CAPACITY;j++)if(g->town_buyback[i].slots[j].instance_id==id)return false;
    }
    return true;
}
bool pe_hunt_loot_valid(const PeGame *g){
    if(!g||g->hunt_loot.count>PE_HUNT_LOOT_CAPACITY)return false;
    for(unsigned i=g->hunt_loot.count;i<PE_HUNT_LOOT_CAPACITY;i++)if(!empty_drop(g->hunt_loot.drops[i]))return false;
    if(!g->hunt_loot.count)return true;
    if(g->mode!=PE_GAME_ADVENTURE||!g->inventory.categorized||!pe_settlement_is_route(&g->world)||
       !isfinite(g->game_seconds)||g->game_seconds<0||g->game_seconds>1e15||
       !pe_inventory_state_valid(&g->inventory)||!g->inventory.next_instance_id)return false;
    for(unsigned i=0;i<g->hunt_loot.count;i++){
        PeHuntLootDrop d=g->hunt_loot.drops[i];
        if(!supported(d.item_id,d.count)||d.instance_id>=g->inventory.next_instance_id||!unbound(g,d.instance_id)||
           !inside(g,d.position)||!isfinite(d.created_seconds)||d.created_seconds<0||signbit(d.created_seconds)||d.created_seconds>g->game_seconds)return false;
        for(unsigned j=0;j<i;j++)if(g->hunt_loot.drops[j].instance_id==d.instance_id)return false;
    }
    return true;
}
void pe_hunt_loot_clear(PeGame *g){if(g)memset(&g->hunt_loot,0,sizeof g->hunt_loot);}
static uint32_t random_next(uint32_t *rng){uint32_t x=*rng;x^=x<<13;x^=x>>17;x^=x<<5;return *rng=x;}
static float random_fraction(uint32_t *rng){return (float)(random_next(rng)>>8)*(1.f/16777216.f);}
bool pe_hunt_loot_spawn(PeGame *g,const PeVec3 *positions,size_t count){
    if(!pe_hunt_loot_valid(g)||count>6||(!positions&&count))return false;
    if(!count)return true;
    if(g->mode!=PE_GAME_ADVENTURE||!g->inventory.categorized||!pe_settlement_is_route(&g->world)||
       !g->rng_state||!g->inventory.next_instance_id||!pe_inventory_state_valid(&g->inventory)||
       !isfinite(g->game_seconds)||g->game_seconds<0||signbit(g->game_seconds)||g->game_seconds>1e15)return false;
    for(size_t i=0;i<count;i++)if(!inside(g,positions[i]))return false;
    PeHuntLootState next=g->hunt_loot;uint32_t rng=g->rng_state,identity=g->inventory.next_instance_id;
    for(size_t death=0;death<count;death++){
        int items[1+PE_HUNT_LOOT_TRIALS]={PE_HUNT_LOOT_MEAT_ITEM},amounts[1+PE_HUNT_LOOT_TRIALS]={0};
        unsigned groups=1;
        amounts[0]=PE_HUNT_LOOT_MEAT_MIN+(int)(random_next(&rng)%(PE_HUNT_LOOT_MEAT_MAX_EXCLUSIVE-PE_HUNT_LOOT_MEAT_MIN));
        for(unsigned trial=0;trial<PE_HUNT_LOOT_TRIALS;trial++)for(unsigned entry=0;entry<PE_HUNT_LOOT_ENTRY_COUNT;entry++){
            if(random_fraction(&rng)<pe_hunt_loot_roll_entries[entry].probability){
                int item=pe_hunt_loot_roll_entries[entry].item_id;
                if(item){unsigned slot=0;while(slot<groups&&items[slot]!=item)slot++;
                    if(slot==groups){items[groups]=item;groups++;}amounts[slot]++;}
                break;
            }
        }
        if(groups>PE_HUNT_LOOT_CAPACITY-next.count)return false;
        for(unsigned i=0;i<groups;i++){
            if(identity==UINT32_MAX||!unbound(g,identity)||!supported(items[i],amounts[i]))return false;
            next.drops[next.count++]=(PeHuntLootDrop){identity++,items[i],amounts[i],positions[death],g->game_seconds};
        }
    }
    g->hunt_loot=next;g->rng_state=rng;g->inventory.next_instance_id=identity;return true;
}
bool pe_hunt_loot_update(PeGame *g,unsigned *picked_up,unsigned *expired){
    if(!pe_hunt_loot_valid(g))return false;
    if(!g->hunt_loot.count){if(picked_up)*picked_up=0;if(expired)*expired=0;return true;}
    if(!isfinite(g->hp)||g->hp<0||!isfinite(g->player.x)||!isfinite(g->player.y)||!isfinite(g->player.z))return false;
    PeInventory bag=g->inventory;PeHuntLootState next={0};unsigned picked=0,removed=0;
    for(unsigned i=0;i<g->hunt_loot.count;i++){
        PeHuntLootDrop d=g->hunt_loot.drops[i];
        if(g->game_seconds-d.created_seconds>PE_HUNT_LOOT_LIFETIME_SECONDS){removed++;continue;}
        double x=(double)g->player.x-d.position.x,y=(double)g->player.y-d.position.y,z=(double)g->player.z-d.position.z;
        if(g->hp>0&&x*x+y*y+z*z<(double)PE_HUNT_LOOT_PICKUP_DISTANCE*PE_HUNT_LOOT_PICKUP_DISTANCE&&
           pe_inventory_add(&bag,d.item_id,d.count)){picked++;continue;}
        next.drops[next.count++]=d;
    }
    g->inventory=bag;g->hunt_loot=next;
    if(picked_up)*picked_up=picked;
    if(expired)*expired=removed;
    return true;
}
