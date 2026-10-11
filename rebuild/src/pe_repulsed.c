#include "pe_repulsed.h"
#include "../generated/pe_repulsed_source.h"
#include <math.h>

/* Bounded unweighted, clamped four-key curves. Polynomial ordering follows
 * the separately qualified Unity5 finite-key SSE arithmetic; this module does
 * not evaluate humanoid animation or claim a new engine/motor comparison. */
static float add(float a,float b){volatile float v=a+b;return v;}
static float sub(float a,float b){volatile float v=a-b;return v;}
static float mul(float a,float b){volatile float v=a*b;return v;}
static float divide(float a,float b){volatile float v=a/b;return v;}
static float curve(const PeRepulsedCurveKey keys[4],float t){
    if(t<=keys[0].time)return keys[0].value;
    if(t>=keys[3].time)return keys[3].value;
    unsigned i=t<keys[1].time?0:t<keys[2].time?1:2;
    const PeRepulsedCurveKey *a=&keys[i],*b=&keys[i+1];
    float h=fmaxf(sub(b->time,a->time),1e-4f),delta=sub(b->value,a->value);
    float left=mul(h,a->out_slope),right=mul(h,b->in_slope),inv=divide(1,mul(h,h));
    float cubic=divide(mul(sub(sub(add(left,right),delta),delta),inv),h);
    float quadratic=mul(sub(sub(sub(add(add(delta,delta),delta),left),left),right),inv);
    float x=sub(t,a->time);
    return add(mul(add(mul(add(mul(cubic,x),quadratic),x),a->out_slope),x),a->value);
}
static bool finite3(PeVec3 v){return isfinite(v.x)&&isfinite(v.y)&&isfinite(v.z);}
static PeVec3 scale(PeVec3 v,float n){return (PeVec3){mul(v.x,n),mul(v.y,n),mul(v.z,n)};}
static bool direction_valid(PeVec3 v){
    if(!finite3(v))return false;
    float square=v.x*v.x+v.y*v.y+v.z*v.z;
    return (v.x==0&&v.y==0&&v.z==0)||fabsf(square-1)<=1e-4f;
}
static bool valid(const PeRepulsed *a){
    return a&&direction_valid(a->direction)&&isfinite(a->force)&&a->force>=0&&
        isfinite(a->velocity)&&a->velocity>=0&&isfinite(a->move_time)&&a->move_time>=0&&
        isfinite(a->stop_time)&&a->stop_time>=0&&isfinite(a->wentfly_time)&&a->wentfly_time>=0&&
        isfinite(a->elapsed)&&a->elapsed>=0;
}
PeRepulsedRelation pe_repulsed_relation(void){
    return (PeRepulsedRelation){pe_repulsed_source_forbidden_masks,
        pe_repulsed_source_pause_actions,pe_repulsed_source_end_immediately_actions};
}
bool pe_repulsed_source_admissible(uint64_t masks){
    return !(masks>>56)&&!(masks&pe_repulsed_source_forbidden_masks);
}
bool pe_repulsed_profile(float force,PeRepulsedProfile *out){
    if(!out||!isfinite(force)||force<0)return false;
    PeRepulsedProfile next={curve(pe_repulsed_force_velocity_curve,force),
        curve(pe_repulsed_move_time_curve,force),curve(pe_repulsed_stop_time_curve,force),
        curve(pe_repulsed_wentfly_time_curve,force)};
    if(!isfinite(next.velocity)||next.velocity<0||!isfinite(next.move_time)||next.move_time<0||
       !isfinite(next.stop_time)||next.stop_time<0||!isfinite(next.wentfly_time)||next.wentfly_time<0)return false;
    *out=next;return true;
}
bool pe_repulsed_begin(PeRepulsed *a,PeRepulsedStart in,PeRepulsedUpdate *out){
    if(!out||!valid(a))return false;
    if(a->running){*out=(PeRepulsedUpdate){.repeat_ignored=true};return true;}
    if(!isfinite(in.force)||in.force<0||!isfinite(in.previous_direction_angle)||
       in.previous_direction_angle<0||in.previous_direction_angle>180||
       !direction_valid(in.projected_direction)||!finite3(in.position)||!finite3(in.hit_position))return false;
    PeRepulsed next=*a;PeRepulsedUpdate result={.started=true,.set_mask=true,.mask=true,.set_position=true};
    /* Pinned Unity5 Angle normalizes both operands: its zero-vector dot is0,
     * hence Acos0 ->90 degrees. Preserve this even if a caller used modern
     * zero-angle arithmetic for the default/retained zero direction. */
    float previous_angle=a->direction.x==0&&a->direction.y==0&&a->direction.z==0?90:in.previous_direction_angle;
    next.running=true;next.blocked=in.shield_running&&previous_angle<30;
    next.force=mul(in.force,next.blocked?.5f:1);next.direction=in.projected_direction;
    PeRepulsedProfile profile;if(!pe_repulsed_profile(next.force,&profile))return false;
    next.velocity=profile.velocity;next.move_time=profile.move_time;
    next.stop_time=profile.stop_time;next.wentfly_time=profile.wentfly_time;
    next.elapsed=0;next.stop_applied=false;
    result.position=(PeVec3){divide(add(in.position.x,in.hit_position.x),2),
        divide(add(in.position.y,in.hit_position.y),2),divide(add(in.position.z,in.hit_position.z),2)};
    if(!finite3(result.position))return false;
    result.set_initial_block=in.animator_present;result.block_target=next.blocked?1:0;
    result.apply_motor_force=in.move_motor_present;result.motor_force=scale(next.direction,next.velocity);
    *a=next;*out=result;return true;
}
static void finish(PeRepulsed *a,PeRepulsedTick in,PeRepulsedUpdate *out){
    a->running=false;a->elapsed=a->move_time;
    out->ended=true;out->set_mask=true;out->mask=false;
    out->reset_anim_parameters=in.animator_present;
    out->clear_desired_direction=in.human_phy_present;out->cancel_move_request=in.human_phy_present;
    out->apply_motor_force=in.move_motor_present;out->motor_force=(PeVec3){0};
    out->continue_actions=pe_repulsed_source_pause_actions;
}
bool pe_repulsed_cancel(PeRepulsed *a,PeRepulsedTick in,PeRepulsedUpdate *out){
    if(!out||!valid(a))return false;
    PeRepulsed next=*a;PeRepulsedUpdate result={0};
    if(next.running)finish(&next,in,&result);
    *a=next;*out=result;return true;
}
bool pe_repulsed_step(PeRepulsed *a,PeRepulsedTick in,PeRepulsedUpdate *out){
    if(!out||!valid(a)||!isfinite(in.delta_time)||in.delta_time<0)return false;
    PeRepulsed next=*a;PeRepulsedUpdate result={0};
    if(next.running){
        if(!in.context_present)finish(&next,in,&result);
        else if(in.wentfly_bone_present&&next.elapsed>next.wentfly_time){
            result.request_wentfly=true;result.wentfly_direction=next.direction;
            result.wentfly_force=mul(pe_repulsed_wentfly_force_scale,next.force);
            finish(&next,in,&result);
        }else if(next.elapsed<next.move_time){
            float final=mul(next.velocity,curve(pe_repulsed_time_velocity_curve,divide(next.elapsed,next.move_time)));
            PeVec3 velocity=scale(next.direction,final);
            if(!isfinite(final)||!finite3(velocity))return false;
            result.apply_move_request=in.human_phy_present;result.move_request=velocity;
            result.apply_motor_force=in.move_motor_present;result.motor_force=velocity;
            if(!next.stop_applied&&next.elapsed>next.stop_time){next.stop_applied=true;result.request_halt=true;}
            next.elapsed=add(next.elapsed,in.delta_time);
            if(!isfinite(next.elapsed))return false;
            result.blend_anim_parameters=in.animator_present&&in.human_phy_present;
            result.anim_blend_fraction=fminf(mul(5,in.delta_time),1);
            result.block_target=next.blocked?1:0;
        }else finish(&next,in,&result);
    }
    *a=next;*out=result;return true;
}
