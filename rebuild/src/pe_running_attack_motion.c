#include "pe_running_attack_motion.h"
#include <math.h>

bool pe_running_attack_motion_request(const PeRootMotion *motion,
                                     const PeCombatInterval *interval,
                                     PeVec3 current_velocity,
                                     double body_yaw_degrees,double dt,
                                     PeVec3 *request){
    if(!motion||!interval||!request||!interval->valid||
       interval->variant!=PE_COMBAT_RUNNING||interval->gender!=PE_COMBAT_FEMALE||
       interval->view!=PE_COMBAT_THIRD_PERSON||interval->stage!=0||
       !isfinite(current_velocity.x)||!isfinite(current_velocity.y)||!isfinite(current_velocity.z)||
       !isfinite(body_yaw_degrees)||!isfinite(dt)||dt<=0)return false;
    PeVec3 delta;
    if(!pe_root_motion_delta(motion,interval->from,interval->to,&delta))return false;
    const double radians=remainder(body_yaw_degrees,360.0)*0.017453292519943295769;
    const double c=cos(radians),s=sin(radians);
    double x=(c*delta.x+s*delta.z)/dt,z=(c*delta.z-s*delta.x)/dt;
    if(!isfinite(x)||!isfinite(z))return false;
    const double dx=x-current_velocity.x,dz=z-current_velocity.z,length=hypot(dx,dz);
    if(!isfinite(length))return false;
    if(length>=12.0){
        x=current_velocity.x+12.0*(dx/length);
        z=current_velocity.z+12.0*(dz/length);
    }
    const PeVec3 result={(float)x,0,(float)z};
    if(!isfinite(result.x)||!isfinite(result.z))return false;
    *request=result;return true;
}
