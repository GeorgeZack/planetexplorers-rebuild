#include "pe_hit_reaction.h"
#include "../generated/pe_hit_reaction_source.h"
#include <math.h>
#include <string.h>

/* Same qualified finite-key SSE polynomial ordering as attack playback.
 * Local evaluation is bounded to the two pinned three-key angle curves. */
static float add(float a,float b){volatile float v=a+b;return v;}
static float sub(float a,float b){volatile float v=a-b;return v;}
static float mul(float a,float b){volatile float v=a*b;return v;}
static float divide(float a,float b){volatile float v=a/b;return v;}
static float curve(const PeHitReactionCurveKey keys[3],float t){
    if(t<=keys[0].time)return keys[0].value;
    if(t>=keys[2].time)return keys[2].value;
    unsigned i=t<keys[1].time?0:1;
    const PeHitReactionCurveKey *a=&keys[i],*b=&keys[i+1];
    float h=fmaxf(sub(b->time,a->time),1e-4f),delta=sub(b->value,a->value);
    float left=mul(h,a->out_slope),right=mul(h,b->in_slope),inv=divide(1,mul(h,h));
    float cubic=divide(mul(sub(sub(add(left,right),delta),delta),inv),h);
    float quadratic=mul(sub(sub(sub(add(add(delta,delta),delta),left),left),right),inv);
    float x=sub(t,a->time);
    return add(mul(add(mul(add(mul(cubic,x),quadratic),x),a->out_slope),x),a->value);
}
bool pe_hit_reaction_player_thresholds(bool shield,float out[4]){
    if(!out)return false;
    memcpy(out,shield?pe_hit_reaction_shield_thresholds:pe_hit_reaction_base_thresholds,4*sizeof *out);
    return true;
}
bool pe_hit_reaction_hare_force(int id,float *force){
    if(!force)return false;
    for(unsigned i=0;i<3;i++)if(pe_hit_reaction_triggers[i].trigger_id==id){
        *force=pe_hit_reaction_triggers[i].force;return true;
    }
    return false;
}
bool pe_hit_reaction_decide(PeHitReactionInput in,PeHitReactionDecision *out){
    if(!out||!isfinite(in.angle_degrees)||in.angle_degrees<0||in.angle_degrees>180||
       !isfinite(in.force)||in.force<0)return false;
    for(unsigned i=0;i<4;i++)if(!isfinite(in.thresholds[i])||in.thresholds[i]<0)return false;
    PeHitReactionDecision next={0};
    next.threshold_scale=curve(pe_hit_reaction_threshold_curve,in.angle_degrees);
    next.scaled_force=mul(in.force,curve(pe_hit_reaction_force_curve,in.angle_degrees));
    if(in.in_air)next.threshold_scale=mul(next.threshold_scale,pe_hit_reaction_air_scale);
    if(!isfinite(next.scaled_force)||!isfinite(next.threshold_scale))return false;
    unsigned effect=4;
    for(unsigned i=0;i<4;i++){
        float threshold=mul(in.thresholds[i],next.threshold_scale);
        if(!isfinite(threshold))return false;
        if(next.scaled_force<threshold){effect=i;break;}
    }
    next.effect=(PeHitReactionEffect)effect;
    next.requested_action=effect==1?5:effect==2?6:effect>=3?7:0;
    next.request_hit_ik=effect==0||effect==2;
    *out=next;return true;
}
