#include "pe_hit_ik.h"
#include "../generated/pe_hit_ik_source.h"
#include <math.h>
_Static_assert(sizeof pe_hit_ik_parts/sizeof pe_hit_ik_parts[0]==PE_HIT_IK_PARTS,"hit part count");
/* Finite-key order qualified for Unity5; not claimed bit-exact with Mono. */
static float add(float a,float b){volatile float v=a+b;return v;}
static float sub(float a,float b){volatile float v=a-b;return v;}
static float mul(float a,float b){volatile float v=a*b;return v;}
static float divide(float a,float b){volatile float v=a/b;return v;}
static float curve(PeHitIkCurve c,float t){
    if(t<=c.keys[0].time)return c.keys[0].value;
    if(t>=c.keys[c.count-1].time)return c.keys[c.count-1].value;
    unsigned i=0;
    while(i+2<c.count&&t>=c.keys[i+1].time)i++;
    const PeHitIkKey *a=&c.keys[i],*b=&c.keys[i+1];
    float h=fmaxf(sub(b->time,a->time),1e-4f),delta=sub(b->value,a->value);
    float left=mul(h,a->out_slope),right=mul(h,b->in_slope),inv=divide(1,mul(h,h));
    float cubic=divide(mul(sub(sub(add(left,right),delta),delta),inv),h);
    float quadratic=mul(sub(sub(sub(add(add(delta,delta),delta),left),left),right),inv);
    float x=sub(t,a->time);
    return add(mul(add(mul(add(mul(cubic,x),quadratic),x),a->out_slope),x),a->value);
}

static bool finite3(PeVec3 v){return isfinite(v.x)&&isfinite(v.y)&&isfinite(v.z);}
static bool valid(const PeHitIk *s){
    if(!s)return false;
    for(unsigned i=0;i<PE_HIT_IK_PARTS;i++){
        const PeHitIkEffect *e=&s->parts[i];
        if(!finite3(e->direction)||!finite3(e->root_start)||!isfinite(e->weight)||
           !isfinite(e->duration)||e->duration<0||!isfinite(e->remaining)||e->remaining>e->duration)return false;
    }
    return true;
}
bool pe_hit_ik_profile(float force,float draw,PeHitIkProfile *out){
    if(!out||!isfinite(force)||force<0||!isfinite(draw)||fabsf(draw)>pe_hit_ik_random_scale)return false;
    float randomized=mul(force,add(1,draw));
    if(!isfinite(randomized))return false;
    PeHitIkProfile next={randomized,curve(pe_hit_ik_weight_curve,randomized),curve(pe_hit_ik_time_curve,randomized)};
    if(!isfinite(next.weight)||!isfinite(next.duration)||next.duration<0)return false;
    *out=next;return true;
}
bool pe_hit_ik_begin(PeHitIk *s,PeHitIkStart in,PeHitIkUpdate *out){
    if(!out||!valid(s)||!finite3(in.direction)||!finite3(in.root_position)||!isfinite(in.weight)||
       !isfinite(in.duration)||in.duration<0||!isfinite(in.weight_scale))return false;
    PeHitIk next=*s;PeHitIkUpdate result={0};
    if(in.controller_present&&in.solver_present){
        float weight=mul(in.weight,in.weight_scale);
        if(!isfinite(weight))return false;
        for(unsigned i=0;i<PE_HIT_IK_PARTS;i++){
            const PeHitIkPart *p=&pe_hit_ik_parts[i];bool match=false;
            for(unsigned j=0;j<p->transform_count;j++)if(p->transforms[j]==in.transform_id)match=true;
            if(match){next.parts[i]=(PeHitIkEffect){in.direction,in.root_position,weight,in.duration,in.duration};result.matched_parts++;}
        }
    }
    for(unsigned i=0;i<PE_HIT_IK_PARTS;i++)if(next.parts[i].remaining>0)result.active_parts++;
    *s=next;*out=result;return true;
}
bool pe_hit_ik_step(PeHitIk *s,PeHitIkTick in,PeHitIkUpdate *out){
    if(!out||!valid(s)||!finite3(in.root_position)||!isfinite(in.delta_time)||in.delta_time<0||
       !isfinite(in.master_weight)||!isfinite(in.weight_scale))return false;
    PeHitIk next=*s;PeHitIkUpdate result={0};
    if(in.controller_present&&in.solver_present&&in.enabled&&in.master_weight>0&&in.delta_time>0){
        float master=in.master_weight>1?1:in.master_weight;
        for(unsigned i=0;i<PE_HIT_IK_PARTS;i++){
            PeHitIkEffect *e=&next.parts[i];if(!(e->remaining>0))continue;
            float elapsed=sub(e->duration,e->remaining),progress=divide(elapsed,e->duration);
            float fade=fminf(1,divide(elapsed,.10000000149f));
            float scale=mul(mul(mul(master,in.weight_scale),fade),e->weight);
            PeVec3 delta={sub(in.root_position.x,e->root_start.x),sub(in.root_position.y,e->root_start.y),sub(in.root_position.z,e->root_start.z)};
            if(!isfinite(progress)||!isfinite(scale)||!finite3(delta))return false;
            const PeHitIkPart *p=&pe_hit_ik_parts[i];
            for(unsigned j=0;j<p->link_count;j++){
                const PeHitIkLink *l=&p->links[j];
                float force=curve(l->force,progress),up=curve(l->up,progress);
                PeVec3 v={mul(sub(mul(force,e->direction.x),l->pin?delta.x:0),scale),
                          mul(sub(add(mul(force,e->direction.y),up),l->pin?delta.y:0),scale),
                          mul(sub(mul(force,e->direction.z),l->pin?delta.z:0),scale)};
                PeVec3 *sum=&result.offsets[l->effector];
                sum->x=add(sum->x,v.x);sum->y=add(sum->y,v.y);sum->z=add(sum->z,v.z);
                if(!finite3(*sum))return false;
            }
            e->remaining=sub(e->remaining,in.delta_time);
            if(!isfinite(e->remaining))return false;
        }
    }
    for(unsigned i=0;i<PE_HIT_IK_PARTS;i++)if(next.parts[i].remaining>0)result.active_parts++;
    *s=next;*out=result;return true;
}
