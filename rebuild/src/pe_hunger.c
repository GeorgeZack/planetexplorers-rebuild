#include "pe_hunger.h"
#include "../generated/pe_hunger_source.h"
#include <math.h>
#include <stddef.h>

static bool valid(const PeHunger *h){
    const float max=pe_hunger_source.hunger_max;
    return isfinite(h->raw)&&isfinite(h->hunger)&&h->hunger>=0&&h->hunger<=max&&
        isfinite(h->start_elapsed)&&h->start_elapsed>=0&&isfinite(h->end_elapsed)&&h->end_elapsed>=0&&
        isfinite(h->food_cooldown)&&h->food_cooldown>=0&&h->food_cooldown<=pe_hunger_source.food_cooldown_seconds&&
        h->drain_ticks<UINT64_MAX-1;
}
static float clamp_hunger(float v){return fminf(pe_hunger_source.hunger_max,fmaxf(0,v));}
/* HitAttr.GetRate: zero outside [min,max], else Lerp(minRate,maxRate,t). */
static float rate(float v,float lo,float hi,float lo_rate,float hi_rate){
    if(v<lo||v>hi)return 0;
    float t=(v-lo)/(hi-lo);t=t<0?0:t>1?1:t;
    return lo_rate+(hi_rate-lo_rate)*t;
}
/* SkBuff.Exec: the first evaluation at once, then at most one overdue interval per pass. */
static unsigned pulses(uint64_t *index,float elapsed,double interval){
    unsigned count=0;
    if(!*index){++*index;count++;}
    if(elapsed>(float)*index*(float)interval){++*index;count++;}
    return count;
}
void pe_hunger_fresh(PeHunger *h){
    if(h)*h=(PeHunger){.raw=pe_hunger_source.initial_hunger,.hunger=pe_hunger_source.initial_hunger};
}
bool pe_hunger_loaded(PeHunger *h,float value,bool starving){
    if(!h||!isfinite(value)||value<0||value>pe_hunger_source.hunger_max)return false;
    *h=(PeHunger){.raw=value,.hunger=value,.starving=starving};return true;
}
bool pe_hunger_frame(PeHunger *h,const PePassiveRecovery *before,const PePassiveRecovery *after,
    PeAttributeFrame frame,float dt,bool alive,PeHungerEvent *event){
    if(event)*event=PE_HUNGER_NO_CHANGE;
    if(!h||!before||!after||!valid(h)||!after->initialized||!isfinite(frame.now)||frame.now<0||
       after->last_time!=frame.now||after->frame!=frame.frame||h->frame>=frame.frame||!isfinite(dt)||dt<0)return false;
    PeHunger next=*h;PeHungerEvent happened=PE_HUNGER_NO_CHANGE;
    next.initialized=true;next.frame=frame.frame;
    bool remount=after->mounted&&(!before->mounted||!before->initialized||
        before->mounted_at!=after->mounted_at||before->initialization_checked!=after->initialization_checked);
    if(remount||!after->mounted)next.drain_ticks=0;
    if(frame.attributes_due){
        if(after->mounted){
            unsigned n=pulses(&next.drain_ticks,frame.now-after->mounted_at,pe_hunger_source.drain_interval_seconds);
            for(unsigned i=0;i<n;i++)next.raw+=pe_hunger_source.recovery_speed*pe_hunger_source.reduction_percent*pe_hunger_source.drain_scale;
        }
        /* Dirty raw sums (no modifiers on 16), then the current/max pair clamps it. */
        next.hunger=next.raw=clamp_hunger(next.raw);
    }
    /* PEAbnormal_N.Update: check the running timer, then accumulate this frame. */
    if(next.starving){
        bool hit=next.end_elapsed>=pe_hunger_source.end_interval_seconds;
        if(hit)next.end_elapsed=0;
        if(hit&&rate(next.hunger,pe_hunger_source.end_minimum,pe_hunger_source.end_maximum,
                     pe_hunger_source.end_rate,pe_hunger_source.end_rate)>0){
            next.starving=false;next.start_elapsed=next.end_elapsed=0;happened=PE_HUNGER_ENDED;
        }
    }else{
        bool hit=next.start_elapsed>=pe_hunger_source.start_interval_seconds;
        if(hit)next.start_elapsed=0;
        if(hit&&alive&&rate(next.hunger,pe_hunger_source.start_minimum,pe_hunger_source.start_maximum,
                            pe_hunger_source.start_rate,pe_hunger_source.start_rate)>0){
            next.starving=true;next.start_elapsed=next.end_elapsed=0;happened=PE_HUNGER_STARTED;
        }
    }
    next.start_elapsed+=dt;next.end_elapsed+=dt;
    next.food_cooldown=fmaxf(0,next.food_cooldown-dt);
    if(!valid(&next))return false;
    *h=next;if(event)*event=happened;return true;
}
static const PeHungerFoodSource *food(int item_id){
    for(int i=0;i<PE_HUNGER_SOURCE_FOODS;i++)if(pe_hunger_source_foods[i].item_id==item_id)return &pe_hunger_source_foods[i];
    return NULL;
}
bool pe_hunger_is_food(int item_id){return food(item_id)!=NULL;}
bool pe_hunger_eat(PeHunger *h,int item_id,PeHungerMeal *meal){
    const PeHungerFoodSource *f=food(item_id);
    if(!h||!f||!valid(h)||!h->initialized||h->food_cooldown>0)return false;
    PeHunger next=*h;
    next.raw+=f->hunger;   /* exp,16: raw += gain; the pair clamps at the next pass */
    next.food_cooldown=pe_hunger_source.food_cooldown_seconds;
    if(!valid(&next))return false;
    *h=next;
    if(meal)*meal=(PeHungerMeal){f->item_id,f->hunger,f->nutrient,f->nutrient_gain};
    return true;
}
PeHungerStarvation pe_hunger_starvation(const PeHunger *h){
    if(!h||!h->starving)return (PeHungerStarvation){0};
    return (PeHungerStarvation){pe_hunger_source.stamina_reduce_percent_add,pe_hunger_source.stamina_recover_speed_add,
        pe_hunger_source.stamina_recover_delay_add,pe_hunger_source.comfort_reduce_percent_add,pe_hunger_source.oxygen_reduce_percent_add};
}
