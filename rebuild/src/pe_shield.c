#include "pe_shield.h"
#include "../generated/pe_starter_sword_shield_source.h"
#include <math.h>

bool pe_shield_slot_valid(const PeSlot *s){
    return s&&s->item_id==pe_starter_shield_source.item_id&&s->count==1&&s->instance_id&&
        isfinite(s->durability)&&s->durability>=0&&s->durability<=pe_starter_shield_source.durability;
}
static const PeSlot *owned(const PeGame *g,uint32_t id){
    if(!g||g->mode!=PE_GAME_ADVENTURE||!id||!g->inventory.next_instance_id||id>=g->inventory.next_instance_id||
       id==g->equipped_instance_id||g->deployed_count>PE_MAX_DEPLOYABLES)return NULL;
    const PeSlot *found=NULL;
    for(unsigned i=0;i<PE_INVENTORY_SLOTS;i++)if(g->inventory.slots[i].instance_id==id){
        if(found||!pe_shield_slot_valid(&g->inventory.slots[i]))return NULL;
        found=&g->inventory.slots[i];
    }
    if(!found)return NULL;
    for(unsigned i=0;i<PE_OUTFIT_SLOTS;i++)if(g->outfit.items[i].instance_id==id)return NULL;
    for(unsigned i=0;i<PE_MAX_DEPLOYABLES;i++)if(g->deployed[i].instance_id==id)return NULL;
    for(unsigned i=0;i<PE_QUICKSLOTS;i++)if(g->quickslots[i].instance_id==id&&g->quickslots[i].item_id!=PE_SHIELD_ITEM)return NULL;
    return found;
}
bool pe_shield_state_valid(const PeGame *g){
    return g&&(g->mode==PE_GAME_LEGACY||g->mode==PE_GAME_ADVENTURE)&&
        (!g->shield_instance_id||owned(g,g->shield_instance_id));
}
const PeSlot *pe_shield_equipped(const PeGame *g){return g?owned(g,g->shield_instance_id):NULL;}
bool pe_shield_equip(PeGame *g,int index){
    if(!g||index<0||index>=PE_INVENTORY_SLOTS||!isfinite(g->hp)||g->hp<=0||!pe_shield_state_valid(g))return false;
    const PeSlot *s=&g->inventory.slots[index];
    if(owned(g,s->instance_id)!=s)return false;
    g->shield_instance_id=s->instance_id;return true;
}
bool pe_shield_unequip(PeGame *g){
    if(!pe_shield_state_valid(g))return false;
    g->shield_instance_id=0;return true;
}
float pe_shield_defense(const PeSlot *s){
    if(!s)return 0;
    if(!pe_shield_slot_valid(s))return NAN;
    return s->durability>0?pe_starter_shield_source.defense:0;
}
float pe_shield_protection(const PeSlot *s){
    if(!s)return 0;
    if(!pe_shield_slot_valid(s))return NAN;
    return s->durability>0?pe_starter_shield_source.melee_protect:0;
}
bool pe_shield_wear(PeSlot *s,float damage){
    if(!pe_shield_slot_valid(s)||!isfinite(damage))return false;
    if(damage<=0)return true;
    float loss=damage*pe_starter_shield_source.wear_factor;if(!isfinite(loss))return false;
    s->durability=fmaxf(0,s->durability-loss);return true;
}
void pe_shield_clear_missing(PeGame *g){
    if(!g||!g->shield_instance_id)return;
    for(unsigned i=0;i<PE_INVENTORY_SLOTS;i++)
        if(g->inventory.slots[i].count&&g->inventory.slots[i].instance_id==g->shield_instance_id)return;
    g->shield_instance_id=0;
}
