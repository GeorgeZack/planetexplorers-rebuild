#include "pe_revival.h"
#include "pe_game.h"
#include "../generated/pe_revival_source.h"
#include "../generated/pe_stamina_recovery_source.h"
#include <math.h>

static bool frame_valid(PeAttributeFrame f){return isfinite(f.now)&&f.now>=0&&f.frame;}
static bool valid(const PeRevival *s){
    return s&&s->phase>=PE_REVIVAL_IDLE&&s->phase<=PE_REVIVAL_GRACE&&
        isfinite(s->last_time)&&s->last_time>=0&&isfinite(s->enable_at)&&s->enable_at>=0&&
        (s->frame||(!s->last_time&&s->phase==PE_REVIVAL_IDLE))&&
        (s->phase==PE_REVIVAL_GRACE?s->enable_at>s->last_time:s->enable_at==0);
}
bool pe_revival_frozen(const PeRevival *s){return !valid(s)||s->phase==PE_REVIVAL_GETTING_UP;}
bool pe_revival_damage_disabled(const PeRevival *s){return !valid(s)||s->phase!=PE_REVIVAL_IDLE;}
bool pe_revival_frame(PeRevival *state,PeAttributeFrame frame,bool alive){
    if(!valid(state)||!frame_valid(frame)||frame.frame<=state->frame||frame.now<state->last_time)return false;
    PeRevival next=*state;next.frame=frame.frame;next.last_time=frame.now;
    if(!alive){next.phase=PE_REVIVAL_IDLE;next.enable_at=0;}
    else if(next.phase==PE_REVIVAL_GETTING_UP){
        next.phase=PE_REVIVAL_GRACE;next.enable_at=frame.now+pe_revival_source.injured_grace_seconds;
    }else if(next.phase==PE_REVIVAL_GRACE&&frame.now>=next.enable_at){next.phase=PE_REVIVAL_IDLE;next.enable_at=0;}
    if(!valid(&next))return false;
    *state=next;return true;
}
static bool inventory_valid(const PeInventory *p){
    if(!p->next_instance_id)return false;
    for(unsigned i=0;i<PE_INVENTORY_SLOTS;i++){
        PeSlot s=p->slots[i];
        if(!s.count){if(s.item_id||s.instance_id||s.durability!=0)return false;continue;}
        const PeItemDef *d=pe_item_def(s.item_id);
        if(!d||s.count<1||s.count>d->stack_limit||!isfinite(s.durability))return false;
        if(d->stack_limit==1){
            if(!s.instance_id||s.instance_id>=p->next_instance_id||s.durability<0||s.durability>d->durability_max)return false;
            for(unsigned j=0;j<i;j++)if(p->slots[j].instance_id==s.instance_id)return false;
        }else if(s.instance_id||s.durability!=0)return false;
    }
    return true;
}
bool pe_game_revive(PeGame *g,PeAttributeFrame frame,PeRevivalGate gate){
    if(!g||g->mode!=PE_GAME_ADVENTURE||!frame_valid(frame)||!valid(&g->revival)||
       g->revival.phase!=PE_REVIVAL_IDLE||frame.frame<g->revival.frame||frame.now<g->revival.last_time||
       !gate.death_mask||gate.fall_mask||gate.ride_mask||!gate.getup_ready||!gate.item_skill_ready||
       g->hp!=0||!isfinite(g->max_hp)||g->max_hp<=0||
       !isfinite(g->stamina)||!isfinite(g->stamina_max)||g->stamina_max<=0||g->stamina<0||g->stamina>g->stamina_max||
       !isfinite(g->comfort)||g->comfort<0||g->comfort>pe_stamina_recovery_source.comfort_max||
       !inventory_valid(&g->inventory))return false;
    PePassiveRecovery recovery=g->passive_recovery;PeStaminaRecovery stamina=g->stamina_recovery;
    float raw_comfort=(stamina.comfort_pending?stamina.pending_raw_comfort:g->comfort)-pe_revival_source.comfort_cost;
    float hp=g->max_hp*pe_revival_source.hp_fraction;
    if(!isfinite(hp)||hp<=0||hp>g->max_hp||
       !pe_passive_recovery_revive(&recovery,frame)||!pe_stamina_recovery_revive(&stamina,frame,raw_comfort))return false;
    PeInventory inventory=g->inventory;
    if(!pe_inventory_remove(&inventory,pe_revival_source.item_id,1))return false;
    PeRevival revival={PE_REVIVAL_GETTING_UP,frame.now,0,frame.frame};
    if(!valid(&revival))return false;
    /* Native preflight makes action+item atomic; source GUI starts the action
     * first and ignores Use's return. No world or borrowed geometry is copied.
     * Source action floors HP to1 before the item overwrites raw HP with .333.
     * Native publishes the final supported vitals together. Oxygen is absent. */
    g->inventory=inventory;g->hp=hp;g->stamina=g->stamina_max;
    g->comfort=fmaxf(0,raw_comfort);g->passive_recovery=recovery;g->stamina_recovery=stamina;g->revival=revival;
    g->velocity=(PeVec3){0};g->sprinting=false;g->jumping=false;g->jump_action_elapsed=0;
    g->tool_cooldown=0;g->stamina_idle=0;
    return true;
}
