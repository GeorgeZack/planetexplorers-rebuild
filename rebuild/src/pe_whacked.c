#include "pe_whacked.h"
#include "../generated/pe_reaction_presentation_source.h"
#include <math.h>

#define LEAVES (sizeof pe_whacked_source_leaves/sizeof *pe_whacked_source_leaves)

static bool valid(const PeWhacked *a){
    return a&&isfinite(a->normalized_time)&&a->normalized_time>=0&&(a->running||!a->presenting);
}
PeWhackedRelation pe_whacked_relation(void){
    return (PeWhackedRelation){pe_whacked_source_forbidden_masks,
        pe_whacked_source_pause_actions,pe_whacked_source_end_immediately_actions};
}
bool pe_whacked_source_admissible(uint64_t masks){
    return !(masks>>56)&&!(masks&pe_whacked_source_forbidden_masks);
}
static void select(float style,PeWhackedUpdate *out){
    /* Thresholds are the integers 0..10; values outside clamp to the end leaves. */
    float top=(float)(LEAVES-1),value=style<0?0:style>top?top:style;
    unsigned low=(unsigned)value;
    if(low>=LEAVES-1)low=LEAVES-2;
    float weight=value-(float)low;
    out->clip_a=pe_whacked_source_leaves[low].clip;out->clip_b=pe_whacked_source_leaves[low+1].clip;
    out->weight_b=weight;
    out->state_length=pe_whacked_source_leaves[low].length*(1-weight)+pe_whacked_source_leaves[low+1].length*weight;
}
bool pe_whacked_selection(float style,PeWhackedUpdate *out){
    if(!out||!isfinite(style))return false;
    PeWhackedUpdate result={0};select(style,&result);*out=result;return true;
}
static void finish(PeWhacked *a,bool animator,PeWhackedUpdate *out){
    a->running=a->presenting=false;a->mask=false;a->normalized_time=0;
    out->ended=true;out->set_mask=true;out->mask=false;
    out->set_trigger_reset_full_body=animator;out->reset_trigger_be_hit=animator;
    out->continue_actions=pe_whacked_source_pause_actions;
}
bool pe_whacked_begin(PeWhacked *a,bool animator,PeWhackedUpdate *out){
    if(!out||!valid(a))return false;
    if(a->running){*out=(PeWhackedUpdate){.repeat_ignored=true};return true;}
    a->running=true;a->mask=true;a->presenting=animator;a->normalized_time=0;
    *out=(PeWhackedUpdate){.started=true,.set_mask=true,.mask=true,.set_trigger_be_hit=animator};
    return true;
}
bool pe_whacked_cancel(PeWhacked *a,bool animator,PeWhackedUpdate *out){
    if(!out||!valid(a))return false;
    PeWhackedUpdate result={0};
    if(a->running)finish(a,animator,&result);
    *out=result;return true;
}
bool pe_whacked_step(PeWhacked *a,PeWhackedTick in,PeWhackedUpdate *out){
    if(!out||!valid(a)||!isfinite(in.delta_time)||in.delta_time<0||!isfinite(in.move_style))return false;
    PeWhacked next=*a;PeWhackedUpdate result={0};
    if(next.running){
        if(!in.animator_present){
            next.running=next.presenting=false;next.normalized_time=0;
            result.ended=true;result.continue_actions=pe_whacked_source_pause_actions;
        }else{
            select(in.move_style,&result);
            if(!next.presenting){
                /* The animator appeared after DoAction: the trigger was never set. */
                result.state_length=0;result.clip_a=result.clip_b=0;result.weight_b=0;
            }else{
                next.normalized_time+=in.delta_time/result.state_length;
                if(!isfinite(next.normalized_time))return false;
                float end=pe_whacked_source_exit_time+pe_whacked_source_exit_duration;
                float blend=(next.normalized_time-pe_whacked_source_exit_time)/pe_whacked_source_exit_duration;
                result.presenting=true;
                result.normalized_time=next.normalized_time>end?end:next.normalized_time;
                result.exit_blend=blend<0?0:blend>1?1:blend;
                if(next.normalized_time>=end){
                    result.hit_anim_end=true;result.presenting=false;
                    finish(&next,true,&result);
                }
            }
        }
    }
    *a=next;*out=result;return true;
}
