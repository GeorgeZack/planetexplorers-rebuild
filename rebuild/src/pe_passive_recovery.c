#include "pe_passive_recovery.h"
#include "../generated/pe_passive_recovery_source.h"
#include <math.h>
#include <stddef.h>

bool pe_attribute_clock_step(PeAttributeClock *clock,double dt,PeAttributeFrame *out){
    if(!clock||!out||!isfinite(dt)||dt<0||!isfinite(clock->seconds)||clock->seconds<0||
       clock->seconds+dt>1e12||clock->frame==UINT64_MAX||clock->attribute_frame>clock->frame)return false;
    PeAttributeClock next=*clock;
    next.seconds+=dt;next.frame++;
    bool due=next.frame-next.attribute_frame>=pe_passive_recovery_source.attribute_frame_interval;
    if(due)next.attribute_frame=next.frame;
    *out=(PeAttributeFrame){(float)next.seconds,next.frame,due};*clock=next;return true;
}
static bool valid(const PePassiveRecovery *s){
    if(!s->initialized)return !s->mounted&&!s->initialization_checked&&!s->evaluations&&!s->frame&&
        s->created==0&&s->mounted_at==0&&s->last_hurt==0&&s->last_time==0;
    return isfinite(s->created)&&s->created>=0&&isfinite(s->mounted_at)&&s->mounted_at>=s->created&&
        isfinite(s->last_hurt)&&s->last_hurt>=0&&isfinite(s->last_time)&&s->last_time>=s->mounted_at&&
        s->last_hurt<=s->last_time&&s->evaluations<UINT64_MAX;
}
static void evaluate(const PePassiveRecovery *s,float now,float *hp,float max_hp){
    if(now>=s->last_hurt+(float)pe_passive_recovery_source.cooldown_seconds&&
       *hp/max_hp<pe_passive_recovery_source.threshold_fraction)
        *hp=fminf(max_hp,*hp+max_hp*pe_passive_recovery_source.heal_fraction);
}
bool pe_passive_recovery_frame(PePassiveRecovery *state,PeAttributeFrame frame,float *hp,float max_hp){
    if(!state||!hp||!valid(state)||!isfinite(*hp)||!isfinite(max_hp)||max_hp<=0||
       *hp<0||*hp>max_hp||!isfinite(frame.now)||frame.now<0||!frame.frame||
       (state->initialized&&(frame.now<state->last_time||frame.frame<=state->frame)))return false;
    PePassiveRecovery next=*state;float life=*hp;
    if(!next.initialized){
        next.initialized=true;next.created=next.mounted_at=frame.now;next.mounted=life>0&&!next.loaded;
    }
    next.last_time=frame.now;next.frame=frame.frame;
    /* Source delayed CheckInitAttr replaces a same-ID stack-limit-one buff.
     * Dispatch before the attribute pass when both fall on this native frame. */
    if(!next.initialization_checked&&frame.now>=next.created+(float)pe_passive_recovery_source.initialization_check_delay_seconds){
        next.initialization_checked=true;next.mounted=life>0;next.mounted_at=frame.now;next.evaluations=0;
    }
    if(life<=0){next.mounted=false;next.evaluations=0;}
    if(next.mounted&&frame.attributes_due){
        if(next.evaluations==0){evaluate(&next,frame.now,&life,max_hp);next.evaluations++;}
        /* Deliberately not a loop: one overdue periodic evaluation per updater
         * pass, in addition to the initial execution if it was deferred. */
        if(frame.now-next.mounted_at>(float)next.evaluations*(float)pe_passive_recovery_source.interval_seconds){
            evaluate(&next,frame.now,&life,max_hp);next.evaluations++;
        }
    }
    if(!valid(&next))return false;
    *state=next;*hp=life;return true;
}
void pe_passive_recovery_hurt(PePassiveRecovery *state,float hp){
    if(!state||!state->initialized)return;
    state->last_hurt=state->last_time;
    if(hp<=0){state->mounted=false;state->evaluations=0;}
}

bool pe_passive_recovery_revive(PePassiveRecovery *state,PeAttributeFrame frame){
    if(!state||!valid(state)||!state->initialized||state->mounted||
       !isfinite(frame.now)||frame.now<0||!frame.frame||
       state->last_time!=frame.now||state->frame!=frame.frame)return false;
    PePassiveRecovery next=*state;
    next.mounted=true;next.mounted_at=frame.now;next.evaluations=0;
    *state=next;return true;
}
