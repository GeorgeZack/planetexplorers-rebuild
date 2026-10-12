#include "pe_running_attack_motion.h"
#include "pe_sword_motion.h"
#include <math.h>

bool pe_running_attack_motion_delta(const PeRootMotion *motion,const PeCombatInterval *interval,
                                    double body_yaw_degrees,double world_delta[3]){
    if(!motion||!interval||!world_delta||!interval->valid||
       interval->variant!=PE_COMBAT_RUNNING||interval->gender!=PE_COMBAT_FEMALE||
       interval->view!=PE_COMBAT_THIRD_PERSON||interval->stage!=0||!isfinite(body_yaw_degrees))return false;
    PeVec3 delta;
    if(!pe_root_motion_delta(motion,interval->from,interval->to,&delta))return false;
    const double radians=remainder(body_yaw_degrees,360.0)*0.017453292519943295769;
    const double c=cos(radians),s=sin(radians);
    world_delta[0]=c*delta.x+s*delta.z;world_delta[1]=delta.y;world_delta[2]=c*delta.z-s*delta.x;
    return true;
}
bool pe_running_attack_motion_request(const PeRootMotion *motion,
                                     const PeCombatInterval *interval,
                                     PeVec3 current_velocity,
                                     double body_yaw_degrees,double dt,
                                     PeVec3 *request){
    if(!request||!isfinite(current_velocity.x)||!isfinite(current_velocity.y)||!isfinite(current_velocity.z)||
       !isfinite(dt)||dt<=0)return false;
    double world_delta[3];
    if(!pe_running_attack_motion_delta(motion,interval,body_yaw_degrees,world_delta))return false;
    return pe_sword_motion_request(world_delta,current_velocity,dt,request);
}
