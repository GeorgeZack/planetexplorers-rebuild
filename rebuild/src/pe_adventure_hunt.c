#include "pe_adventure_hunt.h"
#include "pe_adventure_medicine.h"
#include "pe_settlement.h"
#include "../generated/pe_adventure_hunt_source.h"
#include <math.h>
#include <stdio.h>

bool pe_adventure_hunt_reference(PeVec3 *out){
    if(!out||!PE_HUNT_REFERENCE_AVAILABLE)return false;
    *out=pe_hunt_reference_destination;return true;
}
bool pe_adventure_hunt_state_valid(const PeGame *g){
    if(!g||g->hunt_state>PE_HUNT_COMPLETE||g->hunt_kills>4)return false;
    if((g->hunt_state<PE_HUNT_ENCOUNTER_PENDING&&g->hunt_kills)||
       (g->hunt_state==PE_HUNT_ENCOUNTER_PENDING&&g->hunt_kills>=4)||
       (g->hunt_state>=PE_HUNT_RETURN_PENDING&&g->hunt_kills!=4))return false;
    PeVec3 p=g->hunt_destination;
    if(!isfinite(p.x)||!isfinite(p.y)||!isfinite(p.z))return false;
    if(g->hunt_state==PE_HUNT_NONE)return p.x==0&&p.y==0&&p.z==0;
    PeVec3 expected;
    return pe_adventure_medicine_state_valid(g)&&g->medicine_state==PE_MEDICINE_COMPLETE&&
        pe_adventure_hunt_reference(&expected)&&p.x==expected.x&&p.y==expected.y&&p.z==expected.z;
}
static bool alive(const PeGame *g){return g&&g->mode==PE_GAME_ADVENTURE&&isfinite(g->hp)&&g->hp>0;}
bool pe_adventure_hunt_available(const PeGame *g){
    PeVec3 p;
    if(!alive(g)||!pe_adventure_hunt_state_valid(g)||!pe_adventure_medicine_state_valid(g)||
       g->medicine_state!=PE_MEDICINE_COMPLETE||g->hunt_state!=PE_HUNT_NONE||
       !pe_adventure_hunt_reference(&p)||!pe_settlement_is_route(&g->world))return false;
    double x=(double)p.x-g->world.origin_x,y=(double)p.y-g->world.origin_y,z=(double)p.z-g->world.origin_z;
    double margin=PE_HUNT_RADIUS+PE_PLAYER_RADIUS;
    return x>=margin&&x<=pe_world_width(&g->world)-1-margin&&
        z>=margin&&z<=pe_world_depth(&g->world)-1-margin&&
        y>=0&&y+PE_PLAYER_HEIGHT<pe_world_size_y(&g->world)-1;
}
bool pe_adventure_hunt_accept(PeGame *g){
    if(!g)return false;
    if(!pe_adventure_hunt_available(g)){
        snprintf(g->message,sizeof g->message,"This hunt is not available in this world yet.");return false;
    }
    PeSandboxCollision *collision=pe_sandbox_collision_create();char error[160];
    if(!collision)return false;
    bool valid=pe_game_collision_update(g,collision,error,sizeof error);
    PeVec3 p=pe_hunt_reference_destination;
    PeVec3 local={p.x-g->world.origin_x,p.y-g->world.origin_y,p.z-g->world.origin_z},supported;
    if(valid)valid=pe_sandbox_collision_settle(collision,local,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT,4,5,&supported);
    if(valid){
        PeSceneHit support=pe_sandbox_collision_support(collision,supported,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT,.1f,.65f);
        valid=support.hit;
    }
    pe_sandbox_collision_free(collision);
    if(!valid){snprintf(g->message,sizeof g->message,"The hunting area has no safe arrival surface in this world.");return false;}
    g->hunt_state=PE_HUNT_TRAVEL;g->hunt_destination=p;
    snprintf(g->message,sizeof g->message,"Bug Hunt: travel to the hunting area.");return true;
}
bool pe_adventure_hunt_step(PeGame *g){
    if(!alive(g)||!pe_adventure_hunt_state_valid(g)||g->hunt_state!=PE_HUNT_TRAVEL||
       !pe_settlement_is_route(&g->world)||!isfinite(g->player.x)||!isfinite(g->player.y)||!isfinite(g->player.z))return false;
    double x=(double)g->player.x+g->world.origin_x,y=(double)g->player.y+g->world.origin_y,z=(double)g->player.z+g->world.origin_z;
    if(y<0||hypot(x-g->hunt_destination.x,z-g->hunt_destination.z)>=PE_HUNT_RADIUS)return false;
    g->hunt_state=PE_HUNT_ENCOUNTER_PENDING;
    snprintf(g->message,sizeof g->message,"Hunt area reached. Preparing the Lepus hare encounter.");return true;
}
bool pe_adventure_hunt_return_reference(PeVec3*out){
    if(!out||!PE_HUNT_RETURN_REFERENCE_AVAILABLE)return false;
    *out=pe_hunt_return_destination;return true;
}
bool pe_adventure_hunt_return_step(PeGame*g){
    PeVec3 destination;
    if(!alive(g)||!pe_adventure_hunt_state_valid(g)||g->hunt_state!=PE_HUNT_RETURN_PENDING||
       !pe_settlement_is_route(&g->world)||!pe_adventure_hunt_return_reference(&destination)||
       !isfinite(g->player.x)||!isfinite(g->player.y)||!isfinite(g->player.z))return false;
    double x=(double)g->player.x+g->world.origin_x,y=(double)g->player.y+g->world.origin_y,z=(double)g->player.z+g->world.origin_z;
    if(y<0||hypot(x-destination.x,z-destination.z)>=PE_HUNT_RETURN_RADIUS)return false;
    PeInventory staged=g->inventory;
    for(size_t i=0;i<sizeof pe_hunt_return_rewards/sizeof pe_hunt_return_rewards[0];i++){
        const PeIngredient*reward=&pe_hunt_return_rewards[i];
        if(!pe_inventory_add(&staged,reward->item_id,reward->count)){
            snprintf(g->message,sizeof g->message,"Make room for all four Bug Hunt reward scripts.");return false;
        }
    }
    g->inventory=staged;g->hunt_state=PE_HUNT_RETURN_TALK1;
    snprintf(g->message,sizeof g->message,"Bug Hunt complete. Four reward scripts received.");return true;
}
int pe_adventure_hunt_return_talk(const PeGame*g){
    if(!alive(g)||!pe_adventure_hunt_state_valid(g))return 0;
    return g->hunt_state==PE_HUNT_RETURN_TALK1?4105:g->hunt_state==PE_HUNT_RETURN_TALK2?4106:0;
}
bool pe_adventure_hunt_return_advance(PeGame*g,int expected_talk){
    if(!expected_talk||pe_adventure_hunt_return_talk(g)!=expected_talk)return false;
    g->hunt_state++;
    if(g->hunt_state==PE_HUNT_COMPLETE)snprintf(g->message,sizeof g->message,"Bug Hunt reported. Town Defense is not available yet.");
    return true;
}
