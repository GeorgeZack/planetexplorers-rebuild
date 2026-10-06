#include "pe_sword.h"
#include "../generated/pe_starter_sword_shield_source.h"
#include <math.h>
bool pe_sword_slot_valid(const PeSlot *s){
    return s&&s->item_id==pe_starter_sword_source.item_id&&s->count==1&&s->instance_id&&
        isfinite(s->durability)&&s->durability>=0&&s->durability<=pe_starter_sword_source.durability;
}
float pe_sword_attack(const PeSlot *s,float base){
    if(!pe_sword_slot_valid(s)||!isfinite(base)||base<0||base>1000000)return NAN;
    float attack=base+(s->durability>0?pe_starter_sword_source.attack:0);
    return isfinite(attack)&&attack<=1000000?attack:NAN;
}
bool pe_sword_wear(PeSlot *s){
    if(!pe_sword_slot_valid(s))return false;
    s->durability=fmaxf(0,s->durability-pe_starter_sword_source.wear_per_damage_notification);
    return true;
}
