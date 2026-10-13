#include "pe_animfollow_drive.h"
#include "../generated/pe_animfollow_drive_source.h"
#include <limits.h>
#include <math.h>
#include <string.h>
#define BODY_MASK ((1u<<PE_AF_DRIVE_BODIES)-1u)
_Static_assert(PE_AF_DRIVE_BODIES==PE_AF_DRIVE_SOURCE_BODIES,"Female drive profile count");
static bool finite3(PeVec3 v){return isfinite(v.x)&&isfinite(v.y)&&isfinite(v.z);}
static bool nonnegative(float v){return isfinite(v)&&v>=0;}
static bool valid(const PeAnimFollowDrive *s){
    if(!s||!s->initialized||(s->joint_mask&~BODY_MASK)||!nonnegative(s->joint_torque)||!nonnegative(s->joint_damping)||
       !nonnegative(s->drive_spring)||!nonnegative(s->drive_damper)||!nonnegative(s->drive_maximum_force)||
       !isfinite(s->reciprocal_fixed_delta_time)||s->reciprocal_fixed_delta_time<=0||
       !finite3(s->total_torque_error)||!finite3(s->total_force_error)||!finite3(s->torque_signal)||!finite3(s->force_signal))return false;
    for(unsigned i=0;i<PE_AF_DRIVE_BODIES;i++)if(!finite3(s->torque_last_error[i])||!finite3(s->force_last_error[i]))return false;
    return true;
}
static PeVec3 add(PeVec3 a,PeVec3 b){return (PeVec3){a.x+b.x,a.y+b.y,a.z+b.z};}
static PeVec3 sub(PeVec3 a,PeVec3 b){return (PeVec3){a.x-b.x,a.y-b.y,a.z-b.z};}
static PeVec3 scale(PeVec3 a,float b){return (PeVec3){a.x*b,a.y*b,a.z*b};}
static bool pd(float p,float d,PeVec3 error,PeVec3 *last,float reciprocal,PeVec3 *signal){
    /* Explicit source operator order; never replace derivative with velocity. */
    PeVec3 change=sub(error,*last),damped=scale(change,d),rate=scale(damped,reciprocal),sum=add(error,rate),result=scale(sum,p);
    if(!finite3(change)||!finite3(damped)||!finite3(rate)||!finite3(sum)||!finite3(result))return false;
    *last=error;*signal=result;return true;
}
static bool clamp(PeVec3 value,float cap,PeVec3 *out){
    if(!finite3(value)||!nonnegative(cap))return false;
    /* Double norm is a labelled native guard against intermediate overflow. */
    double n=sqrt((double)value.x*value.x+(double)value.y*value.y+(double)value.z*value.z);
    *out=n>cap&&n>0?scale(value,(float)(cap/n)):value;return finite3(*out);
}
bool pe_animfollow_drive_binding(unsigned i,PeAnimFollowDriveBinding *out){
    if(!out||i>=PE_AF_DRIVE_BODIES)return false;
    const PeAfDriveSourceBody *p=&pe_af_drive_source_bodies[i];
    *out=(PeAnimFollowDriveBinding){p->master,p->slave,p->body,p->joint};return true;
}
static bool joints(PeAnimFollowDrive *s,float torque,bool with_damping,float damping,PeAnimFollowDriveUpdate *u){
    for(unsigned i=1;i<PE_AF_DRIVE_BODIES;i++)if(s->joint_mask&(1u<<i)){
        const PeAfDriveSourceBody *p=&pe_af_drive_source_bodies[i];
        s->drive_spring=torque*p->maxJointTorqueProfile;
        if(with_damping)s->drive_damper=damping*p->jointDampingProfile;
        if(!nonnegative(s->drive_spring)||!nonnegative(s->drive_damper))return false;
        u->joints[i]=(PeAnimFollowJointRequest){true,s->drive_spring,s->drive_damper,s->drive_maximum_force,1};
    }
    s->joint_torque=torque;if(with_damping)s->joint_damping=damping;return true;
}
bool pe_animfollow_drive_init(PeAnimFollowDrive *s,float dt){
    if(!s||!isfinite(dt)||dt<=0||!isfinite(1/dt))return false;
    PeAnimFollowDrive next={.initialized=true,.torque_enabled=true,.force_enabled=true,.mimic_nonrigids=true,
        .joint_mask=BODY_MASK&~1u,.reciprocal_fixed_delta_time=1/dt,
        .joint_torque=pe_af_drive_source_maxJointTorque,.joint_damping=pe_af_drive_source_jointDamping,
        .drive_damper=pe_af_drive_source_initial_damper,.drive_maximum_force=pe_af_drive_source_maximum_force};
    PeAnimFollowDriveUpdate u={0};if(!joints(&next,next.joint_torque,false,0,&u)||!valid(&next))return false;
    *s=next;return true;
}
bool pe_animfollow_drive_set_joints(PeAnimFollowDrive *s,float torque,bool with_damping,float damping,PeAnimFollowDriveUpdate *out){
    if(!out||!valid(s)||!nonnegative(torque)||(with_damping&&!nonnegative(damping)))return false;
    PeAnimFollowDrive next=*s;PeAnimFollowDriveUpdate u={0};
    if(!joints(&next,torque,with_damping,damping,&u))return false;
    *s=next;*out=u;return true;
}
bool pe_animfollow_drive_joint_limits(const PeAnimFollowDrive *s,bool enabled,uint16_t *mask,unsigned *motion){
    if(!valid(s)||!mask||!motion)return false;
    *mask=s->joint_mask&~1u;*motion=enabled?1:2;return true;
}
bool pe_animfollow_drive_sync(uint16_t bodies,uint16_t masters,uint16_t *out){
    if(!out||((bodies|masters)&~BODY_MASK))return false;
    *out=bodies&masters;return true;
}
bool pe_animfollow_drive_step(PeAnimFollowDrive *s,PeAnimFollowDriveTick in,PeAnimFollowDriveUpdate *out){
    if(!out||!valid(s))return false;
    PeAnimFollowDriveUpdate u={0};
    if(in.missing_assignments||!in.active){*out=u;return true;}
    if(in.secondary_update<=0||!nonnegative(in.torque_cap)||!nonnegative(in.force_cap)||!nonnegative(in.joint_cap)||
       !nonnegative(in.joint_damping)||!nonnegative(in.drag)||!nonnegative(in.angular_drag))return false;
    PeAnimFollowDrive next=*s;next.total_torque_error=next.total_force_error=(PeVec3){0};
    u.driven=true;u.nonrigid_begin=2;u.nonrigid_end=pe_af_drive_source_nonrigids-1;
    if(next.frame_counter%in.secondary_update==0){
        u.capture_nonrigid=next.mimic_nonrigids;
        if(!joints(&next,in.joint_cap,true,in.joint_damping,&u))return false;
    }
    u.write_nonrigid=next.frame_counter%2==0;
    u.blend_nonrigid=u.write_nonrigid&&in.secondary_update>2;
    if(u.blend_nonrigid)u.nonrigid_lerp=2.0f/(float)in.secondary_update;
    for(unsigned i=0;i<PE_AF_DRIVE_BODIES;i++){
        const PeAfDriveSourceBody *p=&pe_af_drive_source_bodies[i];PeAnimFollowDriveBodyInput body=in.bodies[i];
        if(!finite3(body.master_position)||!finite3(body.slave_position))return false;
        PeAnimFollowDriveBodyRequest *r=&u.bodies[i];
        r->set_drag=true;r->drag=in.drag;r->angular_drag=in.angular_drag;r->force_mode=2;
        if(next.torque_enabled){
            if(!isfinite(body.angle)||!finite3(body.axis))return false;
            float angle=body.angle>180?body.angle-360:body.angle;PeVec3 error=scale(body.axis,angle);
            if(!finite3(error))return false;
            if(body.angle==360)next.torque_signal=(PeVec3){0};
            else{
                next.total_torque_error=add(next.total_torque_error,error);
                if(!pd(pe_af_drive_source_PTorque*p->PTorqueProfile,pe_af_drive_source_DTorque,error,
                    &next.torque_last_error[i],next.reciprocal_fixed_delta_time,&next.torque_signal))return false;
            }
            if(!clamp(next.torque_signal,in.torque_cap*p->maxTorqueProfile,&next.torque_signal))return false;
            r->apply_torque=true;r->torque=next.torque_signal;
        }
        PeVec3 error=sub(body.master_position,body.slave_position);
        if(!finite3(error))return false;
        next.total_force_error=add(next.total_force_error,scale(error,p->forceErrorWeightProfile));
        if(next.force_enabled){
            if(!pd(pe_af_drive_source_PForce*p->PForceProfile,pe_af_drive_source_DForce,error,&next.force_last_error[i],
                  next.reciprocal_fixed_delta_time,&next.force_signal)||
               !clamp(next.force_signal,in.force_cap*p->maxForceProfile,&next.force_signal))return false;
            r->apply_force=true;r->force=next.force_signal;r->force_position=body.slave_position;
        }
        r->set_target_rotation=i>0&&(next.joint_mask&(1u<<i));
    }
    next.frame_counter=next.frame_counter==INT32_MAX?INT32_MIN:next.frame_counter+1;
    if(!valid(&next))return false;
    u.total_torque_error=next.total_torque_error;u.total_force_error=next.total_force_error;
    *s=next;*out=u;return true;
}
