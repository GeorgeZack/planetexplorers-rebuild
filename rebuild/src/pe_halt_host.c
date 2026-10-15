#include "pe_halt_host.h"
#include <math.h>
#include <string.h>

static bool finite3(PeVec3 v){return isfinite(v.x)&&isfinite(v.y)&&isfinite(v.z);}
void pe_halt_host_reset(PeHaltHost *h){if(h)memset(h,0,sizeof *h);}
bool pe_halt_host_masked(const PeHaltHost *h){return h&&h->action.running&&h->action.mask;}
static bool offsets(PeHaltHost *h,float dt,bool *ended){
    PeHaltUpdate update;
    PeHaltTick tick={.delta_time=dt,.effect_present=true,.motor_present=true,.effect_enabled=true,.master_weight=1};
    if(!pe_halt_step(&h->action,tick,&update))return false;
    h->offset_count=update.offset_count;memcpy(h->offsets,update.offsets,sizeof h->offsets);
    if(update.ended)*ended=true;
    return true;
}
bool pe_halt_host_step(PeHaltHost *host,PeHaltHostInput in,PeHaltHostOutput *out){
    if(!host||!out||!isfinite(in.delta_time)||in.delta_time<=0||!finite3(in.velocity)||!finite3(in.left_foot)||!finite3(in.right_foot))return false;
    PeHaltHost next=*host;PeHaltHostOutput result={0};PeHaltUpdate update;
    next.offset_count=0;
    if(!in.alive){
        /* Death lists Halt in EndImmediately and forbids Move. */
        if(next.action.running&&!pe_halt_cancel(&next.action,&update))return false;
        result.ended=host->action.running;next.moving=false;
        *host=next;*out=result;return true;
    }
    if(next.action.running&&(in.move_requested||in.jump_requested)){
        /* Native control policy: player input interrupts the stop effect
         * before offsets or movement admission are evaluated this step. */
        if(!pe_halt_cancel(&next.action,&update))return false;
        result.ended=update.ended;
    }
    if(next.action.running&&!offsets(&next,in.delta_time,&result.ended))return false;
    bool mask=next.action.mask;
    if(in.jump_requested&&in.grounded&&!mask)next.moving=false;              /* Jump ends Move and Sprint immediately */
    else if(in.move_requested&&in.move_admissible&&!mask)next.moving=true;
    else if(next.moving){
        next.moving=false;
        /* Only a release is Move.EndAction; a lost ground or an action mask ends Move immediately. */
        if(!in.move_requested&&in.grounded&&!in.in_water&&!in.equipment_held){
            PeHaltStart start={.effect_present=true,.motor_present=true,.velocity=in.velocity,.left_foot=in.left_foot,.right_foot=in.right_foot};
            if(!pe_halt_begin(&next.action,start,&update))return false;
            result.started=update.started;
            if(next.action.running&&!offsets(&next,in.delta_time,&result.ended))return false;
        }
    }
    result.block_move=result.block_jump=next.action.mask;
    *host=next;*out=result;return true;
}
