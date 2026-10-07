#include "pe_debug.h"
#include "pe_adventure_hunt.h"
#include "pe_adventure_medicine.h"
#include "pe_build_history.h"
#include "pe_deployable.h"
#include "pe_hunt_loot.h"
#include "pe_settlement.h"
#include "pe_shield.h"
#include "pe_town_trade.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static bool fail(char *error,size_t capacity,const char *message){
    if(error&&capacity)snprintf(error,capacity,"%s",message);
    return false;
}
/* This command accepts an existing gameplay state, not an untrusted save.
 * Check the owners it may copy/allocate against before staging any change. */
static bool valid(const PeGame *g){
    if(!g||g->mode!=PE_GAME_ADVENTURE||!g->inventory.categorized||
       !pe_inventory_state_valid(&g->inventory)||!pe_outfit_state_valid(g)||
       !pe_shield_state_valid(g)||!pe_game_repair_state_valid(g)||
       !pe_deployable_state_valid(g)||!pe_town_trade_state_valid(g)||
       !pe_hunt_loot_valid(g)||!pe_build_history_state_valid(g)||
       !pe_adventure_medicine_state_valid(g)||!pe_adventure_hunt_state_valid(g)||
       !isfinite(g->max_hp)||g->max_hp<=0||g->max_hp>1000000||
       !isfinite(g->hp)||g->hp<0||g->hp>g->max_hp||
       !isfinite(g->stamina_max)||g->stamina_max<=0||g->stamina_max>1000000||
       !isfinite(g->stamina)||g->stamina<0||g->stamina>g->stamina_max||
       !g->rng_state)return false;
    if(g->welcome_page<PE_ADVENTURE_WELCOME_PAGES)
        return !g->adventure_mission&&!g->arrival_complete&&!g->supplies_claimed;
    return g->welcome_page==PE_ADVENTURE_WELCOME_PAGES&&
        (g->adventure_mission==PE_ADVENTURE_ARRIVAL||g->adventure_mission==PE_ADVENTURE_SUPPLIES)&&
        g->arrival_complete==(g->adventure_mission==PE_ADVENTURE_SUPPLIES)&&
        (!g->supplies_claimed||g->arrival_complete);
}
static bool topup(PeGame *g,int item,int target){
    int count=pe_inventory_count(&g->inventory,item);
    return count>=target||pe_inventory_add(&g->inventory,item,target-count);
}
static bool supplies(PeGame *g){
    if(!topup(g,33,1)||!topup(g,925,20)||!topup(g,916,20)||!topup(g,937,5))return false;
    g->hp=g->max_hp;g->stamina=g->stamina_max;g->revival=(PeRevival){0};
    return true;
}
static bool hunt(PeGame *g,char *error,size_t capacity){
    if(g->hunt_state>=PE_HUNT_RETURN_PENDING)
        return fail(error,capacity,"This hunt is already finished. Debug travel cannot reset kills or rewards.");
    if(!pe_settlement_is_route(&g->world))
        return fail(error,capacity,"Debug hunt travel requires the captured Adventure route world.");
    if(!supplies(g))return fail(error,capacity,"Make room for all combat supplies; nothing was changed.");
    bool equipped=false;
    for(int i=0;i<pe_inventory_capacity(&g->inventory);i++)
        if(g->inventory.slots[i].item_id==33&&pe_game_equip_slot(g,i)){equipped=true;break;}
    if(!equipped)return fail(error,capacity,"A usable Knife is required; debug travel does not repair equipment.");
    /* Explicit skip: no imitation of acceptance grants, turn-in debit, source
     * rewards or presented conversation pages. Normal source rewards later in
     * the hunt remain governed by the unchanged mission state machine. */
    g->welcome_page=PE_ADVENTURE_WELCOME_PAGES;g->adventure_mission=PE_ADVENTURE_SUPPLIES;
    g->arrival_complete=true;g->supplies_claimed=true;g->annot_complete=true;
    g->medicine_state=PE_MEDICINE_COMPLETE;
    if(g->hunt_state==PE_HUNT_NONE&&!pe_adventure_hunt_accept(g))
        return fail(error,capacity,"The hunt has no safe arrival surface in this world.");
    PeSandboxCollision *collision=pe_sandbox_collision_create();
    if(!collision)return fail(error,capacity,"Cannot allocate the hunt collision snapshot.");
    bool ok=pe_game_collision_update(g,collision,error,capacity);
    if(ok){
        g->player=(PeVec3){g->hunt_destination.x-g->world.origin_x,
            g->hunt_destination.y-g->world.origin_y,g->hunt_destination.z-g->world.origin_z};
        ok=pe_game_collision_settle(g,collision)&&
            pe_sandbox_collision_support(collision,g->player,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT,.1f,.65f).hit&&
            !pe_sandbox_collision_overlap(collision,g->player,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT);
    }
    pe_sandbox_collision_free(collision);
    if(!ok)return fail(error,capacity,"The hunt has no safe arrival surface in this world.");
    if(g->hunt_state==PE_HUNT_TRAVEL&&!pe_adventure_hunt_step(g))
        return fail(error,capacity,"The settled destination did not reach the hunt area.");
    if(!pe_adventure_hunt_state_valid(g)||g->hunt_state!=PE_HUNT_ENCOUNTER_PENDING)
        return fail(error,capacity,"The hunt transition failed validation.");
    return true;
}
bool pe_debug_apply(PeGame *g,PeDebugAction action,char *error,size_t capacity){
    if(error&&capacity)error[0]=0;
    if(action<PE_DEBUG_MEDICINE||action>PE_DEBUG_START_HUNT)
        return fail(error,capacity,"Choose a debug action.");
    if(!valid(g))return fail(error,capacity,"Debug actions require a valid Adventure game.");
    PeGame *next=calloc(1,sizeof *next);
    if(!next)return fail(error,capacity,"Cannot allocate the debug transaction.");
    bool ok=pe_game_clone(next,g);
    if(!ok)fail(error,capacity,"Cannot clone the game for this debug action.");
    else if(action==PE_DEBUG_MEDICINE){
        ok=topup(next,917,5);
        if(!ok)fail(error,capacity,"Make room for five Herbal Drinks; nothing was changed.");
    }else if(action==PE_DEBUG_COMBAT_SUPPLIES){
        ok=supplies(next);
        if(!ok)fail(error,capacity,"Make room for all combat supplies; nothing was changed.");
    }else ok=hunt(next,error,capacity);
    if(ok&&!valid(next)){ok=false;fail(error,capacity,"Debug transaction failed ownership validation.");}
    if(ok){
        const char *message=action==PE_DEBUG_MEDICINE?"Debug: Herbal Drinks topped up to five. Turn them in to Naoko.":
            action==PE_DEBUG_COMBAT_SUPPLIES?"Debug: combat supplies topped up; health and stamina restored.":
            "Debug: prerequisites skipped; Knife equipped and hunt area reached.";
        snprintf(next->message,sizeof next->message,"%s",message);
        pe_game_move(g,next);
    }
    pe_game_destroy(next);free(next);return ok;
}
