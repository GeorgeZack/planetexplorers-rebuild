#include "pe_grounder_host.h"
#include "pe_human_phy.h"
#include <math.h>

/* The actor root's rotation, as pe_ik_drive_model_vector/world_vector. */
static PeVec3 to_world(PeVec3 m,float yaw_degrees){
    double yaw=(double)yaw_degrees*3.14159265358979323846/180,c=cos(yaw),s=sin(yaw);
    return (PeVec3){(float)(c*m.x+s*m.z),m.y,(float)(-s*m.x+c*m.z)};
}
static PeVec3 to_model(PeVec3 w,float yaw_degrees){
    double yaw=(double)yaw_degrees*3.14159265358979323846/180,c=cos(yaw),s=sin(yaw);
    return (PeVec3){(float)(c*w.x-s*w.z),w.y,(float)(s*w.x+c*w.z)};
}
static bool finite3(PeVec3 v){return isfinite(v.x)&&isfinite(v.y)&&isfinite(v.z);}
void pe_grounder_host_reset(PeGrounderHost *h){if(h)*h=(PeGrounderHost){0};}
bool pe_grounder_host_update(PeGrounderHost *h,const PeGrounderHostInput *in){
    if(!h)return false;
    h->active=false;
    if(!in||!in->ray||!isfinite(in->time)||!isfinite(in->delta_time)||in->delta_time<=0||!isfinite(in->yaw_degrees)||
       !finite3(in->root)||!finite3(in->foot[0])||!finite3(in->foot[1]))return false;
    PeGrounderHost next=*h;
    if(!next.started){
        /* Grounding.Initiate stores Time.deltaTime and the current positions. */
        if(!pe_grounder_init(&next.grounder,pe_grounder_female_settings(),
            (PeGrounderStart){in->delta_time,in->root,{in->foot[0],in->foot[1]}}))return false;
        next.started=true;next.forward_ground_angle=0;
    }
    const PeVec3 up={0,1,0};
    PeVec3 forward=to_world((PeVec3){0,0,1},in->yaw_degrees),right=to_world((PeVec3){1,0,0},in->yaw_degrees);
    /* UpdateGroundState: a contact within 90 degrees of world up sets the angle; a contact
     * outside that leaves it unchanged; no contact lets it decay. */
    if(in->ground_contact&&!finite3(in->ground_normal))return false;
    if(!in->ground_contact)next.forward_ground_angle=pe_human_phy_forward_angle_decay(next.forward_ground_angle,in->delta_time);
    else if(in->ground_normal.y>0)next.forward_ground_angle=pe_human_phy_slope_forward_angle(in->ground_normal,forward);
    if(!isfinite(next.forward_ground_angle)||!pe_grounder_set_live(&next.grounder,next.forward_ground_angle,in->spine_effect))return false;
    PeGrounderInput frame={.time=in->time,.delta_time=in->delta_time,.enabled=true,.weight=1,.root_position=in->root,
        .root_forward=forward,.root_right=right,.root_up=up,.foot={in->foot[0],in->foot[1]},.ray=in->ray,.context=in->context};
    PeGrounderOutput out;
    if(!pe_grounder_update(&next.grounder,&frame,&out)||!out.ran)return false;
    /* World vectors into the drawn model's frame; a rotation's vector part turns with them. */
    next.pose.pelvis_offset=to_model(out.pelvis_offset,in->yaw_degrees);
    for(unsigned e=0;e<PE_FBBIK_EFFECTORS;e++)next.effector_offset[e]=to_model(out.effector_offset[e],in->yaw_degrees);
    for(unsigned i=0;i<PE_GROUNDER_LEGS;i++){
        unsigned e=7+i;   /* FullBodyBipedEffector.LeftFoot, RightFoot */
        PeVec3 offset=to_model(out.foot_offset[i],in->yaw_degrees);
        next.effector_offset[e]=(PeVec3){next.effector_offset[e].x+offset.x,next.effector_offset[e].y+offset.y,next.effector_offset[e].z+offset.z};
        PeFbbikQuat q=out.foot_rotation[i];PeVec3 axis=to_model((PeVec3){q.x,q.y,q.z},in->yaw_degrees);
        next.pose.foot_rotation[i]=(PeFbbikQuat){axis.x,axis.y,axis.z,q.w};
    }
    next.grounded=out.grounded;next.root_grounded=out.root_grounded;next.active=true;
    *h=next;return true;
}
