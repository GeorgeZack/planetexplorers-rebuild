#include "pe_animfollow.h"
#include "../generated/pe_animfollow_source.h"
#include <math.h>
#include <string.h>

#define P pe_animfollow_source
static bool finite3(PeVec3 v){return isfinite(v.x)&&isfinite(v.y)&&isfinite(v.z);}
static bool valid(const PeAnimFollow *s){
    return s&&s->initialized&&isfinite(s->torque)&&isfinite(s->force)&&isfinite(s->joint_torque)&&
        isfinite(s->drag)&&isfinite(s->angular_drag)&&isfinite(s->contact_time)&&isfinite(s->no_contact_time)&&s->secondary_update>0;
}
static float lerp(float a,float b,float t){
    t=t<0?0:t>1?1:t;volatile float delta=b-a,product=delta*t,value=a+product;return value;
}
static void blend(PeAnimFollow *s,float torque,float force,float joint,float t){
    s->torque=lerp(s->torque,torque,t);s->force=lerp(s->force,force,t);s->joint_torque=lerp(s->joint_torque,joint,t);
}
static void animator_speed(PeAnimFollowUpdate *u,bool admitted,float factor){
    if(admitted){u->set_animator_speed=true;u->animator_speed=factor;} /* private animatorSpeed is 1 in constructor */
}
static bool idle(PeAnimFollowState s){return s==PE_AF_GROUND_MOVE||s==PE_AF_WATER_MOVE||s==PE_AF_MONSTER_IDLE;}
bool pe_animfollow_state(const char *path,PeAnimFollowState *out){
    if(!path||!out)return false;
    /* HashIDs: 3 idle states, 4 signal names, then 8 get-up states. */
    PeAnimFollowState kind=PE_AF_UNKNOWN;
    for(unsigned i=0;i<3;i++)if(!strcmp(path,pe_animfollow_source_names[i]))kind=(PeAnimFollowState)(i+1);
    for(unsigned i=0;i<8;i++)if(!strcmp(path,pe_animfollow_source_names[7+i]))kind=(PeAnimFollowState)(PE_AF_FRONT+i);
    *out=kind;return true;
}
bool pe_animfollow_get_up_transition(const char *name){
    if(!name)return false;
    for(unsigned i=15;i<23;i++)if(!strcmp(name,pe_animfollow_source_names[i]))return true;
    return false;
}
const char *pe_animfollow_signal_name(PeAnimFollowSignal signal){
    return signal>=PE_AF_FRONT_SIGNAL&&signal<=PE_AF_BACK_MIRROR_SIGNAL?pe_animfollow_source_names[2+(unsigned)signal]:NULL;
}
const char *pe_animfollow_lock_tag(void){return pe_animfollow_source_lock_tag;}
PeAnimFollowBodyPolicy pe_animfollow_body_policy(bool active,bool ragdoll_lock){
    return (PeAnimFollowBodyPolicy){ragdoll_lock||!active,!active,active};
}
bool pe_animfollow_init(PeAnimFollow *s){
    if(!s)return false;
    *s=(PeAnimFollow){.initialized=true,.orientated=true,.torque=P.maxTorque,.force=P.maxForce,.joint_torque=P.maxJointTorque,
        .drag=pe_animfollow_source_drag,.angular_drag=pe_animfollow_source_angularDrag,
        .no_contact_time=10,.secondary_update=pe_animfollow_source_secondary_update};return true;
}
static void active(PeAnimFollow *s,bool value,PeAnimFollowUpdate *u){
    if(s->active!=value){
        s->active=value;u->active_changed=true;u->active=value;
        u->sync_transforms=u->renderer_swap=u->body_policy_changed=true;
    }
}
bool pe_animfollow_activate(PeAnimFollow *s,bool ready,PeAnimFollowUpdate *out){
    if(!valid(s)||!out)return false;
    PeAnimFollowUpdate u={0};s->shot_by_bullet=true;active(s,true,&u);s->get_up_ready=ready;*out=u;return true;
}
bool pe_animfollow_deactivate(PeAnimFollow *s,bool immediately,PeAnimFollowUpdate *out){
    if(!valid(s)||!out)return false;
    PeAnimFollowUpdate u={0};
    if(immediately){s->falling=false;s->getting_up=false;active(s,false,&u);s->wrapper_active=false;u.on_get_up_finished=true;}
    else s->get_up_ready=true;
    *out=u;return true;
}
bool pe_animfollow_step(PeAnimFollow *s,PeAnimFollowTick in,PeAnimFollowUpdate *out){
    if(!out||!valid(s)||!isfinite(in.fixed_delta_time)||in.fixed_delta_time<0||
       !isfinite(in.limb_error_magnitude)||in.limb_error_magnitude<0||!isfinite(in.root_speed)||in.root_speed<0||
       !isfinite(in.collision_speed)||in.collision_speed<0||!isfinite(in.forward_dot_down)||
       !finite3(in.root_position)||!isfinite(in.entity_height)||in.entity_height<0||
       in.current_state<PE_AF_UNKNOWN||in.current_state>PE_AF_BACK_MIRROR_WATER)return false;
    PeAnimFollow next=*s;PeAnimFollowUpdate u={0};
    if(in.missing_assignments){*out=u;return true;}
    if(next.calls==2)u.capture_root_alignment=true;
    next.calls++;
    bool animator=in.animator_present&&in.controller_present;
    if(animator)next.get_up_state=in.current_state>=PE_AF_FRONT;
    next.was_transition_to_get_up=next.transition_to_get_up;
    if(animator)next.transition_to_get_up=in.in_get_up_transition;
    if(next.shot_by_bullet||(in.limb_error_magnitude>P.noGhostLimit2&&next.orientated)){
        if(!next.falling){
            float scale=in.animator_present&&in.current_state!=PE_AF_GROUND_MOVE&&!next.get_up_state?1:P.residualIdleFactor;
            next.torque=P.residualTorque*scale;next.force=P.residualForce*scale;next.joint_torque=P.residualJointTorque*scale;
            u.set_joint_torque=true;u.joint_torque_request=next.joint_torque;
            u.set_joint_limits=true;u.joint_limits=true;next.joint_limits=true;next.secondary_update=100;
            u.restore_friction=true;next.drag=pe_animfollow_source_drag;next.angular_drag=pe_animfollow_source_angularDrag;
            u.on_fall_begin=true;next.wrapper_active=true;
        }
        next.shot_by_bullet=false;next.falling=true;next.getting_up=false;next.orientated=false;
        animator_speed(&u,animator,1);next.delayed_get_up_done=false;
        next.fell_on_speed=in.collision_count>0&&in.collision_speed>P.graceSpeed;
    }else if(next.falling||next.getting_up){
        if(next.getting_up){
            if(next.orientate&&!next.transition_to_get_up&&next.was_transition_to_get_up){
                next.falling=false;u.align_master_horizontal=true;next.orientate=false;next.orientated=true;
                u.zero_friction=true;next.angular_drag=P.getupAngularDrag;next.drag=P.getupDrag;u.on_get_up_begin=true;
            }
            if(next.orientated){
                if(next.torque<P.wakeUpStrength){
                    animator_speed(&u,animator,P.getup1AnimatorSpeedFactor);
                    blend(&next,P.contactTorque,P.contactForce,P.contactJointTorque,P.getupLerp1*in.fixed_delta_time);
                    next.secondary_update=20;
                }else if(!next.transition_to_get_up&&!next.get_up_state){
                    next.angular_drag=pe_animfollow_source_angularDrag;next.drag=pe_animfollow_source_drag;
                    animator_speed(&u,animator,1);next.secondary_update=pe_animfollow_source_secondary_update;u.restore_friction=true;
                    if(in.limb_error_magnitude<P.maxErrorWhenMatching||(in.animator_present&&idle(in.current_state))){
                        next.getting_up=false;next.delayed_get_up_done=false;active(&next,false,&u);u.on_get_up_finished=true;next.wrapper_active=false;
                    }else if(!in.animator_present){
                        /* Source IsIdle(anim) dereferences null after the first
                         * limb-error predicate fails. Do not invent completion. */
                        u.source_missing_animator_fault=true;*s=next;*out=u;return true;
                    }else next.delayed_get_up_done=true;
                }else{
                    animator_speed(&u,in.animator_present,P.getup2AnimatorSpeedFactor);
                    blend(&next,P.contactTorque,P.contactForce,P.contactJointTorque,P.getupLerp2*in.fixed_delta_time);
                    next.secondary_update=2*pe_animfollow_source_secondary_update;
                    if(next.joint_limits){next.joint_limits=false;u.set_joint_limits=true;u.joint_limits=false;}
                }
            }
        }else{
            blend(&next,0,0,0,P.fallLerp*in.fixed_delta_time);u.set_joint_torque=true;u.joint_torque_request=next.joint_torque;
            bool water=in.master_entity_animator_present&&in.root_under_water&&in.no_ground_below;
            u.set_water_bool=in.master_entity_animator_present;u.water_bool=water;u.set_master_position=true;
            u.master_position=in.root_position;if(water)u.master_position.y-=in.entity_height;
            if(in.root_speed<P.settledSpeed){
                if(next.get_up_ready){
                    next.getting_up=true;next.orientate=true;animator_speed(&u,animator,P.masterFallAnimatorSpeedFactor);
                    next.torque=next.force=next.joint_torque=0;
                    if(animator){
                        if(in.forward_dot_down>=0)u.signal=in.current_state==PE_AF_FRONT?PE_AF_FRONT_MIRROR_SIGNAL:PE_AF_FRONT_SIGNAL;
                        else u.signal=in.current_state==PE_AF_BACK?PE_AF_BACK_MIRROR_SIGNAL:PE_AF_BACK_SIGNAL;
                    }
                }
                u.on_fall_finished=true; /* also repeats on every settled tick before readiness */
            }
        }
    }
    /* Contact control runs after lifecycle control, including after a finished
     * get-up. Joint-torque requests above retain their earlier scalar value. */
    u.clear_collision_speed=true;
    bool recover=!(next.getting_up||next.falling)||next.delayed_get_up_done;
    if(in.collision_count){
        next.contact_time+=in.fixed_delta_time;next.no_contact_time=0;
        if(recover)blend(&next,P.contactTorque,P.contactForce,P.contactJointTorque,P.toContactLerp*in.fixed_delta_time);
    }else{
        next.no_contact_time+=in.fixed_delta_time;next.contact_time=0;
        if(recover)blend(&next,P.maxTorque,P.maxForce,P.maxJointTorque,P.fromContactLerp*in.fixed_delta_time);
    }
    if(!valid(&next)||!finite3(u.master_position)||!isfinite(u.joint_torque_request)||!isfinite(u.animator_speed))return false;
    *s=next;*out=u;return true;
}
