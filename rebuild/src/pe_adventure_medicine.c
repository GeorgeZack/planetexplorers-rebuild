#include "pe_adventure_medicine.h"
#include "../generated/pe_adventure_medicine_source.h"
#include <math.h>
#include <stdio.h>

bool pe_adventure_medicine_state_valid(const PeGame *g){
    if(!g||g->medicine_state>PE_MEDICINE_COMPLETE)return false;
    return g->medicine_state==PE_MEDICINE_NONE||
        (g->mode==PE_GAME_ADVENTURE&&g->annot_complete&&g->supplies_claimed&&
         g->arrival_complete&&g->welcome_page==PE_ADVENTURE_WELCOME_PAGES&&
         g->adventure_mission==PE_ADVENTURE_SUPPLIES);
}
static bool eligible(const PeGame *g){
    return pe_adventure_medicine_state_valid(g)&&g->mode==PE_GAME_ADVENTURE&&
        g->annot_complete&&g->supplies_claimed&&g->arrival_complete&&
        g->welcome_page==PE_ADVENTURE_WELCOME_PAGES&&g->adventure_mission==PE_ADVENTURE_SUPPLIES&&
        isfinite(g->hp)&&g->hp>0;
}
bool pe_adventure_medicine_ready(const PeGame *g){
    return eligible(g)&&g->medicine_state==PE_MEDICINE_ACTIVE&&
        pe_inventory_count(&g->inventory,PE_MEDICINE_ITEM)>=PE_MEDICINE_REQUIRED;
}
bool pe_adventure_medicine_accept(PeGame *g){
    if(!eligible(g)||g->medicine_state!=PE_MEDICINE_NONE)return false;
    PeInventory staged=g->inventory;
    if(!pe_inventory_add(&staged,PE_MEDICINE_SCRIPT,1)){
        snprintf(g->message,sizeof g->message,"Make room for the Herbal Drink script.");return false;
    }
    g->inventory=staged;g->medicine_state=PE_MEDICINE_ACTIVE;
    snprintf(g->message,sizeof g->message,"Medicine: bring Naoko five Herbal Drinks.");return true;
}
bool pe_adventure_medicine_complete(PeGame *g){
    if(!pe_adventure_medicine_ready(g))return false;
    PeInventory staged=g->inventory;
    if(!pe_inventory_remove(&staged,PE_MEDICINE_ITEM,PE_MEDICINE_REQUIRED))return false;
    for(size_t i=0;i<sizeof pe_medicine_rewards/sizeof pe_medicine_rewards[0];i++){
        const PeIngredient *reward=&pe_medicine_rewards[i];
        if(!pe_inventory_add(&staged,reward->item_id,reward->count)){
            snprintf(g->message,sizeof g->message,"Make room for the complete medicine reward.");return false;
        }
    }
    g->inventory=staged;g->medicine_state=PE_MEDICINE_COMPLETE;
    snprintf(g->message,sizeof g->message,"Medicine delivered. Naoko's next quest is not implemented yet.");return true;
}
