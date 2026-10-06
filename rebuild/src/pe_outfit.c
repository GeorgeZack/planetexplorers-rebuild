#include "pe_outfit.h"
#include "pe_game.h"
#include "../generated/pe_starter_outfit_source.h"
#include <math.h>
#include <stddef.h>

_Static_assert(PE_OUTFIT_SLOTS==PE_STARTER_OUTFIT_SOURCE_COUNT,"starter outfit slot count");
static bool empty(const PeOutfit *o){
    if(!o)return false;
    for(unsigned i=0;i<PE_OUTFIT_SLOTS;i++)
        if(o->items[i].item_id||o->items[i].instance_id||o->items[i].durability!=0)return false;
    return true;
}
bool pe_outfit_valid(const PeOutfit *o){
    if(!o)return false;
    if(empty(o))return true;
    for(unsigned i=0;i<PE_OUTFIT_SLOTS;i++){
        const PeWornItem *item=&o->items[i];
        if(item->item_id!=pe_starter_outfit_sources[i].item_id||!item->instance_id||
           !isfinite(item->durability)||item->durability<0||
           item->durability>pe_starter_outfit_sources[i].durability)return false;
        for(unsigned j=0;j<i;j++)if(item->instance_id==o->items[j].instance_id)return false;
    }
    return true;
}
static bool allocator_valid(const PeGame *g){
    uint32_t next=g->inventory.next_instance_id;
    if(!next||g->deployed_count>PE_MAX_DEPLOYABLES)return false;
    if(g->equipped_instance_id>=next)return false;
    for(unsigned i=0;i<PE_INVENTORY_SLOTS;i++)
        if(g->inventory.slots[i].instance_id>=next)return false;
    for(unsigned i=0;i<PE_QUICKSLOTS;i++)
        if(g->quickslots[i].instance_id>=next)return false;
    for(unsigned i=0;i<PE_MAX_DEPLOYABLES;i++)
        if(g->deployed[i].instance_id>=next)return false;
    return true;
}
bool pe_outfit_state_valid(const PeGame *g){
    if(!g||!pe_outfit_valid(&g->outfit))return false;
    if(g->mode==PE_GAME_LEGACY)return empty(&g->outfit);
    if(g->mode!=PE_GAME_ADVENTURE||empty(&g->outfit)||!allocator_valid(g))return false;
    for(unsigned i=0;i<PE_OUTFIT_SLOTS;i++){
        uint32_t id=g->outfit.items[i].instance_id;
        if(id>=g->inventory.next_instance_id||id==g->equipped_instance_id)return false;
        for(unsigned j=0;j<PE_INVENTORY_SLOTS;j++)if(id==g->inventory.slots[j].instance_id)return false;
        for(unsigned j=0;j<PE_QUICKSLOTS;j++)if(id==g->quickslots[j].instance_id)return false;
        for(unsigned j=0;j<PE_MAX_DEPLOYABLES;j++)if(id==g->deployed[j].instance_id)return false;
    }
    return true;
}
bool pe_outfit_init(PeGame *g){
    if(!g||!empty(&g->outfit))return false;
    if(g->mode==PE_GAME_LEGACY)return true;
    if(g->mode!=PE_GAME_ADVENTURE||!allocator_valid(g)||
       g->inventory.next_instance_id>UINT32_MAX-PE_OUTFIT_SLOTS)return false;
    PeOutfit next={0};uint32_t identity=g->inventory.next_instance_id;
    for(unsigned i=0;i<PE_OUTFIT_SLOTS;i++)
        next.items[i]=(PeWornItem){pe_starter_outfit_sources[i].item_id,identity++,pe_starter_outfit_sources[i].durability};
    if(!pe_outfit_valid(&next))return false;
    g->outfit=next;g->inventory.next_instance_id=identity;
    return true;
}
float pe_outfit_defense(const PeOutfit *o){
    if(!pe_outfit_valid(o))return NAN;
    float defense=0;
    for(unsigned i=0;i<PE_OUTFIT_SLOTS;i++)
        if(o->items[i].durability>0)defense+=pe_starter_outfit_sources[i].defense;
    return defense;
}
bool pe_outfit_wear(PeOutfit *o,float damage){
    if(!isfinite(damage)||!pe_outfit_valid(o))return false;
    if(empty(o)||damage<=0)return true;
    PeOutfit next=*o;
    for(unsigned i=0;i<PE_OUTFIT_SLOTS;i++){
        float loss=fmaxf(0,damage*pe_starter_outfit_sources[i].wear_factor);
        if(!isfinite(loss))return false;
        next.items[i].durability=fmaxf(0,next.items[i].durability-loss);
    }
    if(!pe_outfit_valid(&next))return false;
    *o=next;return true;
}
