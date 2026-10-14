#include "pe_human_phy.h"
#include "../generated/pe_human_phy_source.h"
#include <float.h>
#include <limits.h>
#include <math.h>

/* Vector helpers follow the pinned Unity5 Vector3 CIL: Normalize returns zero
 * at or below magnitude 1e-5, equality is squared distance below ~1e-10, and
 * Project returns zero when the normal's squared length is below
 * Mathf.Epsilon. Float evaluation is not claimed bit-exact with Mono. */
static const PeVec3 zero={0,0,0},world_up={0,1,0};
static bool finite3(PeVec3 v){return isfinite(v.x)&&isfinite(v.y)&&isfinite(v.z);}
static PeVec3 add(PeVec3 a,PeVec3 b){return (PeVec3){a.x+b.x,a.y+b.y,a.z+b.z};}
static PeVec3 sub(PeVec3 a,PeVec3 b){return (PeVec3){a.x-b.x,a.y-b.y,a.z-b.z};}
static PeVec3 scale(PeVec3 v,float n){return (PeVec3){v.x*n,v.y*n,v.z*n};}
static float dot(PeVec3 a,PeVec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static float square(PeVec3 v){return dot(v,v);}
static float magnitude(PeVec3 v){return sqrtf(square(v));}
static bool same(PeVec3 a,PeVec3 b){return square(sub(a,b))<9.99999944e-11f;}
static PeVec3 normalized(PeVec3 v){
    float m=magnitude(v);
    return m>9.99999975e-06f?(PeVec3){v.x/m,v.y/m,v.z/m}:zero;
}
static PeVec3 cross(PeVec3 a,PeVec3 b){
    return (PeVec3){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
}
/* Vector3.ProjectOnPlane and the first-pass Util.ProjectOntoPlane share this body. */
static PeVec3 on_plane(PeVec3 v,PeVec3 normal){
    float n=dot(normal,normal);
    /* Mathf.Epsilon is the smallest normal or denormal float depending on the
     * runtime's flush-to-zero probe, which is not qualified here. The callers
     * pass unit or exactly zero normals, where both give the same result. */
    if(n<FLT_MIN)return v;
    float d=dot(v,normal);
    return sub(v,(PeVec3){normal.x*d/n,normal.y*d/n,normal.z*d/n});
}
static float angle(PeVec3 a,PeVec3 b){
    float d=dot(normalized(a),normalized(b));
    d=d<-1?-1:d>1?1:d;
    return (float)acos((double)d)*57.29578f;
}
static bool valid(const PeHumanPhy *p){
    return p&&finite3(p->request_velocity)&&finite3(p->applied_velocity)&&finite3(p->inertia_velocity)&&
        finite3(p->desired_direction)&&finite3(p->gravity_up)&&finite3(p->water_acc)&&finite3(p->sub_acc)&&
        isfinite(p->move_speed)&&isfinite(p->net_move_speed_scale)&&isfinite(p->gravity)&&isfinite(p->drag)&&
        isfinite(p->forward_ground_angle)&&isfinite(p->impact_time)&&isfinite(p->last_ground_time)&&
        isfinite(p->water_to_root)&&isfinite(p->water_height)&&p->fixed_updates>=0;
}
void pe_human_phy_init(PeHumanPhy *p){
    if(!p)return;
    /* Constructor and serialized female values, then Awake. impact_time and
     * last_ground_time start at 0, as their unassigned source fields do. */
    *p=(PeHumanPhy){.gravity_up=world_up,.move_speed=pe_human_phy_source_default_move_speed,
        .net_move_speed_scale=pe_human_phy_source_net_move_speed_scale,
        .gravity=pe_human_phy_source_gravity_y,.is_controller=pe_human_phy_source_is_controller!=0};
}
bool pe_human_phy_reset_speed(PeHumanPhy *p,float speed){
    if(!valid(p)||!isfinite(speed))return false;
    if(speed<pe_human_phy_source_minimum_move_speed)speed=pe_human_phy_source_minimum_move_speed;
    p->move_speed=speed*pe_human_phy_source_speed_times;return true;
}
bool pe_human_phy_apply_move_request(PeHumanPhy *p,PeVec3 velocity){
    if(!valid(p)||!finite3(velocity))return false;
    PeVec3 next=add(p->request_velocity,velocity);
    if(magnitude(next)>pe_human_phy_source_max_sub_speed)
        next=scale(normalized(next),pe_human_phy_source_max_sub_speed);
    if(!finite3(next))return false;
    p->request_velocity=next;return true;
}
bool pe_human_phy_cancel_move_request(PeHumanPhy *p){
    if(!valid(p))return false;
    p->request_velocity=zero;p->applied_velocity=zero;return true;
}
bool pe_human_phy_apply_impact(PeHumanPhy *p,float time){
    if(!valid(p)||!isfinite(time))return false;
    p->impact_time=time;p->grounded=p->phy_ground=false;return true;
}
static void ground_state(PeHumanPhy *p,const PeHumanPhyFrame *in){
    bool was=p->grounded,contacts=in->ground_normal_count>0;
    if(!(in->time-p->impact_time<pe_human_phy_source_leave_ground_time)){
        p->phy_ground=p->is_controller?contacts||in->rigidbody_kinematic:in->overlap_ground;
        if(p->phy_ground)p->last_ground_time=in->time;
        p->grounded=in->time-p->last_ground_time<pe_human_phy_source_leave_ground_time;
    }else p->grounded=p->phy_ground=false;
    p->fall_ground=!was&&p->grounded;
    if(p->spine_in_water||!contacts){
        p->gravity_up=world_up;
        float from=p->is_controller?p->forward_ground_angle:0; /* get_forwardGroundAngle */
        p->forward_ground_angle=pe_human_phy_forward_angle_decay(from,in->delta_time);
        return;
    }
    PeVec3 slope=zero;p->gravity_up=zero;
    if(!in->rigidbody_kinematic)for(size_t i=0;i<in->ground_normal_count;i++){
        PeVec3 normal=in->ground_normals[i];float a=angle(normal,world_up);
        if(a<pe_human_phy_source_max_ground_balance_angle)p->gravity_up=add(p->gravity_up,normal);
        if(a<90)slope=add(slope,normal);
    }
    if(!same(zero,slope)){
        /* A zero sum leaves the previous forward angle untouched. */
        p->forward_ground_angle=pe_human_phy_slope_forward_angle(slope,in->forward);
    }
    p->gravity_up=same(zero,p->gravity_up)?world_up:normalized(p->gravity_up);
}
float pe_human_phy_slope_forward_angle(PeVec3 slope,PeVec3 forward){
    const PeVec3 world_up={0,1,0};
    slope=normalized(slope);forward=on_plane(forward,world_up);
    return angle(on_plane(slope,cross(world_up,forward)),forward)-90;
}
float pe_human_phy_forward_angle_decay(float current,float delta_time){
    float t=pe_human_phy_source_forward_angle_lerp_speed*delta_time;
    return current+(0-current)*(t<0?0:t>1?1:t);
}
static void water_state(PeHumanPhy *p,const PeHumanPhyFrame *in){
    p->spine_in_water=p->feet_in_water=p->head_in_water=false;
    if(in->point_in_water[p->grounded?1:0]>.5f){
        p->water_height=in->water_ray_hit?in->water_ray_y:
            in->position.y+pe_human_phy_source_human_height+.5f;
        float depth=p->water_height-in->position.y;
        p->water_to_root=depth<0?0:depth>pe_human_phy_source_human_height?pe_human_phy_source_human_height:depth;
        p->feet_in_water=p->water_to_root>pe_human_phy_source_feet_height;
        p->spine_in_water=p->water_to_root>pe_human_phy_source_spine_height;
        p->head_in_water=!(p->water_to_root+1.1920929e-07f<pe_human_phy_source_human_height);
    }
    p->water_acc=scale(world_up,p->spine_in_water?-p->gravity:0);
}
bool pe_human_phy_update(PeHumanPhy *p,const PeHumanPhyFrame *in){
    if(!valid(p)||!in||!isfinite(in->time)||!isfinite(in->delta_time)||in->delta_time<0||
       !finite3(in->position)||!finite3(in->forward)||(in->ground_normal_count&&!in->ground_normals)||
       !isfinite(in->point_in_water[0])||!isfinite(in->point_in_water[1])||!isfinite(in->water_ray_y))return false;
    for(size_t i=0;i<in->ground_normal_count;i++)if(!finite3(in->ground_normals[i]))return false;
    PeHumanPhy next=*p;
    next.fixed_updates=0;
    ground_state(&next,in);water_state(&next,in);
    next.drag=next.grounded?pe_human_phy_source_ground_drag:
        next.spine_in_water?pe_human_phy_source_water_drag:pe_human_phy_source_air_drag;
    if(!valid(&next))return false;
    *p=next;return true;
}
bool pe_human_phy_late_update(PeHumanPhy *p,PeVec3 velocity){
    if(!valid(p)||!finite3(velocity))return false;
    p->inertia_velocity=velocity;p->applied_velocity=p->request_velocity;p->request_velocity=zero;
    return true;
}
bool pe_human_phy_fixed_update(PeHumanPhy *p,PeHumanPhyStep in,PeHumanPhyForces *out){
    if(!out||!valid(p)||!finite3(in.velocity)||!finite3(in.position)||!finite3(in.up)||
       !isfinite(in.fixed_delta_time)||in.fixed_delta_time<0)return false;
    PeHumanPhyForces result={0};
    if(p->freeze_update){*out=result;return true;}
    /* get_desiredVelocity: the movement direction alone, without requests. */
    PeVec3 own=same(p->desired_direction,zero)?zero:scale(p->desired_direction,p->move_speed);
    PeVec3 desired=scale(add(own,p->applied_velocity),p->net_move_speed_scale),position=in.position;
    int count=p->fixed_updates;
    if(in.rigidbody_kinematic){
        /* The counter is not advanced on this branch. */
        result.ran=result.kinematic=result.zero_velocity=true;
        if(!p->is_controller){
            result.move_position=true;
            result.move_position_target=add(position,scale(desired,in.fixed_delta_time));
            if(!finite3(result.move_position_target))return false;
        }
        *out=result;return true;
    }
    if(count>=pe_human_phy_source_fixed_updates_per_update){
        /* The source still increments its counter before returning. */
        if(count<INT_MAX)p->fixed_updates=count+1;
        *out=result;return true;
    }
    result.ran=true;
    if(!p->grounded&&p->feet_in_water&&p->sub_acc.y<=0&&desired.y>0&&
       p->water_to_root>pe_human_phy_source_spine_height-pe_human_phy_source_spine_fixed_height&&
       p->water_to_root+1.1920929e-07f<pe_human_phy_source_human_height){
        desired.y=0;
        PeVec3 hold=position;
        hold.y=p->water_height-pe_human_phy_source_spine_height-pe_human_phy_source_spine_fixed_height;
        if(position.y-hold.y<pe_human_phy_source_spine_height){
            position=hold;result.set_position=true;
        }
    }
    PeVec3 force=zero,velocity=in.velocity;
    if(!same(desired,zero))
        force=p->phy_ground||p->spine_in_water||p->use_rope_gun?sub(desired,velocity):
            sub(on_plane(desired,in.up),on_plane(velocity,in.up));
    float speed_square=square(velocity);
    if(speed_square>pe_human_phy_source_overspeed_square)force=scale(velocity,-1);
    result.velocity_change=force;
    result.acceleration=add(add(scale(p->gravity_up,p->gravity),p->water_acc),p->sub_acc);
    if(same(desired,zero))
        result.acceleration=add(result.acceleration,scale(normalized(velocity),-p->drag*speed_square));
    if(same(own,zero)&&!same(p->sub_acc,zero)&&
       speed_square<pe_human_phy_source_stop_speed*pe_human_phy_source_stop_speed)
        result.velocity_change=add(result.velocity_change,sub(zero,velocity)); /* set_velocity(zero) */
    if(position.y<pe_human_phy_source_rescue_below_y){
        position.y=pe_human_phy_source_rescue_y;result.set_position=true;
        result.velocity_change=add(result.velocity_change,scale(velocity,-1));
    }
    if(speed_square>pe_human_phy_source_max_speed*pe_human_phy_source_max_speed)
        result.velocity_change=add(result.velocity_change,
            scale(scale(normalized(velocity),-1),magnitude(velocity)-pe_human_phy_source_max_speed));
    result.position=position;
    if(!finite3(result.velocity_change)||!finite3(result.acceleration)||!finite3(result.position))return false;
    p->fixed_updates=count+1;*out=result;return true;
}
