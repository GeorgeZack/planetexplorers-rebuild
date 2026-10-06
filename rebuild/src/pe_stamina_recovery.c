#include "pe_stamina_recovery.h"
#include "../generated/pe_stamina_recovery_source.h"
#include <math.h>

static bool valid(const PeStaminaRecovery *s){
    return isfinite(s->last_consumption)&&s->last_consumption>=0&&isfinite(s->summed_delay)&&s->summed_delay>=0&&
        s->stamina_ticks<UINT64_MAX-1&&s->comfort_ticks<UINT64_MAX-1&&s->delay_ticks<UINT64_MAX-1&&
        (s->initialized||(!s->last_consumption&&!s->summed_delay&&!s->stamina_ticks&&!s->comfort_ticks&&!s->delay_ticks&&!s->frame));
}
static unsigned pulses(uint64_t *index,float elapsed,double interval){
    unsigned count=0;
    if(!*index){++*index;count++;}
    if(elapsed>(float)*index*(float)interval){++*index;count++;}
    return count;
}
bool pe_stamina_recovery_frame(PeStaminaRecovery *state,const PePassiveRecovery *before,
    const PePassiveRecovery *after,PeAttributeFrame frame,PeStaminaVitals *vitals){
    if(!state||!before||!after||!vitals||!valid(state)||!after->initialized||
       !isfinite(frame.now)||frame.now<0||after->last_time!=frame.now||after->frame!=frame.frame||
       state->frame>=frame.frame||state->last_consumption>frame.now||
       !isfinite(vitals->stamina)||!isfinite(vitals->maximum)||vitals->maximum<=0||
       vitals->stamina<0||vitals->stamina>vitals->maximum||!isfinite(vitals->recovery_speed)||vitals->recovery_speed<0||
       !isfinite(vitals->base_delay)||vitals->base_delay<0||!isfinite(vitals->comfort)||
       vitals->comfort<0||vitals->comfort>pe_stamina_recovery_source.comfort_max)return false;
    PeStaminaRecovery next=*state;PeStaminaVitals value=*vitals;
    if(!next.initialized){next.initialized=true;next.summed_delay=value.base_delay;}
    next.frame=frame.frame;
    bool remount=after->mounted&&(!before->mounted||!before->initialized||
        before->mounted_at!=after->mounted_at||before->initialization_checked!=after->initialization_checked);
    if(remount||!after->mounted)next.stamina_ticks=next.comfort_ticks=next.delay_ticks=0;
    /* Removing initial buffs dirties delay6; the next attribute pass loses its
     * temporary modifier, while raw/base delay and comfort remain owned. */
    if(!after->mounted&&frame.attributes_due)next.summed_delay=value.base_delay;
    if(after->mounted&&frame.attributes_due){
        float elapsed=frame.now-after->mounted_at;
        /* Reverse initial-buff order: delay dirty flag, stamina, comfort.
         * Stamina reads the PREVIOUS summed delay, before dirty attributes sum. */
        bool delay_dirty=pulses(&next.delay_ticks,elapsed,pe_stamina_recovery_source.delay_interval_seconds)>0;
        unsigned stamina_count=pulses(&next.stamina_ticks,elapsed,pe_stamina_recovery_source.stamina_interval_seconds);
        for(unsigned i=0;i<stamina_count;i++)if(frame.now>next.last_consumption+
            fmaxf(next.summed_delay,pe_stamina_recovery_source.minimum_delay_seconds))
                value.stamina+=value.recovery_speed*pe_stamina_recovery_source.reduction_percent;
        unsigned comfort_count=pulses(&next.comfort_ticks,elapsed,pe_stamina_recovery_source.comfort_interval_seconds);
        for(unsigned i=0;i<comfort_count;i++)value.comfort+=pe_stamina_recovery_source.comfort_recovery_speed*
            pe_stamina_recovery_source.comfort_reduction_percent*(float)pe_stamina_recovery_source.comfort_interval_seconds;
        /* Ascending attribute order: delay6 sees raw comfort8 BEFORE clamping.
         * The temporary modifier is assignment, not repeated accumulation. */
        if(delay_dirty)next.summed_delay=value.base_delay+
            (1-value.comfort/pe_stamina_recovery_source.comfort_max)*pe_stamina_recovery_source.comfort_delay_scale;
        value.stamina=fminf(value.maximum,value.stamina);
        value.comfort=fminf(pe_stamina_recovery_source.comfort_max,fmaxf(0,value.comfort));
    }
    if(!valid(&next)||!isfinite(value.stamina))return false;
    *state=next;*vitals=value;return true;
}
void pe_stamina_recovery_consume(PeStaminaRecovery *state,float now){
    if(state&&state->initialized)state->last_consumption=now;
}
