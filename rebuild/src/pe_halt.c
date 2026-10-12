#include "pe_halt.h"
#include "../generated/pe_halt_source.h"
#include <math.h>

/* Clamped Unity curve with the finite-key operation order qualified for the
 * Repulsed curves, generalized to any key count. A segment uses its left key's
 * out-slope and its right key's in-slope. Not claimed bit-exact with Mono. */
static float add(float a,float b){volatile float v=a+b;return v;}
static float sub(float a,float b){volatile float v=a-b;return v;}
static float mul(float a,float b){volatile float v=a*b;return v;}
static float divide(float a,float b){volatile float v=a/b;return v;}
static float curve(PeHaltCurve c,float t){
    if(t<=c.keys[0].time)return c.keys[0].value;
    if(t>=c.keys[c.count-1].time)return c.keys[c.count-1].value;
    unsigned i=0;
    while(i+2<c.count&&t>=c.keys[i+1].time)i++;
    const PeHaltCurveKey *a=&c.keys[i],*b=&c.keys[i+1];
    float h=fmaxf(sub(b->time,a->time),1e-4f),delta=sub(b->value,a->value);
    float left=mul(h,a->out_slope),right=mul(h,b->in_slope),inv=divide(1,mul(h,h));
    float cubic=divide(mul(sub(sub(add(left,right),delta),delta),inv),h);
    float quadratic=mul(sub(sub(sub(add(add(delta,delta),delta),left),left),right),inv);
    float x=sub(t,a->time);
    return add(mul(add(mul(add(mul(cubic,x),quadratic),x),a->out_slope),x),a->value);
}
static bool finite3(PeVec3 v){return isfinite(v.x)&&isfinite(v.y)&&isfinite(v.z);}
static PeVec3 normalized(PeVec3 v){
    float m=sqrtf(v.x*v.x+v.y*v.y+v.z*v.z);
    return m>9.99999975e-06f?(PeVec3){v.x/m,v.y/m,v.z/m}:(PeVec3){0};
}
static bool valid(const PeHalt *a){
    return a&&finite3(a->direction)&&isfinite(a->weight)&&isfinite(a->step_time)&&a->step_time>=0&&
        isfinite(a->remaining)&&a->remaining<=a->step_time;
}
PeHaltRelation pe_halt_relation(void){
    return (PeHaltRelation){pe_halt_source_forbidden_masks,pe_halt_source_end_immediately_actions};
}
bool pe_halt_source_admissible(uint64_t masks){
    return !(masks>>56)&&!(masks&pe_halt_source_forbidden_masks);
}
bool pe_halt_profile(float speed,float *weight,float *step_time){
    if(!weight||!step_time||!isfinite(speed)||speed<0)return false;
    float w=curve(pe_halt_source_speed_to_weight,speed),t=curve(pe_halt_source_speed_to_time,speed);
    if(!isfinite(w)||!isfinite(t)||t<0)return false;
    *weight=w;*step_time=t;return true;
}
bool pe_halt_begin(PeHalt *a,PeHaltStart in,PeHaltUpdate *out){
    if(!out||!valid(a)||!finite3(in.velocity)||!finite3(in.left_foot)||!finite3(in.right_foot))return false;
    if(a->running){*out=(PeHaltUpdate){.repeat_ignored=true};return true;}
    PeHalt next=*a;
    if(in.effect_present&&in.motor_present){
        /* StopMove projects on the ground plane; the foot test uses unit
         * vectors, so a zero motion or coincident feet read as 90 degrees: left. */
        PeVec3 flat={in.velocity.x,0,in.velocity.z},unit=normalized(flat);
        PeVec3 feet=normalized((PeVec3){in.left_foot.x-in.right_foot.x,in.left_foot.y-in.right_foot.y,in.left_foot.z-in.right_foot.z});
        float cosine=feet.x*unit.x+feet.y*unit.y+feet.z*unit.z;
        cosine=cosine<-1?-1:cosine>1?1:cosine;
        float weight,time;
        if(!pe_halt_profile(sqrtf(flat.x*flat.x+flat.z*flat.z),&weight,&time))return false;
        next.right_foot=(float)acos((double)cosine)*57.29578f>90;
        next.direction=unit;next.weight=weight;next.step_time=time;next.remaining=time;
        /* Starting one foot's effect leaves the other foot's untouched; only
         * one can be live here because EndImmediately clears both. */
        next.effect_running=time>0;
    }
    next.running=true;next.mask=true;
    *a=next;*out=(PeHaltUpdate){.started=true,.set_mask=true,.mask=true};return true;
}
static void finish(PeHalt *a,PeHaltUpdate *out){
    a->running=false;a->mask=false;out->ended=true;out->set_mask=true;out->mask=false;
}
bool pe_halt_cancel(PeHalt *a,PeHaltUpdate *out){
    if(!out||!valid(a))return false;
    PeHaltUpdate result={0};
    a->remaining=0;a->effect_running=false; /* EndMoveEffect runs even when the action is not held */
    if(a->running)finish(a,&result);
    *out=result;return true;
}
bool pe_halt_step(PeHalt *a,PeHaltTick in,PeHaltUpdate *out){
    if(!out||!valid(a)||!isfinite(in.delta_time)||in.delta_time<0||!isfinite(in.master_weight))return false;
    PeHalt next=*a;PeHaltUpdate result={0};
    bool effect_live=next.effect_running&&next.remaining>0;
    if(next.running&&!(in.effect_present&&in.motor_present&&in.effect_enabled&&in.master_weight>0&&effect_live))
        finish(&next,&result);
    /* Solver pre-update. It runs whether or not the action is still held. */
    if(in.effect_present&&in.effect_enabled&&in.master_weight>0&&in.delta_time>0&&effect_live){
        float master=in.master_weight>1?1:in.master_weight;
        float elapsed=next.step_time-next.remaining;
        float fade=elapsed/(next.right_foot?pe_halt_source_right_fade_time:pe_halt_source_left_fade_time);
        fade=fade<0?0:fade>1?1:fade;
        float progress=elapsed/next.step_time,scale=master*fade*next.weight;
        const PeHaltLink *links=next.right_foot?pe_halt_source_right_links:pe_halt_source_left_links;
        for(unsigned i=0;i<PE_HALT_LINKS;i++){
            float force=curve(links[i].force,progress),up=curve(links[i].up,progress);
            PeVec3 offset={force*next.direction.x*scale,(force*next.direction.y+up)*scale,force*next.direction.z*scale};
            if(!finite3(offset))return false;
            result.offsets[i]=(PeHaltOffset){links[i].effector,offset};
        }
        result.offset_count=PE_HALT_LINKS;result.progress=progress;
        next.remaining-=in.delta_time;
        if(!(next.remaining>0))next.effect_running=false;
    }
    *a=next;*out=result;return true;
}
