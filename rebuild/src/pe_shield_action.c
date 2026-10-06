#include "pe_shield_action.h"
#include "../generated/pe_starter_sword_shield_source.h"
#include <stddef.h>
#define BIT(i) PE_SHIELD_ACTION_BIT(i)
static const uint64_t wait_actions=BIT(PE_SHIELD_ACTION_STEP)|BIT(PE_SHIELD_ACTION_LEISURE);
static const uint64_t pause_actions=BIT(PE_SHIELD_ACTION_FLASHLIGHT);
uint64_t pe_shield_action_forbidden_masks(void){
    /* ActionRelation15: column values2 mean false,0 means no dependency.
     * PEAction.cs binds masks by ordinal, not by the database display name. */
    return BIT(4)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(14)|
        BIT(24)|BIT(25)|BIT(29)|BIT(30)|BIT(31)|BIT(34)|BIT(35)|BIT(36)|BIT(38)|
        BIT(40)|BIT(44)|BIT(45)|BIT(46)|BIT(47)|BIT(48)|BIT(51)|BIT(55);
}
bool pe_shield_action_source_admissible(bool shield_present,uint64_t masks){
    return shield_present&&!(masks>>PE_SHIELD_MASK_COUNT)&&!(masks&pe_shield_action_forbidden_masks());
}
static bool valid(const PeShieldAction *a,PeShieldActionInput in){
    return a&&a->phase>=PE_SHIELD_ACTION_IDLE&&a->phase<=PE_SHIELD_ACTION_HOLDING&&
        ((a->phase==PE_SHIELD_ACTION_IDLE)==(a->instance_id==0))&&
        in.request>=PE_SHIELD_REQUEST_NONE&&in.request<=PE_SHIELD_REQUEST_RELEASE&&
        !(in.source_masks>>PE_SHIELD_MASK_COUNT)&&!(in.running_actions>>PE_SHIELD_ACTION_COUNT);
}
static void finish(PeShieldAction *a,PeShieldActionInput in,PeShieldActionUpdate *out){
    out->ended=true;out->cancel_buff=true;out->continue_actions=pause_actions;
    out->restore_base_style=in.move_present&&!(in.running_actions&BIT(PE_SHIELD_ACTION_EQUIPMENT_HOLD));
    out->set_shield_visible=in.hide_equipment_inactive&&in.shield_instance_id==a->instance_id;
    *a=(PeShieldAction){0};
}
bool pe_shield_action_step(PeShieldAction *a,PeShieldActionInput in,PeShieldActionUpdate *out){
    if(!out||!valid(a,in))return false;
    PeShieldAction next=*a;PeShieldActionUpdate result={.hold_buff_id=pe_starter_shield_source.hold_buff_id};
    bool lost=!in.host_capable||!in.animator_present||!in.shield_instance_id;
    bool changed=next.phase!=PE_SHIELD_ACTION_IDLE&&in.shield_instance_id!=next.instance_id;
    if(next.phase!=PE_SHIELD_ACTION_IDLE&&(in.cancel||lost||changed||in.request==PE_SHIELD_REQUEST_RELEASE)){
        finish(&next,in,&result);
    }else if(!in.cancel&&!lost){
        if(next.phase==PE_SHIELD_ACTION_IDLE&&in.request==PE_SHIELD_REQUEST_HOLD){
            if(pe_shield_action_source_admissible(true,in.source_masks)){
                next.phase=PE_SHIELD_ACTION_WAITING;next.instance_id=in.shield_instance_id;
                result.admitted=true;result.pause_actions=pause_actions;result.end_actions=wait_actions;
                result.end_immediately_actions=BIT(PE_SHIELD_ACTION_SPRINT);
            }else result.denied=true;
        }
        if(next.phase==PE_SHIELD_ACTION_WAITING&&!(in.running_actions&wait_actions)&&!in.action_paused){
            next.phase=PE_SHIELD_ACTION_HOLDING;result.started=true;result.mount_buff=true;
            result.set_shield_visible=in.hide_equipment_inactive;result.shield_visible=true;
        }
        if(next.phase==PE_SHIELD_ACTION_HOLDING){
            result.holding=true;
            result.set_sword_style=in.move_present&&!(in.running_actions&BIT(PE_SHIELD_ACTION_EQUIPMENT_HOLD));
        }
    }else if(in.request==PE_SHIELD_REQUEST_HOLD)result.denied=true;
    *a=next;*out=result;return true;
}
